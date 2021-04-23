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

#define LOGTAG "EmuViewController"
#include <emuframework/EmuApp.hh>
#include <emuframework/EmuSystem.hh>
#include <emuframework/EmuView.hh>
#include <emuframework/EmuVideoLayer.hh>
#include <emuframework/EmuMainMenuView.hh>
#include <emuframework/FilePicker.hh>
#include <imagine/base/Base.hh>
#include <imagine/gfx/Renderer.hh>
#include <imagine/gfx/RendererTask.hh>
#include <imagine/gfx/RendererCommands.hh>
#include <imagine/gui/AlertView.hh>
#include <imagine/gui/ToastView.hh>
#include "EmuOptions.hh"
#include "private.hh"
#include "privateInput.hh"
#include "configFile.hh"
#include "EmuSystemTask.hh"
#include "EmuTiming.hh"

class AutoStateConfirmAlertView : public YesNoAlertView
{
public:
	AutoStateConfirmAlertView(ViewAttachParams attach, const char *dateStr, bool addToRecent):
		YesNoAlertView
		{
			attach,
			"",
			"继续游戏",
			"重新开始",
			[this, addToRecent]()
			{
				launchSystem(true, addToRecent);
			},
			[this, addToRecent]()
			{
				launchSystem(false, addToRecent);
			}
		}
	{
		setLabel(string_makePrintf<96>("自动存档存在:\n%s", dateStr).data());
	}
};

EmuViewController::EmuViewController(ViewAttachParams viewAttach,
	VController &vCtrl, EmuVideoLayer &videoLayer, EmuSystemTask &systemTask):
	emuView{viewAttach, &videoLayer},
	emuInputView{viewAttach, vCtrl, videoLayer},
	popup{viewAttach},
	rendererTask_{&viewAttach.rendererTask()},
	systemTask{&systemTask},
	onResume
	{
		[this](bool focused)
		{
			configureSecondaryScreens();
			prepareDraw();
			if(showingEmulation && focused && EmuSystem::isPaused())
			{
				logMsg("resuming emulation due to app resume");
				#ifdef CONFIG_EMUFRAMEWORK_VCONTROLS
				emuInputView.activeVController()->resetInput();
				#endif
				startEmulation();
			}
			return true;
		}, 10
	},
	onExit
	{
		[this](bool backgrounded)
		{
			if(backgrounded)
			{
				showUI();
				if(optionShowOnSecondScreen && Base::Screen::screens() > 1)
				{
					setEmuViewOnExtraWindow(false, *Base::Screen::screen(1));
				}
				viewStack.top().onHide();
			}
			else
			{
				closeSystem();
			}
			return true;
		}, -10
	}
{
	emuInputView.setController(this, Input::defaultEvent());
	if constexpr(Config::envIsAndroid)
	{
		emuInputView.setConsumeUnboundGamepadKeys(optionConsumeUnboundGamepadKeys);
	}
	auto &win = viewAttach.window();

	win.setOnInputEvent(
		[this](Base::Window &win, Input::Event e)
		{
			return inputEvent(e);
		});

	win.setOnFocusChange(
		[this](Base::Window &win, uint in)
		{
			windowData(win).focused = in;
			onFocusChange(in);
		});

	win.setOnDragDrop(
		[this](Base::Window &win, const char *filename)
		{
			logMsg("got DnD: %s", filename);
			handleOpenFileCommand(filename);
		});

	win.setOnSurfaceChange(
		[this](Base::Window &win, Base::Window::SurfaceChange change)
		{
			auto &winData = windowData(win);
			rendererTask().updateDrawableForSurfaceChange(win, change);
			if(change.resized())
			{
				updateWindowViewport(win, change);
				if(winData.hasEmuView)
				{
					emuView.setViewRect(winData.viewport().bounds(), winData.projection.plane());
				}
				emuInputView.setViewRect(winData.viewport().bounds(), winData.projection.plane());
				placeElements();
			}
		});

	win.setOnDraw(
		[this](Base::Window &win, Base::Window::DrawParams params)
		{
			auto &winData = windowData(win);
			rendererTask().draw(win, params, {}, winData.viewport(), winData.projection.matrix(),
				[this](Base::Window &win, Gfx::RendererCommands &cmds)
				{
					auto &winData = windowData(win);
					cmds.clear();
					drawMainWindow(win, cmds, winData.hasEmuView, winData.hasPopup);
				});
			return false;
		});

	initViews(viewAttach);
	configureSecondaryScreens();
}

