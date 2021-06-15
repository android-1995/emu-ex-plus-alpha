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

#include <emuframework/EmuInputView.hh>
#include <emuframework/VController.hh>
#include <emuframework/EmuApp.hh>
#include <emuframework/EmuVideo.hh>
#include <emuframework/EmuVideoLayer.hh>
#include <emuframework/FilePicker.hh>
#include "EmuOptions.hh"
#include "private.hh"
#include "privateInput.hh"
#include <imagine/gui/AlertView.hh>
#include <imagine/gfx/RendererCommands.hh>

EmuInputView::EmuInputView() {}

EmuInputView::EmuInputView(ViewAttachParams attach, SysVController &vCtrl, EmuVideoLayer &videoLayer)
	: View(attach), vController{&vCtrl},
		videoLayer{&videoLayer}
{}

void EmuInputView::draw(Gfx::RendererCommands &cmds)
{
	cmds.loadTransform(projP.makeTranslate());
	vController->draw(cmds, touchControlsOn && EmuSystem::touchControlsApplicable(), ffToggleActive);
}

void EmuInputView::place()
{
	#ifdef CONFIG_EMUFRAMEWORK_VCONTROLS
	EmuControls::setupVControllerVars(*vController);
	vController->place();
	#endif
}

void EmuInputView::resetInput()
{
	#ifdef CONFIG_EMUFRAMEWORK_VCONTROLS
	vController->resetInput();
	#endif
	ffToggleActive = false;
}

void EmuInputView::updateFastforward()
{
	app().viewController().setFastForwardActive(ffToggleActive);
}

