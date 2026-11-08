// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch_mupen64plus), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"
#include "util/IniFile.h"

#include <filesystem>
#include <map>
#include <string>

namespace es::launcher::mupen64plus
{
	// Configures Mupen64Plus (Nintendo 64), its console front-end. Registered as "mupen64plus".
	//
	// The core picks the video plugin: "gliden64", "glide64mk2" or "rice" (Batocera's names),
	// passed with --gfx; without one, Mupen64Plus uses its own default. Its settings are
	// mupen64plus.cfg in ctx.paths.configFor("mupen64plus"), given as --configdir and --datadir.
	// Saves and save states go to <saves>/n64, the 64DD IPL is 64DD_IPL.bin in the BIOS folder.
	//
	// Each pad is bound input by input in [Input-SDL-ControlN], from the mapping made in ES, the
	// way batocera-launch does (its input.xml layouts). Options are the ones of batocera-launch
	// ("mupen64plus_ratio", "mupen64-controller1"...); mupen64plus.<section>.<key> sets any key.
	class Mupen64PlusGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		static std::filesystem::path settingsPath(const LaunchContext& ctx);

		// Public for the tests
		static void writeSettings(const LaunchContext& ctx, util::IniFile& ini);

		// The [Input-SDL-ControlN] keys of one pad: "A Button" -> "button(1)"...
		static std::map<std::string, std::string> padBindings(const LaunchContext& ctx, const Controller& pad);
	};
}