static bool shouldExitFromViewRootWithoutPrompt(Input::Event e)
{
	return e.map() == Input::Map::SYSTEM && (Config::envIsAndroid || Config::envIsLinux);
}

EmuMenuViewStack::EmuMenuViewStack(EmuViewController &emuViewController):
	ViewStack{}, emuViewControllerPtr{&emuViewController}
{}

bool EmuMenuViewStack::inputEvent(Input::Event e)
{
	if(ViewStack::inputEvent(e))
	{
		return true;
	}
	if(e.pushed() && e.isDefaultCancelButton())
	{
		if(size() == 1)
		{
			//logMsg("cancel button at view stack root");
			if(e.repeated())
			{
				return true;
			}
			if(EmuSystem::gameIsRunning() ||
					(!EmuSystem::gameIsRunning() && !shouldExitFromViewRootWithoutPrompt(e)))
			{
				EmuApp::showExitAlert(top().attachParams(), e);
			}
			else
			{
				Base::exit();
			}
		}
		else
		{
			popAndShow();
		}
		return true;
	}
	if(e.pushed() && isMenuDismissKey(e, *emuViewControllerPtr) && !hasModalView())
	{
		if(EmuSystem::gameIsRunning())
		{
			emuViewControllerPtr->showEmulation();
		}
		return true;
	}
	return false;
}

void EmuViewController::initViews(ViewAttachParams viewAttach)
{
	auto &winData = windowData(viewAttach.window());
	winData.hasEmuView = true;
	winData.hasPopup = true;
	if(!Base::Screen::supportsTimestamps() && (!Config::envIsLinux || viewAttach.window().screen()->frameRate() < 100.))
	{
		setWindowFrameClockSource(Base::Window::FrameTimeSource::RENDERER);
	}
	else
	{
		setWindowFrameClockSource(Base::Window::FrameTimeSource::SCREEN);
	}
	logMsg("timestamp source:%s", useRendererTime() ? "renderer" : "screen");
	onFrameUpdate = [this, &r = std::as_const(viewAttach.renderer())](IG::FrameParams params)
		{
			bool skipForward = false;
			bool fastForwarding = false;
			if(unlikely(EmuSystem::shouldFastForward()))
			{
				// for skipping loading on disk-based computers
				fastForwarding = true;
				skipForward = true;
				EmuSystem::setSpeedMultiplier(8);
			}
			else if(unlikely(targetFastForwardSpeed > 1))
			{
				fastForwarding = true;
				EmuSystem::setSpeedMultiplier(targetFastForwardSpeed);
			}
			else
			{
				EmuSystem::setSpeedMultiplier(1);
			}
			auto frameInfo = EmuSystem::advanceFramesWithTime(params.timestamp());
			if(!frameInfo.advanced)
			{
				return true;
			}
			if(!optionSkipLateFrames && !fastForwarding)
			{
				frameInfo.advanced = currentFrameInterval();
			}
			constexpr uint maxFrameSkip = 8;
			uint32_t framesToEmulate = std::min(frameInfo.advanced, maxFrameSkip);
			EmuAudio *audioPtr = emuAudio ? &emuAudio : nullptr;
			systemTask->runFrame(&videoLayer().emuVideo(), audioPtr, framesToEmulate, skipForward);
			r.setPresentationTime(emuWindow(), params.presentTime());
			/*logMsg("frame present time:%.4f next display frame:%.4f",
				std::chrono::duration_cast<IG::FloatSeconds>(frameInfo.presentTime).count(),
				std::chrono::duration_cast<IG::FloatSeconds>(params.presentTime()).count());*/
			return false;
		};

	popup.setFace(View::defaultFace);
	{
		auto viewNav = std::make_unique<BasicNavView>
		(
			viewAttach,
			&View::defaultFace,
			&getAsset(emuView.renderer(), ASSET_ARROW),
			&getAsset(emuView.renderer(), ASSET_GAME_ICON)
		);
		viewNav->rotateLeftBtn = true;
		viewNav->setOnPushLeftBtn(
			[this](Input::Event)
			{
				viewStack.popAndShow();
			});
		viewNav->setOnPushRightBtn(
			[this](Input::Event)
			{
				if(EmuSystem::gameIsRunning())
				{
					showEmulation();
				}
			});
		viewNav->showRightBtn(false);
		viewStack.setShowNavViewBackButton(View::needsBackControl);
		EmuApp::onCustomizeNavView(*viewNav);
		viewStack.setNavView(std::move(viewNav));
	}
	viewStack.showNavView(optionTitleBar);
	emuView.setLayoutInputView(&inputView());
	placeElements();
	auto mainMenu = makeEmuView(viewAttach, EmuApp::ViewID::MAIN_MENU);
	static_cast<EmuMainMenuView*>(mainMenu.get())->setAudioVideo(emuAudio, videoLayer());
	pushAndShow(std::move(mainMenu), Input::defaultEvent());
	applyFrameRates();
	videoLayer().emuVideo().setOnFrameFinished(
		[this](EmuVideo &)
		{
			addOnFrame();
			emuWindow().drawNow();
		});
	videoLayer().emuVideo().setOnFormatChanged(
		[this, &videoLayer = videoLayer()](EmuVideo &)
		{
			#ifdef CONFIG_GFX_OPENGL_SHADER_PIPELINE
			videoLayer.setEffect(optionImgEffect, optionImageEffectPixelFormatValue());
			#else
			videoLayer.resetImage();
			#endif
			videoLayer.setOverlay(optionOverlayEffect);
			if((uint)optionImageZoom > 100)
			{
				placeEmuViews();
			}
		});
	setPhysicalControlsPresent(Input::keyInputIsPresent());
	#ifdef CONFIG_VCONTROLS_GAMEPAD
	if((int)optionTouchCtrl == 2)
		updateAutoOnScreenControlVisible();
	else
		setOnScreenControls(optionTouchCtrl);
	#endif
}

