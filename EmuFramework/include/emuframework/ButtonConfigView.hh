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

#include <imagine/gfx/GfxText.hh>
#include <imagine/gui/TableView.hh>
#include <imagine/gui/MenuItem.hh>

class InputManagerView;
struct InputDeviceConfig;
struct KeyCategory;

class ButtonConfigSetView : public View
{
public:
	using SetDelegate = DelegateFunc<void (Input::Event e)>;

	ButtonConfigSetView(ViewAttachParams attach, InputManagerView &rootIMView,
		Input::Device &dev, const char *actionName, SetDelegate onSet);
	void place() final;
	bool inputEvent(Input::Event e) final;
	void draw(Gfx::RendererCommands &cmds) final;
	void onAddedToController(ViewController *c, Input::Event e) final;

private:
	#ifdef CONFIG_INPUT_POINTING_DEVICES
	IG::WindowRect unbindB{}, cancelB{};
	#endif
	std::array<char, 24> actionStr{};
	Gfx::Text text{};
	#ifdef CONFIG_INPUT_POINTING_DEVICES
	Gfx::Text unbind{}, cancel{};
	#endif
	SetDelegate onSetD{};
	const Input::Device &dev;
	const Input::Device *savedDev{};
	InputManagerView &rootIMView;

	void initPointerUI();
	bool pointerUIIsInit();
};

class ButtonConfigView : public TableView
{
private:
	struct BtnConfigMenuItem : public DualTextMenuItem
	{
		using DualTextMenuItem::DualTextMenuItem;
		void draw(Gfx::RendererCommands &cmds, Gfx::GC xPos, Gfx::GC yPos, Gfx::GC xSize, Gfx::GC ySize,
			_2DOrigin align, const Gfx::ProjectionPlane &projP, Gfx::Color color) const final;
	};

	InputManagerView &rootIMView;
	TextMenuItem reset{};
	using KeyNameStr = std::array<char, 20>;
	std::unique_ptr<BtnConfigMenuItem[]> btn{};
	const KeyCategory *cat{};
	InputDeviceConfig *devConf{};
	Input::Time leftKeyPushTime{};

	void onSet(Input::Key mapKey, int keyToSet);
	static KeyNameStr makeKeyNameStr(Input::Key key, const char *name);

public:
	ButtonConfigView(ViewAttachParams attach, InputManagerView &rootIMView, const KeyCategory *cat, InputDeviceConfig &devConf);
	bool inputEvent(Input::Event e) final;
};
