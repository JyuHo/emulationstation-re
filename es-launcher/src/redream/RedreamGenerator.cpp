// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.redream), Copyright (c) 2026 The Batocera Team.

#include "redream/RedreamGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Files.h"
#include "util/Gamepad.h"

#include <array>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <system_error>

namespace es::launcher::redream
{
	namespace fs = std::filesystem;

	ES_LAUNCHER_REGISTER_GENERATOR("redream", RedreamGenerator)

	namespace
	{
		// ES input -> Redream control. Redream names the Dreamcast buttons by position, the
		// other way round from ES.
		constexpr std::array<std::pair<const char*, const char*>, 7> buttons = { {
			{ "a", "b" }, { "b", "a" }, { "x", "y" }, { "y", "x" }, { "start", "start" }, { "select", "menu" }, { "pageup", "turbo" },
		} };

		constexpr std::array<const char*, 4> directions = { "up", "down", "left", "right" };

		// Redream reads the Switch Pro pad its own way: Batocera's profile for it
		constexpr const char* nintendoGuid = "030000007e0500000920000011810000";
		constexpr const char* nintendoProfile =
			"b:joy1,a:joy0,dpad_down:hat1,ljoy_left:-axis0,ljoy_right:+axis0,ljoy_up:-axis1,"
			"ljoy_down:+axis1,ltrig:joy6,dpad_left:hat2,rtrig:joy7,dpad_right:hat3,turbo:joy8,"
			"start:joy9,dpad_up:hat0,y:joy2,x:joy3,";

		std::string key(const std::string& line)
		{
			size_t eq = line.find('=');
			return eq == std::string::npos ? std::string() : line.substr(0, eq);
		}

		bool sameFile(const fs::path& a, const fs::path& b)
		{
			std::error_code ec;
			if (!fs::exists(b, ec) || fs::file_size(a, ec) != fs::file_size(b, ec))
				return false;
			std::ifstream fa(a, std::ios::binary), fb(b, std::ios::binary);
			std::stringstream ca, cb;
			ca << fa.rdbuf();
			cb << fb.rdbuf();
			return ca.str() == cb.str();
		}
	}

	std::string RedreamGenerator::padProfile(const Controller& pad, int port)
	{
		std::string profile = "profile" + std::to_string(port) + "=name:" + pad.guid + ",type:controller,deadzone:12,crosshair:1,";
		if (util::guidWithoutCrc(pad.guid) == util::guidWithoutCrc(nintendoGuid))
			return profile + nintendoProfile + "exit:f10";

		using Type = InputBinding::Type;
		for (const auto& [input, control] : buttons)
			if (const InputBinding* b = pad.find(input); b != nullptr && b->type == Type::Button)
				profile += std::string(control) + ":joy" + std::to_string(b->id) + ",";

		for (const char* trigger : { "l2", "r2" })
		{
			const InputBinding* b = pad.find(trigger);
			if (b == nullptr)
				continue;
			std::string control = trigger[0] == 'l' ? "ltrig" : "rtrig";
			if (b->type == Type::Button)
				profile += control + ":joy" + std::to_string(b->id) + ",";
			else if (b->type == Type::Axis)
				profile += control + ":+axis" + std::to_string(b->id) + ",";
		}

		for (size_t i = 0; i < directions.size(); ++i)
		{
			const InputBinding* b = pad.find(directions[i]);
			if (b == nullptr)
				continue;
			if (b->type == Type::Button)
				profile += std::string("dpad_") + directions[i] + ":joy" + std::to_string(b->id) + ",";
			else if (b->type == Type::Hat)
				profile += std::string("dpad_") + directions[i] + ":hat" + std::to_string(i) + ",";
		}

		// the left stick, as Batocera binds it: axes 0 and 1
		if (const InputBinding* b = pad.find("joystick1left"); b != nullptr && b->type == Type::Axis)
			profile += "ljoy_left:-axis0,ljoy_right:+axis0,";
		if (const InputBinding* b = pad.find("joystick1up"); b != nullptr && b->type == Type::Axis)
			profile += "ljoy_up:-axis1,ljoy_down:+axis1,";

		return profile + "exit:f10";
	}