void EmuViewController::pushAndShow(std::unique_ptr<View> v, Input::Event e, bool needsNavView, bool isModal)
{
	showUI(false);
	viewStack.pushAndShow(std::move(v), e, needsNavView, isModal);
}

void EmuViewController::pop()
{
	viewStack.pop();
}

void EmuViewController::popTo(View &v)
{
	viewStack.popTo(v);
}

void EmuViewController::dismissView(View &v, bool refreshLayout)
{
	viewStack.dismissView(v, showingEmulation ? false : refreshLayout);
}

void EmuViewController::dismissView(int idx, bool refreshLayout)
{
	viewStack.dismissView(idx, showingEmulation ? false : refreshLayout);
}

bool EmuViewController::inputEvent(Input::Event e)
{
	if(e.isPointer())
	{
		//logMsg("Pointer %s @ %d,%d", e.actionToStr(e.state), e.x, e.y);
	}
	else
	{
		//logMsg("%s %s from %s", e.device->keyName(e.button), e.actionToStr(e.state), e.device->name());
	}
	if(showingEmulation)
	{
		return emuInputView.inputEvent(e);
	}
	return viewStack.inputEvent(e);
}

void EmuViewController::movePopupToWindow(Base::Window &win)
{
	auto &origWin = popup.window();
	if(origWin == win)
		return;
	auto &origWinData = windowData(origWin);
	origWinData.hasPopup = false;
	auto &winData = windowData(win);
	winData.hasPopup = true;
	popup.setWindow(&win);
}

void EmuViewController::moveEmuViewToWindow(Base::Window &win)
{
	auto &origWin = emuView.window();
	if(origWin == win)
		return;
	if(showingEmulation)
	{
		win.setDrawEventPriority(origWin.setDrawEventPriority());
	}
	auto &origWinData = windowData(origWin);
	origWinData.hasEmuView = false;
	auto &winData = windowData(win);
	winData.hasEmuView = true;
	emuView.setWindow(&win);
	emuView.setViewRect(winData.viewport().bounds(), winData.projection.plane());
}

void EmuViewController::configureAppForEmulation(bool running)
{
	Base::setIdleDisplayPowerSave(running ? (bool)optionIdleDisplayPowerSave : true);
	applyOSNavStyle(running);
	Input::setHintKeyRepeat(!running);
}

void EmuViewController::configureWindowForEmulation(Base::Window &win, bool running)
{
	#if defined CONFIG_BASE_SCREEN_FRAME_INTERVAL
	win.screen()->setFrameInterval(optionFrameInterval);
	#endif
	emuView.renderer().setWindowValidOrientations(win, running ? optionGameOrientation : optionMenuOrientation);
	win.setIntendedFrameRate(running ? EmuSystem::frameRate() : 0.);
	movePopupToWindow(running ? emuView.window() : emuInputView.window());
}

