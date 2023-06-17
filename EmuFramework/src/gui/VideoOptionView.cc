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

#include <emuframework/VideoOptionView.hh>
#include <emuframework/EmuApp.hh>
#include <emuframework/EmuAppHelper.hh>
#include <emuframework/EmuVideoLayer.hh>
#include <emuframework/EmuVideo.hh>
#include <emuframework/VideoImageEffect.hh>
#include "../EmuOptions.hh"
#include "PlaceVideoView.hh"
#include <imagine/base/Screen.hh>
#include <imagine/base/ApplicationContext.hh>
#include <imagine/gfx/Renderer.hh>
#include <imagine/gfx/RendererCommands.hh>
#include <imagine/gui/TextTableView.hh>
#include <format>

namespace EmuEx
{

class DetectFrameRateView final: public View, public EmuAppHelper<DetectFrameRateView>
{
public:
	using DetectFrameRateDelegate = DelegateFunc<void (SteadyClockTime frameTime)>;
	DetectFrameRateDelegate onDetectFrameTime;
	IG::OnFrameDelegate detectFrameRate;
	SteadyClockTime totalFrameTime{};
	SteadyClockTimePoint lastFrameTimestamp{};
	Gfx::Text fpsText;
	int allTotalFrames{};
	int callbacks{};
	std::vector<SteadyClockTime> frameTimeSample{};
	bool useRenderTaskTime = false;

	DetectFrameRateView(ViewAttachParams attach): View(attach),
		fpsText{&defaultFace()}
	{
		defaultFace().precacheAlphaNum(attach.renderer());
		defaultFace().precache(attach.renderer(), ".");
		fpsText.resetString("准备检测刷新率...");
		useRenderTaskTime = !screen()->supportsTimestamps();
		frameTimeSample.reserve(std::round(screen()->frameRate() * 2.));
	}

	~DetectFrameRateView() final
	{
		window().setIntendedFrameRate(0);
		app().setCPUNeedsLowLatency(appContext(), false);
		window().removeOnFrame(detectFrameRate);
	}

	void place() final
	{
		fpsText.compile(renderer());
	}

	bool inputEvent(const Input::Event &e) final
	{
		if(e.keyEvent() && e.keyEvent()->pushed(Input::DefaultKey::CANCEL))
		{
			logMsg("aborted detection");
			dismiss();
			return true;
		}
		return false;
	}

	void draw(Gfx::RendererCommands &__restrict__ cmds) final
	{
		using namespace IG::Gfx;
		cmds.basicEffect().enableAlphaTexture(cmds);
		fpsText.draw(cmds, viewRect().center(), C2DO, ColorName::WHITE);
	}

	bool runFrameTimeDetection(SteadyClockTime timestampDiff, double slack)
	{
		const int framesToTime = frameTimeSample.capacity() * 10;
		allTotalFrames++;
		frameTimeSample.emplace_back(timestampDiff);
		if(frameTimeSample.size() == frameTimeSample.capacity())
		{
			bool stableFrameTime = true;
			SteadyClockTime frameTimeTotal{};
			{
				SteadyClockTime lastFrameTime{};
				for(auto frameTime : frameTimeSample)
				{
					frameTimeTotal += frameTime;
					if(!stableFrameTime)
						continue;
					double frameTimeDiffSecs =
						std::abs(IG::FloatSeconds(lastFrameTime - frameTime).count());
					if(lastFrameTime.count() && frameTimeDiffSecs > slack)
					{
						logMsg("frame times differed by:%f", frameTimeDiffSecs);
						stableFrameTime = false;
					}
					lastFrameTime = frameTime;
				}
			}
			auto frameTimeTotalSecs = FloatSeconds(frameTimeTotal);
			auto detectedFrameTimeSecs = frameTimeTotalSecs / (double)frameTimeSample.size();
			auto detectedFrameTime = round<SteadyClockTime>(detectedFrameTimeSecs);
			{
				waitForDrawFinished();
				if(detectedFrameTime.count())
					fpsText.resetString(std::format("{:g}fps", toHz(detectedFrameTimeSecs)));
				else
					fpsText.resetString("0fps");
				fpsText.compile(renderer());
			}
			if(stableFrameTime)
			{
				logMsg("found frame time:%f", detectedFrameTimeSecs.count());
				onDetectFrameTime(detectedFrameTime);
				dismiss();
				return false;
			}
			frameTimeSample.erase(frameTimeSample.cbegin());
			postDraw();
		}
		else
		{
			//logMsg("waiting for capacity:%zd/%zd", frameTimeSample.size(), frameTimeSample.capacity());
		}
		if(allTotalFrames >= framesToTime)
		{
			onDetectFrameTime(SteadyClockTime{});
			dismiss();
			return false;
		}
		else
		{
			if(useRenderTaskTime)
				postDraw();
			return true;
		}
	}

