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
#include <emuframework/EmuAppHelper.hh>
#include <emuframework/EmuVideoLayer.hh>
#include <emuframework/EmuVideo.hh>
#include <emuframework/VideoImageEffect.hh>
#include "EmuOptions.hh"
#include "private.hh"
#include <imagine/base/Screen.hh>
#include <imagine/base/ApplicationContext.hh>
#include <imagine/gfx/Renderer.hh>
#include <imagine/gfx/RendererCommands.hh>
#include <imagine/gui/AlertView.hh>
#include <imagine/gui/TextTableView.hh>
#include <imagine/util/format.hh>

namespace EmuEx
{

class DetectFrameRateView final: public View, public EmuAppHelper<DetectFrameRateView>
{
public:
	using DetectFrameRateDelegate = DelegateFunc<void (IG::FloatSeconds frameTime)>;
	DetectFrameRateDelegate onDetectFrameTime;
	IG::OnFrameDelegate detectFrameRate;
	IG::FrameTime totalFrameTime{};
	IG::FrameTime lastFrameTimestamp{};
	Gfx::Text fpsText;
	unsigned allTotalFrames = 0;
	unsigned callbacks = 0;
	std::vector<IG::FrameTime> frameTimeSample{};
	bool useRenderTaskTime = false;

	DetectFrameRateView(ViewAttachParams attach): View(attach),
		fpsText{{}, &defaultFace()}
	{
		defaultFace().precacheAlphaNum(attach.renderer());
		defaultFace().precache(attach.renderer(), ".");
		fpsText.setString("准备检测刷新率...");
		useRenderTaskTime = !screen()->supportsTimestamps();
		frameTimeSample.reserve(std::round(screen()->frameRate() * 2.));
	}

	~DetectFrameRateView() final
	{
		window().setIntendedFrameRate(0.);
		app().setCPUNeedsLowLatency(appContext(), false);
		window().removeOnFrame(detectFrameRate);
	}

	void place() final
	{
		fpsText.compile(renderer(), projP);
	}

	bool inputEvent(const Input::Event &e) final
	{
		if(e.keyEvent() && e.asKeyEvent().pushed(Input::DefaultKey::CANCEL))
		{
			logMsg("aborted detection");
			dismiss();
			return true;
		}
		return false;
	}

	void draw(Gfx::RendererCommands &cmds) final
	{
		using namespace IG::Gfx;
		cmds.setColor(1., 1., 1., 1.);
		cmds.setCommonProgram(CommonProgram::TEX_ALPHA, projP.makeTranslate());
		fpsText.draw(cmds, projP.alignXToPixel(projP.bounds().xCenter()),
			projP.alignYToPixel(projP.bounds().yCenter()), C2DO, projP);
	}

