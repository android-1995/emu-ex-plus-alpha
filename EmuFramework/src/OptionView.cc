#include <OptionView.hh>
#include <MsgPopup.hh>

static ButtonConfigCategoryView bcatMenu;
extern MsgPopup popup;

void soundHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	optionSound = item.on;
}

#ifdef CONFIG_AUDIO_OPENSL_ES
static void soundUnderrunCheckHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	optionSoundUnderrunCheck = item.on;
}
#endif

#if defined(CONFIG_INPUT_ANDROID) && CONFIG_ENV_ANDROID_MINSDK >= 9
static void useOSInputMethodHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	Input::setEventsUseOSInputMethod(!item.on);
}
#endif

#ifdef CONFIG_ENV_WEBOS
static void touchCtrlHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	optionTouchCtrl = item.on;
}

void OptionView::touchCtrlInit()
{
	touchCtrl.init("On-screen Controls", int(optionTouchCtrl));
	touchCtrl.selectDelegate().bind<&touchCtrlHandler>();
}
#else
void touchCtrlSet(MultiChoiceMenuItem &, int val)
{
	optionTouchCtrl = val;
}

void OptionView::touchCtrlInit()
{
	static const char *str[] =
	{
		"Off", "On", "Auto"
	};
	touchCtrl.init("On-screen Controls", str, int(optionTouchCtrl), sizeofArray(str));
	touchCtrl.valueDelegate().bind<&touchCtrlSet>();
}
#endif

void touchCtrlConfigHandler(TextMenuItem &, const InputEvent &e)
{
	#ifndef CONFIG_BASE_PS3
		logMsg("init touch config menu");
		tcMenu.init(!e.isPointer());
		viewStack.pushAndShow(&tcMenu);
	#endif
}

void autoSaveStateSet(MultiChoiceMenuItem &, int val)
{
	switch(val)
	{
		bcase 0: optionAutoSaveState.val = 0;
		bcase 1: optionAutoSaveState.val = 1;
		bcase 2: optionAutoSaveState.val = 15;
		bcase 3: optionAutoSaveState.val = 30;
	}
	logMsg("set auto-savestate %d", optionAutoSaveState.val);
}

void OptionView::autoSaveStateInit()
{
	static const char *str[] =
	{
		"Off", "On Exit",
		"15mins", "30mins"
	};
	int val = 0;
	switch(optionAutoSaveState.val)
	{
		bcase 1: val = 1;
		bcase 15: val = 2;
		bcase 30: val = 3;
	}
	autoSaveState.init("Auto-save State", str, val, sizeofArray(str));
	autoSaveState.valueDelegate().bind<&autoSaveStateSet>();
}

void statusBarSet(MultiChoiceMenuItem &, int val)
{
	optionHideStatusBar = val;
	setupStatusBarInMenu();
}

void OptionView::statusBarInit()
{
	static const char *str[] =
	{
		"Off", "In Game", "On",
	};
	int val = 2;
	if(optionHideStatusBar < 2)
		val = optionHideStatusBar;
	statusBar.init("Hide Status Bar", str, val, sizeofArray(str));
	statusBar.valueDelegate().bind<&statusBarSet>();
}

void frameSkipSet(MultiChoiceMenuItem &, int val)
{
	if(val == -1)
	{
		optionFrameSkip.val = EmuSystem::optionFrameSkipAuto;
		logMsg("set auto frame skip");
	}
	else
	{
		optionFrameSkip.val = val;
		logMsg("set frame skip: %d", int(optionFrameSkip));
	}
	EmuSystem::configAudioRate();
}

void OptionView::frameSkipInit()
{
	static const char *str[] =
	{
		"Auto", "0",
		#if defined(CONFIG_BASE_IOS) || defined(CONFIG_BASE_X11)
		"1", "2", "3", "4"
		#endif
	};
	int baseVal = -1;
	int val = int(optionFrameSkip);
	if(optionFrameSkip.val == EmuSystem::optionFrameSkipAuto)
		val = -1;
	frameSkip.init("Frame Skip", str, val, sizeofArray(str), baseVal);
	frameSkip.valueDelegate().bind<&frameSkipSet>();
}

