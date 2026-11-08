// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.redream), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"

#include <string>
#include <vector>

namespace es::launcher::redream
{
	// Configures Redream (Dreamcast, x86_64). Registered as "redream".
	//
	// Redream reads redream.cfg (and the player's redream.key, for its premium version) next to
	// its program: ES copies the program into the emulator's folder below the emulators folder
	// and writes the configuration there. The lines ES does not set are kept.
	//
	// Options: redreamResolution (1 to 6, premium only), redreamRatio (4:3, 16:9, stretch),
	// redreamFrameSkip, redreamVsync, redreamRender (gl_hle_perstrip, gl_hle_perpixel, gl_lle),
	// redreamRegion, redreamLanguage, redreamBroadcast, redreamCable.
	class RedreamGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		// The lines of redream.cfg ES sets: "key=value"
		static std::vector<std::string> settingLines(const LaunchContext& ctx);

		// The pad profile of a controller: "profile0=name:<guid>,type:controller,...,exit:f10"
		static std::string padProfile(const Controller& pad, int port);
	};
}
