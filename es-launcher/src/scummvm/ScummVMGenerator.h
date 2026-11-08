// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.scummvm), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"

#include <string>
#include <vector>

namespace es::launcher::scummvm
{
	// Starts ScummVM. Registered as "scummvm".
	//
	// The game is a <game>.scummvm file (holding the ScummVM game id, or empty: the id is the file
	// name) next to the game's files, or a folder holding one; without a valid id, ScummVM detects
	// the game. Its settings are scummvm.ini in ctx.paths.configFor("scummvm") (--config), saves go
	// to <saves>/scummvm, and the extra files to <bios>/scummvm/extra. Options are the ones of
	// batocera-launch ("scumm_scale", "scumm_stretch"...).
	class ScummVMGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		// Public for the tests: the arguments after the program
		static std::vector<std::string> arguments(const LaunchContext& ctx);
	};
}