void audioRateSet(MultiChoiceMenuItem &, int val)
{
	uint rate = 44100;
	switch(val)
	{
		bcase 0: rate = 22050;
		bcase 1: rate = 32000;
		bcase 3: rate = 48000;
	}
	if(rate != optionSoundRate)
	{
		optionSoundRate = rate;
		EmuSystem::configAudioRate();
	}
}

void OptionView::audioRateInit()
{
	static const char *str[] =
	{
		"22KHz", "32KHz", "44KHz", "48KHz"
	};
	int rates = 3;
	if(Audio::supportsRateNative(48000))
	{
		logMsg("supports 48KHz");
		rates++;
	}

	int val = 2; // default to 44KHz
	switch(optionSoundRate)
	{
		bcase 22050: val = 0;
		bcase 32000: val = 1;
		bcase 48000: val = 3;
	}

	audioRate.init("Sound Rate", str, val, rates);
	audioRate.valueDelegate().bind<&audioRateSet>();
}

#ifdef SUPPORT_ANDROID_DIRECT_TEXTURE
static void directTextureHandler(BoolMenuItem &item, const InputEvent &e)
{
	if(!item.active)
	{
		popup.postError(Gfx::androidDirectTextureError());
		return;
	}
	item.toggle();
	Gfx::setUseAndroidDirectTexture(item.on);
	optionDirectTexture.val = item.on;
	if(emuView.vidImg.impl)
		emuView.reinitImage();
}

static void glSyncHackHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	glSyncHackEnabled = item.on;
}
#endif

void confirmAutoLoadStateHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	optionConfirmAutoLoadState = item.on;
}

void pauseUnfocusedHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	optionPauseUnfocused = item.on;
}

void largeFontsHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	optionLargeFonts = item.on;
	setupFont();
	Gfx::onViewChange(nullptr);
}

void notificationIconHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	optionNotificationIcon = item.on;
}

void lowProfileOSNavHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	optionLowProfileOSNav = item.on;
	applyOSNavStyle();
}

void hideOSNavHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	optionHideOSNav = item.on;
	applyOSNavStyle();
}

void idleDisplayPowerSaveHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	optionIdleDisplayPowerSave = item.on;
	Base::setIdleDisplayPowerSave(item.on);
}

void altGamepadConfirmHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	input_swappedGamepadConfirm = item.on;
}

void ditherHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	Gfx::setDither(item.on);
}

void navViewHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	optionTitleBar = item.on;
	viewStack.setNavView(item.on ? &viewNav : 0);
	Gfx::onViewChange(nullptr);
}

void backNavHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	View::setNeedsBackControl(item.on);
	viewNav.setBackImage(View::needsBackControl ? getArrowAsset() : nullptr);
	Gfx::onViewChange(nullptr);
}

void rememberLastMenuHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	optionRememberLastMenu = item.on;
}

void buttonConfigHandler(TextMenuItem &, const InputEvent &e)
{
	#if defined(CONFIG_BASE_PS3)
		bcatMenu.init(InputEvent::DEV_PS3PAD, !e.isPointer());
	#elif defined(INPUT_SUPPORTS_KEYBOARD)
		bcatMenu.init(InputEvent::DEV_KEYBOARD, !e.isPointer());
	#else
		bug_exit("invalid dev type in initButtonConfigMenu");
	#endif
	viewStack.pushAndShow(&bcatMenu);
}

#ifdef CONFIG_INPUT_ICADE
void iCadeHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	Input::setICadeActive(item.on);
}

void iCadeButtonConfigHandler(TextMenuItem &, const InputEvent &e)
{
	bcatMenu.init(InputEvent::DEV_ICADE, !e.isPointer());
	viewStack.pushAndShow(&bcatMenu);
}
#endif

#ifdef CONFIG_BLUETOOTH

void wiiButtonConfigHandler(TextMenuItem &, const InputEvent &e)
{
	bcatMenu.init(InputEvent::DEV_WIIMOTE, !e.isPointer());
	viewStack.pushAndShow(&bcatMenu);
}

