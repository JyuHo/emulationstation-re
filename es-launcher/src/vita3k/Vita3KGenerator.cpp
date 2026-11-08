// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.vita3k), Copyright (c) 2026 The Batocera Team.

#include "vita3k/Vita3KGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Files.h"
#include "util/Gamepad.h"
#include "util/IniFile.h"
#include "util/YamlFile.h"

#include <cstdlib>
#include <system_error>

namespace es::launcher::vita3k
{
	namespace fs = std::filesystem;
	using util::YamlFile;

	ES_LAUNCHER_REGISTER_GENERATOR("vita3k", Vita3KGenerator)

	std::string Vita3KGenerator::titleId(const fs::path& rom)
	{
		std::string stem = rom.stem().string();
		size_t begin = stem.find('[');
		size_t end = stem.rfind(']');
		if (begin == std::string::npos || end == std::string::npos || end <= begin)
			return {};
		return stem.substr(begin + 1, end - begin - 1);
	}

	namespace
	{
		void writeSettings(const LaunchContext& ctx, const fs::path& storage, YamlFile& yaml)
		{
			const Settings& s = ctx.settings;

			yaml.set({ "pref-path" }, storage.string());
			yaml.set({ "initial-setup" }, false);
			yaml.set({ "boot-apps-full-screen" }, true);
			yaml.set({ "validation-layer" }, false);
			yaml.set({ "discord-rich-presence" }, false);
			yaml.set({ "show-welcome" }, false);
			yaml.setRaw({ "check-for-updates-mode" }, "0");
			yaml.setRaw({ "log-level" }, "6");   // none

			// Vita3K picks its renderer itself until the player chooses one
			std::string backend = s.getString("vita3k_gfxbackend");
			if (backend == "OpenGL" || backend == "Vulkan")
				yaml.set({ "backend-renderer" }, backend);

			// a number: 1, 1.5...
			std::string scale = s.getString("vita3k_resolution", "1");
			if (scale.empty() || scale.find_first_not_of("0123456789.") != std::string::npos)
				scale = "1";
			yaml.setRaw({ "resolution-multiplier" }, scale.front() == '.' ? "0" + scale : scale);
			yaml.set({ "v-sync" }, s.getBool("vita3k_vsync", true));
			yaml.setRaw({ "anisotropic-filtering" }, std::to_string(s.getInt("vita3k_anisotropic", 1)));
			yaml.set({ "screen-filter" }, s.getString("vita3k_filter", "Bilinear"));
			yaml.set({ "disable-surface-sync" }, s.getBool("vita3k_surface", true));
			yaml.set({ "async-pipeline-compilation" }, s.getBool("vita3k_sync", true));
			yaml.set({ "fullscreen_hd_res_pixel_perfect" }, s.getBool("vita3k_hd_pixel", false));
			yaml.set({ "high-accuracy" }, s.getBool("vita3k_accuracy", false));
			yaml.set({ "texture-cache" }, s.getBool("vita3k_texture", true));
			yaml.set({ "shader-cache" }, s.getBool("vita3k_shader", true));
			yaml.set({ "memory-mapping" }, s.getString("vita3k_mapping", "double-buffer"));
			yaml.setRaw({ "sys-lang" }, std::to_string(s.getInt("vita3k_system_language", 1)));
		}
	}

	Command Vita3KGenerator::configure(const LaunchContext& ctx)
	{
		fs::path home = ctx.paths.configFor("vita3k");
		fs::path storage = ctx.paths.saves / "psvita";

		auto program = util::emulatorCommand(ctx, "vita3k", { "Vita3K", "vita3k" }, { home, storage, ctx.game.rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "Vita3K is not installed: install it from the emulator catalog.");

		fs::path configPath = home / "config.yml";
		YamlFile yaml = YamlFile::load(configPath);
		writeSettings(ctx, storage, yaml);
		try
		{
			fs::create_directories(storage);
			// Vita3K reads its yml as yaml-cpp writes it, document markers included
			util::writeFileAtomically(configPath, "---\n" + yaml.toString() + "...\n");

			// no warning about running as administrator in the Qt window
			fs::path guiConfig = home / "gui-configs" / "CurrentSettings.ini";
			util::IniFile ini = util::IniFile::load(guiConfig);
			ini.set("MainWindow", "warnAdminPrivileges", "false");
			ini.save(guiConfig);
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the Vita3K configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *program;
		// -w and -f: start from this config without rewriting it, and without asking anything
		cmd.argv.insert(cmd.argv.end(), { "-F", "-w", "-f", "-c", configPath.string() });

		std::string id = titleId(ctx.game.rom);
		std::error_code ec;
		if (!id.empty() && fs::is_directory(storage / "ux0" / "app" / id, ec))
			cmd.argv.insert(cmd.argv.end(), { "-r", id });
		else
			cmd.argv.push_back(ctx.game.rom.string());   // not installed yet: Vita3K installs it

		// Vita3K crashes on some games under Wayland: XWayland, when there is one
		if (std::getenv("DISPLAY") != nullptr)
		{
			cmd.env["SDL_VIDEODRIVER"] = "x11";
			cmd.env["QT_QPA_PLATFORM"] = "xcb";
		}
		cmd.closeOnHotkey = true;
		util::setPadEnvironment(cmd, ctx.controllers);
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