void EmuViewController::showEmulation()
{
	if(showingEmulation)
		return;
	viewStack.top().onHide();
	showingEmulation = true;
	configureAppForEmulation(true);
	configureWindowForEmulation(emuView.window(), true);
	if(emuView.window() != emuInputView.window())
		emuInputView.postDraw();
	commonInitInput(*this);
	popup.clear();
	emuInputView.resetInput();
	startEmulation();
	placeEmuViews();

	//回调一下C层
    Base::showEmulationCallbackAiWu(true);
}

void EmuViewController::showUI(bool updateTopView)
{
	if(!showingEmulation)
		return;
	showingEmulation = false;
	pauseEmulation();
	configureAppForEmulation(false);
	configureWindowForEmulation(emuView.window(), false);
	emuView.postDraw();
	if(updateTopView)
	{
		viewStack.show();
		viewStack.top().postDraw();
	}
    //回调一下C层
    Base::showEmulationCallbackAiWu(false);
}

bool EmuViewController::showAutoStateConfirm(Input::Event e, bool addToRecent)
{
	if(!(optionConfirmAutoLoadState && optionAutoSaveState))
	{
		return false;
	}
	auto saveStr = EmuSystem::sprintStateFilename(-1);
	if(FS::exists(saveStr))
	{
		auto mTime = FS::status(saveStr).lastWriteTimeLocal();
		char dateStr[64]{};
		std::strftime(dateStr, sizeof(dateStr), strftimeFormat, &mTime);
		pushAndShowModal(std::make_unique<AutoStateConfirmAlertView>(viewStack.top().attachParams(), dateStr, addToRecent), e, false);
		return true;
	}
	return false;
}

void EmuViewController::placeEmuViews()
{
	emuView.place();
	emuInputView.place();
}

void EmuViewController::placeElements()
{
	//logMsg("placing app elements");
	{
		auto &winData = windowData(popup.window());
		popup.setViewRect(winData.viewport().bounds(), winData.projection.plane());
		popup.place();
	}
	auto &winData = mainWindowData();
	TableView::setDefaultXIndent(inputView().window(), winData.projection.plane());
	placeEmuViews();
	viewStack.place(winData.viewport().bounds(), winData.projection.plane());
}

static bool hasExtraWindow()
{
	return Base::Window::windows() == 2;
}

static void dismissExtraWindow()
{
	if(!hasExtraWindow())
		return;
	Base::Window::window(1)->dismiss();
}

static bool extraWindowIsFocused()
{
	if(!hasExtraWindow())
		return false;
	return windowData(*Base::Window::window(1)).focused;
}

static Base::Screen *extraWindowScreen()
{
	if(!hasExtraWindow())
		return nullptr;
	return Base::Window::window(1)->screen();
}