void iCPButtonConfigHandler(TextMenuItem &, const InputEvent &e)
{
	bcatMenu.init(InputEvent::DEV_ICONTROLPAD, !e.isPointer());
	viewStack.pushAndShow(&bcatMenu);
}

void zeemoteButtonConfigHandler(TextMenuItem &, const InputEvent &e)
{
	bcatMenu.init(InputEvent::DEV_ZEEMOTE, !e.isPointer());
	viewStack.pushAndShow(&bcatMenu);
}

void btScanSecsSet(MultiChoiceMenuItem &, int val)
{
	switch(val)
	{
		bcase 0: Bluetooth::scanSecs = 2;
		bcase 1: Bluetooth::scanSecs = 4;
		bcase 2: Bluetooth::scanSecs = 6;
		bcase 3: Bluetooth::scanSecs = 8;
		bcase 4: Bluetooth::scanSecs = 10;
	}
	logMsg("set bluetooth scan time %d", Bluetooth::scanSecs);
}

void OptionView::btScanSecsInit()
{
	static const char *str[] =
	{
		"2secs", "4secs", "6secs", "8secs", "10secs"
	};

	int val = 1;
	switch(Bluetooth::scanSecs)
	{
		bcase 2: val = 0;
		bcase 4: val = 1;
		bcase 6: val = 2;
		bcase 8: val = 3;
		bcase 10: val = 4;
	}
	btScanSecs.init("Bluetooth Scan", str, val, sizeofArray(str));
	btScanSecs.valueDelegate().bind<&btScanSecsSet>();
}

void keepBtActiveHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	optionKeepBluetoothActive = item.on;
}

void btScanCacheHandler(BoolMenuItem &item, const InputEvent &e)
{
	item.toggle();
	optionBlueToothScanCache = item.on;
}
#endif

enum { O_AUTO = -1, O_90, O_270, O_0 };

int convertOrientationMenuValueToOption(int val)
{
	if(val == O_AUTO)
		return Gfx::VIEW_ROTATE_AUTO;
	else if(val == O_90)
		return Gfx::VIEW_ROTATE_90;
	else if(val == O_270)
		return Gfx::VIEW_ROTATE_270;
	else if(val == O_0)
		return Gfx::VIEW_ROTATE_0;
	assert(0);
	return 0;
}

static void orientationInit(MultiChoiceSelectMenuItem &item, const char *name, uint option)
{
	static const char *str[] =
	{
		#if defined(CONFIG_BASE_IOS) || defined(CONFIG_BASE_ANDROID) || CONFIG_ENV_WEBOS_OS >= 3
		"Auto",
		#endif

		#if defined(CONFIG_BASE_IOS) || defined(CONFIG_BASE_ANDROID) || (defined CONFIG_ENV_WEBOS && CONFIG_ENV_WEBOS_OS <= 2)
		"Landscape", "Landscape 2", "Portrait"
		#else
		"90 Left", "90 Right", "Standard"
		#endif
	};
	int baseVal = 0;
	#if defined(CONFIG_BASE_IOS) || defined(CONFIG_BASE_ANDROID) || CONFIG_ENV_WEBOS_OS >= 3
		baseVal = -1;
	#endif
	uint initVal = O_AUTO;
	if(option == Gfx::VIEW_ROTATE_90)
		initVal = O_90;
	else if(option == Gfx::VIEW_ROTATE_270)
		initVal = O_270;
	else if(option == Gfx::VIEW_ROTATE_0)
		initVal = O_0;
	item.init(name, str, initVal, sizeofArray(str), baseVal);
}

void gameOrientationSet(MultiChoiceMenuItem &, int val)
{
	optionGameOrientation.val = convertOrientationMenuValueToOption(val);
	logMsg("set game orientation: %s", Gfx::orientationName(int(optionGameOrientation)));
}

void OptionView::gameOrientationInit()
{
	orientationInit(gameOrientation, "Orientation", optionGameOrientation);
	gameOrientation.valueDelegate().bind<&gameOrientationSet>();
}

