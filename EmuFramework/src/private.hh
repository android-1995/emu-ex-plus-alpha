#pragma once

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

#include <imagine/base/Window.hh>
#include <imagine/base/Screen.hh>
#include <imagine/base/Base.hh>
#include <imagine/input/Input.hh>
#include <imagine/gui/ViewStack.hh>
#include <imagine/gui/ToastView.hh>
#include <imagine/gfx/AnimatedViewport.hh>
#include <imagine/gfx/DrawableHolder.hh>
#include <emuframework/EmuInputView.hh>
#include <emuframework/EmuApp.hh>
#include <emuframework/EmuSystem.hh>
#include <emuframework/EmuView.hh>
#include <emuframework/EmuAudio.hh>
#include <emuframework/EmuVideo.hh>
#include "Recent.hh"
#include <memory>

enum AssetID { ASSET_ARROW, ASSET_CLOSE, ASSET_ACCEPT, ASSET_GAME_ICON, ASSET_MENU, ASSET_FAST_FORWARD };

class EmuSystemTask;
class EmuViewController;

struct WindowData
{
	Gfx::Viewport viewport() const { return projection.plane().viewport(); }
	Gfx::Projection projection{};
	Gfx::AnimatedViewport animatedViewport{};
	bool hasEmuView = false;
	bool hasPopup = false;
	bool focused = true;
};

class EmuMenuViewStack : public ViewStack
{
public:
	EmuMenuViewStack(EmuViewController &);
	bool inputEvent(Input::Event e) final;

protected:
	EmuViewController *emuViewControllerPtr;
};

class EmuViewController final: public ViewController
{
public:
	EmuViewController() {}
	EmuViewController(ViewAttachParams,
		VController &vCtrl, EmuVideoLayer &videoLayer, EmuSystemTask &systemTask);
	void pushAndShow(std::unique_ptr<View> v, Input::Event e, bool needsNavView, bool isModal = false) final;
	using ViewController::pushAndShow;
	void pushAndShowModal(std::unique_ptr<View> v, Input::Event e, bool needsNavView);
	void pop() final;
	void popTo(View &v) final;
	void dismissView(View &v, bool refreshLayout) final;
	void dismissView(int idx, bool refreshLayout) final;
	bool inputEvent(Input::Event e) final;
	void showEmulation();
	void showUI(bool updateTopView = true);
	bool showAutoStateConfirm(Input::Event e, bool addToRecent);
	void placeEmuViews();
	void placeElements();
	void setEmuViewOnExtraWindow(bool on, Base::Screen &screen);
	void startMainViewportAnimation();
	void updateEmuAudioStats(uint underruns, uint overruns, uint callbacks, double avgCallbackFrames, uint frames);
	void clearEmuAudioStats();
	void closeSystem(bool allowAutosaveState = true);
	void popToSystemActionsMenu();
	void postDrawToEmuWindows();
	Base::Screen *emuWindowScreen() const;
	Base::Window &emuWindow() const;
	WindowData &emuWindowData();
	Gfx::RendererTask &rendererTask() const;
	bool hasModalView() const;
	void popModalViews();
	void prepareDraw();
	void popToRoot();
	void showNavView(bool show);
	void setShowNavViewBackButton(bool show);
	void showSystemActionsView(ViewAttachParams attach, Input::Event e);
    void showSystemActionsViewAiWu();
	void onInputDevicesChanged();
	void onSystemCreated();
	EmuInputView &inputView();
	ToastView &popupMessageView();
	EmuVideoLayer &videoLayer() const;
	void onScreenChange(Base::Screen &screen, Base::Screen::Change change);
	void handleOpenFileCommand(const char *path);
	void setOnScreenControls(bool on);
	void updateAutoOnScreenControlVisible();
	void setPhysicalControlsPresent(bool present);
	void setFastForwardActive(bool active);

    void startEmulation();
    void pauseEmulation();
    void onPauseAiWu();

protected:
	static constexpr bool HAS_USE_RENDER_TIME = Config::envIsLinux
		|| (Config::envIsAndroid && Config::ENV_ANDROID_MINSDK < 16);
	EmuView emuView{};
	EmuInputView emuInputView{};
	ToastView popup{};
	EmuMenuViewStack viewStack{*this};
	Base::OnFrameDelegate onFrameUpdate{};
	Gfx::RendererTask *rendererTask_{};
	EmuSystemTask *systemTask{};
	Base::OnResume onResume{};
	Base::OnExit onExit{};
	bool showingEmulation = false;
	bool physicalControlsPresent = false;
	Base::Window::FrameTimeSource winFrameTimeSrc{};
	uint8_t targetFastForwardSpeed = 0;

	void initViews(ViewAttachParams attach);
	void onFocusChange(uint in);
	Base::OnFrameDelegate makeOnFrameDelayed(uint8_t delay);
	void addOnFrameDelegate(Base::OnFrameDelegate onFrame);
	void addOnFrameDelayed();
	void addOnFrame();
	void removeOnFrame();
	void moveOnFrame(Base::Window &from, Base::Window &to);
	void configureAppForEmulation(bool running);
	void configureWindowForEmulation(Base::Window &win, bool running);
	void startViewportAnimation(Base::Window &win);
	void updateWindowViewport(Base::Window &win, Base::Window::SurfaceChange change);
	void drawMainWindow(Base::Window &win, Gfx::RendererCommands &cmds, bool hasEmuView, bool hasPopup);
	void movePopupToWindow(Base::Window &win);
	void moveEmuViewToWindow(Base::Window &win);
	void applyFrameRates();
	bool allWindowsAreFocused() const;
	WindowData &mainWindowData() const;
	Base::Window &mainWindow() const;
	void setWindowFrameClockSource(Base::Window::FrameTimeSource);
	bool useRendererTime() const;
	void configureSecondaryScreens();
};

extern DelegateFunc<void ()> onUpdateInputDevices;
extern FS::PathString lastLoadPath;
extern EmuVideo emuVideo;
extern EmuAudio emuAudio;
extern RecentGameList recentGameList;
static constexpr const char *strftimeFormat = "%x  %r";

EmuViewController &emuViewController();
void loadConfigFile();
void saveConfigFile();
void addRecentGame(const char *fullPath, const char *name);
bool isMenuDismissKey(Input::Event e, EmuViewController &emuViewController);
void applyOSNavStyle(bool inGame);
const char *appViewTitle();
const char *appName();
const char *appID();
bool hasGooglePlayStoreFeatures();
void setCPUNeedsLowLatency(bool needed);
void onMainMenuItemOptionChanged();
void runBenchmarkOneShot();
void onSelectFileFromPicker(const char* name, Input::Event e, EmuSystemCreateParams params, ViewAttachParams attachParams);
void launchSystem(bool tryAutoState, bool addToRecent);
Gfx::PixmapTexture &getAsset(Gfx::Renderer &r, AssetID assetID);
std::unique_ptr<View> makeEmuView(ViewAttachParams attach, EmuApp::ViewID id);
Gfx::Viewport makeViewport(const Base::Window &win);
Gfx::Projection updateProjection(Gfx::Viewport viewport);
WindowData &windowData(const Base::Window &win);
uint8_t currentFrameInterval();
IG::PixelFormatID optionImageEffectPixelFormatValue();

static void addRecentGame()
{
	addRecentGame(EmuSystem::fullGamePath(), EmuSystem::fullGameName().data());
}
