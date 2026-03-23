/*  This file is part of Swan.emu.

	Swan.emu is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Swan.emu is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Swan.emu.  If not, see <http://www.gnu.org/licenses/> */

#include <emuframework/macros.h>
#include <mednafen/types.h>
import system;
import emuex;
import imagine;
import std;
#include <emuframework/EmuAppInlines.hh>

namespace EmuEx
{

const std::string_view AppMeta::creditsViewStr{CREDITS_INFO_STRING "(c) 2011-2026\nRobert Broglia\nwww.explusalpha.com\n\nPortions (c) the\nMednafen Team\nmednafen.github.io"};
const std::string_view AppMeta::configFilename{"SwanEmu.config"};
const bool AppMeta::needsGlobalInstance{true};
const NameFilterFunc AppMeta::defaultFsFilter = [](std::string_view name) { return endsWithAnyCaseless(name, ".ws", ".wsc", ".bin"); };
const AspectRatioInfo AppMeta::aspectRatioInfo{"14:9 (原画)", {14, 9}};

constexpr auto dpadKeyInfo = makeArray<KeyInfo>
(
	SwanKey::Up,
	SwanKey::Right,
	SwanKey::Down,
	SwanKey::Left
);

constexpr auto centerKeyInfo = makeArray<KeyInfo>(SwanKey::Start);

constexpr auto faceKeyInfo = makeArray<KeyInfo>
(
	SwanKey::BNoRotation,
	SwanKey::ANoRotation
);

constexpr auto turboFaceKeyInfo = turbo(faceKeyInfo);

constexpr auto oppositeDPadKeyInfo = makeArray<KeyInfo>
(
	SwanKey::Y4X4,
	SwanKey::Y3X3,
	SwanKey::Y1X1,
	SwanKey::Y2X2
);

constexpr auto faceButtonCombinedCodes = makeArray<KeyInfo>
(
	SwanKey::BNoRotation,
	SwanKey::ANoRotation,
	SwanKey::Y4X4,
	SwanKey::Y3X3,
	SwanKey::Y1X1,
	SwanKey::Y2X2
);

constexpr auto allFaceKeyInfo = makeArray<KeyInfo>
(
	SwanKey::A,
	SwanKey::B,
	SwanKey::Y1,
	SwanKey::Y2,
	SwanKey::Y4,
	SwanKey::Y3,
	SwanKey::BNoRotation,
	SwanKey::ANoRotation,
	SwanKey::Y4X4,
	SwanKey::Y3X3,
	SwanKey::Y1X1,
	SwanKey::Y2X2
);

constexpr auto gpKeyInfo = concatToArrayNow<dpadKeyInfo, centerKeyInfo, allFaceKeyInfo, turboFaceKeyInfo>;

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
	switch(SwanKey(c))
	{
		case SwanKey::Up: return "Up X1 ↷ Y2";
		case SwanKey::Right: return "Right X2 ↷ Y3";
		case SwanKey::Down: return "Down X3 ↷ Y4";
		case SwanKey::Left: return "Left X4 ↷ Y1";
		case SwanKey::Start: return "Start";
		case SwanKey::A: return "A ↷ X4";
		case SwanKey::B: return "B ↷ X1";
		case SwanKey::Y1: return "Y1 ↷ B";
		case SwanKey::Y2: return "Y2 ↷ A";
		case SwanKey::Y3: return "Y3 ↷ X3";
		case SwanKey::Y4: return "Y4 ↷ X2";
		case SwanKey::ANoRotation: return "A";
		case SwanKey::BNoRotation: return "B";
		case SwanKey::Y1X1: return "Y1 ↷ X1";
		case SwanKey::Y2X2: return "Y2 ↷ X2";
		case SwanKey::Y3X3: return "Y3 ↷ X3";
		case SwanKey::Y4X4: return "Y4 ↷ X4";
		default: return "";
	}
}