void menuOrientationSet(MultiChoiceMenuItem &, int val)
{
	optionMenuOrientation.val = convertOrientationMenuValueToOption(val);
	if(!Gfx::setValidOrientations(optionMenuOrientation, 1))
		Gfx::onViewChange(nullptr);
	logMsg("set menu orientation: %s", Gfx::orientationName(int(optionMenuOrientation)));
}

void OptionView::menuOrientationInit()
{
	orientationInit(menuOrientation, "Orientation", optionMenuOrientation);
	menuOrientation.valueDelegate().bind<&menuOrientationSet>();
}

void aspectRatioSet(MultiChoiceMenuItem &, int val)
{
	optionAspectRatio.val = val;
	logMsg("set aspect ratio: %d", int(optionAspectRatio));
	emuView.placeEmu();
}

void OptionView::aspectRatioInit()
{
	static const char *str[] = { systemAspectRatioString, "1:1", "Full Screen" };
	aspectRatio.init("Aspect Ratio", str, optionAspectRatio, sizeofArray(str));
	aspectRatio.valueDelegate().bind<&aspectRatioSet>();
}

#ifdef CONFIG_AUDIO_CAN_USE_MAX_BUFFERS_HINT
	void soundBuffersSet(MultiChoiceMenuItem &, int val)
	{
		optionSoundBuffers = val+4;
	}

	void OptionView::soundBuffersInit()
	{
		static const char *str[] = { "4", "5", "6", "7", "8", "9", "10", "11", "12" };
		soundBuffers.init("Buffer Size In Frames", str, IG::max((int)optionSoundBuffers - 4, 0), sizeofArray(str));
		soundBuffers.valueDelegate().bind<&soundBuffersSet>();
	}
#endif

void zoomSet(MultiChoiceMenuItem &, int val)
{
	switch(val)
	{
		bcase 0: optionImageZoom.val = 100;
		bcase 1: optionImageZoom.val = 90;
		bcase 2: optionImageZoom.val = 80;
		bcase 3: optionImageZoom.val = 70;
		bcase 4: optionImageZoom.val = optionImageZoomIntegerOnly;
	}
	logMsg("set image zoom: %d", int(optionImageZoom));
	emuView.placeEmu();
}

void OptionView::zoomInit()
{
	static const char *str[] = { "100%", "90%", "80%", "70%", "Integer-only" };
	int val = 0;
	switch(optionImageZoom.val)
	{
		bcase 100: val = 0;
		bcase 90: val = 1;
		bcase 80: val = 2;
		bcase 70: val = 3;
		bcase optionImageZoomIntegerOnly: val = 4;
	}
	zoom.init("Zoom", str, val, sizeofArray(str));
	zoom.valueDelegate().bind<&zoomSet>();
}

void dpiSet(MultiChoiceMenuItem &, int val)
{
	switch(val)
	{
		bdefault: optionDPI.val = 0;
		bcase 1: optionDPI.val = 96;
		bcase 2: optionDPI.val = 120;
		bcase 3: optionDPI.val = 130;
		bcase 4: optionDPI.val = 160;
		bcase 5: optionDPI.val = 220;
		bcase 6: optionDPI.val = 240;
		bcase 7: optionDPI.val = 265;
		bcase 8: optionDPI.val = 320;
	}
	Base::setDPI(optionDPI);
	logMsg("set DPI: %d", (int)optionDPI);
	setupFont();
	Gfx::onViewChange(nullptr);
}

void OptionView::dpiInit()
{
	static const char *str[] = { "Auto", "96", "120", "130", "160", "220", "240", "265", "320" };
	uint init = 0;
	switch(optionDPI)
	{
		bcase 96: init = 1;
		bcase 120: init = 2;
		bcase 130: init = 3;
		bcase 160: init = 4;
		bcase 220: init = 5;
		bcase 240: init = 6;
		bcase 265: init = 7;
		bcase 320: init = 8;
	}
	assert(init < sizeofArray(str));
	dpi.init("DPI Override", str, init, sizeofArray(str));
	dpi.valueDelegate().bind<&dpiSet>();
}

void imgFilterSet(MultiChoiceMenuItem &, int val)
{
	optionImgFilter.val = val;
	if(emuView.disp.img)
		emuView.vidImg.setFilter(val);
}

