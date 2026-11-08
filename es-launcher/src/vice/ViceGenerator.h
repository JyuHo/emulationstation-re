// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.vice), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"

#include <string>

namespace es::launcher::vice
{
	// Configures VICE (Commodore 64, 128, VIC-20, Plus/4, PET). Registered as "vice".
	//
	// The core is VICE's program for the machine: x64sc, x64, xscpu64, x64dtv, x128, xvic,
	// xplus4, xpet; without one, the system chooses (c64 -> x64sc, c128 -> x128...). VICE is
	// given its own vicerc and joystick map, in its folder below the emulators folder: player 1
	// is on joystick port 2, the one most games read (port 1 on the VIC-20), player 2 on port 1.
	//
	// Options: noborder (hide the borders), and vice.<section>.<resource> for any resource of
	// the vicerc ("vice.C64.SidModel").
	class ViceGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		// The program of VICE for the game: the core, else the one of the system
		static std::string machine(const LaunchContext& ctx);

		// The joystick map: each pad's controls to the joystick lines, select to VICE's menu
		static std::string joymap(const LaunchContext& ctx);
	};
}
