// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.shadps4), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"

#include <filesystem>

namespace es::launcher::shadps4
{
	// Configures shadPS4 (PlayStation 4, x86_64, Vulkan). Registered as "shadps4".
	//
	// shadPS4 keeps its settings in shadPS4/config.json below its XDG_DATA_HOME, which ES points
	// to the emulator's folder below the emulators folder. The games are their dumped folders:
	// the ROM is that folder (or a file in it, such as "Game.ps4"), started through its
	// eboot.bin, or a .zar archive.
	//
	// Options: shadps4_fsr, shadps4_rcas, shadps4_hdr, shadps4_present_mode (Mailbox, Fifo,
	// Immediate), shadps4_vblank_freq, shadps4_show_fps, shadps4_neo_mode, shadps4_console_lang,
	// shadps4_copy_gpu_buffers, shadps4_readbacks_mode, shadps4_dma, shadps4_pipeline_cache,
	// shadps4_logging.
	class ShadPS4Generator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		// What shadPS4 is given to start the game: the .zar, or the eboot.bin of the game folder
		static std::filesystem::path gameFile(const std::filesystem::path& rom);

		// config.json, with ES's settings set in it. text is the file as it is, maybe empty.
		static std::string writeSettings(const LaunchContext& ctx, const std::string& text);
	};
}