std::span<const KeyConfigDesc> AppMeta::defaultKeyConfigs()
{
	using namespace Input;

	static constexpr std::array pcKeyboardMap
	{
		KeyMapping{SwanKey::Up, Keycode::UP},
		KeyMapping{SwanKey::Right, Keycode::RIGHT},
		KeyMapping{SwanKey::Down, Keycode::DOWN},
		KeyMapping{SwanKey::Left, Keycode::LEFT},
		KeyMapping{SwanKey::Start, Keycode::ENTER},
		KeyMapping{SwanKey::A, Keycode::X},
		KeyMapping{SwanKey::B, Keycode::Z},
		KeyMapping{SwanKey::Y1, Keycode::Q},
		KeyMapping{SwanKey::Y2, Keycode::W},
		KeyMapping{SwanKey::Y3, Keycode::S},
		KeyMapping{SwanKey::Y4, Keycode::A},
	};

	static constexpr std::array genericGamepadMap
	{
		KeyMapping{SwanKey::Up, Keycode::UP},
		KeyMapping{SwanKey::Right, Keycode::RIGHT},
		KeyMapping{SwanKey::Down, Keycode::DOWN},
		KeyMapping{SwanKey::Left, Keycode::LEFT},
		KeyMapping{SwanKey::Start, Keycode::GAME_START},
		KeyMapping{SwanKey::A, Keycode::GAME_A},
		KeyMapping{SwanKey::B, Keycode::GAME_X},
		KeyMapping{SwanKey::Y1, Keycode::GAME_L1},
		KeyMapping{SwanKey::Y2, Keycode::GAME_R1},
		KeyMapping{SwanKey::Y3, Keycode::GAME_B},
		KeyMapping{SwanKey::Y4, Keycode::GAME_Y},
	};

	static constexpr std::array wiimoteMap
	{
		KeyMapping{SwanKey::Up, WiimoteKey::UP},
		KeyMapping{SwanKey::Right, WiimoteKey::RIGHT},
		KeyMapping{SwanKey::Down, WiimoteKey::DOWN},
		KeyMapping{SwanKey::Left, WiimoteKey::LEFT},
		KeyMapping{SwanKey::B, WiimoteKey::_1},
		KeyMapping{SwanKey::A, WiimoteKey::_2},
		KeyMapping{SwanKey::Start, WiimoteKey::PLUS},
	};

	return genericKeyConfigs<pcKeyboardMap, genericGamepadMap, wiimoteMap>();
}

bool AppMeta::allowsTurboModifier(KeyCode c)
{
	switch(SwanKey(c))
	{
		case SwanKey::A ... SwanKey::B:
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

		a{AssetFileID::gamepadOverlay,     gpImageCoords({{4, 0}, {2, 2}})},
		b{AssetFileID::gamepadOverlay,     gpImageCoords({{6, 0}, {2, 2}})},
		d1{AssetFileID::gamepadOverlay,    gpImageCoords({{4, 2}, {2, 2}})},
		d2{AssetFileID::gamepadOverlay,    gpImageCoords({{6, 2}, {2, 2}})},
		d3{AssetFileID::gamepadOverlay,    gpImageCoords({{0, 4}, {2, 2}})},
		d4{AssetFileID::gamepadOverlay,    gpImageCoords({{2, 4}, {2, 2}})},
		start{AssetFileID::gamepadOverlay, gpImageCoords({{0, 6}, {2, 1}}), {1, 2}},

		blank{AssetFileID::gamepadOverlay, gpImageCoords({{4, 4}, {2, 2}})};
	} virtualControllerAssets;

	if(key[0] == 0)
		return virtualControllerAssets.dpad;
	switch(SwanKey(key[0]))
	{
		case SwanKey::ANoRotation:
		case SwanKey::A: return virtualControllerAssets.a;
		case SwanKey::BNoRotation:
		case SwanKey::B: return virtualControllerAssets.b;
		case SwanKey::Y1X1:
		case SwanKey::Y1: return virtualControllerAssets.d1;
		case SwanKey::Y2X2:
		case SwanKey::Y2: return virtualControllerAssets.d2;
		case SwanKey::Y3X3:
		case SwanKey::Y3: return virtualControllerAssets.d3;
		case SwanKey::Y4X4:
		case SwanKey::Y4: return virtualControllerAssets.d4;
		case SwanKey::Start: return virtualControllerAssets.start;
		default: return virtualControllerAssets.blank;
	}
}

SystemInputDeviceDesc AppMeta::inputDeviceDesc(int idx)
{
	static constexpr std::array gamepadComponents
	{
		InputComponentDesc{"D-Pad", dpadKeyInfo, InputComponent::dPad, LB2DO},
		InputComponentDesc{"Face Buttons + Opposite D-Pad Buttons", faceButtonCombinedCodes, InputComponent::button, RB2DO, {.rowSize = 2}},
		InputComponentDesc{"Face Buttons", faceKeyInfo, InputComponent::button, RB2DO, {.altConfig = true}},
		InputComponentDesc{"Opposite D-Pad Buttons", oppositeDPadKeyInfo, InputComponent::button, RB2DO, {.altConfig = true, .staggeredLayout = true}},
		InputComponentDesc{"Start", centerKeyInfo, InputComponent::button, RB2DO},
	};
	static constexpr SystemInputDeviceDesc gamepadDesc{"Gamepad", gamepadComponents};
	return gamepadDesc;
}

void AppMeta::onCustomizeNavView(AppNavView& view)
{
	const Gfx::LGradientStopDesc navViewGrad[] =
	{
		{ .0, Gfx::PackedColor::format.build((0./255.) * .4, (158./255.) * .4, (211./255.) * .4, 1.) },
		{ .3, Gfx::PackedColor::format.build((0./255.) * .4, (158./255.) * .4, (211./255.) * .4, 1.) },
		{ .97, Gfx::PackedColor::format.build((0./255.) * .4, (53./255.) * .4, (70./255.) * .4, 1.) },
		{ 1., view.separatorColor() },
	};
	view.setBackgroundGradient(navViewGrad);
}

}
