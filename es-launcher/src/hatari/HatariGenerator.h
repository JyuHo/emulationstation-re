// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.hatari), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace es::launcher::hatari
{
	// Configures Hatari (Atari ST, STE, Mega STE, TT and Falcon). Registered as "hatari".
	//
	// Hatari is given its own hatari.cfg, in its folder below the emulators folder, and the TOS
	// of the machine from the BIOS folder: tos<version><language>.img (tos104fr.img) or EmuTOS
	// (etos256us.img...). A .hd file is a hard disk image, a .gemdos file a folder used as the
	// hard disk; anything else goes in floppy drive A.
	//
	// Options: model (520st_auto, 1040ste_162, falcon_etos512...: machine and TOS), language
	// (TOS language: us, uk, de, fr...), ram (ST RAM, in MB; 0 for 512 KB), hatari_drive (IDE,
	// ACSI: how a .hd image is plugged).
	class HatariGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		// "1040ste_162" -> { "ste", "162" }; anything else -> { "st", "auto" }
		static std::pair<std::string, std::string> model(std::string_view setting);

		// The best TOS image for the machine in biosDir: the version and language asked for,
		// then the others in Batocera's order of preference.
		static std::optional<std::filesystem::path> findTos(const std::filesystem::path& biosDir, std::string_view machine,
			std::string_view version, std::string_view language);
	};
}
