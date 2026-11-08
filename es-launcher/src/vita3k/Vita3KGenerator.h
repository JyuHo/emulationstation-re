// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.vita3k), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"

#include <filesystem>
#include <string>

namespace es::launcher::vita3k
{
	// Configures Vita3K (PlayStation Vita). Registered as "vita3k".
	//
	// Vita3K keeps its settings in config.yml, in its folder below the emulators folder, and the
	// Vita's own storage (ux0: the installed games, their saves) in the psvita saves folder.
	// A game is a file whose name holds its title id in brackets ("Persona 4 [PCSG00004].zip"):
	// once installed (ux0/app/PCSG00004), it starts straight away; before, Vita3K installs it.
	//
	// Options: vita3k_gfxbackend (OpenGL, Vulkan), vita3k_resolution (render scale, 0.5 to 8),
	// vita3k_vsync, vita3k_anisotropic, vita3k_filter, vita3k_surface, vita3k_sync,
	// vita3k_hd_pixel, vita3k_accuracy, vita3k_texture, vita3k_shader, vita3k_mapping,
	// vita3k_system_language.
	class Vita3KGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		// "Persona 4 Golden [PCSG00004]" -> "PCSG00004": what is inside the outermost brackets
		static std::string titleId(const std::filesystem::path& rom);
	};
}