	void onAddedToController(ViewController *, const Input::Event &e) final
	{
		lastFrameTimestamp = SteadyClock::now();
		detectFrameRate =
			[this](IG::FrameParams params)
			{
				const int callbacksToSkip = 10;
				callbacks++;
				if(callbacks < callbacksToSkip)
				{
					if(useRenderTaskTime)
						postDraw();
					return true;
				}
				return runFrameTimeDetection(params.timestamp - std::exchange(lastFrameTimestamp, params.timestamp), 0.00175);
			};
		window().addOnFrame(detectFrameRate);
		app().setCPUNeedsLowLatency(appContext(), true);
	}
};

static std::string makeFrameRateStr(VideoSystem vidSys, const OutputTimingManager &mgr)
{
	auto frameTimeOpt = mgr.frameTimeOption(vidSys);
	if(frameTimeOpt == OutputTimingManager::autoOption)
		return "自动";
	else if(frameTimeOpt == OutputTimingManager::originalOption)
		return "原始";
	else
		return std::format("{:g}Hz", toHz(frameTimeOpt));
}

static const char *autoWindowPixelFormatStr(IG::ApplicationContext ctx)
{
	return ctx.defaultWindowPixelFormat() == PIXEL_RGB565 ? "RGB565" : "RGBA8888";
}

constexpr uint16_t pack(Gfx::DrawableConfig c)
{
	return to_underlying(c.pixelFormat.id()) | to_underlying(c.colorSpace) << sizeof(c.colorSpace) * 8;
}

constexpr Gfx::DrawableConfig unpackDrawableConfig(uint16_t c)
{
	return {PixelFormatID(c & 0xFF), Gfx::ColorSpace(c >> sizeof(Gfx::DrawableConfig::colorSpace) * 8)};
}

VideoOptionView::VideoOptionView(ViewAttachParams attach, bool customMenu):
	TableView{"视频设置", attach, item},
	textureBufferModeItem
	{
		[&]
		{
			decltype(textureBufferModeItem) items;
			items.emplace_back("自动（设置最佳模式）", &defaultFace(), [this](View &view)
			{
				app().textureBufferModeOption() = 0;
				auto defaultMode = renderer().makeValidTextureBufferMode();
				emuVideo().setTextureBufferMode(system(), defaultMode);
				textureBufferMode.setSelected(MenuItem::Id(defaultMode));
				view.dismiss();
				return false;
			}, 0);
			for(auto desc: renderer().textureBufferModes())
			{
				items.emplace_back(desc.name, &defaultFace(), [this](MenuItem &item)
				{
					app().textureBufferModeOption() = item.id();
					emuVideo().setTextureBufferMode(system(), Gfx::TextureBufferMode(item.id()));
				}, to_underlying(desc.mode));
			}
			return items;
		}()
	},
	textureBufferMode
	{
		"GPU复制模式", &defaultFace(),
		MenuItem::Id(renderer().makeValidTextureBufferMode(Gfx::TextureBufferMode(app().textureBufferModeOption().val))),
		textureBufferModeItem
	},
	frameIntervalItem
	{
		{"满速（不跳帧）", &defaultFace(), 0},
		{"满速",           &defaultFace(), 1},
		{"1/2",            &defaultFace(), 2},
		{"1/3",            &defaultFace(), 3},
		{"1/4",            &defaultFace(), 4},
	},
	frameInterval
	{
		"目标帧率", &defaultFace(),
		MultiChoiceMenuItem::Delegates
		{
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().setFrameInterval(item.id()); }
		},
		MenuItem::Id(app().frameInterval()),
		frameIntervalItem
	},
	frameRateItems
	{
		{"自动（帧数相近时使用屏幕刷新率）", &defaultFace(),
			[this]
			{
				if(!app().viewController().emuWindowScreen()->frameRateIsReliable())
				{
					app().postErrorMessage("报告的刷新率可能不可靠,"
						"使用检测到的刷新率可能会更好");
				}
				onFrameTimeChange(activeVideoSystem, OutputTimingManager::autoOption);
			}, int(OutputTimingManager::autoOption.count())
		},
		{"原始（使用模拟系统的速率）", &defaultFace(),
			[this]
			{
				onFrameTimeChange(activeVideoSystem, OutputTimingManager::originalOption);
			}, int(OutputTimingManager::originalOption.count())
		},
		{"检测屏幕刷新率并设置", &defaultFace(),
			[this](const Input::Event &e)
			{
				window().setIntendedFrameRate(system().frameRate());
				auto frView = makeView<DetectFrameRateView>();
				frView->onDetectFrameTime =
					[this](SteadyClockTime frameTime)
					{
						if(frameTime.count())
						{
							if(onFrameTimeChange(activeVideoSystem, frameTime))
								dismissPrevious();
						}
						else
						{
							app().postErrorMessage("检测到的刷新率太不稳定而无法使用");
						}
					};
				pushAndShowModal(std::move(frView), e);
				return false;
			}
		},
		{"自定义刷新率", &defaultFace(),
			[this](const Input::Event &e)
			{
				app().pushAndShowNewCollectValueInputView<std::pair<double, double>>(attachParams(), e,
					"输入整数或小数", "",
					[this](EmuApp &, auto val)
					{
						if(onFrameTimeChange(activeVideoSystem, fromSeconds<SteadyClockTime>(val.second / val.first)))
						{
							dismissPrevious();
							return true;
						}
						else
							return false;
					});
				return false;
			}, MenuItem::DEFAULT_ID
		},
	},
	frameRate
	{
		"刷新率", &defaultFace(),
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				t.resetString(makeFrameRateStr(VideoSystem::NATIVE_NTSC, app().outputTimingManager));
				return true;
			},
			.onSelect = [this](MultiChoiceMenuItem &item, View &view, const Input::Event &e)
			{
				activeVideoSystem = VideoSystem::NATIVE_NTSC;
				item.defaultOnSelect(view, e);
			},
		},
		app().outputTimingManager.frameTimeOptionAsMenuId(VideoSystem::NATIVE_NTSC),
		frameRateItems
	},
	frameRatePAL
	{
		"刷新率 (PAL)", &defaultFace(),
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				t.resetString(makeFrameRateStr(VideoSystem::PAL, app().outputTimingManager));
				return true;
			},
			.onSelect = [this](MultiChoiceMenuItem &item, View &view, const Input::Event &e)
			{
				activeVideoSystem = VideoSystem::PAL;
				item.defaultOnSelect(view, e);
			},
		},
		app().outputTimingManager.frameTimeOptionAsMenuId(VideoSystem::PAL),
		frameRateItems
	},
	frameTimeStats
	{
		"显示帧时间统计信息", &defaultFace(),
		app().showFrameTimeStats,
		[this](BoolMenuItem &item) { app().showFrameTimeStats = item.flipBoolValue(*this); }
	},
	aspectRatioItem
	{
		[&]()
		{
			StaticArrayList<TextMenuItem, MAX_ASPECT_RATIO_ITEMS> aspectRatioItem;
			for(const auto &i : EmuSystem::aspectRatioInfos())
			{
				aspectRatioItem.emplace_back(i.name, &defaultFace(), [this](TextMenuItem &item)
				{
					app().setVideoAspectRatio(std::bit_cast<float>(item.id()));
				}, std::bit_cast<MenuItem::Id>(i.aspect.ratio<float>()));
			}
			if(EmuSystem::hasRectangularPixels)
			{
				aspectRatioItem.emplace_back("正方形像素", &defaultFace(), [this]()
				{
					app().setVideoAspectRatio(-1);
				}, std::bit_cast<MenuItem::Id>(-1.f));
			}
			aspectRatioItem.emplace_back("填充屏幕", &defaultFace(), [this]()
			{
				app().setVideoAspectRatio(0);
			}, 0);
			aspectRatioItem.emplace_back("自定义数值", &defaultFace(), [this](const Input::Event &e)
			{
				app().pushAndShowNewCollectValueInputView<std::pair<float, float>>(attachParams(), e,
					"输入整数或小数", "",
					[this](EmuApp &app, auto val)
					{
						float ratio = val.first / val.second;
						if(app.setVideoAspectRatio(ratio))
						{
							aspectRatio.setSelected(std::bit_cast<MenuItem::Id>(ratio), *this);
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
			}, MenuItem::DEFAULT_ID);
			return aspectRatioItem;
		}()
	},
	aspectRatio
	{
		"显示比例", &defaultFace(),
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				if(idx == aspectRatioItem.size() - 1)
				{
					t.resetString(std::format("{:g}", app().videoAspectRatio()));
					return true;
				}
				return false;
			}
		},
		std::bit_cast<MenuItem::Id>(app().videoAspectRatio()),
		aspectRatioItem
	},
	zoomItem
	{
		{"100%",                  &defaultFace(), 100},
		{"90%",                   &defaultFace(), 90},
		{"80%",                   &defaultFace(), 80},
		{"仅整数倍",          &defaultFace(), optionImageZoomIntegerOnly},
		{"仅整数倍(高度)", &defaultFace(), optionImageZoomIntegerOnlyY},
		{"自定义数值", &defaultFace(),
			[this](const Input::Event &e)
			{
				app().pushAndShowNewCollectValueRangeInputView<int, 10, 100>(attachParams(), e, "输入10到100", "",
					[this](EmuApp &app, auto val)
					{
						app.setVideoZoom(val);
						zoom.setSelected((MenuItem::Id)val, *this);
						dismissPrevious();
						return true;
					});
				return false;
			}, MenuItem::DEFAULT_ID
		},
	},
	zoom
	{
		"画面缩放", &defaultFace(),
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				if(app().videoZoom() <= 100)
				{
					t.resetString(std::format("{}%", app().videoZoom()));
					return true;
				}
				return false;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().setVideoZoom(item.id()); }
		},
		(MenuItem::Id)app().videoZoom(),
		zoomItem
	},
	viewportZoomItem
	{
		{"100%", &defaultFace(), 100},
		{"95%", &defaultFace(),  95},
		{"90%", &defaultFace(),  90},
		{"自定义数值", &defaultFace(),
			[this](const Input::Event &e)
			{
				app().pushAndShowNewCollectValueRangeInputView<int, 50, 100>(attachParams(), e, "输入50到100", "",
					[this](EmuApp &app, auto val)
					{
						app.setViewportZoom(val);
						viewportZoom.setSelected((MenuItem::Id)val, *this);
						dismissPrevious();
						return true;
					});
				return false;
			}, MenuItem::DEFAULT_ID
		},
	},
	viewportZoom
	{
		"应用缩放", &defaultFace(),
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				t.resetString(std::format("{}%", app().viewportZoom()));
				return true;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().setViewportZoom(item.id()); }
		},
		(MenuItem::Id)app().viewportZoom(),
		viewportZoomItem
	},
	contentRotationItem
	{
		{"自动",        &defaultFace(), std::to_underlying(Rotation::ANY)},
		{"标准",    &defaultFace(), std::to_underlying(Rotation::UP)},
		{"右旋转90°",   &defaultFace(), std::to_underlying(Rotation::RIGHT)},
		{"上下翻转", &defaultFace(), std::to_underlying(Rotation::DOWN)},
		{"左旋转90°",    &defaultFace(), std::to_underlying(Rotation::LEFT)},
	},
	contentRotation
	{
		"画面旋转", &defaultFace(),
		{
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().setContentRotation(Rotation(item.id())); }
		},
		(MenuItem::Id)app().contentRotation(),
		contentRotationItem
	},
	placeVideo
	{
		"设置画面位置", &defaultFace(),
		[this](const Input::Event &e)
		{
			if(!system().hasContent())
				return;
			pushAndShowModal(makeView<PlaceVideoView>(*videoLayer, app().defaultVController()), e);
		}
	},
	imgFilter
	{
		"图像插值", &defaultFace(),
		(bool)app().videoFilterOption(),
		"无", "线性",
		[this](BoolMenuItem &item)
		{
			app().videoFilterOption().val = item.flipBoolValue(*this);
			videoLayer->setLinearFilter(app().videoFilterOption());
			app().viewController().postDrawToEmuWindows();
		}
	},
	imgEffectItem
	{
		{"关",         &defaultFace(), std::to_underlying(ImageEffectId::DIRECT)},
		{"hq2x",        &defaultFace(), std::to_underlying(ImageEffectId::HQ2X)},
		{"Scale2x",     &defaultFace(), std::to_underlying(ImageEffectId::SCALE2X)},
		{"Prescale 2x", &defaultFace(), std::to_underlying(ImageEffectId::PRESCALE2X)},
		{"Prescale 3x", &defaultFace(), std::to_underlying(ImageEffectId::PRESCALE3X)},
		{"Prescale 4x", &defaultFace(), std::to_underlying(ImageEffectId::PRESCALE4X)},
	},
	imgEffect
	{
		"图像效果", &defaultFace(),
		{
			.defaultItemOnSelect = [this](TextMenuItem &item)
			{
				app().videoEffectOption() = item.id();
				if(emuVideo().image())
				{
					videoLayer->setEffect(system(), ImageEffectId(item.id()), app().videoEffectPixelFormat());
					app().viewController().postDrawToEmuWindows();
				}
			}
		},
		(MenuItem::Id)app().videoEffectOption().val,
		imgEffectItem
	},
	overlayEffectItem
	{
		{"关",            &defaultFace(), 0},
		{"扫描线",      &defaultFace(), std::to_underlying(ImageOverlayId::SCANLINES)},
		{"扫描线 2x",   &defaultFace(), std::to_underlying(ImageOverlayId::SCANLINES_2)},
		{"LCD Grid",       &defaultFace(), std::to_underlying(ImageOverlayId::LCD)},
		{"CRT Mask",       &defaultFace(), std::to_underlying(ImageOverlayId::CRT_MASK)},
		{"CRT Mask .5x",   &defaultFace(), std::to_underlying(ImageOverlayId::CRT_MASK_2)},
		{"CRT Grille",     &defaultFace(), std::to_underlying(ImageOverlayId::CRT_GRILLE)},
		{"CRT Grille .5x", &defaultFace(), std::to_underlying(ImageOverlayId::CRT_GRILLE_2)}
	},
	overlayEffect
	{
		"叠加效果", &defaultFace(),
		{
			.defaultItemOnSelect = [this](TextMenuItem &item)
			{
				app().overlayEffectOption() = item.id();
				videoLayer->setOverlay((ImageOverlayId)item.id());
				app().viewController().postDrawToEmuWindows();
			}
		},
		(MenuItem::Id)app().overlayEffectOption().val,
		overlayEffectItem
	},
	overlayEffectLevelItem
	{
		{"100%", &defaultFace(), 100},
		{"75%",  &defaultFace(), 75},
		{"50%",  &defaultFace(), 50},
		{"25%",  &defaultFace(), 25},
		{"自定义数值", &defaultFace(),
			[this](const Input::Event &e)
			{
				app().pushAndShowNewCollectValueRangeInputView<int, 0, 100>(attachParams(), e, "输入0到100", "",
					[this](EmuApp &app, auto val)
					{
						app.setOverlayEffectLevel(*videoLayer, val);
						overlayEffectLevel.setSelected((MenuItem::Id)val, *this);
						dismissPrevious();
						return true;
					});
				return false;
			}, MenuItem::DEFAULT_ID
		},
	},
	overlayEffectLevel
	{
		"叠加效果级别", &defaultFace(),
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				t.resetString(std::format("{}%", app().overlayEffectLevel()));
				return true;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().setOverlayEffectLevel(*videoLayer, item.id()); }
		},
		(MenuItem::Id)app().overlayEffectLevel(),
		overlayEffectLevelItem
	},
	imgEffectPixelFormatItem
	{
		{"自动(匹配显示格式)", &defaultFace(), PIXEL_NONE},
		{"RGBA8888",                    &defaultFace(), PIXEL_RGBA8888},
		{"RGB565",                      &defaultFace(), PIXEL_RGB565},
	},
	imgEffectPixelFormat
	{
		"效果颜色格式", &defaultFace(),
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				if(idx == 0)
				{
					t.resetString(app().videoEffectPixelFormat().name());
					return true;
				}
				else
					return false;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item)
			{
				app().videoEffectPixelFormatOption() = item.id();
				videoLayer->setEffectFormat(app().videoEffectPixelFormat());
				app().viewController().postDrawToEmuWindows();
			}
		},
		(MenuItem::Id)app().videoEffectPixelFormatOption().val,
		imgEffectPixelFormatItem
	},
	windowPixelFormatItem
	{
		[&]
		{
			decltype(windowPixelFormatItem) items;
			auto setWindowDrawableConfigDel = [this](TextMenuItem &item)
			{
				auto conf = unpackDrawableConfig(item.id());
				if(!app().setWindowDrawableConfig(conf))
				{
					app().postMessage("重启APP后设置生效");
					return;
				}
				renderPixelFormat.updateDisplayString();
				imgEffectPixelFormat.updateDisplayString();
			};
			items.emplace_back("自动", &defaultFace(), setWindowDrawableConfigDel, 0);
			for(auto desc: renderer().supportedDrawableConfigs())
			{
				items.emplace_back(desc.name, &defaultFace(), setWindowDrawableConfigDel, pack(desc.config));
			}
			return items;
		}()
	},
	windowPixelFormat
	{
		"显示颜色格式", &defaultFace(),
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				if(idx == 0)
				{
					t.resetString(autoWindowPixelFormatStr(appContext()));
					return true;
				}
				else
					return false;
			}
		},
		MenuItem::Id(pack(app().windowDrawableConfig())),
		windowPixelFormatItem
	},
	secondDisplay
	{
		"2nd Window (for testing only)", &defaultFace(),
		false,
		[this](BoolMenuItem &item)
		{
			app().setEmuViewOnExtraWindow(item.flipBoolValue(*this), appContext().mainScreen());
		}
	},
	showOnSecondScreen
	{
		"外接屏幕", &defaultFace(),
		(bool)app().showOnSecondScreenOption(),
		"系统管理", "游戏画面",
		[this](BoolMenuItem &item)
		{
			app().showOnSecondScreenOption() = item.flipBoolValue(*this);
			if(appContext().screens().size() > 1)
				app().setEmuViewOnExtraWindow(app().showOnSecondScreenOption(), *appContext().screens()[1]);
		}
	},
	imageBuffersItem
	{
		{"自动",                                     &defaultFace(), 0},
		{"1 (每帧同步GPU，减少输入延迟)", &defaultFace(), 1},
		{"2 (更稳定，可能会增加1帧延迟)",  &defaultFace(), 2},
	},
	imageBuffers
	{
		"图像缓冲区", &defaultFace(),
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				t.resetString(emuVideo().imageBuffers() == 1 ? "1" : "2");
				return true;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item)
			{
				app().videoImageBuffersOption() = item.id();
				emuVideo().setImageBuffers(item.id());
			}
		},
		(MenuItem::Id)app().videoImageBuffersOption().val,
		imageBuffersItem
	},
	presentModeItems
	{
		{"自动",                                                    &defaultFace(), to_underlying(Gfx::PresentMode::Auto)},
		{"立即(延迟较低，但可能会丢帧)", &defaultFace(), to_underlying(Gfx::PresentMode::Immediate)},
		{"队列(帧率更稳)",                    &defaultFace(), to_underlying(Gfx::PresentMode::FIFO)},
	},
	presentMode
	{
		"呈现模式", &defaultFace(),
		MultiChoiceMenuItem::Delegates
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				t.resetString(renderer().evalPresentMode(app().emuWindow(), app().presentMode) == Gfx::PresentMode::FIFO ? "队列" : "立即");
				return true;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item)
			{
				app().presentMode = Gfx::PresentMode(item.id());
			}
		},
		MenuItem::Id(Gfx::PresentMode(app().presentMode)),
		presentModeItems
	},
	renderPixelFormatItem
	{
		{"自动(根据需要匹配渲染格式)", &defaultFace(), PIXEL_NONE},
		{"RGBA8888",                    &defaultFace(), PIXEL_RGBA8888},
		{"RGB565",                      &defaultFace(), PIXEL_RGB565},
	},
	renderPixelFormat
	{
		"渲染颜色格式", &defaultFace(),
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				if(idx == 0)
				{
					t.resetString(emuVideo().internalRenderPixelFormat().name());
					return true;
				}
				return false;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().setRenderPixelFormat(PixelFormatID(item.id())); }
		},
		(MenuItem::Id)app().renderPixelFormat().id(),
		renderPixelFormatItem
	},
	screenFrameRateItems
	{
		[&]
		{
			std::vector<TextMenuItem> items;
			auto setRateDel = [this](TextMenuItem &item) { app().overrideScreenFrameRate = std::bit_cast<FrameRate>(item.id()); };
			items.emplace_back("关", &defaultFace(), setRateDel, 0);
			for(auto rate : app().emuScreen().supportedFrameRates())
				items.emplace_back(std::format("{:g}Hz", rate), &defaultFace(), setRateDel, std::bit_cast<MenuItem::Id>(rate));
			return items;
		}()
	},
	screenFrameRate
	{
		"覆盖屏幕帧率", &defaultFace(),
		std::bit_cast<MenuItem::Id>(FrameRate(app().overrideScreenFrameRate)),
		screenFrameRateItems
	},
	presentationTime
	{
		"精确帧节奏", &defaultFace(),
		app().usePresentationTime,
		[this](BoolMenuItem &item) { app().usePresentationTime = item.flipBoolValue(*this); }
	},
	blankFrameInsertion
	{
		"允许插入空白帧", &defaultFace(),
		app().allowBlankFrameInsertion,
		[this](BoolMenuItem &item) { app().allowBlankFrameInsertion = item.flipBoolValue(*this); }
	},
	brightnessItem
	{
		{
			"默认", &defaultFace(), [this](View &v)
			{
				app().setVideoBrightness(1.f, ImageChannel::All);
				setAllColorLevelsSelected(MenuItem::Id{100});
				v.dismiss();
			}
		},
		{"自定义数值", &defaultFace(), setVideoBrightnessCustomDel(ImageChannel::All)},
	},
	redItem
	{
		{"默认", &defaultFace(), [this](){ app().setVideoBrightness(1.f, ImageChannel::Red); }, 100},
		{"自定义数值", &defaultFace(), setVideoBrightnessCustomDel(ImageChannel::Red), MenuItem::DEFAULT_ID},
	},
	greenItem
	{
		{"默认", &defaultFace(), [this](){ app().setVideoBrightness(1.f, ImageChannel::Green); }, 100},
		{"自定义数值", &defaultFace(), setVideoBrightnessCustomDel(ImageChannel::Green), MenuItem::DEFAULT_ID},
	},
	blueItem
	{
		{"默认", &defaultFace(), [this](){ app().setVideoBrightness(1.f, ImageChannel::Blue); }, 100},
		{"自定义数值", &defaultFace(), setVideoBrightnessCustomDel(ImageChannel::Blue), MenuItem::DEFAULT_ID},
	},
	brightness
	{
		"设置所有级别", &defaultFace(),
		[this](const Input::Event &e)
		{
			pushAndShow(makeViewWithName<TableView>("所有级别", brightnessItem), e);
		}
	},
	red
	{
		"红", &defaultFace(),
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				t.resetString(std::format("{}%", app().videoBrightnessAsInt(ImageChannel::Red)));
				return true;
			}
		},
		MenuItem::Id{app().videoBrightnessAsInt(ImageChannel::Red)},
		redItem
	},
	green
	{
		"绿", &defaultFace(),
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				t.resetString(std::format("{}%", app().videoBrightnessAsInt(ImageChannel::Green)));
				return true;
			}
		},
		MenuItem::Id{app().videoBrightnessAsInt(ImageChannel::Green)},
		greenItem
	},
	blue
	{
		"蓝", &defaultFace(),
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				t.resetString(std::format("{}%", app().videoBrightnessAsInt(ImageChannel::Blue)));
				return true;
			}
		},
		MenuItem::Id{app().videoBrightnessAsInt(ImageChannel::Blue)},
		blueItem
	},
	visualsHeading{"视觉效果", &defaultBoldFace()},
	screenShapeHeading{"屏幕形状", &defaultBoldFace()},
	colorLevelsHeading{"颜色级别", &defaultBoldFace()},
	advancedHeading{"高级", &defaultBoldFace()},
	systemSpecificHeading{"系统特定", &defaultBoldFace()}
{
	if(!customMenu)
	{
		loadStockItems();
	}
}