void EmuViewController::setEmuViewOnExtraWindow(bool on, Base::Screen &screen)
{
	if(on && !hasExtraWindow())
	{
		logMsg("setting emu view on extra window");
		Base::WindowConfig winConf;
		winConf.setScreen(screen);
		winConf.setTitle(appName());
		auto extraWin = Base::Window::makeWindow(winConf,
			[this](Base::Window &win)
			{
				emuView.renderer().attachWindow(win);
				win.makeAppData<WindowData>();
				auto &mainWinData = mainWindowData();
				auto &extraWinData = windowData(win);
				extraWinData.focused = true;
				if(EmuSystem::isActive())
				{
					systemTask->pause();
					moveOnFrame(mainWindow(), win);
					applyFrameRates();
				}
				extraWinData.projection = updateProjection(makeViewport(win));
				moveEmuViewToWindow(win);
				emuView.setLayoutInputView(nullptr);

				win.setOnSurfaceChange(
					[this](Base::Window &win, Base::Window::SurfaceChange change)
					{
						auto &winData = windowData(win);
						rendererTask().updateDrawableForSurfaceChange(win, change);
						if(change.resized())
						{
							logMsg("view resize for extra window");
							winData.projection = updateProjection(makeViewport(win));
							emuView.setViewRect(winData.viewport().bounds(), winData.projection.plane());
							emuView.place();
						}
					});

				win.setOnDraw(
					[this](Base::Window &win, Base::Window::DrawParams params)
					{
						auto &winData = windowData(win);
						rendererTask().draw(win, params, {}, winData.viewport(), winData.projection.matrix(),
							[this, &winData](Base::Window &win, Gfx::RendererCommands &cmds)
							{
								cmds.clear();
								emuView.draw(cmds);
								if(winData.hasPopup)
								{
									popup.draw(cmds);
								}
								cmds.present();
							});
						return false;
					});

				win.setOnInputEvent(
					[this](Base::Window &win, Input::Event e)
					{
						if(likely(EmuSystem::isActive()) && e.isKey())
						{
							return emuInputView.inputEvent(e);
						}
						return false;
					});

				win.setOnFocusChange(
					[this](Base::Window &win, uint in)
					{
						windowData(win).focused = in;
						onFocusChange(in);
					});

				win.setOnDismissRequest(
					[](Base::Window &win)
					{
						win.dismiss();
					});

				win.setOnDismiss(
					[this](Base::Window &win)
					{
						EmuSystem::resetFrameTime();
						logMsg("setting emu view on main window");
						moveEmuViewToWindow(mainWindow());
						movePopupToWindow(mainWindow());
						emuView.setLayoutInputView(&inputView());
						placeEmuViews();
						mainWindow().postDraw();
						if(EmuSystem::isActive())
						{
							systemTask->pause();
							moveOnFrame(win, mainWindow());
							applyFrameRates();
						}
					});

				win.show();
				placeEmuViews();
				mainWindow().postDraw();
			});
		if(!extraWin)
		{
			logErr("error creating extra window");
			return;
		}
	}
	else if(!on && hasExtraWindow())
	{
		dismissExtraWindow();
	}
}

void EmuViewController::startViewportAnimation(Base::Window &win)
{
	auto &winData = windowData(win);
	auto oldViewport = winData.viewport();
	auto newViewport = makeViewport(win);
	winData.animatedViewport.start(win, oldViewport, newViewport);
	win.postDraw();
}

void EmuViewController::startMainViewportAnimation()
{
	startViewportAnimation(mainWindow());
}

void EmuViewController::updateWindowViewport(Base::Window &win, Base::Window::SurfaceChange change)
{
	auto &winData = windowData(win);
	if(change.surfaceResized())
	{
		winData.animatedViewport.finish();
		winData.projection = updateProjection(makeViewport(win));
	}
	else if(change.contentRectResized())
	{
		startViewportAnimation(win);
	}
	else if(change.customViewportResized())
	{
		winData.projection = updateProjection(winData.animatedViewport.viewport());
	}
}

void EmuViewController::updateEmuAudioStats(uint underruns, uint overruns, uint callbacks, double avgCallbackFrames, uint frames)
{
	emuView.updateAudioStats(underruns, overruns, callbacks, avgCallbackFrames, frames);
}

void EmuViewController::clearEmuAudioStats()
{
	emuView.clearAudioStats();
}

bool EmuViewController::allWindowsAreFocused() const
{
	return mainWindowData().focused && (!hasExtraWindow() || extraWindowIsFocused());
}

void EmuViewController::applyFrameRates()
{
	EmuSystem::setFrameTime(EmuSystem::VIDSYS_NATIVE_NTSC,
		optionFrameRate.val ? IG::FloatSeconds(optionFrameRate.val) : emuView.window().screen()->frameTime());
	EmuSystem::setFrameTime(EmuSystem::VIDSYS_PAL,
		optionFrameRatePAL.val ? IG::FloatSeconds(optionFrameRatePAL.val) : emuView.window().screen()->frameTime());
	EmuSystem::configFrameTime(optionSoundRate);
}

Base::OnFrameDelegate EmuViewController::makeOnFrameDelayed(uint8_t delay)
{
	return
		[this, delay](IG::FrameParams params)
		{
			if(delay)
			{
				addOnFrameDelegate(makeOnFrameDelayed(delay - 1));
			}
			else
			{
				if(EmuSystem::isActive())
				{
					emuWindow().setDrawEventPriority(1); // block UI from posting draws
					addOnFrame();
				}
			}
			return false;
		};
}

void EmuViewController::addOnFrameDelegate(Base::OnFrameDelegate onFrame)
{
	emuWindow().addOnFrame(onFrame, winFrameTimeSrc);
}

