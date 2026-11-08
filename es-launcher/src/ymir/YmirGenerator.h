// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.ymir), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"
#include "util/IniFile.h"

#include <filesystem>
#include <string>

namespace es::launcher::ymir
{
	// Configures Ymir (Sega Saturn). Registered as "ymir".
	//
	// Its settings are Ymir.toml in ctx.paths.configFor("ymir"), given with -p. Backup memory,
	// save states and dumps go to <saves>/saturn/ymir; the Saturn IPL ROMs are looked up in the
	// BIOS folder.
	//
	// Players 1 and 2 are bound as SDL gamepads ("GamepadA@<index>"), with Ymir's stock layout for
	// each peripheral, plus its keyboard keys. Options are the ones of batocera-launch
	// ("ymir_aspect", "ymir_peripheral_p1"...).
	class YmirGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		static std::filesystem::path settingsPath(const LaunchContext& ctx);

		// Public for the tests
		static void writeSettings(const LaunchContext& ctx, util::IniFile& toml);

		// "1.7777" -> "16:9": Ymir's aspect ratio from batocera-launch's decimal value
		static std::string aspectRatio(std::string_view value);
	};
}
