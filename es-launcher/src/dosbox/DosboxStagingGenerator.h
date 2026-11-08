// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.dosbox_staging), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"

#include <string>
#include <vector>

namespace es::launcher::dosbox
{
	// Starts DOSBox Staging (DOS). Registered as "dosbox_staging".
	//
	// The game is a folder (or a file in one): DOSBox runs in it, full screen. Its dosbox.cfg or
	// dosbox.conf is loaded and its dosbox.bat run, then DOSBox quits with the game; a folder
	// without them opens a C:\> prompt on the folder. The batch files can read WORKDIR, RESDIR
	// (ctx.paths.configFor("dosbox"), for shared resources), RESCONF (its dosbox-staging.conf),
	// GAMECFG, GAMECONF and GAMEBAT, as with batocera-launch.
	class DosboxStagingGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		// Public for the tests: the arguments after the program
		static std::vector<std::string> arguments(const LaunchContext& ctx);
	};
}