	std::vector<std::string> RedreamGenerator::settingLines(const LaunchContext& ctx)
	{
		const Settings& s = ctx.settings;
		std::vector<std::string> lines = {
			"gamedir=" + ctx.game.rom.parent_path().string(),
			"mode=exclusive fullscreen",
			"fullmode=exclusive fullscreen",
		};

		std::set<std::string, std::less<>> profiles;
		int port = 0;
		for (const Controller& pad : ctx.controllers)
		{
			if (port == 4)
				break;
			int device = pad.deviceIndex >= 0 ? pad.deviceIndex : port;
			lines.push_back("port" + std::to_string(port) + "=dev:" + std::to_string(4 + device) + ",desc:" + pad.guid + ",type:controller");
			if (profiles.insert(pad.guid).second)
				lines.push_back(padProfile(pad, port));
			++port;
		}

		if (ctx.screenWidth > 0 && ctx.screenHeight > 0)
		{
			std::string w = std::to_string(ctx.screenWidth), h = std::to_string(ctx.screenHeight);
			lines.insert(lines.end(), { "width=" + w, "height=" + h, "fullwidth=" + w, "fullheight=" + h });
		}

		std::string renderer = s.getString("redreamRender", "gl_hle_perstrip");
		if (!renderer.starts_with("gl_") && !renderer.starts_with("vk_"))
			renderer = "gl_" + renderer;

		lines.insert(lines.end(), {
			"res=" + s.getString("redreamResolution", "2"),
			"aspect=" + s.getString("redreamRatio", "4:3"),
			"frameskip=" + s.getString("redreamFrameSkip", "0"),
			"vsync=" + s.getString("redreamVsync", "0"),
			"renderer=" + renderer,
			"region=" + s.getString("redreamRegion", "usa"),
			"language=" + s.getString("redreamLanguage", "english"),
			"broadcast=" + s.getString("redreamBroadcast", "ntsc"),
			"cable=" + s.getString("redreamCable", "vga"),
		});
		return lines;
	}

	Command RedreamGenerator::configure(const LaunchContext& ctx)
	{
		if (ctx.architecture != Architecture::X86_64)
			throw LaunchError(ErrorCode::Generic, "Redream only runs on x86_64 computers.");

		auto program = util::emulatorCommand(ctx, "redream", "redream");
		std::error_code ec;
		if (!program || program->size() != 1 || !fs::is_regular_file(program->front(), ec))
			throw LaunchError(ErrorCode::EmulatorMissing, "Redream is not installed: install it from the emulator catalog.");

		fs::path home = ctx.paths.configFor("redream");
		fs::path copy = home / "redream";
		fs::path configPath = home / "redream.cfg";

		// the lines ES does not set (the player's own), then ES's
		std::vector<std::string> lines = settingLines(ctx);
		std::set<std::string, std::less<>> keys;
		for (const std::string& line : lines)
			keys.insert(key(line));
		std::vector<std::string> kept;
		std::ifstream in(configPath);
		for (std::string line; std::getline(in, line);)
		{
			std::string k = key(line);
			// pad profiles and ports are ES's: the old ones would point to other pads
			if (!line.empty() && !keys.contains(k) && !k.starts_with("port") && !k.starts_with("profile"))
				kept.push_back(line);
		}
		in.close();

		std::string text;
		for (const std::string& line : kept)
			text += line + "\n";
		for (const std::string& line : lines)
			text += line + "\n";

		try
		{
			fs::create_directories(home);
			if (!sameFile(program->front(), copy))
			{
				fs::copy_file(program->front(), copy, fs::copy_options::overwrite_existing);
				fs::permissions(copy, fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec
					| fs::perms::others_read | fs::perms::others_exec);
			}
			util::writeFileAtomically(configPath, text);
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the Redream configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = { copy.string(), ctx.game.rom.string() };
		cmd.workingDirectory = home;
		cmd.closeOnHotkey = true;
		util::setPadEnvironment(cmd, ctx.controllers);
		return cmd;
	}
}
