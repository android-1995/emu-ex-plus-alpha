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

#include <emuframework/OptionView.hh>
#include <emuframework/EmuApp.hh>
#include <emuframework/EmuAudio.hh>
#include "EmuOptions.hh"
#include <imagine/util/format.hh>

namespace EmuEx
{

AudioOptionView::AudioOptionView(ViewAttachParams attach, bool customMenu):
	TableView{"音频设置", attach, item},
	snd
	{
		"声音", &defaultFace(),
		app().soundIsEnabled(),
		[this](BoolMenuItem &item)
		{
			app().setSoundEnabled(item.flipBoolValue(*this));
		}
	},
	soundDuringFastForward
	{
		"加速时声音", &defaultFace(),
		app().soundDuringFastForwardIsEnabled(),
		[this](BoolMenuItem &item)
		{
			app().setSoundDuringFastForwardEnabled(item.flipBoolValue(*this));
		}
	},
	soundVolumeItem
	{
		{"100%", &defaultFace(), setVolumeDel(), 100},
		{"50%",  &defaultFace(), setVolumeDel(), 50},
		{"25%",  &defaultFace(), setVolumeDel(), 25},
		{"自定义", &defaultFace(),
			[this](const Input::Event &e)
			{
				app().pushAndShowNewCollectValueInputView<int>(attachParams(), e, "Input 0 to 100", "",
					[this](EmuApp &app, auto val)
					{
						if(app.setSoundVolume(val))
						{
							soundVolume.setSelected(std::size(soundVolumeItem) - 1, *this);
							dismissPrevious();
							return true;
						}
						else
						{
							app.postErrorMessage("值错误");
							return false;
						}
					});
				return false;
			}
		},
	},
	soundVolume
	{
		"音量", &defaultFace(),
		[this](size_t idx, Gfx::Text &t)
		{
			t.setString(fmt::format("{}%", app().soundVolume()));
			return true;
		},
		(MenuItem::Id)app().soundVolume(),
		soundVolumeItem
	},
	soundBuffersItem
	{
		{"1", &defaultFace(), setBuffersDel(), 1},
		{"2", &defaultFace(), setBuffersDel(), 2},
		{"3", &defaultFace(), setBuffersDel(), 3},
		{"4", &defaultFace(), setBuffersDel(), 4},
		{"5", &defaultFace(), setBuffersDel(), 5},
		{"6", &defaultFace(), setBuffersDel(), 6},
		{"7", &defaultFace(), setBuffersDel(), 7},
	},
	soundBuffers
	{
		"缓冲区大小(以帧为单位)", &defaultFace(),
		(MenuItem::Id)app().soundBuffers(),
		soundBuffersItem
	},
	addSoundBuffersOnUnderrun
	{
		"自动增加缓冲区大小", &defaultFace(),
		app().addSoundBuffersOnUnderrun(),
		[this](BoolMenuItem &item)
		{
			app().setAddSoundBuffersOnUnderrun(item.flipBoolValue(*this));
		}
	},
	audioRate
	{
		"音频采样率", &defaultFace(),
		0,
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
				app().setAudioOutputAPI(Audio::Api::DEFAULT);
				doIfUsed(api, [&](auto &api){ api.setSelected((MenuItem::Id)app().audioManager().makeValidAPI()); });
				view.dismiss();
				return false;
			});
			auto &audioManager = app().audioManager();
			for(auto desc: audioManager.audioAPIs())
			{
				items.emplace_back(desc.name, &defaultFace(), [this](TextMenuItem &item)
				{
					app().setAudioOutputAPI((Audio::Api)item.id());
				}, (MenuItem::Id)desc.api);
			}
			return items;
		}()
	},
	api
	{
		"音频驱动", &defaultFace(),
		(MenuItem::Id)app().audioManager().makeValidAPI(app().audioOutputAPI()),
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
	item.emplace_back(&soundDuringFastForward);
	item.emplace_back(&soundVolume);
	if(app().canChangeSoundRate())
	{
		audioRateItem.clear();
		audioRateItem.emplace_back("与本机一致", &defaultFace(),
			[this](View &view)
			{
				app().setSoundRate(0);
				audioRate.setSelected((MenuItem::Id)app().soundRate());
				view.dismiss();
				return false;
			});
		audioRateItem.emplace_back("22KHz", &defaultFace(), setRateDel(), 22050);
		audioRateItem.emplace_back("32KHz", &defaultFace(), setRateDel(), 32000);
		audioRateItem.emplace_back("44KHz", &defaultFace(), setRateDel(), 44100);
		if(app().soundRateMax() >= 48000)
			audioRateItem.emplace_back("48KHz", &defaultFace(), setRateDel(), 48000);
		item.emplace_back(&audioRate);
		audioRate.setSelected((MenuItem::Id)app().soundRate());
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

TextMenuItem::SelectDelegate AudioOptionView::setRateDel()
{
	return [this](TextMenuItem &item) { app().setSoundRate(item.id()); };
}

TextMenuItem::SelectDelegate AudioOptionView::setBuffersDel()
{
	return [this](TextMenuItem &item) { app().setSoundBuffers(item.id()); };
}

TextMenuItem::SelectDelegate AudioOptionView::setVolumeDel()
{
	return [this](TextMenuItem &item) { app().setSoundVolume(item.id()); };
}

}
