/*  This file is part of GBC.emu.

	GBC.emu is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	GBC.emu is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with GBC.emu.  If not, see <http://www.gnu.org/licenses/> */

#include <emuframework/EmuApp.hh>
#include <emuframework/EmuAppHelper.hh>
#include <emuframework/OptionView.hh>
#include <emuframework/EmuSystemActionsView.hh>
#include "EmuCheatViews.hh"
#include "Palette.hh"
#include "internal.hh"
#include <resample/resamplerinfo.h>

namespace EmuEx
{

static constexpr unsigned MAX_RESAMPLERS = 4;

class CustomAudioOptionView : public AudioOptionView
{
	StaticArrayList<TextMenuItem, MAX_RESAMPLERS> resamplerItem{};

	MultiChoiceMenuItem resampler
	{
		"重采样器", &defaultFace(),
		optionAudioResampler.val,
		resamplerItem
	};

public:
	CustomAudioOptionView(ViewAttachParams attach): AudioOptionView{attach, true}
	{
		loadStockItems();
		logMsg("%d resamplers", (int)ResamplerInfo::num());
		auto resamplers = std::min((unsigned)ResamplerInfo::num(), MAX_RESAMPLERS);
		iterateTimes(resamplers, i)
		{
			ResamplerInfo r = ResamplerInfo::get(i);
			logMsg("%d %s", i, r.desc);
			resamplerItem.emplace_back(r.desc, &defaultFace(),
				[this, i]()
				{
					optionAudioResampler = i;
					app().configFrameTime();
				});
		}
		item.emplace_back(&resampler);
	}
};

class CustomVideoOptionView : public VideoOptionView
{
	TextMenuItem gbPaletteItem[13]
	{
		{"原画", &defaultFace(), [](){ optionGBPal = 0; applyGBPalette(); }},
		{"棕色", &defaultFace(), [](){ optionGBPal = 1; applyGBPalette(); }},
		{"红色", &defaultFace(), [](){ optionGBPal = 2; applyGBPalette(); }},
		{"深棕色", &defaultFace(), [](){ optionGBPal = 3; applyGBPalette(); }},
		{"粉彩", &defaultFace(), [](){ optionGBPal = 4; applyGBPalette(); }},
		{"橙色", &defaultFace(), [](){ optionGBPal = 5; applyGBPalette(); }},
		{"黄色", &defaultFace(), [](){ optionGBPal = 6; applyGBPalette(); }},
		{"蓝色", &defaultFace(), [](){ optionGBPal = 7; applyGBPalette(); }},
		{"深蓝色", &defaultFace(), [](){ optionGBPal = 8; applyGBPalette(); }},
		{"灰色", &defaultFace(), [](){ optionGBPal = 9; applyGBPalette(); }},
		{"绿色", &defaultFace(), [](){ optionGBPal = 10; applyGBPalette(); }},
		{"深绿色", &defaultFace(), [](){ optionGBPal = 11; applyGBPalette(); }},
		{"反相", &defaultFace(), [](){ optionGBPal = 12; applyGBPalette(); }},
	};

	MultiChoiceMenuItem gbPalette
	{
		"GB调色板", &defaultFace(),
		optionGBPal.val,
		gbPaletteItem
	};

	BoolMenuItem fullSaturation
	{
		"饱和GBC颜色", &defaultFace(),
		(bool)optionFullGbcSaturation,
		[this](BoolMenuItem &item, View &, Input::Event e)
		{
			optionFullGbcSaturation = item.flipBoolValue(*this);
			if(system().hasContent())
			{
				gbEmu.refreshPalettes();
			}
		}
	};

public:
	CustomVideoOptionView(ViewAttachParams attach): VideoOptionView{attach, true}
	{
		loadStockItems();
		item.emplace_back(&systemSpecificHeading);
		item.emplace_back(&gbPalette);
		item.emplace_back(&fullSaturation);
	}
};

class ConsoleOptionView : public TableView, public EmuAppHelper<ConsoleOptionView>
{
	BoolMenuItem useBuiltinGBPalette
	{
		"使用内置GB调色板", &defaultFace(),
		(bool)optionUseBuiltinGBPalette,
		[this](BoolMenuItem &item, View &, Input::Event e)
		{
			system().sessionOptionSet();
			optionUseBuiltinGBPalette = item.flipBoolValue(*this);
			applyGBPalette();
		}
	};

	BoolMenuItem reportAsGba
	{
		"将硬件报告为GBA", &defaultFace(),
		(bool)optionReportAsGba,
		[this](BoolMenuItem &item, View &, Input::Event e)
		{
			system().sessionOptionSet();
			optionReportAsGba = item.flipBoolValue(*this);
			app().promptSystemReloadDueToSetOption(attachParams(), e);
		}
	};

	std::array<MenuItem*, 2> menuItem
	{
		&useBuiltinGBPalette,
		&reportAsGba
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
		case ViewID::VIDEO_OPTIONS: return std::make_unique<CustomVideoOptionView>(attach);
		case ViewID::AUDIO_OPTIONS: return std::make_unique<CustomAudioOptionView>(attach);
		case ViewID::SYSTEM_ACTIONS: return std::make_unique<CustomSystemActionsView>(attach);
		case ViewID::EDIT_CHEATS: return std::make_unique<EmuEditCheatListView>(attach);
		case ViewID::LIST_CHEATS: return std::make_unique<EmuCheatsView>(attach);
		default: return nullptr;
	}
}

}
