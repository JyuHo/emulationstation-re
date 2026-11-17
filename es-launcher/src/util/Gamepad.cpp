// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.devices.controller), Copyright (c) 2026 The Batocera Team.

#include "util/Gamepad.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

namespace es::launcher::util
{
	namespace
	{
		// ES input -> SDL_GAMECONTROLLERCONFIG name (_DEFAULT_SDL_MAPPING in batocera-launch)
		constexpr std::pair<const char*, const char*> sdlNames[] = {
			{ "b", "a" },
			{ "a", "b" },
			{ "x", "y" },
			{ "y", "x" },
			{ "l2", "lefttrigger" },
			{ "r2", "righttrigger" },
			{ "l3", "leftstick" },
			{ "r3", "rightstick" },
			{ "pageup", "leftshoulder" },
			{ "pagedown", "rightshoulder" },
			{ "start", "start" },
			{ "select", "back" },
			{ "up", "dpup" },
			{ "down", "dpdown" },
			{ "left", "dpleft" },
			{ "right", "dpright" },
			{ "joystick1up", "lefty" },
			{ "joystick1left", "leftx" },
			{ "joystick2up", "righty" },
			{ "joystick2left", "rightx" },
			{ "hotkey", "guide" },
		};

		// SDL's per-family hints default to SDL_JOYSTICK_HIDAPI, but a hint set with SDL_SetHint()
		// wins over the global one: Cemu sets SDL_JOYSTICK_HIDAPI_PS4 and SDL_JOYSTICK_HIDAPI_PS5
		// before SDL_Init, so it still gets the HIDAPI GUID of the pads when only
		// SDL_JOYSTICK_HIDAPI is disabled. SDL_SetHint() cannot override an environment variable,
		// so each family is disabled in the environment too.
		constexpr const char* hidapiFamilies[] = {
			"SDL_JOYSTICK_HIDAPI_8BITDO",
			"SDL_JOYSTICK_HIDAPI_FLYDIGI",
			"SDL_JOYSTICK_HIDAPI_GAMECUBE",
			"SDL_JOYSTICK_HIDAPI_GIP",
			"SDL_JOYSTICK_HIDAPI_JOY_CONS",
			"SDL_JOYSTICK_HIDAPI_LG4FF",
			"SDL_JOYSTICK_HIDAPI_LUNA",
			"SDL_JOYSTICK_HIDAPI_NINTENDO_CLASSIC",
			"SDL_JOYSTICK_HIDAPI_PS3",
			"SDL_JOYSTICK_HIDAPI_PS4",
			"SDL_JOYSTICK_HIDAPI_PS5",
			"SDL_JOYSTICK_HIDAPI_SHIELD",
			"SDL_JOYSTICK_HIDAPI_SINPUT",
			"SDL_JOYSTICK_HIDAPI_STADIA",
			"SDL_JOYSTICK_HIDAPI_STEAM",
			"SDL_JOYSTICK_HIDAPI_STEAM_HORI",
			"SDL_JOYSTICK_HIDAPI_STEAMDECK",
			"SDL_JOYSTICK_HIDAPI_SWITCH",
			"SDL_JOYSTICK_HIDAPI_SWITCH2",
			"SDL_JOYSTICK_HIDAPI_WII",
			"SDL_JOYSTICK_HIDAPI_XBOX",
			"SDL_JOYSTICK_HIDAPI_XBOX_360",
			"SDL_JOYSTICK_HIDAPI_XBOX_360_WIRELESS",
			"SDL_JOYSTICK_HIDAPI_XBOX_ONE",
			"SDL_JOYSTICK_HIDAPI_ZUIKI",
		};

		constexpr std::pair<const char*, PadControl> controls[] = {
			{ "b", PadControl::South },
			{ "a", PadControl::East },
			{ "y", PadControl::West },
			{ "x", PadControl::North },
			{ "select", PadControl::Back },
			{ "start", PadControl::Start },
			{ "l3", PadControl::LeftStick },
			{ "r3", PadControl::RightStick },
			{ "pageup", PadControl::LeftShoulder },
			{ "pagedown", PadControl::RightShoulder },
			{ "up", PadControl::DPadUp },
			{ "down", PadControl::DPadDown },
			{ "left", PadControl::DPadLeft },
			{ "right", PadControl::DPadRight },
			{ "l2", PadControl::LeftTrigger },
			{ "r2", PadControl::RightTrigger },
		};

