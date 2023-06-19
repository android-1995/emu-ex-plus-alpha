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

#include "CPUAffinityView.hh"
#include <emuframework/EmuApp.hh>
#include <imagine/logger/logger.h>
#include <cstdio>
#include <format>

namespace EmuEx
{

CPUAffinityView::CPUAffinityView(ViewAttachParams attach, int cpuCount):
	TableView{"配置CPU相关性", attach, menuItems},
	affinityModeItems
	{
		{"自动（仅使用大核以实现低延迟）", &defaultFace(), to_underlying(CPUAffinityMode::Auto)},
		{"任意（使用任意内核，可能增加延迟）",            &defaultFace(), to_underlying(CPUAffinityMode::Any)},
		{"手动（使用设置的内核）",                    &defaultFace(), to_underlying(CPUAffinityMode::Manual)},
	},
	affinityMode
	{
		"CPU相关性模式", &defaultFace(),
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				t.resetString(wise_enum::to_string(CPUAffinityMode(affinityModeItems[idx].id())));
				return true;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().cpuAffinityMode = CPUAffinityMode(item.id()); }
		},
		MenuItem::Id(uint8_t(app().cpuAffinityMode)),
		affinityModeItems
	},
	cpusHeading{"手动配置CPU相关性", &defaultBoldFace()}
{
	menuItems.emplace_back(&affinityMode);
	menuItems.emplace_back(&cpusHeading);
	cpuAffinityItems.reserve(cpuCount);
	for(int i : iotaCount(cpuCount))
	{
		auto &item = cpuAffinityItems.emplace_back([&]
			{
				auto freq = appContext().maxCPUFrequencyKHz(i);
				if(!freq)
					return std::format("{} (离线)", i);
				return std::format("{} ({}MHz)", i, freq / 1000);
			}(),
			&defaultFace(), app().cpuAffinity(i),
			[this, i](BoolMenuItem &item) { app().setCPUAffinity(i, item.flipBoolValue(*this)); });
		menuItems.emplace_back(&item);
	}
}

void CPUAffinityView::onShow()
{
	bool isInManualMode = app().cpuAffinityMode == CPUAffinityMode::Manual;
	for(auto &item : cpuAffinityItems)
	{
		item.setActive(isInManualMode);
	}
}

}