void OptionView::imgFilterInit()
{
	static const char *str[] = { "None", "Linear" };
	imgFilter.init("Image Filter", str, optionImgFilter, sizeofArray(str));
	imgFilter.valueDelegate().bind<&imgFilterSet>();
}

void overlayEffectSet(MultiChoiceMenuItem &, int val)
{
	uint setVal = 0;
	switch(val)
	{
		bcase 1: setVal = VideoImageOverlay::SCANLINES;
		bcase 2: setVal = VideoImageOverlay::SCANLINES_2;
		bcase 3: setVal = VideoImageOverlay::CRT;
		bcase 4: setVal = VideoImageOverlay::CRT_RGB;
		bcase 5: setVal = VideoImageOverlay::CRT_RGB_2;
	}
	optionOverlayEffect.val = setVal;
	emuView.vidImgOverlay.setEffect(setVal);
	emuView.placeOverlay();
}

void OptionView::overlayEffectInit()
{
	static const char *str[] = { "Off", "Scanlines", "Scanlines 2x", "CRT Mask", "CRT", "CRT 2x" };
	uint init = 0;
	switch(optionOverlayEffect)
	{
		bcase VideoImageOverlay::SCANLINES: init = 1;
		bcase VideoImageOverlay::SCANLINES_2: init = 2;
		bcase VideoImageOverlay::CRT: init = 3;
		bcase VideoImageOverlay::CRT_RGB: init = 4;
		bcase VideoImageOverlay::CRT_RGB_2: init = 5;
	}
	overlayEffect.init("Overlay Effect", str, init, sizeofArray(str));
	overlayEffect.valueDelegate().bind<&overlayEffectSet>();
}

void overlayEffectLevelSet(MultiChoiceMenuItem &, int val)
{
	uint setVal = 10;
	switch(val)
	{
		bcase 1: setVal = 25;
		bcase 2: setVal = 33;
		bcase 3: setVal = 50;
		bcase 4: setVal = 66;
		bcase 5: setVal = 75;
		bcase 6: setVal = 100;
	}
	optionOverlayEffectLevel.val = setVal;
	emuView.vidImgOverlay.intensity = setVal/100.;
}

void OptionView::overlayEffectLevelInit()
{
	static const char *str[] = { "10%", "25%", "33%", "50%", "66%", "75%", "100%" };
	uint init = 0;
	switch(optionOverlayEffectLevel)
	{
		bcase 25: init = 1;
		bcase 33: init = 2;
		bcase 50: init = 3;
		bcase 66: init = 4;
		bcase 75: init = 5;
		bcase 100: init = 6;
	}
	overlayEffectLevel.init("Overlay Effect Level", str, init, sizeofArray(str));
	overlayEffectLevel.valueDelegate().bind<&overlayEffectLevelSet>();
}

void relativePointerDecelSet(MultiChoiceMenuItem &, int val)
{
	#if defined(CONFIG_BASE_ANDROID)
	if(val == 0)
		optionRelPointerDecel.val = optionRelPointerDecelLow;
	else if(val == 1)
		optionRelPointerDecel.val = optionRelPointerDecelMed;
	else if(val == 2)
		optionRelPointerDecel.val = optionRelPointerDecelHigh;
	#endif
}

void OptionView::relativePointerDecelInit()
{
	static const char *str[] = { "Low", "Med.", "High" };
	int init = 0;
	if(optionRelPointerDecel == optionRelPointerDecelLow)
		init = 0;
	if(optionRelPointerDecel == optionRelPointerDecelMed)
		init = 1;
	if(optionRelPointerDecel == optionRelPointerDecelHigh)
		init = 2;
	relativePointerDecel.init("Trackball Sensitivity", str, init, sizeofArray(str));
	relativePointerDecel.valueDelegate().bind<&relativePointerDecelSet>();
}

