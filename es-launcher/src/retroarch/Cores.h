// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch_libretro.core and cores/), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/LaunchContext.h"
#include "retroarch/ConfigFile.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace es::launcher::retroarch
{
	// ES input name -> RetroArch button name ("pageup" -> "l")
	using ButtonMap = std::vector<std::pair<std::string, std::string>>;

	// What a libretro core changes in the RetroArch configuration. The default behavior suits any
	// core, so a core with no profile still runs; a profile adds the core options and fixes the
	// player expects for that core.
	class Core
	{
	public:
		virtual ~Core() = default;

		// Core options (retroarch-core-options.cfg)
		virtual void setCoreOptions(const LaunchContext& /*ctx*/, ConfigFile& /*options*/) const { }

		// Extra retroarch.cfg settings, written after the generic ones
		virtual void setConfig(const LaunchContext& /*ctx*/, ConfigFile& /*config*/) const { }

		// Lets a core move buttons around for one controller (N64 Z trigger, ...)
		virtual void adjustButtons(const LaunchContext& /*ctx*/, const Controller& /*pad*/, ButtonMap& /*buttons*/) const { }

		// input_playerN_analog_dpad_mode: 1 makes the left stick drive the d-pad
		virtual int analogDpadMode(const LaunchContext& ctx, const Controller& pad) const;

		// The video driver when the player chose none: generic is the one for any core
		virtual std::string defaultVideoDriver(const std::string& generic) const { return generic; }

		// false when rewinding and run-ahead break the core for this system
		virtual bool supportsRewindAndRunAhead(const LaunchContext& /*ctx*/) const { return true; }

		// The file RetroArch is given: the game, or a file the core reads in its place
		virtual std::filesystem::path romArgument(const LaunchContext& ctx) const { return ctx.game.rom; }

		// Arguments of RetroArch before the game ("--subsystem neocd")
		virtual std::vector<std::string> commandArguments(const LaunchContext& /*ctx*/) const { return {}; }

		// input_libretro_device_pN, when the core needs another device than the RetroPad
		virtual std::optional<std::string> deviceType(const LaunchContext& /*ctx*/, int /*player*/) const { return std::nullopt; }
	};

	// The profile for a core name as written in es_systems.cfg ("snes9x", "mupen64plus-next"...)
	const Core& coreProfile(std::string_view core);

	// "mupen64plus-next" -> "mupen64plus_next_libretro.so"
	std::string coreLibraryName(std::string_view core);

	// value of settingKey, else fallback
	std::string settingOr(const LaunchContext& ctx, std::string_view settingKey, std::string_view fallback);
}