void VideoOptionView::place()
{
	aspectRatio.setSelected(std::bit_cast<MenuItem::Id>(app().videoAspectRatio()), *this);
	TableView::place();
}


void VideoOptionView::loadStockItems()
{
	item.emplace_back(&frameInterval);
	item.emplace_back(&frameRate);
	if(EmuSystem::hasPALVideoSystem)
	{
		item.emplace_back(&frameRatePAL);
	}
	if(used(frameTimeStats))
		item.emplace_back(&frameTimeStats);
	item.emplace_back(&visualsHeading);
	item.emplace_back(&imgFilter);
	item.emplace_back(&imgEffect);
	item.emplace_back(&overlayEffect);
	item.emplace_back(&overlayEffectLevel);
	item.emplace_back(&screenShapeHeading);
	item.emplace_back(&zoom);
	item.emplace_back(&viewportZoom);
	item.emplace_back(&aspectRatio);
	item.emplace_back(&contentRotation);
	placeVideo.setActive(system().hasContent());
	item.emplace_back(&placeVideo);
	item.emplace_back(&colorLevelsHeading);
	item.emplace_back(&brightness);
	item.emplace_back(&red);
	item.emplace_back(&green);
	item.emplace_back(&blue);
	item.emplace_back(&advancedHeading);
	item.emplace_back(&textureBufferMode);
	if(windowPixelFormatItem.size() > 2)
	{
		item.emplace_back(&windowPixelFormat);
	}
	if(EmuSystem::canRenderRGBA8888)
		item.emplace_back(&renderPixelFormat);
	item.emplace_back(&imgEffectPixelFormat);
	if(!app().videoImageBuffersOption().isConst)
		item.emplace_back(&imageBuffers);
	if(used(presentMode) && app().supportsPresentModes())
		item.emplace_back(&presentMode);
	if(used(presentationTime) && renderer().supportsPresentationTime())
		item.emplace_back(&presentationTime);
	item.emplace_back(&blankFrameInsertion);
	if(used(screenFrameRate) && app().emuScreen().supportedFrameRates().size() > 1)
		item.emplace_back(&screenFrameRate);
	if(used(secondDisplay))
		item.emplace_back(&secondDisplay);
	if(used(showOnSecondScreen) && !app().showOnSecondScreenOption().isConst)
		item.emplace_back(&showOnSecondScreen);
}

