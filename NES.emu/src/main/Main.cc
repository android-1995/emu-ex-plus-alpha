/*  This file is part of NES.emu.

	NES.emu is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	NES.emu is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with NES.emu.  If not, see <http://www.gnu.org/licenses/> */

#define LOGTAG "main"
#include <emuframework/EmuApp.hh>
#include <emuframework/EmuAppInlines.hh>
#include <emuframework/EmuAudio.hh>
#include <emuframework/EmuVideo.hh>
#include "internal.hh"
#include "EmuFileIO.hh"
#include <fceu/driver.h>
#include <fceu/state.h>
#include <fceu/fceu.h>
#include <fceu/ppu.h>
#include <fceu/fds.h>
#include <fceu/input.h>
#include <fceu/cheat.h>
#include <fceu/video.h>
#include <fceu/sound.h>
#include <fceu/palette.h>

using PalArray = std::array<pal, 512>;
void ApplyDeemphasisComplete(pal* pal512);
void FCEU_setDefaultPalettePtr(pal *ptr);

const char *EmuSystem::creditsViewStr = CREDITS_INFO_STRING "(c) 2011-2021\nRobert Broglia\nwww.explusalpha.com\n\nPortions (c) the\nFCEUX Team\nfceux.com";
bool EmuSystem::hasCheats = true;
bool EmuSystem::hasPALVideoSystem = true;
bool EmuSystem::hasResetModes = true;
uint fceuCheats = 0;
ESI nesInputPortDev[2]{SI_UNSET, SI_UNSET};
uint autoDetectedRegion = 0;
static constexpr auto pixFmt = IG::PIXEL_FMT_RGB565;
const char *fceuReturnedError = {};
static PalArray defaultPal{};
static uint16 nativeCol[256]{};
static const uint nesPixX = 256, nesPixY = 240, nesVisiblePixY = 224;
static uint8 XBufData[256 * 256 + 16]{};
// Separate front & back buffers not needed for our video implementation
uint8 *XBuf = XBufData;
uint8 *XBackBuf = XBufData;
uint8 *XDBuf{};
uint8 *XDBackBuf{};
int dendy = 0;
bool paldeemphswap = false;
bool swapDuty = false;

bool hasFDSBIOSExtension(const char *name)
{
	return string_hasDotExtension(name, "rom") || string_hasDotExtension(name, "bin");
}

static bool hasFDSExtension(const char *name)
{
	return string_hasDotExtension(name, "fds");
}

static bool hasROMExtension(const char *name)
{
	return string_hasDotExtension(name, "nes") || string_hasDotExtension(name, "unf");
}

static bool hasNESExtension(const char *name)
{
	return hasROMExtension(name) || hasFDSExtension(name);
}

const BundledGameInfo &EmuSystem::bundledGameInfo(uint idx)
{
	static const BundledGameInfo info[]
	{
		{ "Test Game", "game.7z" }
	};

	return info[0];
}

const char *EmuSystem::shortSystemName()
{
	return "FC-NES";
}

const char *EmuSystem::systemName()
{
	return "Famicom (Nintendo Entertainment System)";
}

static void setDirOverrides()
{
	FCEUI_SetBaseDirectory(EmuSystem::savePath());
	FCEUI_SetDirOverride(FCEUIOD_NV, EmuSystem::savePath());
	FCEUI_SetDirOverride(FCEUIOD_CHEATS, EmuSystem::savePath());
}

EmuSystem::NameFilterFunc EmuSystem::defaultFsFilter = hasNESExtension;
EmuSystem::NameFilterFunc EmuSystem::defaultBenchmarkFsFilter = hasNESExtension;

void EmuSystem::reset(ResetMode mode)
{
	assert(gameIsRunning());
	if(mode == RESET_HARD)
		FCEUI_PowerNES();
	else
		FCEUI_ResetNES();
}

const char *saveSlotCharAiWu(int slot)
{
    switch(slot)
    {
        case -1: return "10";
        case 0: return "0";
        case 1: return "1";
        case 2: return "2";
        case 3: return "3";
        case 4: return "4";
        case 5: return "5";
        case 6: return "6";
        case 7: return "7";
        case 8: return "8";
        case 9: return "9";
        default: return "10";
    }
}

FS::PathString EmuSystem::sprintStateFilename(int slot, const char *statePath, const char *gameName)
{
	return FS::makePathStringPrintf("%s/%s.fc%s", statePath, gameName, saveSlotCharAiWu(slot));
}

EmuSystem::Error EmuSystem::saveState(const char *path)
{
	if(!FCEUI_SaveState(path))
		return EmuSystem::makeFileWriteError();
	else
		return {};
}

EmuSystem::Error EmuSystem::loadState(const char *path)
{
	if(!FCEUI_LoadState(path))
		return EmuSystem::makeFileReadError();
	else
		return {};
}

