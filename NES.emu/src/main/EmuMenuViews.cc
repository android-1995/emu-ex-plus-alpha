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

#include <emuframework/EmuApp.hh>
#include <emuframework/OptionView.hh>
#include <emuframework/EmuSystemActionsView.hh>
#include <emuframework/FilePicker.hh>
#include <imagine/gui/AlertView.hh>
#include "EmuCheatViews.hh"
#include "internal.hh"
#include <fceu/fds.h>
#include <fceu/sound.h>
#include <fceu/fceu.h>

extern int pal_emulation;

class ConsoleOptionView : public TableView
{
	BoolMenuItem fourScore
	{
		"4-Player Adapter",
		(bool)optionFourScore,
		[this](BoolMenuItem &item, View &, Input::Event e)
		{
			EmuSystem::sessionOptionSet();
			optionFourScore = item.flipBoolValue(*this);
			setupNESFourScore();
		}
	};

	TextMenuItem inputPortsItem[4]
	{
		{"Auto", [](){ setInputPorts(SI_UNSET, SI_UNSET); }},
		{"Gamepads", [](){ setInputPorts(SI_GAMEPAD, SI_GAMEPAD); }},
		{"Gun (2P, NES)", [](){ setInputPorts(SI_GAMEPAD, SI_ZAPPER); }},
		{"Gun (1P, VS)", [](){ setInputPorts(SI_ZAPPER, SI_GAMEPAD); }},
	};

	MultiChoiceMenuItem inputPorts
	{
		"Input Ports",
		[]()
		{
			if(nesInputPortDev[0] == SI_GAMEPAD && nesInputPortDev[1] == SI_GAMEPAD)
				return 1;
			else if(nesInputPortDev[0] == SI_GAMEPAD && nesInputPortDev[1] == SI_ZAPPER)
				return 2;
			else if(nesInputPortDev[0] == SI_ZAPPER && nesInputPortDev[1] == SI_GAMEPAD)
				return 3;
			else
				return 0;
		}(),
		inputPortsItem
	};

	static void setInputPorts(ESI port1, ESI port2)
	{
		EmuSystem::sessionOptionSet();
		optionInputPort1 = (int)port1;
		optionInputPort2 = (int)port2;
		nesInputPortDev[0] = port1;
		nesInputPortDev[1] = port2;
		setupNESInputPorts();
	}

	TextMenuItem videoSystemItem[4]
	{
		{"自动", [this](TextMenuItem &, View &, Input::Event e){ setVideoSystem(0, e); }},
		{"NTSC", [this](TextMenuItem &, View &, Input::Event e){ setVideoSystem(1, e); }},
		{"PAL", [this](TextMenuItem &, View &, Input::Event e){ setVideoSystem(2, e); }},
		{"Dendy", [this](TextMenuItem &, View &, Input::Event e){ setVideoSystem(3, e); }},
	};

	MultiChoiceMenuItem videoSystem
	{
		"视频系统",
		[this](uint32_t idx, Gfx::Text &t)
		{
			if(idx == 0)
			{
				t.setString(dendy ? "Dendy" : pal_emulation ? "PAL" : "NTSC");
				return true;
			}
			return false;
		},
		optionVideoSystem,
		videoSystemItem
	};

	void setVideoSystem(int val, Input::Event e)
	{
		EmuSystem::sessionOptionSet();
		optionVideoSystem = val;
		setRegion(val, optionDefaultVideoSystem.val, autoDetectedRegion);
		EmuApp::promptSystemReloadDueToSetOption(attachParams(), e);
	}

	BoolMenuItem compatibleFrameskip
	{
		"跳帧模式",
		(bool)optionCompatibleFrameskip,
		"快速", "兼容",
		[this](BoolMenuItem &item, View &, Input::Event e)
		{
			if(!item.boolValue())
			{
				auto ynAlertView = makeView<YesNoAlertView>(
					"如果游戏在快进/跳帧时出现故障，请使用兼容模式，快速模式会增加CPU使用率。");
				ynAlertView->setOnYes(
					[this, &item]()
					{
						EmuSystem::sessionOptionSet();
						optionCompatibleFrameskip = item.flipBoolValue(*this);
					});
				EmuApp::pushAndShowModalView(std::move(ynAlertView), e);
			}
			else
			{
				optionCompatibleFrameskip = item.flipBoolValue(*this);
			}
		}
	};

	std::array<MenuItem*, 4> menuItem
	{
		&inputPorts,
		&fourScore,
		&videoSystem,
		&compatibleFrameskip,
	};

public:
	ConsoleOptionView(ViewAttachParams attach):
		TableView
		{
			"控制台选项",
			attach,
			menuItem
		}
	{}
};

class CustomVideoOptionView : public VideoOptionView
{
	BoolMenuItem spriteLimit
	{
		"Sprite 限制",
		(bool)optionSpriteLimit,
		[this](BoolMenuItem &item, View &, Input::Event e)
		{
			optionSpriteLimit = item.flipBoolValue(*this);
			FCEUI_DisableSpriteLimitation(!optionSpriteLimit);
		}
	};

