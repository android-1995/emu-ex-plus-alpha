#ifndef SNES9X_VERSION_1_4
#include <apu/apu.h>
#include <apu/bapu/snes/snes.hpp>
#include <ppu.h>
#endif
#include <emuframework/EmuApp.hh>
#include <emuframework/OptionView.hh>
#include <emuframework/EmuSystemActionsView.hh>
#include "EmuCheatViews.hh"
#include "internal.hh"
#include <snes9x.h>
#include <imagine/util/format.hh>

namespace EmuEx
{

constexpr bool HAS_NSRT = !IS_SNES9X_VERSION_1_4;

#ifndef SNES9X_VERSION_1_4
class CustomAudioOptionView : public AudioOptionView
{
	void setDSPInterpolation(uint8_t val)
	{
		logMsg("set DSP interpolation:%u", val);
		optionAudioDSPInterpolation = val;
		SNES::dsp.spc_dsp.interpolation = val;
	}

	TextMenuItem dspInterpolationItem[5]
	{
		{"无", &defaultFace(), [this](){ setDSPInterpolation(0); }},
		{"线性", &defaultFace(), [this](){ setDSPInterpolation(1); }},
		{"高斯", &defaultFace(), [this](){ setDSPInterpolation(2); }},
		{"立方", &defaultFace(), [this](){ setDSPInterpolation(3); }},
		{"正弦", &defaultFace(), [this](){ setDSPInterpolation(4); }},
	};

	MultiChoiceMenuItem dspInterpolation
	{
		"DSP插值", &defaultFace(),
		optionAudioDSPInterpolation.val,
		dspInterpolationItem
	};

public:
	CustomAudioOptionView(ViewAttachParams attach): AudioOptionView{attach, true}
	{
		loadStockItems();
		item.emplace_back(&dspInterpolation);
	}
};
#endif

class ConsoleOptionView : public TableView, public EmuAppHelper<ConsoleOptionView>
{
	BoolMenuItem multitap
	{
		"5玩家模式", &defaultFace(),
		(bool)optionMultitap,
		[this](BoolMenuItem &item, View &, Input::Event e)
		{
			system().sessionOptionSet();
			optionMultitap = item.flipBoolValue(*this);
			setupSNESInput(system(), app().defaultVController());
		}
	};

	TextMenuItem inputPortsItem[HAS_NSRT ? 4 : 3]
	{
		#ifndef SNES9X_VERSION_1_4
		{"Auto (NSRT)", &defaultFace(), setInputPortsDel(), SNES_AUTO_INPUT},
		#endif
		{"Gamepads",    &defaultFace(), setInputPortsDel(), SNES_JOYPAD},
		{"Superscope",  &defaultFace(), setInputPortsDel(), SNES_SUPERSCOPE},
		{"Mouse",       &defaultFace(), setInputPortsDel(), SNES_MOUSE_SWAPPED},
	};

	MultiChoiceMenuItem inputPorts
	{
		"Input Ports", &defaultFace(),
		(MenuItem::Id)snesInputPort,
		inputPortsItem
	};

	TextMenuItem::SelectDelegate setInputPortsDel()
	{
		return [this](TextMenuItem &item)
		{
			system().sessionOptionSet();
			optionInputPort = item.id();
			snesInputPort = item.id();
			setupSNESInput(system(), app().defaultVController());
		};
	}

	TextMenuItem videoSystemItem[4]
	{
		{"自动", &defaultFace(), [this](Input::Event e){ setVideoSystem(0, e); }},
		{"NTSC", &defaultFace(), [this](Input::Event e){ setVideoSystem(1, e); }},
		{"PAL", &defaultFace(), [this](Input::Event e){ setVideoSystem(2, e); }},
		{"NTSC + PAL Spoof", &defaultFace(), [this](Input::Event e){ setVideoSystem(3, e); }},
	};

	MultiChoiceMenuItem videoSystem
	{
		"视频制式", &defaultFace(),
		optionVideoSystem.val,
		videoSystemItem
	};

	void setVideoSystem(int val, Input::Event e)
	{
		system().sessionOptionSet();
		optionVideoSystem = val;
		app().promptSystemReloadDueToSetOption(attachParams(), e);
	}

	TextHeadingMenuItem videoHeading{"视频", &defaultBoldFace()};

	BoolMenuItem allowExtendedLines
	{
		"允许扩展 239/478 线路", &defaultFace(),
		(bool)optionAllowExtendedVideoLines,
		[this](BoolMenuItem &item, View &, Input::Event e)
		{
			system().sessionOptionSet();
			optionAllowExtendedVideoLines = item.flipBoolValue(*this);
		}
	};