bool EmuInputView::inputEvent(Input::Event e)
{
	#ifdef CONFIG_EMUFRAMEWORK_VCONTROLS
	if(e.isPointer())
	{
		if(e.pushed() && vController->menuHitTest(e.pos()))
		{
			app().showLastViewFromSystem(attachParams(), e);
			return true;
		}
		else if(e.pushed() && vController->fastForwardHitTest(e.pos()))
		{
			ffToggleActive ^= true;
			updateFastforward();
		}
		else if((touchControlsOn && EmuSystem::touchControlsApplicable())
			|| vController->isInKeyboardMode())
		{
			vController->applyInput(e);
			EmuSystem::handlePointerInputEvent(e, videoLayer->gameRect());
		}
		else if(EmuSystem::handlePointerInputEvent(e, videoLayer->gameRect()))
		{
			//logMsg("game consumed pointer input event");
		}
		#ifdef CONFIG_VCONTROLS_GAMEPAD
		else if(!touchControlsOn && (unsigned)optionTouchCtrl == 2 && optionTouchCtrlShowOnTouch
			&& !vController->isInKeyboardMode()
			&& e.isTouch() && e.pushed()
			)
		{
			logMsg("turning on on-screen controls from touch input");
			touchControlsOn = true;
			app().viewController().placeEmuViews();
		}
		#endif
		return true;
	}
	#else
	if(e.isPointer())
	{
		if(e.state == Input::PUSHED)
		{
			app().showMenuViewFromSystem(attachParams(), e);
		}
		return true;
	}
	#endif
	if(e.isRelativePointer())
	{
		//processRelPtr(app(), e);
		return true;
	}
	else
	{
		#ifdef CONFIG_VCONTROLS_GAMEPAD
		if(vController->keyInput(e))
			return true;
		#endif
		auto &emuApp = app();
		auto &keyMapping = emuApp.keyInputMapping();
		if(!keyMapping) [[unlikely]]
			return false;
		assumeExpr(e.device());
		const KeyMapping::ActionGroup &actionMap = keyMapping.inputDevActionTablePtr[e.device()->idx][e.mapKey()];
		//logMsg("player %d input %s", player, Input::buttonName(e.map, e.button));
		bool didAction = false;
		iterateTimes(KeyMapping::maxKeyActions, i)
		{
			auto action = actionMap[i];
			if(action != 0)
			{
				using namespace EmuControls;
				didAction = true;
				action--;

				switch(action)
				{
					bcase guiKeyIdxFastForward:
					{
						if(e.repeated())
							continue;
						ffToggleActive = e.pushed();
						updateFastforward();
						logMsg("fast-forward state:%d", ffToggleActive);
					}

					bcase guiKeyIdxLoadGame:
					if(e.pushed())
					{
						logMsg("show load game view from key event");
						emuApp.viewController().popToRoot();
						pushAndShow(EmuFilePicker::makeForLoading(attachParams(), e), e, false);
						return true;
					}

					bcase guiKeyIdxMenu:
					if(e.pushed())
					{
						logMsg("show system actions view from key event");
						emuApp.showSystemActionsViewFromSystem(attachParams(), e);
						return true;
					}

					bcase guiKeyIdxSaveState:
					if(e.pushed())
					{
						static auto doSaveState =
							[](EmuApp &app, bool notify)
							{
								if(auto err = app.saveStateWithSlot(EmuSystem::saveStateSlot);
									err)
								{
									app.printfMessage(4, true, "Save State: %s", err->what());
								}
								else if(notify)
								{
									app.postMessage("State Saved");
								}
							};

						if(EmuSystem::shouldOverwriteExistingState())
						{
							emuApp.syncEmulationThread();
							doSaveState(emuApp, optionConfirmOverwriteState);
						}
						else
						{
							auto ynAlertView = makeView<YesNoAlertView>("Really Overwrite State?");
							ynAlertView->setOnYes(
								[this]()
								{
									doSaveState(app(), false);
									app().viewController().showEmulation();
								});
							ynAlertView->setOnNo(
								[this]()
								{
									app().viewController().showEmulation();
								});
							pushAndShowModal(std::move(ynAlertView), e);
						}
						return true;
					}

					bcase guiKeyIdxLoadState:
					if(e.pushed())
					{
						emuApp.syncEmulationThread();
						if(auto err = emuApp.loadStateWithSlot(EmuSystem::saveStateSlot);
							err)
						{
							emuApp.printfMessage(4, true, "Load State: %s", err->what());
						}
						return true;
					}

					bcase guiKeyIdxDecStateSlot:
					if(e.pushed())
					{
						EmuSystem::saveStateSlot--;
						if(EmuSystem::saveStateSlot < -1)
							EmuSystem::saveStateSlot = 9;
						emuApp.printfMessage(1, false, "State Slot: %s", stateNameStr(EmuSystem::saveStateSlot));
					}

					bcase guiKeyIdxIncStateSlot:
					if(e.pushed())
					{
						auto prevSlot = EmuSystem::saveStateSlot;
						EmuSystem::saveStateSlot++;
						if(EmuSystem::saveStateSlot > 9)
							EmuSystem::saveStateSlot = -1;
						emuApp.printfMessage(1, false, "State Slot: %s", stateNameStr(EmuSystem::saveStateSlot));
					}

					bcase guiKeyIdxGameScreenshot:
					if(e.pushed())
					{
						videoLayer->emuVideo().takeGameScreenshot();
						return true;
					}

					bcase guiKeyIdxToggleFastForward:
					if(e.pushed())
					{
						if(e.repeated())
							continue;
						ffToggleActive = !ffToggleActive;
						updateFastforward();
						logMsg("fast-forward state:%d", ffToggleActive);
					}

					bcase guiKeyIdxExit:
					if(e.pushed())
					{
						logMsg("show last view from key event");
						emuApp.showLastViewFromSystem(attachParams(), e);
						return true;
					}

					bdefault:
					{
						if(e.repeated())
						{
							continue;
						}
						//logMsg("action %d, %d", emuKey, state);
						bool turbo;
						unsigned sysAction = EmuSystem::translateInputAction(action, turbo);
						//logMsg("action %d -> %d, pushed %d", action, sysAction, e.state == Input::PUSHED);
						if(turbo)
						{
							if(e.pushed())
							{
								emuApp.addTurboInputEvent(sysAction);
							}
							else
							{
								emuApp.removeTurboInputEvent(sysAction);
							}
						}
						EmuSystem::handleInputAction(&emuApp, e.state(), sysAction);
					}
				}
			}
			else
				break;
		}
		return didAction || (consumeUnboundGamepadKeys && e.isGamepad());
	}
}

void EmuInputView::setTouchControlsOn(bool on)
{
	touchControlsOn = on;
}

bool EmuInputView::touchControlsAreOn() const
{
//	return touchControlsOn;
    //一直显示按键
    return true;
}

void EmuInputView::setConsumeUnboundGamepadKeys(bool on)
{
	consumeUnboundGamepadKeys = on;
}

bool EmuInputView::shouldConsumeUnboundGamepadKeys() const
{
	return consumeUnboundGamepadKeys;
}