void EmuViewController::addOnFrameDelayed()
{
	// delay before adding onFrame handler to let timestamps stabilize
	auto delay = emuWindowScreen()->frameRate() / 4;
	//logMsg("delaying onFrame handler by %d frames", onFrameHandlerDelay);
	addOnFrameDelegate(makeOnFrameDelayed(delay));
}

void EmuViewController::addOnFrame()
{
	addOnFrameDelegate(onFrameUpdate);
}

void EmuViewController::removeOnFrame()
{
	emuWindow().removeOnFrame(onFrameUpdate, winFrameTimeSrc);
}

void EmuViewController::moveOnFrame(Base::Window &from, Base::Window &to)
{
	from.removeOnFrame(onFrameUpdate, winFrameTimeSrc);
	to.addOnFrame(onFrameUpdate, winFrameTimeSrc);
}

void EmuViewController::startEmulation()
{
	setCPUNeedsLowLatency(true);
	systemTask->start();
	EmuSystem::start();
	videoLayer().setBrightness(1.f);
	addOnFrameDelayed();
}

void EmuViewController::pauseEmulation()
{
	setCPUNeedsLowLatency(false);
	systemTask->pause();
	EmuSystem::pause();
	videoLayer().setBrightness(showingEmulation ? .75f : .25f);
	setFastForwardActive(false);
	emuWindow().setDrawEventPriority();
	removeOnFrame();
}

void EmuViewController::closeSystem(bool allowAutosaveState)
{
	showUI();
	systemTask->stop();
	EmuSystem::closeRuntimeSystem(allowAutosaveState);
	viewStack.navView()->showRightBtn(false);
	if(int idx = viewStack.viewIdx("System Actions");
		idx > 0)
	{
		// pop to menu below System Actions
		viewStack.popTo(idx - 1);
	}
}

void EmuViewController::popToSystemActionsMenu()
{
	viewStack.popTo(viewStack.viewIdx("System Actions"));
}

void EmuViewController::postDrawToEmuWindows()
{
	emuView.window().postDraw();
}

Base::Screen *EmuViewController::emuWindowScreen() const
{
	return emuView.window().screen();
}

Base::Window &EmuViewController::emuWindow() const
{
	return emuView.window();
}

WindowData &EmuViewController::emuWindowData()
{
	return windowData(emuView.window());
}

Gfx::RendererTask &EmuViewController::rendererTask() const
{
	return *rendererTask_;
}

void EmuViewController::pushAndShowModal(std::unique_ptr<View> v, Input::Event e, bool needsNavView)
{
	pushAndShow(std::move(v), e, needsNavView, true);
}

bool EmuViewController::hasModalView() const
{
	return viewStack.hasModalView();
}

void EmuViewController::popModalViews()
{
	viewStack.popModalViews();
}

void EmuViewController::prepareDraw()
{
	popup.prepareDraw();
	emuView.prepareDraw();
	viewStack.prepareDraw();
}

void EmuViewController::drawMainWindow(Base::Window &win, Gfx::RendererCommands &cmds, bool hasEmuView, bool hasPopup)
{
	if(showingEmulation)
	{
		if(hasEmuView)
		{
			emuView.draw(cmds);
		}
		emuInputView.draw(cmds);
		if(hasPopup)
			popup.draw(cmds);
	}
	else
	{
		if(hasEmuView)
		{
			emuView.draw(cmds);
		}
		viewStack.draw(cmds);
		popup.draw(cmds);
	}
	cmds.present();
}

void EmuViewController::popToRoot()
{
	viewStack.popToRoot();
}

void EmuViewController::showNavView(bool show)
{
	viewStack.showNavView(show);
}

void EmuViewController::setShowNavViewBackButton(bool show)
{
	viewStack.setShowNavViewBackButton(show);
}

void EmuViewController::showSystemActionsView(ViewAttachParams attach, Input::Event e)
{
	showUI();
	if(!viewStack.contains("System Actions"))
	{
		viewStack.pushAndShow(makeEmuView(attach, EmuApp::ViewID::SYSTEM_ACTIONS), e);
	}
}

void EmuViewController::onInputDevicesChanged()
{
	#ifdef CONFIG_BLUETOOTH
	if(viewStack.size() == 1) // update bluetooth items
		viewStack.top().onShow();
	#endif
}

void EmuViewController::onSystemCreated()
{
	viewStack.navView()->showRightBtn(true);
}