void OptionView::loadVideoItems(MenuItem *item[], uint &items)
{
	name_ = "Video Options";
	if(!optionFrameSkip.isConst) { frameSkipInit(); item[items++] = &frameSkip; }
	if(!optionGameOrientation.isConst) { gameOrientationInit(); item[items++] = &gameOrientation; }
	aspectRatioInit(); item[items++] = &aspectRatio;
	imgFilterInit(); item[items++] = &imgFilter;
	overlayEffectInit(); item[items++] = &overlayEffect;
	overlayEffectLevelInit(); item[items++] = &overlayEffectLevel;
	zoomInit(); item[items++] = &zoom;
	#ifdef SUPPORT_ANDROID_DIRECT_TEXTURE
	if(Base::androidSDK() < 14)
	{
		directTexture.init(optionDirectTexture, Gfx::supportsAndroidDirectTexture()); item[items++] = &directTexture;
		directTexture.selectDelegate().bind<&directTextureHandler>();
	}
	if(!optionGLSyncHack.isConst)
	{
		glSyncHack.init(optionGLSyncHack); item[items++] = &glSyncHack;
		glSyncHack.selectDelegate().bind<&glSyncHackHandler>();
	}
	#endif
	if(!optionDitherImage.isConst)
	{
		dither.init(Gfx::dither()); item[items++] = &dither;
		dither.selectDelegate().bind<&ditherHandler>();
	}
}

void OptionView::loadAudioItems(MenuItem *item[], uint &items)
{
	name_ = "Audio Options";
	snd.init(optionSound); item[items++] = &snd;
	snd.selectDelegate().bind<&soundHandler>();
	if(!optionSoundRate.isConst) { audioRateInit(); item[items++] = &audioRate; }
#ifdef CONFIG_AUDIO_CAN_USE_MAX_BUFFERS_HINT
	soundBuffersInit(); item[items++] = &soundBuffers;
#endif
#ifdef CONFIG_AUDIO_OPENSL_ES
	sndUnderrunCheck.init(optionSoundUnderrunCheck); item[items++] = &sndUnderrunCheck;
	sndUnderrunCheck.selectDelegate().bind<&soundUnderrunCheckHandler>();
#endif
}

void OptionView::loadInputItems(MenuItem *item[], uint &items)
{
	name_ = "Input Options";
	if(!optionTouchCtrl.isConst)
	{
		touchCtrlInit(); item[items++] = &touchCtrl;
		touchCtrlConfig.init(); item[items++] = &touchCtrlConfig;
		touchCtrlConfig.selectDelegate().bind<&touchCtrlConfigHandler>();
	}
	#if !defined(CONFIG_BASE_IOS) && !defined(CONFIG_BASE_PS3)
	buttonConfig.init(); item[items++] = &buttonConfig;
	buttonConfig.selectDelegate().bind<&buttonConfigHandler>();
	#endif
	#ifdef CONFIG_INPUT_ICADE
	iCade.init(Input::iCadeActive()); item[items++] = &iCade;
	iCade.selectDelegate().bind<&iCadeHandler>();
	iCadeButtonConfig.init(); item[items++] = &iCadeButtonConfig;
	iCadeButtonConfig.selectDelegate().bind<&iCadeButtonConfigHandler>();
	#endif
	#ifdef CONFIG_BLUETOOTH
	wiiButtonConfig.init(); item[items++] = &wiiButtonConfig;
	wiiButtonConfig.selectDelegate().bind<&wiiButtonConfigHandler>();
	iCPButtonConfig.init(); item[items++] = &iCPButtonConfig;
	iCPButtonConfig.selectDelegate().bind<&iCPButtonConfigHandler>();
	zeemoteButtonConfig.init(); item[items++] = &zeemoteButtonConfig;
	zeemoteButtonConfig.selectDelegate().bind<&zeemoteButtonConfigHandler>();
	btScanSecsInit(); item[items++] = &btScanSecs;
	keepBtActive.init(optionKeepBluetoothActive); item[items++] = &keepBtActive;
	keepBtActive.selectDelegate().bind<&keepBtActiveHandler>();
	btScanCache.init(optionBlueToothScanCache); item[items++] = &btScanCache;
	btScanCache.selectDelegate().bind<&btScanCacheHandler>();
	#endif
	#if defined(CONFIG_INPUT_ANDROID) && CONFIG_ENV_ANDROID_MINSDK >= 9
	useOSInputMethod.init(!Input::eventsUseOSInputMethod()); item[items++] = &useOSInputMethod;
	useOSInputMethod.selectDelegate().bind<&useOSInputMethodHandler>();
	#endif
	if(!optionRelPointerDecel.isConst) { relativePointerDecelInit(); item[items++] = &relativePointerDecel; }
}