	TextMenuItem videoSystemItem[4]
	{
		{"自动", [this](TextMenuItem &, View &, Input::Event e){ optionDefaultVideoSystem = 0; }},
		{"NTSC", [this](TextMenuItem &, View &, Input::Event e){ optionDefaultVideoSystem = 1; }},
		{"PAL", [this](TextMenuItem &, View &, Input::Event e){ optionDefaultVideoSystem = 2; }},
		{"Dendy", [this](TextMenuItem &, View &, Input::Event e){ optionDefaultVideoSystem = 3; }},
	};

	MultiChoiceMenuItem videoSystem
	{
		"默认视频系统",
		optionDefaultVideoSystem,
		videoSystemItem
	};

	static constexpr const char *firebrandXPalPath = "Smooth (FBX).pal";
	static constexpr const char *wavebeamPalPath = "Wavebeam.pal";
	static constexpr const char *classicPalPath = "Classic (FBX).pal";

	static void setPalette(const char *palPath)
	{
		if(palPath)
			string_copy(defaultPalettePath, palPath);
		else
			defaultPalettePath = {};
		setDefaultPalette(palPath);
	}

	constexpr uint32_t defaultPaletteCustomFileIdx()
	{
		return std::size(defaultPalItem) - 1;
	}

	TextMenuItem defaultPalItem[5]
	{
		{"FCEUX", [](){ setPalette({}); }},
		{"FirebrandX", []() { setPalette(firebrandXPalPath); }},
		{"Wavebeam", []() { setPalette(wavebeamPalPath); }},
		{"经典", []() { setPalette(classicPalPath); }},
		{"自定义文件", [this](TextMenuItem &, View &, Input::Event e)
			{
				auto startPath = EmuApp::mediaSearchPath();
				auto fsFilter = [](const char *name)
					{
						return string_hasDotExtension(name, "pal");
					};
				auto fPicker = makeView<EmuFilePicker>(startPath.data(), false, fsFilter, FS::RootPathInfo{}, e, false, false);
				fPicker->setOnSelectFile(
					[this](FSPicker &picker, const char *name, Input::Event)
					{
						setPalette(picker.makePathString(name).data());
						defaultPal.setSelected(defaultPaletteCustomFileIdx());
						dismissPrevious();
						picker.dismiss();
					});
				EmuApp::pushAndShowModalView(std::move(fPicker), e);
				return false;
			}},
	};

	MultiChoiceMenuItem defaultPal
	{
		"默认调色板",
		[this](uint32_t idx, Gfx::Text &t)
		{
			if(idx == defaultPaletteCustomFileIdx())
			{
				auto paletteName = FS::basename(defaultPalettePath);
				t.setString(FS::makeFileStringWithoutDotExtension(paletteName).data());
				return true;
			}
			return false;
		},
		[this]()
		{
			if(string_equal(defaultPalettePath.data(), ""))
				return 0;
			if(string_equal(defaultPalettePath.data(), firebrandXPalPath))
				return 1;
			else if(string_equal(defaultPalettePath.data(), wavebeamPalPath))
				return 2;
			else if(string_equal(defaultPalettePath.data(), classicPalPath))
				return 3;
			else
				return (int)defaultPaletteCustomFileIdx();
		}(),
		defaultPalItem
	};

public:
	CustomVideoOptionView(ViewAttachParams attach): VideoOptionView{attach, true}
	{
		loadStockItems();
		item.emplace_back(&systemSpecificHeading);
		item.emplace_back(&defaultPal);
		item.emplace_back(&videoSystem);
		item.emplace_back(&spriteLimit);
	}
};

class CustomAudioOptionView : public AudioOptionView
{
	static void setQuality(int quaility)
	{
		optionSoundQuality = quaility;
		FCEUI_SetSoundQuality(quaility);
	}

	TextMenuItem qualityItem[3]
	{
		{"正常", [](){ setQuality(0); }},
		{"高", []() { setQuality(1); }},
		{"最高", []() { setQuality(2); }}
	};

	MultiChoiceMenuItem quality
	{
		"模拟质量",
		optionSoundQuality,
		qualityItem
	};

public:
	CustomAudioOptionView(ViewAttachParams attach): AudioOptionView{attach, true}
	{
		loadStockItems();
		item.emplace_back(&quality);
	}
};

class CustomSystemOptionView : public SystemOptionView
{
	TextMenuItem fdsBiosPath
	{
		nullptr,
		[this](TextMenuItem &, View &, Input::Event e)
		{
			auto biosSelectMenu = makeViewWithName<BiosSelectMenu>("磁盘系统BIOS", &::fdsBiosPath,
				[this]()
				{
					logMsg("set fds bios %s", ::fdsBiosPath.data());
					fdsBiosPath.compile(makeBiosMenuEntryStr().data(), renderer(), projP);
				},
				hasFDSBIOSExtension);
			pushAndShow(std::move(biosSelectMenu), e);
		}
	};

