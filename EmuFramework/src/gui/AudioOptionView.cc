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

#include <emuframework/AudioOptionView.hh>
#include <emuframework/EmuApp.hh>
#include <format>

namespace EmuEx
{

AudioOptionView::AudioOptionView(ViewAttachParams attach, bool customMenu):
	TableView{"音频设置", attach, item},
	snd
	{
		"声音", &defaultFace(),
		app().audio().isEnabled(),
		[this](BoolMenuItem &item)
		{
			app().audio().setEnabled(item.flipBoolValue(*this));
		}
	},
	soundDuringFastSlowMode
	{
		"变速时启用声音", &defaultFace(),
		app().audio().isEnabledDuringAltSpeed(),
		[this](BoolMenuItem &item)
		{
			app().audio().setEnabledDuringAltSpeed(item.flipBoolValue(*this));
		}
	},
	soundVolumeItem
	{
		{"100%", &defaultFace(), 100},
		{"50%",  &defaultFace(), 50},
		{"25%",  &defaultFace(), 25},
		{"自定义", &defaultFace(),
			[this](const Input::Event &e)
			{
				app().pushAndShowNewCollectValueRangeInputView<int, 0, 125>(attachParams(), e, "输入0到125", "",
					[this](EmuApp &app, auto val)
					{
						app.audio().setMaxVolume(val);
						soundVolume.setSelected((MenuItem::Id)val, *this);
						dismissPrevious();
						return true;
					});
				return false;
			}, MenuItem::DEFAULT_ID
		},
	},
	soundVolume
	{
		"音量", &defaultFace(),
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				t.resetString(std::format("{}%", app().audio().maxVolume()));
				return true;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().audio().setMaxVolume(item.id()); }
		},
		MenuItem::Id(app().audio().maxVolume()),
		soundVolumeItem
	},
	soundBuffersItem
	{
		{"1", &defaultFace(), 1},
		{"2", &defaultFace(), 2},
		{"3", &defaultFace(), 3},
		{"4", &defaultFace(), 4},
		{"5", &defaultFace(), 5},
		{"6", &defaultFace(), 6},
		{"7", &defaultFace(), 7},
	},
	soundBuffers
	{
		"缓冲区大小(以帧为单位)", &defaultFace(),
		{
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().audio().soundBuffers = item.id(); }
		},
		MenuItem::Id(app().audio().soundBuffers),
		soundBuffersItem
	},
	addSoundBuffersOnUnderrun
	{
		"自动增加缓冲区大小", &defaultFace(),
		app().audio().addSoundBuffersOnUnderrunSetting,
		[this](BoolMenuItem &item)
		{
			app().audio().addSoundBuffersOnUnderrunSetting = item.flipBoolValue(*this);
		}
	},
	audioRateItem
	{
		[&]
		{
			decltype(audioRateItem) items;
			items.emplace_back("跟随系统", &defaultFace(), [this](View &view)
			{
				app().audio().setRate(0);
				audioRate.setSelected(MenuItem::Id(app().audio().rate()));
				view.dismiss();
				return false;
			});
			auto setRateDel = [this](TextMenuItem &item) { app().audio().setRate(item.id()); };
			items.emplace_back("22KHz", &defaultFace(), setRateDel, 22050);
			items.emplace_back("32KHz", &defaultFace(), setRateDel, 32000);
			items.emplace_back("44KHz", &defaultFace(), setRateDel, 44100);
			if(app().audio().maxRate() >= 48000)
				items.emplace_back("48KHz", &defaultFace(), setRateDel, 48000);
			return items;
		}()
	},
	audioRate
	{
		"音频采样率", &defaultFace(),
		MenuItem::Id(app().audio().rate()),
		audioRateItem
	},
	audioSoloMix
	{
		"允许其他应用后台播放音乐", &defaultFace(),
		!app().audioManager().soloMix(),
		[this](BoolMenuItem &item)
		{
			app().audioManager().setSoloMix(!item.flipBoolValue(*this));
		}
	},
	apiItem
	{
		[this]()
		{
			ApiItemContainer items{};
			items.emplace_back("自动", &defaultFace(), [this](View &view)
			{
				app().audio().setOutputAPI(Audio::Api::DEFAULT);
				doIfUsed(api, [&](auto &api){ api.setSelected(MenuItem::Id(app().audioManager().makeValidAPI())); });
				view.dismiss();
				return false;
			});
			auto &audioManager = app().audioManager();
			for(auto desc: audioManager.audioAPIs())
			{
				items.emplace_back(desc.name, &defaultFace(), [this](TextMenuItem &item)
				{
					app().audio().setOutputAPI(Audio::Api(item.id()));
				}, MenuItem::Id(desc.api));
			}
			return items;
		}()
	},
	api
	{
		"音频驱动", &defaultFace(),
		MenuItem::Id(app().audioManager().makeValidAPI(app().audio().outputAPI())),
		apiItem
	}
{
	if(!customMenu)
	{
		loadStockItems();
	}
}

void AudioOptionView::loadStockItems()
{
	item.emplace_back(&snd);
	item.emplace_back(&soundDuringFastSlowMode);
	item.emplace_back(&soundVolume);
	if(!EmuSystem::forcedSoundRate)
	{
		item.emplace_back(&audioRate);
	}
	item.emplace_back(&soundBuffers);
	item.emplace_back(&addSoundBuffersOnUnderrun);
	if constexpr(IG::Audio::Manager::HAS_SOLO_MIX)
	{
		item.emplace_back(&audioSoloMix);
	}
	doIfUsed(apiItem, [&](auto &apiItem)
	{
		if(apiItem.size() > 2)
		{
			item.emplace_back(&api);
		}
	});
}

}