	bool runFrameTimeDetection(IG::FrameTime timestampDiff, double slack)
	{
		const unsigned framesToTime = frameTimeSample.capacity() * 10;
		allTotalFrames++;
		frameTimeSample.emplace_back(timestampDiff);
		if(frameTimeSample.size() == frameTimeSample.capacity())
		{
			bool stableFrameTime = true;
			IG::FrameTime frameTimeTotal{};
			{
				IG::FrameTime lastFrameTime{};
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
			IG::FloatSeconds frameTimeTotalSecs = IG::FloatSeconds(frameTimeTotal);
			IG::FloatSeconds detectedFrameTime = frameTimeTotalSecs / (double)frameTimeSample.size();
			{
				waitForDrawFinished();
				if(detectedFrameTime.count())
					fpsText.setString(fmt::format("{:.2f}fps", 1. / detectedFrameTime.count()));
				else
					fpsText.setString("0fps");
				fpsText.compile(renderer(), projP);
			}
			if(stableFrameTime)
			{
				logMsg("found frame time:%f", detectedFrameTime.count());
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
			onDetectFrameTime(IG::FloatSeconds{});
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
		lastFrameTimestamp = std::chrono::duration_cast<IG::FrameTime>(IG::steadyClockTimestamp());
		detectFrameRate =
			[this](IG::FrameParams params)
			{
				const unsigned callbacksToSkip = 10;
				callbacks++;
				if(callbacks < callbacksToSkip)
				{
					if(useRenderTaskTime)
						postDraw();
					return true;
				}
				return runFrameTimeDetection(params.timestamp() - std::exchange(lastFrameTimestamp, params.timestamp()), 0.00175);
			};
		window().addOnFrame(detectFrameRate);
		app().setCPUNeedsLowLatency(appContext(), true);
	}
};

static auto makeFrameRateStr(EmuSystem &sys)
{
	return fmt::format("刷新率: {:.2f}Hz",
		sys.frameRate(EmuSystem::VIDSYS_NATIVE_NTSC));
}

static auto makeFrameRatePALStr(EmuSystem &sys)
{
	return fmt::format("刷新率 (PAL): {:.2f}Hz",
		sys.frameRate(EmuSystem::VIDSYS_PAL));
}

TextMenuItem::SelectDelegate VideoOptionView::setFrameIntervalDel()
{
	return [this](TextMenuItem &item)
	{
		app().setFrameInterval(item.id());
		logMsg("set frame interval:%d", item.id());
	};
}

TextMenuItem::SelectDelegate VideoOptionView::setImgEffectDel()
{
	return [this](TextMenuItem &item)
	{
		app().videoEffectOption() = item.id();
		if(emuVideo().image())
		{
			videoLayer->setEffect(system(), (ImageEffectId)item.id(), app().videoEffectPixelFormat());
			app().viewController().postDrawToEmuWindows();
		}
	};
}

TextMenuItem::SelectDelegate VideoOptionView::setOverlayEffectDel()
{
	return [this](TextMenuItem &item)
	{
		app().overlayEffectOption() = item.id();
		videoLayer->setOverlay(item.id());
		app().viewController().postDrawToEmuWindows();
	};
}

TextMenuItem::SelectDelegate VideoOptionView::setImgEffectPixelFormatDel()
{
	return [this](TextMenuItem &item)
	{
		app().videoEffectPixelFormatOption() = item.id();
		videoLayer->setEffectFormat(app().videoEffectPixelFormat());
		app().viewController().postDrawToEmuWindows();
	};
}

TextMenuItem::SelectDelegate VideoOptionView::setRenderPixelFormatDel()
{
	return [this](TextMenuItem &item) { app().setRenderPixelFormat((PixelFormatID)item.id()); };
}

static const char *autoWindowPixelFormatStr(IG::ApplicationContext ctx)
{
	return ctx.defaultWindowPixelFormat() == PIXEL_RGB565 ? "RGB565" : "RGBA8888";
}

TextMenuItem::SelectDelegate VideoOptionView::setWindowDrawableConfigDel(Gfx::DrawableConfig conf)
{
	return [this, conf]()
	{
		if(!app().setWindowDrawableConfig(conf))
		{
			app().postMessage("Restart app for option to take effect");
			return;
		}
		renderPixelFormat.updateDisplayString();
		imgEffectPixelFormat.updateDisplayString();
	};
}

TextMenuItem::SelectDelegate VideoOptionView::setImageBuffersDel()
{
	return [this](TextMenuItem &item)
	{
		app().videoImageBuffersOption() = item.id();
		emuVideo().setImageBuffers(item.id());
	};
}

static int aspectRatioValueIndex(double val)
{
	iterateTimes(EmuSystem::aspectRatioInfos, i)
	{
		if(val == (double)EmuSystem::aspectRatioInfo[i])
		{
			return i;
		}
	}
	return -1;
}

VideoOptionView::VideoOptionView(ViewAttachParams attach, bool customMenu):
	TableView{"视频设置", attach, item},
	textureBufferMode
	{
		"GPU复制模式", &defaultFace(),
		0,
		textureBufferModeItem
	},
	frameIntervalItem
	{
		{"Full", &defaultFace(), setFrameIntervalDel(), 1},
		{"1/2",  &defaultFace(), setFrameIntervalDel(), 2},
		{"1/3",  &defaultFace(), setFrameIntervalDel(), 3},
		{"1/4",  &defaultFace(), setFrameIntervalDel(), 4},
	},
	frameInterval
	{
		"Target Frame Rate", &defaultFace(),
		(MenuItem::Id)app().frameInterval(),
		frameIntervalItem
	},
	dropLateFrames
	{
		"跳过延迟帧", &defaultFace(),
		(bool)app().shouldSkipLateFrames(),
		[this](BoolMenuItem &item)
		{
			app().setShouldSkipLateFrames(item.flipBoolValue(*this));
		}
	},
	frameRate
	{
		{}, &defaultFace(),
		[this](const Input::Event &e)
		{
			pushAndShowFrameRateSelectMenu(EmuSystem::VIDSYS_NATIVE_NTSC, e);
			postDraw();
		}
	},
	frameRatePAL
	{
		{}, &defaultFace(),
		[this](const Input::Event &e)
		{
			pushAndShowFrameRateSelectMenu(EmuSystem::VIDSYS_PAL, e);
			postDraw();
		}
	},
	aspectRatio
	{
		"显示比例", &defaultFace(),
		[this](auto idx, Gfx::Text &t)
		{
			if(idx == EmuSystem::aspectRatioInfos)
			{
				t.setString(fmt::format("{:.2f}", app().videoAspectRatio()));
				return true;
			}
			return false;
		},
		(int)EmuSystem::aspectRatioInfos,
		aspectRatioItem
	},
	zoomItem
	{
		{"100%",                  &defaultFace(), setZoomDel(), 100},
		{"90%",                   &defaultFace(), setZoomDel(), 90},
		{"80%",                   &defaultFace(), setZoomDel(), 80},
		{"仅整数倍",          &defaultFace(), setZoomDel(), optionImageZoomIntegerOnly},
		{"仅整数倍(高度)", &defaultFace(), setZoomDel(), optionImageZoomIntegerOnlyY},
		{"自定义", &defaultFace(),
			[this](const Input::Event &e)
			{
				app().pushAndShowNewCollectValueInputView<int>(attachParams(), e, "输入10到100", "",
					[this](EmuApp &app, auto val)
					{
						if(app.setVideoZoom(val))
						{
							zoom.setSelected(std::size(zoomItem) - 1, *this);
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
	zoom
	{
		"画面缩放", &defaultFace(),
		[this](auto idx, Gfx::Text &t)
		{
			if(app().videoZoom() <= 100)
			{
				t.setString(fmt::format("{}%", app().videoZoom()));
				return true;
			}
			return false;
		},
		(MenuItem::Id)app().videoZoom(),
		zoomItem
	},
	viewportZoomItem
	{
		{"100%", &defaultFace(), setViewportZoomDel(), 100},
		{"95%", &defaultFace(),  setViewportZoomDel(), 95},
		{"90%", &defaultFace(),  setViewportZoomDel(), 90},
		{"自定义", &defaultFace(),
			[this](const Input::Event &e)
			{
				app().pushAndShowNewCollectValueInputView<int>(attachParams(), e, "输入50到100", "",
					[this](EmuApp &app, auto val)
					{
						if(app.setViewportZoom(val))
						{
							viewportZoom.setSelected(std::size(viewportZoomItem) - 1, *this);
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
	viewportZoom
	{
		"应用缩放", &defaultFace(),
		[this](auto idx, Gfx::Text &t)
		{
			t.setString(fmt::format("{}%", app().viewportZoom()));
			return true;
		},
		(MenuItem::Id)app().viewportZoom(),
		viewportZoomItem
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
		{"关",         &defaultFace(), setImgEffectDel(), (uint8_t)ImageEffectId::DIRECT},
		{"hq2x",        &defaultFace(), setImgEffectDel(), (uint8_t)ImageEffectId::HQ2X},
		{"Scale2x",     &defaultFace(), setImgEffectDel(), (uint8_t)ImageEffectId::SCALE2X},
		{"Prescale 2x", &defaultFace(), setImgEffectDel(), (uint8_t)ImageEffectId::PRESCALE2X}
	},
	imgEffect
	{
		"图像效果", &defaultFace(),
		(MenuItem::Id)app().videoEffectOption().val,
		imgEffectItem
	},
	overlayEffectItem
	{
		{"关",          &defaultFace(), setOverlayEffectDel(), 0},
		{"扫描线",    &defaultFace(), setOverlayEffectDel(), VideoImageOverlay::SCANLINES},
		{"扫描线 2x", &defaultFace(), setOverlayEffectDel(), VideoImageOverlay::SCANLINES_2},
		{"CRT Mask",     &defaultFace(), setOverlayEffectDel(), VideoImageOverlay::CRT},
		{"CRT",          &defaultFace(), setOverlayEffectDel(), VideoImageOverlay::CRT_RGB},
		{"CRT 2x",       &defaultFace(), setOverlayEffectDel(), VideoImageOverlay::CRT_RGB_2}
	},
	overlayEffect
	{
		"叠加效果", &defaultFace(),
		(MenuItem::Id)app().overlayEffectOption().val,
		overlayEffectItem
	},
	overlayEffectLevelItem
	{
		{"100%", &defaultFace(), setOverlayEffectLevelDel(), 100},
		{"75%",  &defaultFace(), setOverlayEffectLevelDel(), 75},
		{"50%",  &defaultFace(), setOverlayEffectLevelDel(), 50},
		{"25%",  &defaultFace(), setOverlayEffectLevelDel(), 25},
		{"自定义", &defaultFace(),
			[this](const Input::Event &e)
			{
				app().pushAndShowNewCollectValueInputView<int>(attachParams(), e, "输入10到100", "",
					[this](EmuApp &app, auto val)
					{
						if(app.setOverlayEffectLevel(*videoLayer, val))
						{
							overlayEffectLevel.setSelected(std::size(overlayEffectLevelItem) - 1, *this);
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
	overlayEffectLevel
	{
		"叠加效果级别", &defaultFace(),
		[this](auto idx, Gfx::Text &t)
		{
			t.setString(fmt::format("{}%", app().overlayEffectLevel()));
			return true;
		},
		(MenuItem::Id)app().overlayEffectLevel(),
		overlayEffectLevelItem
	},
	imgEffectPixelFormatItem
	{
		{"自动(匹配显示格式)", &defaultFace(), setImgEffectPixelFormatDel(), PIXEL_NONE},
		{"RGBA8888",                    &defaultFace(), setImgEffectPixelFormatDel(), PIXEL_RGBA8888},
		{"RGB565",                      &defaultFace(), setImgEffectPixelFormatDel(), PIXEL_RGB565},
	},
	imgEffectPixelFormat
	{
		"效果颜色格式", &defaultFace(),
		[this](int idx, Gfx::Text &t)
		{
			if(idx == 0)
			{
				t.setString(app().videoEffectPixelFormat().name());
				return true;
			}
			else
				return false;
		},
		(MenuItem::Id)app().videoEffectPixelFormatOption().val,
		imgEffectPixelFormatItem
	},
	windowPixelFormat
	{
		"显示颜色格式", &defaultFace(),
		[this](int idx, Gfx::Text &t)
		{
			if(idx == 0)
			{
				t.setString(autoWindowPixelFormatStr(appContext()));
				return true;
			}
			else
				return false;
		},
		0,
		windowPixelFormatItem
	},
	#if defined CONFIG_BASE_MULTI_WINDOW && defined CONFIG_BASE_X11
	secondDisplay
	{
		"2nd Window (for testing only)", &defaultFace(),
		false,
		[this](BoolMenuItem &item)
		{
			app().viewController().setEmuViewOnExtraWindow(item.flipBoolValue(*this), appContext().mainScreen());
		}
	},
	#endif
	#if defined CONFIG_BASE_MULTI_WINDOW && defined CONFIG_BASE_MULTI_SCREEN
	showOnSecondScreen
	{
		"外接屏幕", &defaultFace(),
		(bool)app().showOnSecondScreenOption(),
		"系统管理", "游戏内容",
		[this](BoolMenuItem &item)
		{
			app().showOnSecondScreenOption() = item.flipBoolValue(*this);
			if(appContext().screens().size() > 1)
				app().viewController().setEmuViewOnExtraWindow(app().showOnSecondScreenOption(), *appContext().screens()[1]);
		}
	},
	#endif
	imageBuffersItem
	{
		{"自动",                                     &defaultFace(), setImageBuffersDel(), 0},
		{"1 (每帧同步GPU，减少输入延迟)", &defaultFace(), setImageBuffersDel(), 1},
		{"2 (更稳定，可能会增加1帧延迟)",  &defaultFace(), setImageBuffersDel(), 2},
	},
	imageBuffers
	{
		"图像缓冲区", &defaultFace(),
		[this](int idx, Gfx::Text &t)
		{
			t.setString(emuVideo().imageBuffers() == 1 ? "1" : "2");
			return true;
		},
		(MenuItem::Id)app().videoImageBuffersOption().val,
		imageBuffersItem
	},
	renderPixelFormatItem
	{
		{"自动(根据需要匹配渲染格式)", &defaultFace(), setRenderPixelFormatDel(), IG::PIXEL_NONE},
		{"RGBA8888",                    &defaultFace(), setRenderPixelFormatDel(), IG::PIXEL_RGBA8888},
		{"RGB565",                      &defaultFace(), setRenderPixelFormatDel(), IG::PIXEL_RGB565},
	},
	renderPixelFormat
	{
		"渲染颜色格式", &defaultFace(),
		[this](int idx, Gfx::Text &t)
		{
			if(idx == 0)
			{
				t.setString(emuVideo().internalRenderPixelFormat().name());
				return true;
			}
			return false;
		},
		(MenuItem::Id)app().renderPixelFormat().id(),
		renderPixelFormatItem
	},
	presentationTime
	{
		"减少合成器延迟", &defaultFace(),
		app().viewController().usePresentationTime(),
		[this](BoolMenuItem &item)
		{
			app().viewController().setUsePresentationTime(item.flipBoolValue(*this));
		}
	},
	visualsHeading{"视觉效果", &defaultBoldFace()},
	screenShapeHeading{"屏幕形状", &defaultBoldFace()},
	advancedHeading{"高级", &defaultBoldFace()},
	systemSpecificHeading{"系统特定", &defaultBoldFace()}
{
	windowPixelFormatItem.emplace_back("自动", &defaultFace(), setWindowDrawableConfigDel({}));
	{
		auto descs = renderer().supportedDrawableConfigs();
		for(auto desc: descs)
		{
			windowPixelFormatItem.emplace_back(desc.name, &defaultFace(), setWindowDrawableConfigDel(desc.config));
		}
		windowPixelFormat.setSelected(IG::findIndex(descs, app().windowDrawableConfig()) + 1);
	}
	iterateTimes(EmuSystem::aspectRatioInfos, i)
	{
		aspectRatioItem.emplace_back(EmuSystem::aspectRatioInfo[i].name, &defaultFace(),
			[this, i]()
			{
				app().setVideoAspectRatio((double)EmuSystem::aspectRatioInfo[i]);
			});
	}
	aspectRatioItem.emplace_back("自定义", &defaultFace(),
		[this](const Input::Event &e)
		{
			app().pushAndShowNewCollectValueInputView<std::pair<double, double>>(attachParams(), e,
				"输入整数或者分数", "",
				[this](EmuApp &app, auto val)
				{
					double ratio = val.first / val.second;
					if(app.setVideoAspectRatio(ratio))
					{
						if(auto idx = aspectRatioValueIndex(ratio);
							idx != -1)
						{
							aspectRatio.setSelected(idx, *this);
						}
						else
						{
							aspectRatio.setSelected(std::size(aspectRatioItem) - 1, *this);
						}
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
		});
	if(auto idx = aspectRatioValueIndex(app().videoAspectRatio());
		idx != -1)
	{
		aspectRatio.setSelected(idx, *this);
	}
	textureBufferModeItem.emplace_back("自动(设置最佳模式)", &defaultFace(),
		[this](View &view)
		{
			app().textureBufferModeOption() = 0;
			auto defaultMode = renderer().makeValidTextureBufferMode();
			emuVideo().setTextureBufferMode(system(), defaultMode);
			textureBufferMode.setSelected(IG::findIndex(renderer().textureBufferModes(), defaultMode) + 1);
			view.dismiss();
			return false;
		});
	{
		auto descs = renderer().textureBufferModes();
		for(auto desc: descs)
		{
			textureBufferModeItem.emplace_back(desc.name, &defaultFace(),
				[this, mode = desc.mode]()
				{
					app().textureBufferModeOption() = (uint8_t)mode;
					emuVideo().setTextureBufferMode(system(), mode);
				});
		}
		textureBufferMode.setSelected(IG::findIndex(descs, renderer().makeValidTextureBufferMode((Gfx::TextureBufferMode)app().textureBufferModeOption().val)) + 1);
	}
	if(!customMenu)
	{
		loadStockItems();
	}
}

void VideoOptionView::loadStockItems()
{
	if(used(frameInterval))
		item.emplace_back(&frameInterval);
	item.emplace_back(&dropLateFrames);
	if(!app().frameTimeIsConst(EmuSystem::VIDSYS_NATIVE_NTSC))
	{
		frameRate.setName(makeFrameRateStr(system()));
		item.emplace_back(&frameRate);
	}
	if(!app().frameTimeIsConst(EmuSystem::VIDSYS_PAL))
	{
		frameRatePAL.setName(makeFrameRatePALStr(system()));
		item.emplace_back(&frameRatePAL);
	}
	item.emplace_back(&visualsHeading);
	item.emplace_back(&imgFilter);
	item.emplace_back(&imgEffect);
	item.emplace_back(&overlayEffect);
	item.emplace_back(&overlayEffectLevel);
	item.emplace_back(&screenShapeHeading);
	item.emplace_back(&zoom);
	item.emplace_back(&viewportZoom);
	item.emplace_back(&aspectRatio);
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
	if(IG::used(presentationTime) && renderer().supportsPresentationTime())
		item.emplace_back(&presentationTime);
	#if defined CONFIG_BASE_MULTI_WINDOW && defined CONFIG_BASE_X11
	item.emplace_back(&secondDisplay);
	#endif
	#if defined CONFIG_BASE_MULTI_WINDOW && defined CONFIG_BASE_MULTI_SCREEN
	if(!app().showOnSecondScreenOption().isConst)
	{
		item.emplace_back(&showOnSecondScreen);
	}
	#endif
}

void VideoOptionView::setEmuVideoLayer(EmuVideoLayer &videoLayer_)
{
	videoLayer = &videoLayer_;
}

bool VideoOptionView::onFrameTimeChange(EmuSystem::VideoSystem vidSys, IG::FloatSeconds time)
{
	auto wantedTime = time;
	if(!time.count())
	{
		wantedTime = app().viewController().emuWindowScreen()->frameTime();
	}
	if(!system().setFrameTime(vidSys, wantedTime))
	{
		app().postMessage(4, true, fmt::format("{:.2f}Hz 不在有效范围内", 1. / wantedTime.count()));
		return false;
	}
	system().configFrameTime(app().soundRate());
	if(vidSys == EmuSystem::VIDSYS_NATIVE_NTSC)
	{
		app().setFrameTime(EmuSystem::VIDSYS_NATIVE_NTSC, time);
		frameRate.compile(makeFrameRateStr(system()), renderer(), projP);
	}
	else
	{
		app().setFrameTime(EmuSystem::VIDSYS_PAL, time);
		frameRatePAL.compile(makeFrameRatePALStr(system()), renderer(), projP);
	}
	return true;
}

void VideoOptionView::pushAndShowFrameRateSelectMenu(EmuSystem::VideoSystem vidSys, const Input::Event &e)
{
	const bool includeFrameRateDetection = !Config::envIsIOS;
	auto multiChoiceView = makeViewWithName<TextTableView>("刷新率", includeFrameRateDetection ? 4 : 3);
	multiChoiceView->appendItem("与屏幕刷新率保持一致",
		[this, vidSys](View &view)
		{
			if(!app().viewController().emuWindowScreen()->frameRateIsReliable())
			{
				#ifdef __ANDROID__
				if(appContext().androidSDK() <= 10)
				{
					app().postErrorMessage("许多 Android 2.3 设备错误报告其刷新率,"
						"使用检测到的或默认刷新率可能会更好");
				}
				else
				#endif
				{
					app().postErrorMessage("报告的刷新率可能不可靠,"
						"使用检测到的或默认刷新率可能会更好");
				}
			}
			if(onFrameTimeChange(vidSys, {}))
				view.dismiss();
		});
	multiChoiceView->appendItem("设置默认刷新率",
		[this, vidSys](View &view)
		{
			onFrameTimeChange(vidSys, EmuSystem::defaultFrameTime(vidSys));
			view.dismiss();
		});
	multiChoiceView->appendItem("设置自定义刷新率",
		[this, vidSys](const Input::Event &e)
		{
			app().pushAndShowNewCollectValueInputView<std::pair<double, double>>(attachParams(), e,
				"输入小数或分数", "",
				[this, vidSys](EmuApp &, auto val)
				{
					if(onFrameTimeChange(vidSys, IG::FloatSeconds{val.second / val.first}))
					{
						dismissPrevious();
						return true;
					}
					else
						return false;
				});
		});
	if(includeFrameRateDetection)
	{
		multiChoiceView->appendItem("检测屏幕刷新率并设置",
			[this, vidSys](const Input::Event &e)
			{
				window().setIntendedFrameRate(vidSys == EmuSystem::VIDSYS_NATIVE_NTSC ? 60. : 50.);
				auto frView = makeView<DetectFrameRateView>();
				frView->onDetectFrameTime =
					[this, vidSys](IG::FloatSeconds frameTime)
					{
						if(frameTime.count())
						{
							if(onFrameTimeChange(vidSys, frameTime))
								dismissPrevious();
						}
						else
						{
							app().postErrorMessage("检测到的刷新率太不稳定而无法使用");
						}
					};
				pushAndShowModal(std::move(frView), e);
			});
	}
	pushAndShow(std::move(multiChoiceView), e);
}

TextMenuItem::SelectDelegate VideoOptionView::setZoomDel()
{
	return [this](TextMenuItem &item) { app().setVideoZoom(item.id()); };
}

TextMenuItem::SelectDelegate VideoOptionView::setViewportZoomDel()
{
	return [this](TextMenuItem &item) { app().setViewportZoom(item.id()); };
}

TextMenuItem::SelectDelegate VideoOptionView::setOverlayEffectLevelDel()
{
	return [this](TextMenuItem &item) { app().setOverlayEffectLevel(*videoLayer, item.id()); };
}

EmuVideo &VideoOptionView::emuVideo() const
{
	return videoLayer->emuVideo();
}

}
