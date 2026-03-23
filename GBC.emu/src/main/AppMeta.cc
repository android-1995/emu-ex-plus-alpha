/*  This file is part of GBC.emu.

	GBC.emu is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	GBC.emu is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with GBC.emu.  If not, see <http://www.gnu.org/licenses/> */

#include <emuframework/macros.h>
import system;
import emuex;
import imagine;
import std;
#include <emuframework/EmuAppInlines.hh>

namespace EmuEx
{

const std::string_view AppMeta::creditsViewStr{CREDITS_INFO_STRING "(c) 2011-2026\nRobert Broglia\nwww.explusalpha.com\n\n\nPortions (c) the\nGambatte Team\ngambatte.sourceforge.net"};
const std::string_view AppMeta::configFilename{"GbcEmu.config"};
const bool AppMeta::hasCheats{true};
const AspectRatioInfo AppMeta::aspectRatioInfo{"10:9 (原画)", {10, 9}};
const NameFilterFunc AppMeta::defaultFsFilter = [](std::string_view name) { return endsWithAnyCaseless(name, ".gb", ".gbc", ".dmg"); };

constexpr auto dpadKeyInfo = makeArray<KeyInfo>
(
	GbcKey::Up,
	GbcKey::Right,
	GbcKey::Down,
	GbcKey::Left
);

constexpr auto centerKeyInfo = makeArray<KeyInfo>
(
	GbcKey::Select,
	GbcKey::Start
);

constexpr auto faceKeyInfo = makeArray<KeyInfo>
(
	GbcKey::B,
	GbcKey::A
);

constexpr auto turboFaceKeyInfo = turbo(faceKeyInfo);

constexpr std::array comboKeyInfo{KeyInfo{std::array{GbcKey::A, GbcKey::B}}};

constexpr auto gpKeyInfo = concatToArrayNow<dpadKeyInfo, centerKeyInfo, faceKeyInfo, turboFaceKeyInfo, comboKeyInfo>;

std::span<const KeyCategory> AppMeta::keyCategories()
{
	static constexpr KeyCategory categories[]
	{
		{"Gamepad", gpKeyInfo},
	};
	return categories;
}

std::string_view AppMeta::systemKeyCodeToString(KeyCode c)
{
	switch(GbcKey(c))
	{
		case GbcKey::Up: return "Up";
		case GbcKey::Right: return "Right";
		case GbcKey::Down: return "Down";
		case GbcKey::Left: return "Left";
		case GbcKey::Select: return "Select";
		case GbcKey::Start: return "Start";
		case GbcKey::A: return "A";
		case GbcKey::B: return "B";
		default: return "";
	}
}

std::span<const KeyConfigDesc> AppMeta::defaultKeyConfigs()
{
	using namespace Input;

	static constexpr std::array pcKeyboardMap
	{
		KeyMapping{GbcKey::Up, Keycode::UP},
		KeyMapping{GbcKey::Right, Keycode::RIGHT},
		KeyMapping{GbcKey::Down, Keycode::DOWN},
		KeyMapping{GbcKey::Left, Keycode::LEFT},
		KeyMapping{GbcKey::Select, Keycode::SPACE},
		KeyMapping{GbcKey::Start, Keycode::ENTER},
		KeyMapping{GbcKey::A, Keycode::X},
		KeyMapping{GbcKey::B, Keycode::Z},
	};

	static constexpr std::array genericGamepadMap
	{
		KeyMapping{GbcKey::Up, Keycode::UP},
		KeyMapping{GbcKey::Right, Keycode::RIGHT},
		KeyMapping{GbcKey::Down, Keycode::DOWN},
		KeyMapping{GbcKey::Left, Keycode::LEFT},
		KeyMapping{GbcKey::Select, Keycode::GAME_SELECT},
		KeyMapping{GbcKey::Start, Keycode::GAME_START},
		KeyMapping{GbcKey::A, Keycode::GAME_A},
		KeyMapping{GbcKey::B, Keycode::GAME_X},
	};

	static constexpr std::array wiimoteMap
	{
		KeyMapping{GbcKey::Up, WiimoteKey::UP},
		KeyMapping{GbcKey::Right, WiimoteKey::RIGHT},
		KeyMapping{GbcKey::Down, WiimoteKey::DOWN},
		KeyMapping{GbcKey::Left, WiimoteKey::LEFT},
		KeyMapping{GbcKey::B, WiimoteKey::_1},
		KeyMapping{GbcKey::A, WiimoteKey::_2},
		KeyMapping{GbcKey::Select, WiimoteKey::MINUS},
		KeyMapping{GbcKey::Start, WiimoteKey::PLUS},
	};

	return genericKeyConfigs<pcKeyboardMap, genericGamepadMap, wiimoteMap>();
}

bool AppMeta::allowsTurboModifier(KeyCode c)
{
	switch(GbcKey(c))
	{
		case GbcKey::A ... GbcKey::B:
		return true;
		default: return false;
	}
}

constexpr FRect gpImageCoords(IRect cellRelBounds)
{
	constexpr F2Size imageSize{256, 256};
	constexpr int cellSize = 32;
	return (cellRelBounds.relToAbs() * cellSize).as<float>() / imageSize;
}

AssetDesc AppMeta::vControllerAssetDesc(KeyInfo key)
{
	static constexpr struct VirtualControllerAssets
	{
		AssetDesc dpad{AssetFileID::gamepadOverlay, gpImageCoords({{}, {4, 4}})},

		a{AssetFileID::gamepadOverlay,      gpImageCoords({{4, 0}, {2, 2}})},
		b{AssetFileID::gamepadOverlay,      gpImageCoords({{6, 0}, {2, 2}})},
		select{AssetFileID::gamepadOverlay, gpImageCoords({{4, 2}, {2, 1}}), {1, 2}},
		start{AssetFileID::gamepadOverlay,  gpImageCoords({{4, 3}, {2, 1}}), {1, 2}},
		ab{AssetFileID::gamepadOverlay,     gpImageCoords({{6, 2}, {2, 2}})},

		blank{AssetFileID::gamepadOverlay, gpImageCoords({{0, 4}, {2, 2}})};
	} virtualControllerAssets;

	if(key[0] == 0)
		return virtualControllerAssets.dpad;
	switch(GbcKey(key[0]))
	{
		case GbcKey::A: return GbcKey(key[1]) == GbcKey::B ? virtualControllerAssets.ab : virtualControllerAssets.a;
		case GbcKey::B: return virtualControllerAssets.b;
		case GbcKey::Select: return virtualControllerAssets.select;
		case GbcKey::Start: return virtualControllerAssets.start;
		default: return virtualControllerAssets.blank;
	}
}

SystemInputDeviceDesc AppMeta::inputDeviceDesc(int)
{
	static constexpr std::array gamepadComponents
	{
		InputComponentDesc{"D-Pad", dpadKeyInfo, InputComponent::dPad, LB2DO},
		InputComponentDesc{"Face Buttons", faceKeyInfo, InputComponent::button, RB2DO},
		InputComponentDesc{"Select", {&centerKeyInfo[0], 1}, InputComponent::button, LB2DO},
		InputComponentDesc{"Start", {&centerKeyInfo[1], 1}, InputComponent::button, RB2DO},
		InputComponentDesc{"Select/Start", centerKeyInfo, InputComponent::button, CB2DO, {.altConfig = true}},
	};
	static constexpr SystemInputDeviceDesc gamepadDesc{"Gamepad", gamepadComponents};
	return gamepadDesc;
}

void AppMeta::onCustomizeNavView(EmuApp::NavView &view)
{
	const Gfx::LGradientStopDesc navViewGrad[] =
	{
		{ .0, Gfx::PackedColor::format.build((8./255.) * .4, (232./255.) * .4, (222./255.) * .4, 1.) },
		{ .3, Gfx::PackedColor::format.build((8./255.) * .4, (232./255.) * .4, (222./255.) * .4, 1.) },
		{ .97, Gfx::PackedColor::format.build((0./255.) * .4, (77./255.) * .4, (74./255.) * .4, 1.) },
		{ 1., view.separatorColor() },
	};
	view.setBackgroundGradient(navViewGrad);
}

}