		bool sameInput(const InputBinding& a, const InputBinding& b)
		{
			return a.type == b.type && a.id == b.id && (a.type == InputBinding::Type::Button || a.value == b.value);
		}

		std::optional<std::string> sdlBinding(std::string_view esInput, std::string_view sdlName, const InputBinding& input)
		{
			std::string id = std::to_string(input.id);

			switch (input.type)
			{
			case InputBinding::Type::Button:
				return std::string(sdlName) + ":b" + id;

			case InputBinding::Type::Hat:
				return std::string(sdlName) + ":h" + id + "." + std::to_string(input.value);

			case InputBinding::Type::Axis:
				// ES saves the up and left directions of a stick: an axis that grows the other
				// way is inverted
				if (esInput.starts_with("joystick"))
					return std::string(sdlName) + ":a" + id + (input.value > 0 ? "~" : "");
				if (sdlName.starts_with("dp"))
					return std::string(sdlName) + ":" + (input.value < 0 ? "-" : "+") + "a" + id;
				if (sdlName.ends_with("trigger"))
					return std::string(sdlName) + ":a" + id + (input.value < 0 ? "~" : "");
				return std::string(sdlName) + ":a" + id;

			case InputBinding::Type::Key:
				break;
			}

			return std::nullopt;
		}
	}

	std::optional<PadControl> padControl(std::string_view esInput)
	{
		for (const auto& [name, control] : controls)
			if (esInput == name)
				return control;
		return std::nullopt;
	}

	PadControl hotkeyControl(const Controller& pad)
	{
		const InputBinding* hotkey = pad.find("hotkey");
		if (hotkey == nullptr)
			return PadControl::Guide;

		for (const auto& [name, control] : controls)
		{
			const InputBinding* other = pad.find(name);
			if (other != nullptr && sameInput(*hotkey, *other))
				return control;
		}

		return PadControl::Guide;
	}

	std::string gameControllerConfig(const std::vector<Controller>& controllers)
	{
		std::string result;

		for (const Controller& pad : controllers)
		{
			if (pad.guid.empty())
				continue;

			std::string name = pad.name;
			std::ranges::replace(name, ',', '.');

			std::string line = pad.guid + "," + name + ",platform:Linux,";

			// "hotkey" often shares a button with another input, and a button mapped twice
			// confuses SDL: it only becomes "guide" when it is a button of its own
			const InputBinding* hotkey = pad.find("hotkey");
			bool hotkeyShared = hotkey != nullptr && hotkeyControl(pad) != PadControl::Guide;

			for (const auto& [esInput, sdlName] : sdlNames)
			{
				const InputBinding* input = pad.find(esInput);
				if (input == nullptr || (input == hotkey && hotkeyShared))
					continue;

				if (auto binding = sdlBinding(esInput, sdlName, *input))
					line += *binding + ",";
			}

			if (!result.empty())
				result += "\n";
			result += line;
		}

		return result;
	}

	std::string guidWithoutCrc(std::string_view guid)
	{
		std::string result(guid);
		std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (result.size() == 32)
			result.replace(4, 4, "0000");
		return result;
	}

	void setPadEnvironment(Command& cmd, const std::vector<Controller>& controllers)
	{
		if (const char* hidapi = std::getenv("SDL_JOYSTICK_HIDAPI"); hidapi == nullptr || hidapi[0] == 0)
		{
			cmd.env["SDL_JOYSTICK_HIDAPI"] = "0";
			for (const char* family : hidapiFamilies)
			{
				const char* value = std::getenv(family);
				if (value == nullptr || value[0] == 0)
					cmd.env[family] = "0";
			}
		}
		if (const char* hats = std::getenv("SDL_JOYSTICK_LINUX_DIGITAL_HATS"); hats == nullptr || hats[0] == 0)
			cmd.env["SDL_JOYSTICK_LINUX_DIGITAL_HATS"] = "1";
		cmd.env["SDL_GAMECONTROLLERCONFIG"] = gameControllerConfig(controllers);
	}

	const Controller* firstPlayer(const LaunchContext& ctx)
	{
		auto it = std::ranges::min_element(ctx.controllers, {}, &Controller::player);
		return it == ctx.controllers.end() ? nullptr : &*it;
	}
}
