// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.openmsx), Copyright (c) 2026 The Batocera Team.

#include "openmsx/OpenMSXGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Files.h"
#include "util/Gamepad.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <map>
#include <system_error>

namespace es::launcher::openmsx
{
	namespace fs = std::filesystem;

	ES_LAUNCHER_REGISTER_GENERATOR("openmsx", OpenMSXGenerator)

	namespace
	{
		std::string lower(std::string text)
		{
			std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		}

		std::string trim(const std::string& text)
		{
			size_t begin = text.find_first_not_of(" \t\r\n");
			size_t end = text.find_last_not_of(" \t\r\n");
			return begin == std::string::npos ? std::string() : text.substr(begin, end - begin + 1);
		}

		// A Tcl word between braces: braces themselves cannot be in a path openMSX opens
		std::string tclPath(const fs::path& path)
		{
			std::string text = path.string();
			std::erase_if(text, [](char c) { return c == '{' || c == '}'; });
			return "{" + text + "}";
		}

		uint32_t le(const std::string& data, size_t at, int bytes)
		{
			uint32_t value = 0;
			for (int i = bytes - 1; i >= 0; --i)
				value = (value << 8) | static_cast<unsigned char>(data[at + static_cast<size_t>(i)]);
			return value;
		}
	}

	std::string OpenMSXGenerator::zipMediaExtension(const fs::path& zip)
	{
		// the central directory, found from its end record in the last 64 KB of the file
		std::ifstream in(zip, std::ios::binary);
		std::error_code ec;
		uintmax_t size = fs::file_size(zip, ec);
		if (!in || ec || size < 22)
			return {};
		size_t tail = static_cast<size_t>(std::min<uintmax_t>(size, 65536 + 22));
		std::string end(tail, '\0');
		in.seekg(static_cast<std::streamoff>(size - tail));
		in.read(end.data(), static_cast<std::streamsize>(tail));

		size_t record = end.rfind(std::string("PK\x05\x06", 4));
		if (record == std::string::npos || record + 22 > end.size())
			return {};
		uint32_t dirSize = le(end, record + 12, 4);
		uint32_t dirOffset = le(end, record + 16, 4);
		if (dirSize > (1u << 24) || uintmax_t(dirOffset) + dirSize > size)
			return {};

		std::string dir(dirSize, '\0');
		in.seekg(dirOffset);
		in.read(dir.data(), dirSize);
		for (size_t at = 0; at + 46 <= dir.size() && dir.compare(at, 4, "PK\x01\x02", 4) == 0;)
		{
			size_t nameLength = le(dir, at + 28, 2);
			size_t extraLength = le(dir, at + 30, 2);
			size_t commentLength = le(dir, at + 32, 2);
			if (at + 46 + nameLength > dir.size())
				break;
			std::string ext = lower(fs::path(dir.substr(at + 46, nameLength)).extension().string());
			if (ext == ".cas" || ext == ".dsk" || ext == ".ogv")
				return ext;
			at += 46 + nameLength + extraLength + commentLength;
		}
		return {};
	}

	std::vector<std::string> OpenMSXGenerator::mediaArguments(const LaunchContext& ctx)
	{
		static const std::map<std::string, std::string, std::less<>> machines = {
			{ "msx1", "Boosted_MSX2_EN" }, { "msx2", "Boosted_MSX2_EN" }, { "msx2+", "Boosted_MSX2+_JP" },
			{ "msxturbor", "Boosted_MSXturboR_with_IDE" }, { "colecovision", "ColecoVision_SGM" },
			{ "spectravideo", "Spectravideo_SVI-328" },
		};
		const fs::path& rom = ctx.game.rom;
		std::string ext = lower(rom.extension().string());
		if (ext == ".zip")
		{
			std::string inside = zipMediaExtension(rom);
			if (!inside.empty())
				ext = inside;
		}

		std::vector<std::string> args;
		auto machine = machines.find(ctx.game.system);
		if (ext == ".ogv")
			args.insert(args.end(), { "-machine", "Pioneer_PX-7" });
		else if (machine != machines.end())
			args.insert(args.end(), { "-machine", machine->second });

		if (ext == ".openmsx")
		{
			// two cartridges or two disks, one per line, next to the file
			std::ifstream in(rom);
			std::vector<fs::path> media;
			for (std::string line; media.size() < 2 && std::getline(in, line);)
				if (std::string name = trim(line); !name.empty())
					media.push_back(rom.parent_path() / name);
			if (media.size() == 2)
			{
				bool disk = lower(media[0].extension().string()) == ".dsk";
				args.insert(args.end(), { disk ? "-diska" : "-carta", media[0].string(), disk ? "-diskb" : "-cartb", media[1].string() });
				return args;
			}
		}

		std::string option = "-cart";
		if (ext == ".ogv")
			option = "-laserdisc";
		else if (ext == ".cas")
			option = "-cassetteplayer";
		else if (ext == ".dsk")
			option = ctx.settings.getString("openmsx_disk") == "hda" ? "-hda" : "-diska";
		args.insert(args.end(), { option, rom.string() });
		return args;
	}