void EmuSystem::saveBackupMem() // for manually saving when not closing game
{
	if(gameIsRunning())
	{
		logMsg("saving backup memory if needed");
		// TODO: fix iOS permissions if needed
		if(isFDS)
			FCEU_FDSWriteModifiedDisk();
		else
			GameInterface(GI_WRITESAVE);
		FCEU_FlushGameCheats(nullptr, 0, false);
	}
}

void EmuSystem::closeSystem()
{
	FCEUI_CloseGame();
	fceuCheats = 0;
}

void FCEUD_SetPalette(uint8 index, uint8 r, uint8 g, uint8 b)
{
	// RGB565
	nativeCol[index] = pixFmt.desc().build(r >> 3, g >> 2, b >> 3, 0);
	//logMsg("set palette %d %X", index, nativeCol[index]);
}

void FCEUD_GetPalette(uint8 index, uint8 *r, uint8 *g, uint8 *b)
{
	bug_unreachable("called FCEUD_GetPalette()");
}

static void setDefaultPalette(IO &io)
{
	auto bytesRead = io.read(defaultPal.data(), 512);
	if(bytesRead < 192)
	{
		logErr("skipped palette with only %d bytes", (int)bytesRead);
		return;
	}
	if(bytesRead != 512)
	{
		ApplyDeemphasisComplete(defaultPal.data());
	}
	FCEU_setDefaultPalettePtr(defaultPal.data());
}

void setDefaultPalette(const char *palPath)
{
	if(!palPath || !strlen(palPath))
	{
		FCEU_setDefaultPalettePtr(nullptr);
		return;
	}
	logMsg("setting default palette with path:%s", palPath);
	if(palPath[0] != '/')
	{
		// load as asset
		auto io = EmuApp::openAppAssetIO(FS::makePathStringPrintf("palette/%s", palPath), IO::AccessHint::ALL);
		if(!io)
			return;
		setDefaultPalette(io);
	}
	else
	{
		FileIO io{};
		io.open(palPath, IO::AccessHint::ALL);
		if(!io)
			return;
		setDefaultPalette(io);
	}
}

static void cacheUsingZapper()
{
	assert(GameInfo);
	iterateTimes(2, i)
	{
		if(joyports[i].type == SI_ZAPPER)
		{
			usingZapper = true;
			return;
		}
	}
	usingZapper = false;
}

static const char* fceuInputToStr(int input)
{
	switch(input)
	{
		case SI_UNSET: return "Unset";
		case SI_GAMEPAD: return "Gamepad";
		case SI_ZAPPER: return "Zapper";
		case SI_NONE: return "None";
		default: bug_unreachable("input == %d", input); return 0;
	}
}

void setupNESFourScore()
{
	if(!GameInfo)
		return;
	if(!usingZapper)
	{
		if(optionFourScore)
			logMsg("attaching four score");
		FCEUI_SetInputFourscore(optionFourScore);
	}
	else
		FCEUI_SetInputFourscore(0);
}

bool EmuSystem::vidSysIsPAL()
{
	return PAL || dendy;
}

void setupNESInputPorts()
{
	if(!GameInfo)
		return;
	iterateTimes(2, i)
	{
		if(nesInputPortDev[i] == SI_UNSET) // user didn't specify device, go with auto settings
			connectNESInput(i, GameInfo->input[i] == SI_UNSET ? SI_GAMEPAD : GameInfo->input[i]);
		else
			connectNESInput(i, nesInputPortDev[i]);
		logMsg("attached %s to port %d%s", fceuInputToStr(joyports[i].type), i, nesInputPortDev[i] == SI_UNSET ? " (auto)" : "");
	}
	cacheUsingZapper();
	setupNESFourScore();
}

static int cheatCallback(char *name, uint32 a, uint8 v, int compare, int s, int type, void *data)
{
	logMsg("cheat: %s, %d", name, s);
	fceuCheats++;
	return 1;
}

const char *regionToStr(int region)
{
	switch(region)
	{
		case 0: return "NTSC";
		case 1: return "PAL";
		case 2: return "Dendy";
	}
	return "Unknown";
}

static int regionFromName(const char *name)
{
	if(strstr(name, "(E)") || strstr(name, "(e)") || strstr(name, "(EU)")
		|| strstr(name, "(Europe)") || strstr(name, "(PAL)")
		|| strstr(name, "(F)") || strstr(name, "(f)")
		|| strstr(name, "(G)") || strstr(name, "(g)")
		|| strstr(name, "(I)") || strstr(name, "(i)"))
	{
		return 1; // PAL
	}
	else if(strstr(name, "(RU)") || strstr(name, "(ru)"))
	{
		return 2; // Dendy
	}
	return 0; // NTSC
}

