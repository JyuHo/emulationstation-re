// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.openmsx), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"

#include <filesystem>
#include <string>
#include <vector>

namespace es::launcher::openmsx
{
	// Configures openMSX (MSX, MSX2, MSX2+, turboR, ColecoVision, Spectravideo). Registered as
	// "openmsx".
	//
	// openMSX keeps its own settings (OPENMSX_HOME is its folder below the emulators folder); ES
	// gives it a startup script: the system ROMs folders of the BIOS folder (Machines/, openmsx/),
	// the pads, loading at full speed. The machine follows the system (Boosted_MSX2_EN...), the
	// media the extension: .rom cartridge, .dsk disk, .cas cassette, .ogv laserdisc, inside a .zip
	// too; a .openmsx file names two cartridges or disks, one per line.
	//
	// Options: openmsx_loading (true: full speed while loading), openmsx_disk (disk, hda: how a
	// .dsk image is plugged).
	class OpenMSXGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		// The arguments after the program: machine and media
		static std::vector<std::string> mediaArguments(const LaunchContext& ctx);

		// The startup script
		static std::string script(const LaunchContext& ctx);

		// The extension (".dsk") of the first media file in a zip archive, if it has one
		static std::string zipMediaExtension(const std::filesystem::path& zip);
	};
}
