// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#pragma once

#include "es-launcher/Generator.h"
#include "util/IniFile.h"

#include <filesystem>

namespace es::launcher::fritchii
{
	// Configures Fritchii (Nintendo Switch 2, x86_64 only). Registered as "fritchii".
	//
	// XDG_CONFIG_HOME is ctx.paths.configFor("fritchii"), so its settings are fritchii/qt-config.ini
	// there; XDG_DATA_HOME is <saves>/switch2, so the emulated NAND (saves, installed updates
	// and DLC) and SD card are in <saves>/switch2/fritchii. Its keys/ folder links to the switch/
	// folder of the BIOS, which must hold prod.keys (title.keys is optional); the Switch
	// firmware is installed from Fritchii's own menu, into the NAND.
	//
	// Pads are bound by SDL joystick inputs, as ES mapped them, for players 1 to 8. Hotkeys are
	// Fritchii's own controller shortcuts, set to the same chords as RetroArch: hotkey + Start
	// quits, hotkey + B pauses, hotkey + A restarts, hotkey + L1 takes a screenshot,
	// hotkey + right toggles the turbo speed.
	//
	// Options: fritchii_backend (0 OpenGL, 1 Vulkan), fritchii_resolution (Fritchii's resolution_setup index,
	// 3 = native), fritchii_vsync, fritchii_ratio, fritchii_antialiasing, fritchii_scaling, fritchii_docked,
	// fritchii_accuracy.
	class FritchiiGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;

		// Public for the tests
		static std::filesystem::path settingsPath(const LaunchContext& ctx);
		static void writeSettings(const LaunchContext& ctx, util::IniFile& ini);

		// Fritchii's GUID of a pad: SDL's, without the CRC of the name (bytes 2 and 3)
		static std::string fritchiiGuid(const std::string& sdlGuid);
	};
}
