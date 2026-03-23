/*  This file is part of NGP.emu.

	NGP.emu is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	NGP.emu is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with NGP.emu.  If not, see <http://www.gnu.org/licenses/> */

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
const std::string_view AppMeta::configFilename{"NgpEmu.config"};
const bool AppMeta::needsGlobalInstance{true};
const AspectRatioInfo AppMeta::aspectRatioInfo{"20:19 (原画)", {20, 19}};
const NameFilterFunc AppMeta::defaultFsFilter = [](std::string_view name)
{
	return endsWithAnyCaseless(name, ".ngc", ".ngp", ".npc", ".ngpc");
};

constexpr auto dpadKeyInfo = makeArray<KeyInfo>
(
	NgpKey::Up,
	NgpKey::Right,
	NgpKey::Down,
	NgpKey::Left
);

constexpr auto optionKeyInfo = makeArray<KeyInfo>(NgpKey::Option);

constexpr auto faceKeyInfo = makeArray<KeyInfo>
(
	NgpKey::A,
	NgpKey::B
);

constexpr auto turboFaceKeyInfo = turbo(faceKeyInfo);

constexpr auto gpKeyInfo = concatToArrayNow<dpadKeyInfo, optionKeyInfo, faceKeyInfo, turboFaceKeyInfo>;

std::span<const KeyCategory> AppMeta::keyCategories()
{
	static constexpr std::array categories
	{
		KeyCategory{"Gamepad", gpKeyInfo},
	};
	return categories;
}

std::string_view AppMeta::systemKeyCodeToString(KeyCode c)
{
	switch(NgpKey(c))
	{
		case NgpKey::Up: return "Up";
		case NgpKey::Right: return "Right";
		case NgpKey::Down: return "Down";
		case NgpKey::Left: return "Left";
		case NgpKey::Option: return "Option";
		case NgpKey::A: return "A";
		case NgpKey::B: return "B";
		default: return "";
	}
}

std::span<const KeyConfigDesc> AppMeta::defaultKeyConfigs()
{
	using namespace Input;

	static constexpr std::array pcKeyboardMap
	{
		KeyMapping{NgpKey::Up, Keycode::UP},
		KeyMapping{NgpKey::Right, Keycode::RIGHT},
		KeyMapping{NgpKey::Down, Keycode::DOWN},
		KeyMapping{NgpKey::Left, Keycode::LEFT},
		KeyMapping{NgpKey::Option, Keycode::ENTER},
		KeyMapping{NgpKey::B, Keycode::Z},
		KeyMapping{NgpKey::A, Keycode::X},
	};

	static constexpr std::array genericGamepadMap
	{
		KeyMapping{NgpKey::Up, Keycode::UP},
		KeyMapping{NgpKey::Right, Keycode::RIGHT},
		KeyMapping{NgpKey::Down, Keycode::DOWN},
		KeyMapping{NgpKey::Left, Keycode::LEFT},
		KeyMapping{NgpKey::Option, Keycode::GAME_START},
		KeyMapping{NgpKey::B, Keycode::GAME_X},
		KeyMapping{NgpKey::A, Keycode::GAME_A},
	};

	static constexpr std::array wiimoteMap
	{
		KeyMapping{NgpKey::Up, WiimoteKey::UP},
		KeyMapping{NgpKey::Right, WiimoteKey::RIGHT},
		KeyMapping{NgpKey::Down, WiimoteKey::DOWN},
		KeyMapping{NgpKey::Left, WiimoteKey::LEFT},
		KeyMapping{NgpKey::B, WiimoteKey::_1},
		KeyMapping{NgpKey::A, WiimoteKey::_2},
		KeyMapping{NgpKey::Option, WiimoteKey::PLUS},
	};

	return genericKeyConfigs<pcKeyboardMap, genericGamepadMap, wiimoteMap>();
}

bool AppMeta::allowsTurboModifier(KeyCode c)
{
	switch(NgpKey(c))
	{
		case NgpKey::A ... NgpKey::B:
			return true;
		default: return false;
	}
}

constexpr FRect gpImageCoords(IRect cellRelBounds)
{
	constexpr F2Size imageSize{256, 128};
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
		option{AssetFileID::gamepadOverlay, gpImageCoords({{4, 2}, {2, 1}}), {1, 2}},

		blank{AssetFileID::gamepadOverlay, gpImageCoords({{6, 2}, {2, 2}})};
	} virtualControllerAssets;

	if(key[0] == 0)
		return virtualControllerAssets.dpad;
	switch(NgpKey(key[0]))
	{
		case NgpKey::A: return virtualControllerAssets.a;
		case NgpKey::B: return virtualControllerAssets.b;
		case NgpKey::Option: return virtualControllerAssets.option;
		default: return virtualControllerAssets.blank;
	}
}

SystemInputDeviceDesc AppMeta::inputDeviceDesc(int idx)
{
	static constexpr std::array gamepadComponents
	{
		InputComponentDesc{"D-Pad", dpadKeyInfo, InputComponent::dPad, LB2DO},
		InputComponentDesc{"Face Buttons", faceKeyInfo, InputComponent::button, RB2DO},
		InputComponentDesc{"Option", optionKeyInfo, InputComponent::button, RB2DO},
	};
	static constexpr SystemInputDeviceDesc gamepadDesc{"Gamepad", gamepadComponents};
	return gamepadDesc;
}

void AppMeta::onCustomizeNavView(AppNavView& view)
{
	const Gfx::LGradientStopDesc navViewGrad[] =
	{
		{ .0, Gfx::PackedColor::format.build((101./255.) * .4, (45./255.) * .4, (193./255.) * .4, 1.) },
		{ .3, Gfx::PackedColor::format.build((101./255.) * .4, (45./255.) * .4, (193./255.) * .4, 1.) },
		{ .97, Gfx::PackedColor::format.build((34./255.) * .4, (15./255.) * .4, (64./255.) * .4, 1.) },
		{ 1., view.separatorColor() },
	};
	view.setBackgroundGradient(navViewGrad);
}

}
