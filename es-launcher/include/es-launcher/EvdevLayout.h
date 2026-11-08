// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#pragma once

#include "es-launcher/Export.h"
#include "es-launcher/LaunchContext.h"

#include <optional>
#include <string>
#include <vector>

namespace es::launcher
{
	// How the controls of a Linux evdev pad are numbered by SDL3 (what ES records) and by
	// RetroArch's udev driver (what RetroArch reads, and what Batocera's es_input.cfg holds: Batocera
	// patches its SDL to number them that way).
	//
	// SDL3: buttons BTN_JOYSTICK..KEY_MAX then 0..BTN_JOYSTICK; axes 0..ABS_MAX without the hats;
	//       hats numbered in order among the hats the pad has.
	// udev: buttons KEY_UP..KEY_DOWN, BTN_MISC..KEY_MAX, 0..KEY_UP, then up to BTN_MISC (64 at
	//       most); axes 0..ABS_MISC with a range, without the hats; hat N is ABS_HAT<N>.
	// Most pads only have buttons from BTN_JOYSTICK on and both agree; the others do not.
	class ES_LAUNCHER_API EvdevLayout
	{
	public:
		struct Abs
		{
			int code = 0;
			int minimum = 0;
			int maximum = 0;
			int fuzz = 0;
			int flat = 0;
			int resolution = 0;
		};

		// The layout of /dev/input/eventN, or nothing when it cannot be read.
		static std::optional<EvdevLayout> read(const std::string& devicePath);

		// From the capabilities of a device. digitalHats: SDL_HINT_JOYSTICK_LINUX_DIGITAL_HATS,
		// which ES and es-launcher set: every hat is a hat; otherwise SDL guesses as it does.
		static EvdevLayout fromCapabilities(std::string name, std::vector<int> keys, std::vector<Abs> abs, bool digitalHats = true);

		const std::string& name() const { return mName; }

		// The same control in the other numbering; nothing when the pad has no such control.
		// Keys are returned unchanged.
		std::optional<InputBinding> sdlToUdev(const InputBinding& input) const;
		std::optional<InputBinding> udevToSdl(const InputBinding& input) const;

		// Every input of the pad in the other numbering; inputs the pad does not have are dropped.
		Controller sdlToUdev(const Controller& pad) const;

		// Whether both numberings are the same for this pad.
		bool sameNumbering() const;

	private:
		std::string mName;
		std::vector<int> mSdlButtons;   // SDL index -> evdev code
		std::vector<int> mUdevButtons;  // udev index -> evdev code
		std::vector<int> mSdlAxes;
		std::vector<int> mUdevAxes;
		std::vector<int> mSdlHats;      // SDL hat index -> ABS_HAT pair (0 to 3)
	};

	// Whether SDL treats every hat as digital (SDL_JOYSTICK_LINUX_DIGITAL_HATS, "1" unless the
	// environment says otherwise: ES and es-launcher set it).
	ES_LAUNCHER_API bool sdlDigitalHats();
}
