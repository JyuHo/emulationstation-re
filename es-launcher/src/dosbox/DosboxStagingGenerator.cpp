// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.dosbox_staging), Copyright (c) 2026 The Batocera Team.

#include "dosbox/DosboxStagingGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Gamepad.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <system_error>

namespace es::launcher::dosbox
{
	namespace fs = std::filesystem;

	ES_LAUNCHER_REGISTER_GENERATOR("dosbox_staging", DosboxStagingGenerator)

	namespace
	{
		std::string lower(std::string s)
		{
			std::ranges::transform(s, s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		}

		// the file of directory named name, whatever its case (DOS games: DOSBOX.BAT)
		std::optional<fs::path> findFile(const fs::path& directory, const std::string& name)
		{
			std::error_code ec;
			for (fs::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec))
			{
				if (it->is_regular_file(ec) && lower(it->path().filename().string()) == name)
					return it->path();
			}
			return std::nullopt;
		}

		fs::path gameDir(const LaunchContext& ctx)
		{
			std::error_code ec;
			return fs::is_directory(ctx.game.rom, ec) ? ctx.game.rom : ctx.game.rom.parent_path();
		}
	}

	std::vector<std::string> DosboxStagingGenerator::arguments(const LaunchContext& ctx)
	{
		fs::path game = gameDir(ctx);
		fs::path resources = ctx.paths.configFor("dosbox");

		std::vector<std::string> args = { "--fullscreen", "--working-dir", game.string(), "-c", "set WORKDIR=" + game.string() };

		std::error_code ec;
		if (fs::is_directory(resources, ec))
		{
			args.insert(args.end(), { "-c", "set RESDIR=" + resources.string() });
			if (auto conf = findFile(resources, "dosbox-staging.conf"))
				args.insert(args.end(), { "-c", "set RESCONF=" + conf->string() });
		}

		auto cfg = findFile(game, "dosbox.cfg");
		auto conf = findFile(game, "dosbox.conf");
		auto bat = findFile(game, "dosbox.bat");
		if (cfg)
		{
			std::string name = cfg->filename().string();
			args.insert(args.end(), { "--conf", name, "-c", "set GAMECFG=" + name });
		}
		else if (conf)
		{
			std::string name = conf->filename().string();
			args.insert(args.end(), { "--conf", name, "-c", "set GAMECONF=" + name });
		}
		if (bat)
		{
			std::string name = bat->filename().string();
			args.insert(args.end(), { name, "-c", "set GAMEBAT=" + name });
		}

		if (cfg || conf || bat)
			// a configured game: no startup logos, and DOSBox quits with the game
			args.insert(args.end(), { "--set", "startup_verbosity=quiet", "--exit" });
		else
			// otherwise a C:\> prompt in the game's folder
			args.insert(args.end(), { "-c", "@echo off", "-c", "mount c .", "-c", "c:" });
		return args;
	}

	Command DosboxStagingGenerator::configure(const LaunchContext& ctx)
	{
		auto program = util::emulatorCommand(ctx, "dosbox_staging", { "dosbox-staging", "dosbox" }, {
			ctx.paths.configFor("dosbox"), gameDir(ctx) });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "DOSBox Staging is not installed: install it with your package manager (dosbox-staging).");

		Command cmd;
		cmd.argv = *program;
		std::vector<std::string> args = arguments(ctx);
		cmd.argv.insert(cmd.argv.end(), args.begin(), args.end());
		cmd.workingDirectory = gameDir(ctx);

		cmd.closeOnHotkey = true;   // it quits on Ctrl+F9
		util::setPadEnvironment(cmd, ctx.controllers);
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
