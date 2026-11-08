// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.devices.controller), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Command.h"
#include "es-launcher/LaunchContext.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace es::launcher::util
{
	// The controls of an SDL gamepad. The standalone emulators read pads through SDL's gamepad
	// API: SDL_GAMECONTROLLERCONFIG (gameControllerConfig()) tells it how ES mapped each pad, so
	// a generator only has to bind these names, whatever the pad.
	enum class PadControl
	{
		South,          // ES "b"
		East,           // ES "a"
		West,           // ES "y"
		North,          // ES "x"
		Back,           // ES "select"
		Guide,          // ES "hotkey", when it is a button of its own
		Start,
		LeftStick,      // ES "l3"
		RightStick,     // ES "r3"
		LeftShoulder,   // ES "pageup"
		RightShoulder,  // ES "pagedown"
		DPadUp,
		DPadDown,
		DPadLeft,
		DPadRight,
		LeftTrigger,    // ES "l2"
		RightTrigger,   // ES "r2"
	};

	// The gamepad control an ES input is mapped to, as batocera-launch maps them. Sticks are not
	// controls (they are axes); "hotkey" is not either: see hotkeyControl().
	std::optional<PadControl> padControl(std::string_view esInput);

	// The control the player holds as the hotkey: the one it shares with another input (often
	// select), else Guide.
	PadControl hotkeyControl(const Controller& pad);

	// One SDL_GAMECONTROLLERCONFIG line per pad ("guid,name,platform:Linux,a:b0,..."), from the
	// mapping the player made in ES. Keyboard inputs are left out.
	std::string gameControllerConfig(const std::vector<Controller>& controllers);

	// A pad GUID without the CRC of its name (characters 4 to 7), lower case. SDL computes the CRC
	// from the name of the pad, which is not the same for every SDL backend and version: lists of
	// known pads and emulators that ignore it compare GUIDs this way.
	std::string guidWithoutCrc(std::string_view guid);

	// The environment that makes the SDL of an emulator (SDL2 or SDL3) see the pads as ES does:
	// - SDL_JOYSTICK_HIDAPI=0: ES reads the pads through Linux evdev, as RetroArch's udev driver.
	//   SDL's HIDAPI drivers (PlayStation, Switch, Xbox... pads) give them other GUIDs, buttons and
	//   axes, and SDL2 and SDL3 do not drive the same pads through HIDAPI. Through evdev, SDL2 and
	//   SDL3 give a pad the same GUID and the same numbers. A SDL_JOYSTICK_HIDAPI set in the
	//   environment of ES wins, as it does in ES.
	// - SDL_JOYSTICK_LINUX_DIGITAL_HATS=1: every hat is a hat, as in ES and RetroArch (SDL would
	//   otherwise guess, and could turn a d-pad into two axes).
	// - SDL_GAMECONTROLLERCONFIG: the mapping of each pad in ES (gameControllerConfig()).
	void setPadEnvironment(Command& cmd, const std::vector<Controller>& controllers);

	// The emulator actions ES binds to the hotkey + another control of player 1, the same as for
	// RetroArch.
	enum class HotkeyAction
	{
		Exit,
		Menu,
		Reset,
		LoadState,
		SaveState,
		NextSlot,
		PreviousSlot,
		Rewind,
		FastForward,
		Screenshot,
	};

	struct Hotkey
	{
		HotkeyAction action;
		PadControl control;   // pressed together with the hotkey
	};

	inline constexpr Hotkey hotkeys[] = {
		{ HotkeyAction::Exit, PadControl::Start },
		{ HotkeyAction::Menu, PadControl::South },
		{ HotkeyAction::Reset, PadControl::East },
		{ HotkeyAction::LoadState, PadControl::North },
		{ HotkeyAction::SaveState, PadControl::West },
		{ HotkeyAction::NextSlot, PadControl::DPadUp },
		{ HotkeyAction::PreviousSlot, PadControl::DPadDown },
		{ HotkeyAction::Rewind, PadControl::DPadLeft },
		{ HotkeyAction::FastForward, PadControl::DPadRight },
		{ HotkeyAction::Screenshot, PadControl::LeftShoulder },
	};

	// Player 1's pad, or nothing when no pad is assigned.
	const Controller* firstPlayer(const LaunchContext& ctx);
}
