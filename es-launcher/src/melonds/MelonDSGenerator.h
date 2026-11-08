// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.melonds), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"
#include "util/IniFile.h"

#include <filesystem>

namespace es::launcher::melonds
{
	// Configures melonDS (Nintendo DS). Registered as "melonds".
	//
	// Its settings are $XDG_CONFIG_HOME/melonDS/melonDS.toml, with XDG_CONFIG_HOME set to
	// ctx.paths.configFor("melonds"). Saves, save states and the DLDI/DSi SD card images go to
	// <saves>/nds. The DS BIOS and firmware (bios7.bin, bios9.bin, firmware.bin) are used when
	// they are all at the root of the BIOS folder, otherwise melonDS runs its own free BIOS.
	//
	// Player 1's pad is bound button by button, from the mapping made in ES. Options are the
	// ones of batocera-launch ("melonds_renderer", "melonds_layout"...).
	class MelonDSGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		static std::filesystem::path settingsPath(const LaunchContext& ctx);

		// Public for the tests
		static void writeSettings(const LaunchContext& ctx, util::IniFile& toml);

		// melonDS's number for an ES input: the button, 0x100 | hat << 4 | direction for a hat,
		// -1 when it cannot be bound
		static int joystickValue(const InputBinding& input);
	};
}