void setRegion(int region, int defaultRegion, int detectedRegion)
{
	if(region)
	{
		logMsg("Forced region:%s", regionToStr(region - 1));
		FCEUI_SetRegion(region - 1, false);
	}
	else if(defaultRegion)
	{
		logMsg("Forced region (Default):%s", regionToStr(defaultRegion - 1));
		FCEUI_SetRegion(defaultRegion - 1, false);
	}
	else
	{
		logMsg("Detected region:%s", regionToStr(detectedRegion));
		FCEUI_SetRegion(detectedRegion, false);
	}
}

EmuSystem::Error EmuSystem::loadGame(IO &io, EmuSystemCreateParams, OnLoadProgressDelegate)
{
	setDirOverrides();
	auto ioStream = new EmuFileIO(io);
	auto file = new FCEUFILE();
	file->filename = fullGamePath();
	file->logicalPath = fullGamePath();
	file->fullFilename = fullGamePath();
	file->archiveIndex = -1;
	file->stream = ioStream;
	file->size = ioStream->size();
	fceuReturnedError = {};
	if(!FCEUI_LoadGameWithFile(file, originalGameFileName().data(), 0))
	{
		if(fceuReturnedError)
			return EmuSystem::makeError("%s", fceuReturnedError);
		else
			return EmuSystem::makeError("Error loading game");
	}
	autoDetectedRegion = regionFromName(gameFileName().data());
	setRegion(optionVideoSystem.val, optionDefaultVideoSystem.val, autoDetectedRegion);
	FCEUI_ListCheats(cheatCallback, 0);
	if(fceuCheats)
		logMsg("%d total cheats", fceuCheats);

	setupNESInputPorts();

	return {};
}

void EmuSystem::onPrepareAudio(EmuAudio &audio)
{
	audio.setStereo(false);
}

void EmuSystem::onPrepareVideo(EmuVideo &video)
{
	video.setFormat({{nesPixX, nesVisiblePixY}, pixFmt});
}

void EmuSystem::configAudioRate(IG::FloatSeconds frameTime, uint32_t rate)
{
	constexpr double ntscFrameRate = 21477272.0 / 357366.0;
	constexpr double palFrameRate = 21281370.0 / 425568.0;
	double systemFrameRate = vidSysIsPAL() ? palFrameRate : ntscFrameRate;
	double mixRate = std::round(rate * (systemFrameRate * frameTime.count()));
	FCEUI_Sound(mixRate);
	logMsg("set NES audio rate %d", FSettings.SndRate);
}

void emulateSound(EmuAudio *audio)
{
	const uint maxAudioFrames = EmuSystem::audioFramesPerVideoFrame+32;
	int32 sound[maxAudioFrames];
	uint frames = FlushEmulateSound(sound);
	//logMsg("%d frames", frames);
	assert(frames <= maxAudioFrames);
	if(audio)
	{
		int16 sound16[maxAudioFrames];
		iterateTimes(maxAudioFrames, i)
		{
			sound16[i] = sound[i];
		}
		audio->writeFrames(sound16, frames);
	}
}

void FCEUPPU_FrameReady(EmuSystemTask *task, EmuVideo *video, uint8 *buf)
{
	if(!video)
	{
		return;
	}
	if(unlikely(!buf))
	{
		video->startUnchangedFrame(task);
		return;
	}
	auto img = video->startFrame(task);
	auto pix = img.pixmap();
	IG::Pixmap ppuPix{{{256, 256}, IG::PIXEL_FMT_I8}, buf};
	auto ppuPixRegion = ppuPix.subView({0, 8}, {256, 224});
	pix.writeTransformed([](uint8 p){ return nativeCol[p]; }, ppuPixRegion);
	img.endFrame();
}

void EmuSystem::runFrame(EmuSystemTask *task, EmuVideo *video, EmuAudio *audio)
{
	bool skip = !video && !optionCompatibleFrameskip;
	FCEUI_Emulate(task, video, skip, audio);
}

void EmuSystem::savePathChanged()
{
	if(gameIsRunning())
		setDirOverrides();
}

void EmuApp::onCustomizeNavView(EmuApp::NavView &view)
{
	const Gfx::LGradientStopDesc navViewGrad[] =
	{
		{ .0, Gfx::VertexColorPixelFormat.build(.5, .5, .5, 1.) },
		{ .03, Gfx::VertexColorPixelFormat.build(1. * .4, 0., 0., 1.) },
		{ .3, Gfx::VertexColorPixelFormat.build(1. * .4, 0., 0., 1.) },
		{ .97, Gfx::VertexColorPixelFormat.build(.5 * .4, 0., 0., 1.) },
		{ 1., Gfx::VertexColorPixelFormat.build(.5, .5, .5, 1.) },
	};
	view.setBackgroundGradient(navViewGrad);
}

EmuSystem::Error EmuSystem::onInit()
{
	backupSavestates = 0;
	if(!FCEUI_Initialize())
	{
		return makeError("Error in FCEUI_Initialize");
	}
	return {};
}

//region爱吾
void EmuSystem::setCheatListAiWu(std::list<std::string> cheats)
{
    setCheatListForAiWu(cheats);
}
//endregion
