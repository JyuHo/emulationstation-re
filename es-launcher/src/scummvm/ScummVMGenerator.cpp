// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.scummvm), Copyright (c) 2026 The Batocera Team.

#include "scummvm/ScummVMGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Gamepad.h"
#include "util/IniFile.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <system_error>

namespace es::launcher::scummvm
{
	namespace fs = std::filesystem;

	ES_LAUNCHER_REGISTER_GENERATOR("scummvm", ScummVMGenerator)

	namespace
	{
		fs::path homeDir(const LaunchContext& ctx) { return ctx.paths.configFor("scummvm"); }
		fs::path savesDir(const LaunchContext& ctx) { return ctx.paths.saves / "scummvm"; }
		fs::path extraDir(const LaunchContext& ctx) { return ctx.paths.bios / "scummvm" / "extra"; }

		bool isDirectory(const fs::path& path)
		{
			std::error_code ec;
			return fs::is_directory(path, ec);
		}

		// "monkey", "scumm:monkey": what ScummVM takes as a target, else --auto-detect
		std::string target(const LaunchContext& ctx)
		{
			fs::path file = ctx.game.rom;
			if (isDirectory(file))
			{
				// a folder (squashfs): its <game>.scummvm file
				file.clear();
				std::error_code ec;
				for (fs::directory_iterator it(ctx.game.rom, ec), end; !ec && it != end; it.increment(ec))
				{
					if (it->path().extension() == ".scummvm")
					{
						file = it->path();
						break;
					}
				}
				if (file.empty())
					return "--auto-detect";
			}

			std::ifstream in(file);
			std::stringstream text;
			text << in.rdbuf();
			std::string id = text.str();
			auto space = [](unsigned char c) { return std::isspace(c) != 0; };
			id.erase(id.begin(), std::ranges::find_if_not(id, space));
			id.erase(std::find_if_not(id.rbegin(), id.rend(), space).base(), id.end());
			std::ranges::transform(id, id.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (id.empty())
				id = file.stem().string();

			static const std::regex gameId("^(?:[a-z0-9-]+:)?[a-z0-9-]+$");
			return std::regex_match(id, gameId) ? id : "--auto-detect";
		}
	}

	std::vector<std::string> ScummVMGenerator::arguments(const LaunchContext& ctx)
	{
		const Settings& s = ctx.settings;
		fs::path gamePath = isDirectory(ctx.game.rom) ? ctx.game.rom : ctx.game.rom.parent_path();

		int joystick = 0;
		auto player1 = std::ranges::find_if(ctx.controllers, [](const Controller& pad) { return pad.player == 1; });
		if (player1 != ctx.controllers.end())
			joystick = std::max(player1->deviceIndex, 0);

		std::vector<std::string> args = { "-f", "--config=" + (homeDir(ctx) / "scummvm.ini").string() };
		if (ctx.screenWidth > 0 && ctx.screenHeight > 0)
			args.push_back("--window-size=" + std::to_string(ctx.screenWidth) + "," + std::to_string(ctx.screenHeight));
		args.push_back("--scale-factor=" + s.getString("scumm_scale", "3"));
		args.push_back("--scaler=" + s.getString("scumm_scaler_mode", "normal"));
		if (auto stretch = s.getString("scumm_stretch"); !stretch.empty())
			args.push_back("--stretch-mode=" + stretch);
		args.push_back("--renderer=" + s.getString("scumm_renderer", "opengl"));
		if (auto language = s.getString("scumm_language"); !language.empty())
			args.insert(args.end(), { "-q", language });

		args.insert(args.end(), {
			"--logfile=" + (homeDir(ctx) / "scummvm.log").string(),
			"--joystick=" + std::to_string(joystick),
			"--screenshotspath=" + ctx.paths.screenshots.string(),
			"--extrapath=" + extraDir(ctx).string(),
			"--savepath=" + savesDir(ctx).string(),
			"--path=" + gamePath.string(),
			target(ctx),
		});
		return args;
	}

	Command ScummVMGenerator::configure(const LaunchContext& ctx)
	{
		fs::path home = homeDir(ctx);
		fs::path gamePath = isDirectory(ctx.game.rom) ? ctx.game.rom : ctx.game.rom.parent_path();

		auto program = util::emulatorCommand(ctx, "scummvm", "scummvm", {
			home, savesDir(ctx), extraDir(ctx), ctx.paths.screenshots, gamePath });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "ScummVM is not installed: install it with your package manager (scummvm).");

		try
		{
			fs::create_directories(extraDir(ctx));
			fs::create_directories(savesDir(ctx));
			// ES's own file browser, not the desktop's
			util::IniFile ini = util::IniFile::load(home / "scummvm.ini");
			ini.set("scummvm", "gui_browser_native", "false");
			ini.save(home / "scummvm.ini");
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the ScummVM configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *program;
		std::vector<std::string> args = arguments(ctx);
		cmd.argv.insert(cmd.argv.end(), args.begin(), args.end());

		cmd.closeOnHotkey = true;   // it quits on Alt+F4
		util::setPadEnvironment(cmd, ctx.controllers);
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