EmuInputView &EmuViewController::inputView()
{
	return emuInputView;
}

ToastView &EmuViewController::popupMessageView()
{
	return popup;
}

EmuVideoLayer &EmuViewController::videoLayer() const
{
	return *emuView.videoLayer();
}

void EmuViewController::onScreenChange(Base::Screen &screen, Base::Screen::Change change)
{
	if(change.added())
	{
		logMsg("screen added");
		if(optionShowOnSecondScreen && screen.screens() > 1)
			setEmuViewOnExtraWindow(true, screen);
	}
	else if(change.removed())
	{
		logMsg("screen removed");
		if(hasExtraWindow() && *extraWindowScreen() == screen)
			setEmuViewOnExtraWindow(false, screen);
	}
}

void EmuViewController::handleOpenFileCommand(const char *path)
{
	auto type = FS::status(path).type();
	if(type == FS::file_type::directory)
	{
		logMsg("changing to dir %s from external command", path);
		showUI(false);
		popToRoot();
		EmuApp::setMediaSearchPath(FS::makePathString(path));
		pushAndShow(EmuFilePicker::makeForLoading(viewStack.top().attachParams(), Input::defaultEvent()), Input::defaultEvent(), false);
		return;
	}
	if(type != FS::file_type::regular || (!EmuApp::hasArchiveExtension(path) && !EmuSystem::defaultFsFilter(path)))
	{
		logMsg("unrecognized file type");
		return;
	}
	logMsg("opening file %s from external command", path);
	showUI();
	popToRoot();
	onSelectFileFromPicker(path, Input::Event{}, {}, viewStack.top().attachParams());
}

void EmuViewController::onFocusChange(uint in)
{
	if(showingEmulation)
	{
		if(in && EmuSystem::isPaused())
		{
			logMsg("resuming emulation due to window focus");
			#ifdef CONFIG_EMUFRAMEWORK_VCONTROLS
			emuInputView.activeVController()->resetInput();
			#endif
			startEmulation();
		}
		else if(optionPauseUnfocused && !EmuSystem::isPaused() && !allWindowsAreFocused())
		{
			logMsg("pausing emulation with all windows unfocused");
			pauseEmulation();
			postDrawToEmuWindows();
		}
	}
}

void EmuViewController::setOnScreenControls(bool on)
{
	emuInputView.setTouchControlsOn(on);
	placeEmuViews();
}

void EmuViewController::updateAutoOnScreenControlVisible()
{
	#ifdef CONFIG_VCONTROLS_GAMEPAD
	if((uint)optionTouchCtrl == 2)
	{
		if(emuInputView.touchControlsAreOn() && physicalControlsPresent)
		{
			logMsg("auto-turning off on-screen controls");
			setOnScreenControls(0);
		}
		else if(!emuInputView.touchControlsAreOn() && !physicalControlsPresent)
		{
			logMsg("auto-turning on on-screen controls");
			setOnScreenControls(1);
		}
	}
	#endif
}

void EmuViewController::setPhysicalControlsPresent(bool present)
{
	if(present != physicalControlsPresent)
	{
		logMsg("Physical controls present:%s", present ? "y" : "n");
	}
	physicalControlsPresent = present;
}

WindowData &EmuViewController::mainWindowData() const
{
	return windowData(emuInputView.window());
}

Base::Window &EmuViewController::mainWindow() const
{
	return emuInputView.window();
}

void EmuViewController::setFastForwardActive(bool active)
{
	targetFastForwardSpeed = active ? optionFastForwardSpeed.val : 0;
	emuAudio.setAddSoundBuffersOnUnderrun(active ? optionAddSoundBuffersOnUnderrun.val : false);
	auto soundVolume = (active && !soundDuringFastForwardIsEnabled()) ? 0 : optionSoundVolume.val;
	emuAudio.setVolume(soundVolume);
}

void EmuViewController::setWindowFrameClockSource(Base::Window::FrameTimeSource src)
{
	winFrameTimeSrc = src;
}

bool EmuViewController::useRendererTime() const
{
	return winFrameTimeSrc == Base::Window::FrameTimeSource::RENDERER;
}

void EmuViewController::configureSecondaryScreens()
{
	if(optionShowOnSecondScreen && Base::Screen::screens() > 1)
	{
		setEmuViewOnExtraWindow(true, *Base::Screen::screen(1));
	}
}