	std::string OpenMSXGenerator::script(const LaunchContext& ctx)
	{
		std::string out = "# Written by ES at each launch\n";
		out += "filepool add -path " + tclPath(ctx.paths.bios / "Machines") + " -types system_rom -position 1\n";
		out += "filepool add -path " + tclPath(ctx.paths.bios / "openmsx") + " -types system_rom -position 2\n";
		out += std::string("set fullspeedwhenloading ") + (ctx.settings.getBool("openmsx_loading", true) ? "on" : "off") + "\n";
		out += "bind F6 \"cycle videosource\"\n";
		out += "bind F5 {screenshot [utils::get_next_numbered_filename " + tclPath(ctx.paths.screenshots / "openmsx")
			+ " \"[guess_title] \" \".png\"]}\n";

		// the pads: player 1 in joystick port A, player 2 in port B
		bool spectravideo = ctx.game.system == "spectravideo";
		for (size_t i = 0; i < ctx.controllers.size() && i < 2; ++i)
		{
			const Controller& pad = ctx.controllers[i];
			std::string n = std::to_string(std::max(pad.deviceIndex, 0) + 1);
			std::string joystick = "joystick" + n;
			out += std::string("plug ") + (i == 0 ? "joyporta " : "joyportb ") + joystick + "\n";
			out += "dict set " + joystick + "_config LEFT {-axis0 L_hat0}\n";
			out += "dict set " + joystick + "_config RIGHT {+axis0 R_hat0}\n";
			out += "dict set " + joystick + "_config UP {-axis1 U_hat0}\n";
			out += "dict set " + joystick + "_config DOWN {+axis1 D_hat0}\n";

			auto bind = [&](const char* input, const std::string& down, const std::string& up) {
				const InputBinding* b = pad.find(input);
				if (b == nullptr || b->type != InputBinding::Type::Button)
					return;
				// bind "joy1 button3 down" "command"
				auto line = [&](const char* event, const std::string& command) {
					out += "bind \"joy";
					out += n;
					out += " button";
					out += std::to_string(b->id);
					out += event;
					out += "\" \"";
					out += command;
					out += "\"\n";
				};
				line(" down", down);
				if (!up.empty())
					line(" up", up);
			};
			// F2 and F3 of the MSX keyboard (row 6), which are Enter and Left on the Spectravideo
			if (!spectravideo)
			{
				bind("y", "keymatrixdown 6 0x40", "keymatrixup 6 0x40");
				bind("x", "keymatrixdown 6 0x80", "keymatrixup 6 0x80");
			}
			bind("pagedown", "set fastforward on", "set fastforward off");
			bind("select", "toggle pause", "");
			bind("start", "main_menu_toggle", "");
			bind("l3", "toggle_osd_keyboard", "");
			bind("r3", "toggle console", "");
		}
		return out;
	}

	Command OpenMSXGenerator::configure(const LaunchContext& ctx)
	{
		fs::path home = ctx.paths.configFor("openmsx");
		auto program = util::emulatorCommand(ctx, "openmsx", "openmsx",
			{ home, ctx.paths.bios, ctx.paths.screenshots, ctx.game.rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "openMSX is not installed: install it from the emulator catalog or your package manager (openmsx).");

		fs::path scriptPath = home / "es-launch.tcl";
		try
		{
			fs::create_directories(ctx.paths.screenshots / "openmsx");
			util::writeFileAtomically(scriptPath, script(ctx));
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the openMSX script: ") + e.what());
		}

		Command cmd;
		cmd.argv = *program;
		for (std::string& arg : mediaArguments(ctx))
			cmd.argv.push_back(std::move(arg));
		cmd.argv.insert(cmd.argv.end(), { "-script", scriptPath.string() });
		cmd.env["OPENMSX_HOME"] = home.string();
		cmd.closeOnHotkey = true;
		util::setPadEnvironment(cmd, ctx.controllers);
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
