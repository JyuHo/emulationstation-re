// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.vice), Copyright (c) 2026 The Batocera Team.

#include "vice/ViceGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Files.h"
#include "util/Gamepad.h"
#include "util/IniFile.h"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <vector>

namespace es::launcher::vice
{
	namespace fs = std::filesystem;
	using util::IniFile;

	ES_LAUNCHER_REGISTER_GENERATOR("vice", ViceGenerator)

	namespace
	{
		// VICE's program -> its section in vicerc
		const std::map<std::string, std::string, std::less<>>& sections()
		{
			static const std::map<std::string, std::string, std::less<>> map = {
				{ "x64", "C64" }, { "x64sc", "C64SC" }, { "x64dtv", "C64DTV" }, { "xscpu64", "SCPU64" },
				{ "x128", "C128" }, { "xvic", "VIC20" }, { "xplus4", "PLUS4" }, { "xpet", "PET" },
			};
			return map;
		}

		// the video chips of each machine, as VICE names their resources
		std::vector<std::string> chips(std::string_view section)
		{
			if (section == "C128")
				return { "VICII", "VDC" };
			if (section == "PLUS4")
				return { "TED" };
			if (section == "VIC20")
				return { "VIC" };
			if (section == "PET")
				return { "Crtc" };
			return { "VICII" };
		}

		// ES input -> joystick line of VICE: up, down, left, right, fire, fire 2, fire 3
		constexpr std::array<std::pair<const char*, int>, 7> pins = { {
			{ "up", 1 }, { "down", 2 }, { "left", 4 }, { "right", 8 }, { "b", 16 }, { "a", 32 }, { "y", 64 },
		} };

		// JoyDevice: 0 none, then the keyboard; the host joysticks from 4
		constexpr int firstJoystick = 4;

		// "inputtype inputindex" of a joymap line: 0 axis (2 per axis, +1 negative), 1 button,
		// 2 hat (4 per hat: up, down, left, right)
		std::optional<std::string> joymapSource(const InputBinding& b)
		{
			switch (b.type)
			{
			case InputBinding::Type::Button:
				return "1 " + std::to_string(b.id);
			case InputBinding::Type::Axis:
				return "0 " + std::to_string(b.id * 2 + (b.value < 0 ? 1 : 0));
			case InputBinding::Type::Hat:
			{
				static const std::map<int, int> direction = { { 1, 0 }, { 4, 1 }, { 8, 2 }, { 2, 3 } };
				auto it = direction.find(b.value);
				if (it == direction.end())
					return std::nullopt;
				return "2 " + std::to_string(b.id * 4 + it->second);
			}
			default:
				return std::nullopt;
			}
		}
	}

	std::string ViceGenerator::machine(const LaunchContext& ctx)
	{
		if (sections().contains(ctx.core))
			return ctx.core;
		static const std::map<std::string, std::string, std::less<>> bySystem = {
			{ "c128", "x128" }, { "c20", "xvic" }, { "vic20", "xvic" }, { "cplus4", "xplus4" }, { "plus4", "xplus4" }, { "pet", "xpet" },
		};
		auto it = bySystem.find(ctx.game.system);
		return it == bySystem.end() ? "x64sc" : it->second;
	}

	std::string ViceGenerator::joymap(const LaunchContext& ctx)
	{
		std::string out = "# Written by ES at each launch\n\n!CLEAR\n";
		for (const Controller& pad : ctx.controllers)
		{
			std::string joy = std::to_string(std::max(pad.deviceIndex, 0));
			out += "\n# " + pad.name + "\n";
			for (const auto& [input, pin] : pins)
				if (const InputBinding* b = pad.find(input))
					if (auto source = joymapSource(*b))
						out += joy + " " + *source + " 1 " + std::to_string(pin) + "\n";
			if (const InputBinding* b = pad.find("select"))
				if (auto source = joymapSource(*b))
					out += joy + " " + *source + " 4\n";   // VICE's menu
		}
		return out;
	}

	Command ViceGenerator::configure(const LaunchContext& ctx)
	{
		const Settings& s = ctx.settings;
		fs::path home = ctx.paths.configFor("vice");
		std::string program = machine(ctx);
		std::string section = sections().at(program);

		auto command = util::emulatorCommand(ctx, "vice", program, { home, ctx.paths.bios, ctx.game.rom.parent_path() });
		if (!command)
			throw LaunchError(ErrorCode::EmulatorMissing, "VICE is not installed: install it from the emulator catalog or your package manager (vice).");

		fs::path vicerc = home / "vicerc";
		fs::path joymapPath = home / "joymap.vjm";
		IniFile ini = IniFile::load(vicerc);
		ini.setSeparator("=");   // VICE reads "Name=Value"

		ini.set(section, "SaveResourcesOnExit", "0");
		bool noBorder = s.getBool("noborder", false);
		for (const std::string& chip : chips(section))
		{
			ini.set(section, chip + "Fullscreen", "1");
			ini.set(section, chip + "FullscreenMode", "0");   // the desktop's resolution
			ini.set(section, chip + "AspectMode", noBorder ? "0" : "2");
			if (chip != "VDC" && chip != "Crtc")
				ini.set(section, chip + "BorderMode", noBorder ? "3" : "0");
		}
		ini.set(section, "JoyPort1Device", "1");   // a joystick

		// VICE gives a pad one port: player 1 takes port 2, which most games read (the VIC-20
		// has port 1 only)
		std::vector<int> ports = section == "VIC20" ? std::vector<int> { 1 } : std::vector<int> { 2, 1 };
		std::map<int, int> devices;
		for (size_t i = 0; i < ports.size(); ++i)
			devices[ports[i]] = 0;
		for (size_t i = 0; i < ports.size() && i < ctx.controllers.size(); ++i)
			devices[ports[i]] = firstJoystick + std::max(ctx.controllers[i].deviceIndex, 0);
		for (const auto& [port, device] : devices)
			ini.set(section, "JoyDevice" + std::to_string(port), device);
		ini.set(section, "JoyMapFile", joymapPath.string());

		// vice.<section>.<resource>: the player's own resources
		for (const auto& [key, value] : s.withPrefix("vice."))
		{
			size_t dot = key.find('.');
			if (dot != std::string::npos && dot > 0 && dot + 1 < key.size())
				ini.set(key.substr(0, dot), key.substr(dot + 1), value);
		}

		try
		{
			ini.save(vicerc);
			util::writeFileAtomically(joymapPath, joymap(ctx));
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the VICE configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *command;
		// the Flatpak starts x64sc: the other machines are its other commands
		if (fs::path(cmd.argv.front()).filename() == "flatpak")
			cmd.argv.insert(cmd.argv.end() - 1, "--command=" + program);
		cmd.argv.insert(cmd.argv.end(), { "-config", vicerc.string(), ctx.game.rom.string() });

		cmd.closeOnHotkey = true;
		util::setPadEnvironment(cmd, ctx.controllers);
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