void VideoOptionView::setEmuVideoLayer(EmuVideoLayer &videoLayer_)
{
	videoLayer = &videoLayer_;
}

bool VideoOptionView::onFrameTimeChange(VideoSystem vidSys, SteadyClockTime time)
{
	if(!app().outputTimingManager.setFrameTimeOption(vidSys, time))
	{
		app().postMessage(4, true, std::format("{:g}Hz 不在有效范围内", toHz(time)));
		return false;
	}
	return true;
}

TextMenuItem::SelectDelegate VideoOptionView::setVideoBrightnessCustomDel(ImageChannel ch)
{
	return [=, this](const Input::Event &e)
	{
		app().pushAndShowNewCollectValueRangeInputView<int, 0, 200>(attachParams(), e, "输入0到200", "",
			[=, this](EmuApp &app, auto val)
			{
				app.setVideoBrightness(val / 100.f, ch);
				if(ch == ImageChannel::All)
					setAllColorLevelsSelected(MenuItem::Id{val});
				else
					[&]() -> MultiChoiceMenuItem&
					{
						switch(ch)
						{
							case ImageChannel::All: break;
							case ImageChannel::Red: return red;
							case ImageChannel::Green: return green;
							case ImageChannel::Blue: return blue;
						}
						bug_unreachable("invalid ImageChannel");
					}().setSelected(MenuItem::Id{val}, *this);
				dismissPrevious();
				return true;
			});
		return false;
	};
}

void VideoOptionView::setAllColorLevelsSelected(MenuItem::Id val)
{
	red.setSelected(val, *this);
	green.setSelected(val, *this);
	blue.setSelected(val, *this);
}

EmuVideo &VideoOptionView::emuVideo() const
{
	return videoLayer->emuVideo();
}

}
