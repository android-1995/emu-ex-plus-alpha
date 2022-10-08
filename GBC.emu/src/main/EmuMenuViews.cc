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
#include <emuframework/OptionView.hh>
#include <emuframework/AudioOptionView.hh>
#include <emuframework/VideoOptionView.hh>
#include <emuframework/EmuSystemActionsView.hh>
#include "EmuCheatViews.hh"
#include "Palette.hh"
#include "MainApp.hh"
#include <resample/resamplerinfo.h>

namespace EmuEx
{

template <class T>
using MainAppHelper = EmuAppHelper<T, MainApp>;

static constexpr size_t MAX_RESAMPLERS = 4;

class CustomAudioOptionView : public AudioOptionView, public MainAppHelper<CustomAudioOptionView>
{
	using MainAppHelper<CustomAudioOptionView>::app;
	using MainAppHelper<CustomAudioOptionView>::system;

	StaticArrayList<TextMenuItem, MAX_RESAMPLERS> resamplerItem;

	MultiChoiceMenuItem resampler
	{
		"重采样器", &defaultFace(),
		system().optionAudioResampler.val,
		resamplerItem
	};

public:
	CustomAudioOptionView(ViewAttachParams attach): AudioOptionView{attach, true}
	{
		loadStockItems();
		logMsg("%d resamplers", (int)ResamplerInfo::num());
		auto resamplers = std::min(ResamplerInfo::num(), MAX_RESAMPLERS);
		for(auto i : iotaCount(resamplers))
		{
			ResamplerInfo r = ResamplerInfo::get(i);
			logMsg("%zu %s", i, r.desc);
			resamplerItem.emplace_back(r.desc, &defaultFace(),
				[this, i]()
				{
					system().optionAudioResampler = i;
					app().configFrameTime();
				});
		}
		item.emplace_back(&resampler);
	}
};

class CustomVideoOptionView : public VideoOptionView, public MainAppHelper<CustomVideoOptionView>
{
	using MainAppHelper<CustomVideoOptionView>::system;

	TextMenuItem::SelectDelegate setGbPaletteDel()
	{
		return [this](TextMenuItem &item)
		{
			system().optionGBPal = item.id();
			system().applyGBPalette();
		};
	}

	TextMenuItem gbPaletteItem[13]
	{
		{"原画",   &defaultFace(), setGbPaletteDel(), 0},
		{"棕色",      &defaultFace(), setGbPaletteDel(), 1},
		{"红色",        &defaultFace(), setGbPaletteDel(), 2},
		{"深棕色", &defaultFace(), setGbPaletteDel(), 3},
		{"粉彩",     &defaultFace(), setGbPaletteDel(), 4},
		{"橙色",     &defaultFace(), setGbPaletteDel(), 5},
		{"黄色",     &defaultFace(), setGbPaletteDel(), 6},
		{"蓝色",       &defaultFace(), setGbPaletteDel(), 7},
		{"深蓝色",  &defaultFace(), setGbPaletteDel(), 8},
		{"灰色",       &defaultFace(), setGbPaletteDel(), 9},
		{"绿色",      &defaultFace(), setGbPaletteDel(), 10},
		{"深绿色", &defaultFace(), setGbPaletteDel(), 11},
		{"颜色反转",    &defaultFace(), setGbPaletteDel(), 12},
	};

	MultiChoiceMenuItem gbPalette
	{
		"GB调色板", &defaultFace(),
		system().optionGBPal.val,
		gbPaletteItem
	};

	BoolMenuItem fullSaturation
	{
		"饱和GBC颜色", &defaultFace(),
		(bool)system().optionFullGbcSaturation,
		[this](BoolMenuItem &item, View &, Input::Event e)
		{
			system().optionFullGbcSaturation = item.flipBoolValue(*this);
			if(system().hasContent())
			{
				system().refreshPalettes();
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

class ConsoleOptionView : public TableView, public MainAppHelper<ConsoleOptionView>
{
	BoolMenuItem useBuiltinGBPalette
	{
		"使用内置GB调色板", &defaultFace(),
		(bool)system().optionUseBuiltinGBPalette,
		[this](BoolMenuItem &item, View &, Input::Event e)
		{
			system().sessionOptionSet();
			system().optionUseBuiltinGBPalette = item.flipBoolValue(*this);
			system().applyGBPalette();
		}
	};

	BoolMenuItem reportAsGba
	{
		"将硬件报告为GBA", &defaultFace(),
		(bool)system().optionReportAsGba,
		[this](BoolMenuItem &item, View &, Input::Event e)
		{
			system().sessionOptionSet();
			system().optionReportAsGba = item.flipBoolValue(*this);
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