	static std::array<char, 256> makeBiosMenuEntryStr()
	{
		return string_makePrintf<256>("磁盘系统BIOS: %s", strlen(::fdsBiosPath.data()) ? FS::basename(::fdsBiosPath).data() : "未设置");
	}

public:
	CustomSystemOptionView(ViewAttachParams attach): SystemOptionView{attach, true}
	{
		loadStockItems();
		fdsBiosPath.setName(makeBiosMenuEntryStr().data());
		item.emplace_back(&fdsBiosPath);
	}
};

class FDSControlView : public TableView
{
private:
	static constexpr uint DISK_SIDES = 4;
	TextMenuItem setSide[DISK_SIDES]
	{
		{
			"Set Disk 1 Side A",
			[](View &view, Input::Event e)
			{
				FCEU_FDSSetDisk(0);
				view.dismiss();
			}
		},
		{
			"Set Disk 1 Side B",
			[](View &view, Input::Event e)
			{
				FCEU_FDSSetDisk(1);
				view.dismiss();
			}
		},
		{
			"Set Disk 2 Side A",
			[](View &view, Input::Event e)
			{
				FCEU_FDSSetDisk(2);
				view.dismiss();
			}
		},
		{
			"Set Disk 2 Side B",
			[](View &view, Input::Event e)
			{
				FCEU_FDSSetDisk(3);
				view.dismiss();
			}
		}
	};

	TextMenuItem insertEject
	{
		"Eject",
		[this](View &view, Input::Event e)
		{
			if(FCEU_FDSInserted())
			{
				FCEU_FDSInsert();
				view.dismiss();
			}
		}
	};

public:
	FDSControlView(ViewAttachParams attach):
		TableView
		{
			"FDS控制器",
			attach,
			[this](const TableView &)
			{
				return 5;
			},
			[this](const TableView &, uint idx) -> MenuItem&
			{
				switch(idx)
				{
					case 0: return setSide[0];
					case 1: return setSide[1];
					case 2: return setSide[2];
					case 3: return setSide[3];
					default: return insertEject;
				}
			}
		}
	{
		setSide[0].setActive(0 < FCEU_FDSSides());
		setSide[1].setActive(1 < FCEU_FDSSides());
		setSide[2].setActive(2 < FCEU_FDSSides());
		setSide[3].setActive(3 < FCEU_FDSSides());
		insertEject.setActive(FCEU_FDSInserted());
	}
};

class CustomSystemActionsView : public EmuSystemActionsView
{
private:
	TextMenuItem fdsControl
	{
		nullptr,
		[this](TextMenuItem &item, View &, Input::Event e)
		{
			if(EmuSystem::gameIsRunning() && isFDS)
			{
				pushAndShow(makeView<FDSControlView>(), e);
			}
			else
				EmuApp::postMessage(2, false, "磁盘系统未使用");
		}
	};

	void refreshFDSItem()
	{
		fdsControl.setActive(isFDS);
		char diskLabel[sizeof("FDS控制器 (磁盘 1:A)")+2]{};
		if(!isFDS)
			strcpy(diskLabel, "FDS控制器");
		else if(!FCEU_FDSInserted())
			strcpy(diskLabel, "FDS控制器 (没有磁盘)");
		else
			sprintf(diskLabel, "FDS控制器 (磁盘 %d:%c)", (FCEU_FDSCurrentSide()>>1)+1, (FCEU_FDSCurrentSide() & 1)? 'B' : 'A');
		fdsControl.compile(diskLabel, renderer(), projP);
	}

	TextMenuItem options
	{
		"控制台选项",
		[this](TextMenuItem &, View &, Input::Event e)
		{
			if(EmuSystem::gameIsRunning())
			{
				pushAndShow(makeView<ConsoleOptionView>(), e);
			}
		}
	};

public:
	CustomSystemActionsView(ViewAttachParams attach): EmuSystemActionsView{attach, true}
	{
		item.emplace_back(&fdsControl);
		item.emplace_back(&options);
		loadStandardItems();
	}

	void onShow()
	{
		EmuSystemActionsView::onShow();
		refreshFDSItem();
	}
};

std::unique_ptr<View> EmuApp::makeCustomView(ViewAttachParams attach, ViewID id)
{
	switch(id)
	{
		case ViewID::SYSTEM_ACTIONS: return std::make_unique<CustomSystemActionsView>(attach);
		case ViewID::VIDEO_OPTIONS: return std::make_unique<CustomVideoOptionView>(attach);
		case ViewID::AUDIO_OPTIONS: return std::make_unique<CustomAudioOptionView>(attach);
		case ViewID::SYSTEM_OPTIONS: return std::make_unique<CustomSystemOptionView>(attach);
		case ViewID::EDIT_CHEATS: return std::make_unique<EmuEditCheatListView>(attach);
		case ViewID::LIST_CHEATS: return std::make_unique<EmuCheatsView>(attach);
		default: return nullptr;
	}
}