	#ifndef SNES9X_VERSION_1_4
	TextHeadingMenuItem emulationHacks{"模拟器黑科技", &defaultBoldFace()};

	BoolMenuItem blockInvalidVRAMAccess
	{
		"允许无效的VRAM访问", &defaultFace(),
		(bool)!optionBlockInvalidVRAMAccess,
		[this](BoolMenuItem &item, View &, Input::Event e)
		{
			system().sessionOptionSet();
			optionBlockInvalidVRAMAccess = !item.flipBoolValue(*this);
			PPU.BlockInvalidVRAMAccess = optionBlockInvalidVRAMAccess;
		}
	};

	BoolMenuItem separateEchoBuffer
	{
		"将回声缓冲区与Ram分开", &defaultFace(),
		(bool)optionSeparateEchoBuffer,
		[this](BoolMenuItem &item, View &, Input::Event e)
		{
			system().sessionOptionSet();
			optionSeparateEchoBuffer = item.flipBoolValue(*this);
			SNES::dsp.spc_dsp.separateEchoBuffer = optionSeparateEchoBuffer;
		}
	};

	void setSuperFXClock(unsigned val)
	{
		system().sessionOptionSet();
		optionSuperFXClockMultiplier = val;
		setSuperFXSpeedMultiplier(optionSuperFXClockMultiplier);
	}

	TextMenuItem superFXClockItem[2]
	{
		{"100%", &defaultFace(), [this]() { setSuperFXClock(100); }},
		{"自定义", &defaultFace(),
			[this](Input::Event e)
			{
				app().pushAndShowNewCollectValueInputView<int>(attachParams(), e, "输入5到250", "",
					[this](EmuApp &app, auto val)
					{
						if(optionSuperFXClockMultiplier.isValidVal(val))
						{
							setSuperFXClock(val);
							superFXClock.setSelected(std::size(superFXClockItem) - 1, *this);
							dismissPrevious();
							return true;
						}
						else
						{
							app.postErrorMessage("值不在范围内");
							return false;
						}
					});
				return false;
			}
		},
	};

	MultiChoiceMenuItem superFXClock
	{
		"SuperFX时钟倍增器", &defaultFace(),
		[this](uint32_t idx, Gfx::Text &t)
		{
			t.setString(fmt::format("{}%", optionSuperFXClockMultiplier.val));
			return true;
		},
		[]()
		{
			if(optionSuperFXClockMultiplier.val == 100)
				return 0;
			else
				return 1;
		}(),
		superFXClockItem
	};
	#endif

	std::array<MenuItem*, IS_SNES9X_VERSION_1_4 ? 4 : 8> menuItem
	{
//		&inputPorts,
		&multitap,
		&videoHeading,
		&videoSystem,
		&allowExtendedLines,
		#ifndef SNES9X_VERSION_1_4
		&emulationHacks,
		&blockInvalidVRAMAccess,
		&separateEchoBuffer,
		&superFXClock,
		#endif
	};

public:
	ConsoleOptionView(ViewAttachParams attach):
		TableView
		{
			"控制台设置",
			attach,
			menuItem
		}
	{}
};

class CustomSystemActionsView : public EmuSystemActionsView
{
private:
	TextMenuItem options
	{
		"控制台设置", &defaultFace(),
		[this](TextMenuItem &, View &, Input::Event e)
		{
			if(system().hasContent())
			{
				pushAndShow(makeView<ConsoleOptionView>(), e);
			}
		}
	};

public:
	CustomSystemActionsView(ViewAttachParams attach): EmuSystemActionsView{attach, true}
	{
		item.emplace_back(&options);
		loadStandardItems();
	}
};

std::unique_ptr<View> EmuApp::makeCustomView(ViewAttachParams attach, ViewID id)
{
	switch(id)
	{
		#ifndef SNES9X_VERSION_1_4
		case ViewID::AUDIO_OPTIONS: return std::make_unique<CustomAudioOptionView>(attach);
		#endif
		case ViewID::SYSTEM_ACTIONS: return std::make_unique<CustomSystemActionsView>(attach);
		case ViewID::EDIT_CHEATS: return std::make_unique<EmuEditCheatListView>(attach);
		case ViewID::LIST_CHEATS: return std::make_unique<EmuCheatsView>(attach);
		default: return nullptr;
	}
}

}
