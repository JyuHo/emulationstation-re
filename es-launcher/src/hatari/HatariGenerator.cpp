// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.hatari), Copyright (c) 2026 The Batocera Team.

#include "hatari/HatariGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Gamepad.h"
#include "util/IniFile.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <map>
#include <system_error>
#include <vector>

namespace es::launcher::hatari
{
	namespace fs = std::filesystem;
	using util::IniFile;

	ES_LAUNCHER_REGISTER_GENERATOR("hatari", HatariGenerator)

	namespace
	{
		// the TOS versions of each machine, best first
		const std::map<std::string, std::vector<std::string>, std::less<>>& machineTos()
		{
			static const std::map<std::string, std::vector<std::string>, std::less<>> tos = {
				{ "st", { "etos256", "104", "102", "100" } },
				{ "ste", { "etos256", "162", "106" } },
				{ "megaste", { "etos256", "206", "205" } },
				{ "tt", { "etos512", "306" } },
				{ "falcon", { "etos512", "404", "402", "400" } },
			};
			return tos;
		}

		constexpr std::array<const char*, 10> languages = { "us", "uk", "de", "es", "fr", "it", "nl", "ru", "se", "" };

		// Hatari's joystick buttons 1 to 3, as Batocera binds them
		constexpr std::array<const char*, 3> padButtons = { "y", "b", "a" };

		std::string lower(std::string text)
		{
			std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		}
	}

	std::pair<std::string, std::string> HatariGenerator::model(std::string_view setting)
	{
		size_t underscore = setting.find('_');
		if (underscore != std::string_view::npos)
		{
			static const std::map<std::string, std::string, std::less<>> machines = {
				{ "520st", "st" }, { "1040ste", "ste" }, { "megaste", "megaste" }, { "tt", "tt" }, { "falcon", "falcon" },
			};
			auto machine = machines.find(setting.substr(0, underscore));
			std::string version(setting.substr(underscore + 1));
			if (machine != machines.end())
			{
				const auto& versions = machineTos().at(machine->second);
				if (version == "auto" || std::ranges::find(versions, version) != versions.end())
					return { machine->second, version };
			}
		}
		return { "st", "auto" };
	}

	std::optional<fs::path> HatariGenerator::findTos(const fs::path& biosDir, std::string_view machine, std::string_view version,
		std::string_view language)
	{
		auto known = machineTos().find(machine);
		if (known == machineTos().end())
			return std::nullopt;

		std::vector<std::string> versions;
		if (version != "auto")
			versions.emplace_back(version);
		versions.insert(versions.end(), known->second.begin(), known->second.end());

		std::vector<std::string> langs;
		if (language != "auto")
			langs.emplace_back(language);
		langs.insert(langs.end(), languages.begin(), languages.end());

		std::error_code ec;
		for (const std::string& v : versions)
			for (const std::string& lang : langs)
			{
				std::string name = v.starts_with("etos") ? v : "tos" + v;
				name += lang;
				name += ".img";
				fs::path tos = biosDir / name;
				if (fs::exists(tos, ec))
					return tos;
			}
		return std::nullopt;
	}

	Command HatariGenerator::configure(const LaunchContext& ctx)
	{
		const Settings& s = ctx.settings;
		fs::path home = ctx.paths.configFor("hatari");
		fs::path rom = ctx.game.rom;

		auto program = util::emulatorCommand(ctx, "hatari", "hatari", { home, ctx.paths.bios, rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "Hatari is not installed: install it with your package manager (hatari).");

		auto [machine, version] = model(s.getString("model", "520st_auto"));
		auto tos = findTos(ctx.paths.bios, machine, version, s.getString("language", "us"));
		if (!tos)
			throw LaunchError(ErrorCode::Generic, "No TOS for the Atari " + machine + " in " + ctx.paths.bios.string()
				+ ": put one there, such as tos104us.img or EmuTOS (etos256us.img).");

		fs::path configPath = home / "hatari.cfg";
		IniFile ini = IniFile::load(configPath);
		// no pad but ES's
		for (int i = 0; i <= 5; ++i)
		{
			std::string section = "Joystick" + std::to_string(i);
			if (ini.hasSection(section))
			{
				ini.set(section, "nJoyId", "-1");
				ini.set(section, "nJoystickMode", "0");
			}
		}
		for (const Controller& pad : ctx.controllers)
		{
			if (pad.player < 1 || pad.player > 5)
				continue;
			std::string section = "Joystick" + std::to_string(pad.player);
			ini.set(section, "nJoyId", std::max(pad.deviceIndex, 0));
			ini.set(section, "nJoystickMode", "1");
			for (size_t i = 0; i < padButtons.size(); ++i)
			{
				const InputBinding* b = pad.find(padButtons[i]);
				int id = b != nullptr && b->type == InputBinding::Type::Button ? b->id : static_cast<int>(i);
				ini.set(section, "nJoyBut" + std::to_string(i + 1) + "Index", id);
			}
		}
		ini.set("Log", "bConfirmQuit", "FALSE");
		ini.set("Screen", "bShowStatusbar", s.getBool("showFPS", false) ? "TRUE" : "FALSE");

		Command cmd;
		cmd.argv = *program;
		cmd.argv.insert(cmd.argv.end(), { "--configfile", configPath.string(), "--fullscreen", "--machine", machine,
			"--tos", tos->string(), "--memsize", std::to_string(std::clamp(s.getInt("ram", 0), 0, 14)) });

		std::string extension = lower(rom.extension().string());
		try
		{
			ini.save(configPath);
			if (extension == ".hd")
				cmd.argv.insert(cmd.argv.end(), { s.getString("hatari_drive") == "ACSI" ? "--acsi" : "--ide-master", rom.string() });
			else if (extension == ".gemdos")
			{
				// a folder as the hard disk: Hatari wants a floppy too
				fs::path blank = home / "blank.st";
				if (!fs::exists(blank))
					std::ofstream(blank).close();
				cmd.argv.insert(cmd.argv.end(), { "--harddrive", rom.string(), blank.string() });
			}
			else
				cmd.argv.insert(cmd.argv.end(), { "--disk-a", rom.string(), "--drive-b", "off" });
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the Hatari configuration: ") + e.what());
		}

		cmd.closeOnHotkey = true;
		util::setPadEnvironment(cmd, ctx.controllers);
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