void OptionView::loadSystemItems(MenuItem *item[], uint &items)
{
	name_ = "System Options";
	autoSaveStateInit(); item[items++] = &autoSaveState;
	confirmAutoLoadState.init(optionConfirmAutoLoadState); item[items++] = &confirmAutoLoadState;
	confirmAutoLoadState.selectDelegate().bind<&confirmAutoLoadStateHandler>();
}

void OptionView::loadGUIItems(MenuItem *item[], uint &items)
{
	name_ = "GUI Options";
	if(!optionMenuOrientation.isConst) { menuOrientationInit(); item[items++] = &menuOrientation; }
	if(!optionPauseUnfocused.isConst)
	{
		pauseUnfocused.init(optionPauseUnfocused); item[items++] = &pauseUnfocused;
		pauseUnfocused.selectDelegate().bind<&pauseUnfocusedHandler>();
	}
	if(!optionNotificationIcon.isConst)
	{
		notificationIcon.init(optionNotificationIcon); item[items++] = &notificationIcon;
		notificationIcon.selectDelegate().bind<&notificationIconHandler>();
	}
	if(!optionTitleBar.isConst)
	{
		navView.init(optionTitleBar); item[items++] = &navView;
		navView.selectDelegate().bind<&navViewHandler>();
	}
	if(!View::needsBackControlIsConst)
	{
		backNav.init(View::needsBackControl); item[items++] = &backNav;
		backNav.selectDelegate().bind<&backNavHandler>();
	}
	rememberLastMenu.init(optionRememberLastMenu); item[items++] = &rememberLastMenu;
	rememberLastMenu.selectDelegate().bind<&rememberLastMenuHandler>();
	if(!optionLargeFonts.isConst)
	{
		largeFonts.init(optionLargeFonts); item[items++] = &largeFonts;
		largeFonts.selectDelegate().bind<&largeFontsHandler>();
	}
	if(!optionDPI.isConst) { dpiInit(); item[items++] = &dpi; }
	#ifndef CONFIG_ENV_WEBOS
	altGamepadConfirm.init(input_swappedGamepadConfirm); item[items++] = &altGamepadConfirm;
	altGamepadConfirm.selectDelegate().bind<&altGamepadConfirmHandler>();
	#endif
	if(!optionIdleDisplayPowerSave.isConst)
	{
		idleDisplayPowerSave.init(optionIdleDisplayPowerSave); item[items++] = &idleDisplayPowerSave;
		idleDisplayPowerSave.selectDelegate().bind<&idleDisplayPowerSaveHandler>();
	}
	if(!optionLowProfileOSNav.isConst)
	{
		lowProfileOSNav.init(optionLowProfileOSNav); item[items++] = &lowProfileOSNav;
		lowProfileOSNav.selectDelegate().bind<&lowProfileOSNavHandler>();
	}
	if(!optionHideOSNav.isConst)
	{
		hideOSNav.init(optionHideOSNav); item[items++] = &hideOSNav;
		hideOSNav.selectDelegate().bind<&hideOSNavHandler>();
	}
	if(!optionHideStatusBar.isConst)
	{
		statusBarInit(); item[items++] = &statusBar;
	}
}

void OptionView::init(uint idx, bool highlightFirst)
{
	uint i = 0;
	switch(idx)
	{
		bcase 0: loadVideoItems(item, i);
		bcase 1: loadAudioItems(item, i);
		bcase 2: loadInputItems(item, i);
		bcase 3: loadSystemItems(item, i);
		bcase 4: loadGUIItems(item, i);
	}
	assert(i <= sizeofArray(item));
	BaseMenuView::init(item, i, highlightFirst);
}