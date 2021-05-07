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
#include <emuframework/EmuSystemActionsView.hh>
#include "EmuCheatViews.hh"
#include "internal.hh"
#include <resample/resamplerinfo.h>

static constexpr uint MAX_RESAMPLERS = 4;

class CustomAudioOptionView : public AudioOptionView
{
	StaticArrayList<TextMenuItem, MAX_RESAMPLERS> resamplerItem{};

	MultiChoiceMenuItem resampler
	{
		"重采样",
		optionAudioResampler,
		resamplerItem
	};

public:
	CustomAudioOptionView(ViewAttachParams attach): AudioOptionView{attach, true}
	{
		loadStockItems();
		logMsg("%d resamplers", (int)ResamplerInfo::num());
		auto resamplers = std::min((uint)ResamplerInfo::num(), MAX_RESAMPLERS);
		iterateTimes(resamplers, i)
		{
			ResamplerInfo r = ResamplerInfo::get(i);
			logMsg("%d %s", i, r.desc);
			resamplerItem.emplace_back(r.desc,
				[i]()
				{
					optionAudioResampler = i;
					EmuSystem::configFrameTime();
				});
		}
		item.emplace_back(&resampler);
	}
};

class CustomVideoOptionView : public VideoOptionView
{
	TextMenuItem gbPaletteItem[13]
	{
		{"原始", [](){ optionGBPal = 0; applyGBPalette(); }},
		{"棕色", [](){ optionGBPal = 1; applyGBPalette(); }},
		{"红色", [](){ optionGBPal = 2; applyGBPalette(); }},
		{"深棕色", [](){ optionGBPal = 3; applyGBPalette(); }},
		{"Pastel", [](){ optionGBPal = 4; applyGBPalette(); }},
		{"橙色", [](){ optionGBPal = 5; applyGBPalette(); }},
		{"黄色", [](){ optionGBPal = 6; applyGBPalette(); }},
		{"蓝色", [](){ optionGBPal = 7; applyGBPalette(); }},
		{"深蓝色", [](){ optionGBPal = 8; applyGBPalette(); }},
		{"灰色", [](){ optionGBPal = 9; applyGBPalette(); }},
		{"绿色", [](){ optionGBPal = 10; applyGBPalette(); }},
		{"深绿色", [](){ optionGBPal = 11; applyGBPalette(); }},
		{"反转", [](){ optionGBPal = 12; applyGBPalette(); }},
	};

	MultiChoiceMenuItem gbPalette
	{
		"GB 调色板",
		optionGBPal,
		gbPaletteItem
	};

	BoolMenuItem fullSaturation
	{
		"GBC颜色饱和",
		(bool)optionFullGbcSaturation,
		[this](BoolMenuItem &item, View &, Input::Event e)
		{
			optionFullGbcSaturation = item.flipBoolValue(*this);
			if(EmuSystem::gameIsRunning())
			{
				gbEmu.refreshPalettes();
			}
		}
	};

	void setRenderFormat(IG::PixelFormatID fmt)
	{
		optionRenderPixelFormat = fmt;
		EmuApp::resetVideo();
	}

	TextMenuItem renderPixelFormatItem[3]
	{
		{"自动 (根据需要匹配显示格式)", [this]() { setRenderFormat(IG::PIXEL_NONE); }},
		{"RGB565", [this]() { setRenderFormat(IG::PIXEL_RGB565); }},
		{"RGBA8888", [this]() { setRenderFormat(IG::PIXEL_RGBA8888); }},
	};

	MultiChoiceMenuItem renderPixelFormat
	{
		"渲染颜色格式",
		[](int idx, Gfx::Text &t)
		{
			if(idx == 0)
			{
				t.setString("自动");
				return true;
			}
			return false;
		},
		[]()
		{
			switch(optionRenderPixelFormat.val)
			{
				default: return 0;
				case IG::PIXEL_RGB565: return 1;
				case IG::PIXEL_RGBA8888: return 2;
			}
		}(),
		renderPixelFormatItem
	};

public:
	CustomVideoOptionView(ViewAttachParams attach): VideoOptionView{attach, true}
	{
		loadStockItems();
		item.emplace_back(&systemSpecificHeading);
		item.emplace_back(&gbPalette);
		item.emplace_back(&fullSaturation);
		item.emplace_back(&renderPixelFormat);
	}
};

class ConsoleOptionView : public TableView
{
	BoolMenuItem useBuiltinGBPalette
	{
		"使用内置GB调色板",
		(bool)optionUseBuiltinGBPalette,
		[this](BoolMenuItem &item, View &, Input::Event e)
		{
			EmuSystem::sessionOptionSet();
			optionUseBuiltinGBPalette = item.flipBoolValue(*this);
			applyGBPalette();
		}
	};

	BoolMenuItem reportAsGba
	{
		"将硬件报告为GBA",
		(bool)optionReportAsGba,
		[this](BoolMenuItem &item, View &, Input::Event e)
		{
			EmuSystem::sessionOptionSet();
			optionReportAsGba = item.flipBoolValue(*this);
			EmuApp::promptSystemReloadDueToSetOption(attachParams(), e);
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
			"控制台选项",
			attach,
			menuItem
		}
	{}
};

class CustomSystemActionsView : public EmuSystemActionsView
{
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
