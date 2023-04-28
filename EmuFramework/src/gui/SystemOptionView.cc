/*  This file is part of EmuFramework.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with EmuFramework.  If not, see <http://www.gnu.org/licenses/> */

#include <emuframework/SystemOptionView.hh>
#include <emuframework/EmuApp.hh>
#include "../EmuOptions.hh"
#include "CPUAffinityView.hh"
#include <imagine/base/ApplicationContext.hh>
#include <imagine/gui/TextTableView.hh>
#include <imagine/fs/FS.hh>
#include <format>

namespace EmuEx
{

SystemOptionView::SystemOptionView(ViewAttachParams attach, bool customMenu):
	TableView{"系统设置", attach, item},
	autosaveTimerItem
	{
		{"关",    &defaultFace(), 0},
		{"5分钟",  &defaultFace(), 5},
		{"10分钟", &defaultFace(), 10},
		{"15分钟", &defaultFace(), 15},
	},
	autosaveTimer
	{
		"自动存档定时器", &defaultFace(),
		{
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().autosaveManager().autosaveTimerMins = IG::Minutes{item.id()}; }
		},
		(MenuItem::Id)app().autosaveManager().autosaveTimerMins.count(),
		autosaveTimerItem
	},
	autosaveLaunchItem
	{
		{"加载即时存档",            &defaultFace(), to_underlying(AutosaveLaunchMode::Load)},
		{"不加载即时存档", &defaultFace(), to_underlying(AutosaveLaunchMode::LoadNoState)},
		//去掉
//		{"不使用自动存档和本体存档",         &defaultFace(), to_underlying(AutosaveLaunchMode::NoSave)},
//		{"询问",          &defaultFace(), to_underlying(AutosaveLaunchMode::Ask)},
	},
	autosaveLaunch
	{
		"自动存档启动模式", &defaultFace(),
		{
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().autosaveManager().autosaveLaunchMode = AutosaveLaunchMode(item.id()); }
		},
		(MenuItem::Id)app().autosaveManager().autosaveLaunchMode,
		autosaveLaunchItem
	},
	autosaveContent
	{
		"自动存档内容", &defaultFace(),
		app().autosaveManager().saveOnlyBackupMemory,
		"即时存档和本体(RAM)存档", "仅本体(RAM)存档",
		[this](BoolMenuItem &item)
		{
			app().autosaveManager().saveOnlyBackupMemory = item.flipBoolValue(*this);
		}
	},
	confirmOverwriteState
	{
		"Confirm Overwrite State", &defaultFace(),
		(bool)app().confirmOverwriteStateOption(),
		[this](BoolMenuItem &item)
		{
			app().confirmOverwriteStateOption() = item.flipBoolValue(*this);
		}
	},
	fastModeSpeedItem
	{
		{"1.5x",  &defaultFace(), 150},
		{"2x",    &defaultFace(), 200},
		{"4x",    &defaultFace(), 400},
		{"8x",    &defaultFace(), 800},
		{"16x",   &defaultFace(), 1600},
		{"自定义", &defaultFace(),
			[this](const Input::Event &e)
			{
				app().pushAndShowNewCollectValueInputView<float>(attachParams(), e, "输入1.0到20.0", "",
					[this](EmuApp &app, auto val)
					{
						auto valAsInt = std::round(val * 100.f);
						if(app.setAltSpeed(AltSpeedMode::fast, valAsInt))
						{
							fastModeSpeed.setSelected((MenuItem::Id)valAsInt, *this);
							dismissPrevious();
							return true;
						}
						else
						{
							app.postErrorMessage("输入错误");
							return false;
						}
					});
				return false;
			}, MenuItem::DEFAULT_ID
		},
	},
	fastModeSpeed
	{
		"加速速度", &defaultFace(),
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				t.resetString(std::format("{:g}x", app().altSpeedAsDouble(AltSpeedMode::fast)));
				return true;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().setAltSpeed(AltSpeedMode::fast, item.id()); }
		},
		(MenuItem::Id)app().altSpeed(AltSpeedMode::fast),
		fastModeSpeedItem
	},
	slowModeSpeedItem
	{
		{"0.25x", &defaultFace(), 25},
		{"0.50x", &defaultFace(), 50},
		{"自定义", &defaultFace(),
			[this](const Input::Event &e)
			{
				app().pushAndShowNewCollectValueInputView<float>(attachParams(), e, "输入0.05到1.0", "",
					[this](EmuApp &app, auto val)
					{
						auto valAsInt = std::round(val * 100.f);
						if(app.setAltSpeed(AltSpeedMode::slow, valAsInt))
						{
							slowModeSpeed.setSelected((MenuItem::Id)valAsInt, *this);
							dismissPrevious();
							return true;
						}
						else
						{
							app.postErrorMessage("输入错误");
							return false;
						}
					});
				return false;
			}, MenuItem::DEFAULT_ID
		},
	},
	slowModeSpeed
	{
		"减速速度", &defaultFace(),
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				t.resetString(std::format("{:g}x", app().altSpeedAsDouble(AltSpeedMode::slow)));
				return true;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().setAltSpeed(AltSpeedMode::slow, item.id()); }
		},
		(MenuItem::Id)app().altSpeed(AltSpeedMode::slow),
		slowModeSpeedItem
	},
	performanceMode
	{
		"性能模式", &defaultFace(),
		(bool)app().sustainedPerformanceModeOption(),
		"正常", "省电",
		[this](BoolMenuItem &item)
		{
			app().sustainedPerformanceModeOption() = item.flipBoolValue(*this);
		}
	},
	noopThread
	{
		"无操作线程（实验性）", &defaultFace(),
		(bool)app().useNoopThread,
		[this](BoolMenuItem &item)
		{
			app().useNoopThread = item.flipBoolValue(*this);
		}
	},
	cpuAffinity
	{
		"配置CPU相关性", &defaultFace(),
		[this](const Input::Event &e)
		{
			pushAndShow(makeView<CPUAffinityView>(appContext().cpuCount()), e);
		}
	}
{
	if(!customMenu)
	{
		loadStockItems();
	}
}

void SystemOptionView::loadStockItems()
{
	item.emplace_back(&autosaveLaunch);
	item.emplace_back(&autosaveTimer);
	item.emplace_back(&autosaveContent);
//	item.emplace_back(&confirmOverwriteState);
//	item.emplace_back(&fastModeSpeed);
//	item.emplace_back(&slowModeSpeed);
	if(used(performanceMode) && appContext().hasSustainedPerformanceMode())
		item.emplace_back(&performanceMode);
	if(used(noopThread))
		item.emplace_back(&noopThread);
	if(used(cpuAffinity) && appContext().cpuCount() > 1)
		item.emplace_back(&cpuAffinity);
}

}
