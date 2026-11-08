// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.supermodel), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"
#include "util/IniFile.h"

#include <filesystem>
#include <string>
#include <vector>

namespace es::launcher::supermodel
{
	// Configures Supermodel (Sega Model 3). Registered as "supermodel".
	//
	// Supermodel reads its files from the Config, NVRAM, Saves... folders of its working directory
	// when a Config folder is there: it runs in ctx.paths.configFor("supermodel"), whose Config
	// folder holds Supermodel.ini and Games.xml. Games.xml (the list of the romsets) comes from
	// the Supermodel install, copied when it is missing or older. NVRAM and Saves are links to
	// <saves>/model3, which the ini also names (NVRAMPath, SavesPath) for the builds that read
	// those keys.
	//
	// Players 1 and 2 are bound in the [Global] section as SDL gamepads (InputSystem =
	// sdlgamepad, "JOY1_BUTTON1"...) next to Supermodel's keyboard keys, as batocera-launch does.
	// Light guns and wheels are left out. Options are the ones of batocera-launch ("m3_wideScreen",
	// "ppcFreq"...).
	class SupermodelGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		// Public for the tests
		static std::vector<std::string> arguments(const LaunchContext& ctx);
		static void writeSettings(const LaunchContext& ctx, util::IniFile& ini);
	};
}
