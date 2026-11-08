// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.ymir), Copyright (c) 2026 The Batocera Team.

#include "ymir/YmirGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Gamepad.h"

#include <array>
#include <cmath>
#include <vector>

namespace es::launcher::ymir
{
	namespace fs = std::filesystem;
	using util::IniFile;
	using util::tomlString;

	ES_LAUNCHER_REGISTER_GENERATOR("ymir", YmirGenerator)

	namespace
	{
		using Binds = std::vector<std::pair<const char*, std::vector<const char*>>>;

		struct Peripheral
		{
			const char* name;
			Binds gamepad;                    // SDL gamepad controls, "@<index>" appended
			std::array<Binds, 2> keyboard;    // players 1 and 2
		};

		// Ymir's stock bindings (Ymir.toml)
		const Binds keyboardPad1 = {
			{ "A", { "J" } }, { "B", { "K" } }, { "C", { "L" } }, { "X", { "U" } }, { "Y", { "I" } }, { "Z", { "O" } },
			{ "L", { "Q" } }, { "R", { "E" } }, { "Up", { "W" } }, { "Down", { "S" } }, { "Left", { "A" } }, { "Right", { "D" } },
			{ "Start", { "F", "G", "H" } },
		};
		const Binds keyboardPad2 = {
			{ "A", { "KeyPad1" } }, { "B", { "KeyPad2" } }, { "C", { "KeyPad3" } }, { "X", { "KeyPad4" } }, { "Y", { "KeyPad5" } },
			{ "Z", { "KeyPad6" } }, { "L", { "Insert", "KeyPad7" } }, { "R", { "PageUp", "KeyPad9" } },
			{ "Up", { "Home", "Up" } }, { "Down", { "End", "Down" } }, { "Left", { "Delete", "Left" } }, { "Right", { "PageDown", "Right" } },
			{ "Start", { "KeyPadEnter" } },
		};

		Binds withSwitchMode(Binds binds, const char* key)
		{
			binds.emplace_back("SwitchMode", std::vector<const char*>{ key });
			return binds;
		}

		const std::array<Peripheral, 4> peripherals = { {
			{ "AnalogPad",
				{ { "A", { "GamepadX" } }, { "B", { "GamepadA" } }, { "C", { "GamepadB" } }, { "X", { "GamepadLeftBumper" } },
					{ "Y", { "GamepadY" } }, { "Z", { "GamepadRightBumper" } }, { "Start", { "GamepadStart" } },
					{ "SwitchMode", { "GamepadLeftThumb" } }, { "DPad", { "GamepadDPad" } }, { "AnalogStick", { "GamepadLeftStick" } },
					{ "AnalogL", { "GamepadLeftTrigger" } }, { "AnalogR", { "GamepadRightTrigger" } } },
				{ withSwitchMode(keyboardPad1, "Ctrl+B"), withSwitchMode(keyboardPad2, "KeyPadAdd") } },
			{ "ControlPad",
				{ { "A", { "GamepadX" } }, { "B", { "GamepadA" } }, { "C", { "GamepadB" } }, { "X", { "GamepadLeftBumper" } },
					{ "Y", { "GamepadY" } }, { "Z", { "GamepadRightBumper" } }, { "L", { "GamepadLeftTriggerButton" } },
					{ "R", { "GamepadRightTriggerButton" } }, { "Start", { "GamepadStart" } }, { "DPad", { "GamepadLeftStick", "GamepadDPad" } } },
				{ keyboardPad1, keyboardPad2 } },
			{ "ArcadeRacer",
				{ { "A", { "GamepadX" } }, { "B", { "GamepadA" } }, { "C", { "GamepadB" } }, { "X", { "GamepadLeftBumper" } },
					{ "Y", { "GamepadY" } }, { "Z", { "GamepadRightBumper" } }, { "Start", { "GamepadStart" } },
					{ "Up", { "GamepadDpadDown", "GamepadRightTriggerButton" } }, { "Down", { "GamepadDpadUp", "GamepadLeftTriggerButton" } },
					{ "Wheel", { "GamepadLeftStickX" } } },
				{ Binds{ { "A", { "J" } }, { "B", { "K" } }, { "C", { "L" } }, { "X", { "U" } }, { "Y", { "I" } }, { "Z", { "O" } },
					  { "Start", { "F", "G", "H" } }, { "Up", { "S" } }, { "Down", { "W" } }, { "WheelLeft", { "A" } }, { "WheelRight", { "D" } } },
					Binds{ { "A", { "KeyPad1" } }, { "B", { "KeyPad2" } }, { "C", { "KeyPad3" } }, { "X", { "KeyPad4" } }, { "Y", { "KeyPad5" } },
						{ "Z", { "KeyPad6" } }, { "Start", { "KeyPadEnter" } }, { "Up", { "Down", "End" } }, { "Down", { "Up", "Home" } },
						{ "WheelLeft", { "Delete", "Left" } }, { "WheelRight", { "PageDown", "Right" } } } } },
			{ "MissionStick",
				{ { "A", { "GamepadX" } }, { "B", { "GamepadA" } }, { "C", { "GamepadB" } }, { "X", { "GamepadLeftBumper" } },
					{ "Y", { "GamepadY" } }, { "Z", { "GamepadRightBumper" } }, { "L", { "GamepadLeftThumb" } }, { "R", { "GamepadRightThumb" } },
					{ "Start", { "GamepadStart" } }, { "SwitchMode", { "GamepadBack" } }, { "MainStick", { "GamepadLeftStick", "GamepadDPad" } },
					{ "MainThrottle", { "GamepadLeftTrigger" } }, { "SubStick", { "GamepadRightStick" } }, { "SubThrottle", { "GamepadRightTrigger" } } },
				{ Binds{ { "A", { "X" } }, { "B", { "C" } }, { "C", { "V" } }, { "X", { "B" } }, { "Y", { "N" } }, { "Z", { "M" } },
					  { "L", { "Q" } }, { "R", { "E" } }, { "Start", { "G" } }, { "SwitchMode", { "Ctrl+B" } },
					  { "MainUp", { "W" } }, { "MainDown", { "S" } }, { "MainLeft", { "A" } }, { "MainRight", { "D" } },
					  { "MainThrottleUp", { "R" } }, { "MainThrottleDown", { "F" } }, { "MainThrottleMax", { "Shift+R" } },
					  { "MainThrottleMin", { "Shift+F" } }, { "SubUp", { "I" } }, { "SubDown", { "K" } }, { "SubLeft", { "J" } },
					  { "SubRight", { "L" } }, { "SubThrottleUp", { "Y" } }, { "SubThrottleDown", { "H" } },
					  { "SubThrottleMax", { "Shift+Y" } }, { "SubThrottleMin", { "Shift+H" } } },
					Binds{} } },
		} };

		constexpr std::array peripheralOptions = { "ymir_peripheral_p1", "ymir_peripheral_p2" };

		const char* tomlBool(bool value) { return value ? "true" : "false"; }

		// a TOML array of strings: [ "J", "GamepadX@0" ]
		std::string tomlArray(const std::vector<std::string>& values)
		{
			std::string out = "[";
			for (size_t i = 0; i < values.size(); ++i)
			{
				out += i == 0 ? " " : ", ";
				out += tomlString(values[i]);
			}
			out += values.empty() ? "]" : " ]";
			return out;
		}
	}

	fs::path YmirGenerator::settingsPath(const LaunchContext& ctx)
	{
		return ctx.paths.configFor("ymir") / "Ymir.toml";
	}

	std::string YmirGenerator::aspectRatio(std::string_view value)
	{
		double ratio = 4.0 / 3.0;
		try
		{
			if (!value.empty())
				ratio = std::stod(std::string(value));
		}
		catch (const std::exception&)
		{
		}
		if (!(ratio > 0.0))
			ratio = 4.0 / 3.0;

		// the closest fraction with a denominator up to 100, as batocera-launch does
		int bestNumerator = 4;
		int bestDenominator = 3;
		double bestError = std::abs(ratio - 4.0 / 3.0);
		for (int denominator = 1; denominator <= 100; ++denominator)
		{
			int numerator = static_cast<int>(std::lround(ratio * denominator));
			double error = std::abs(ratio - static_cast<double>(numerator) / denominator);
			if (numerator > 0 && error < bestError - 1e-12)
			{
				bestNumerator = numerator;
				bestDenominator = denominator;
				bestError = error;
			}
		}
		return std::to_string(bestNumerator) + ":" + std::to_string(bestDenominator);
	}

	void YmirGenerator::writeSettings(const LaunchContext& ctx, IniFile& toml)
	{
		const Settings& s = ctx.settings;
		fs::path home = ctx.paths.configFor("ymir");
		fs::path saves = ctx.paths.saves / "saturn" / "ymir";
		fs::path roms = ctx.game.rom.parent_path();

		// the layout of the keys written below
		toml.set("", "ConfigVersion", "6");

		toml.set("General", "CheckForUpdates", "false");
		toml.set("General", "IncludeNightlyBuilds", "false");
		toml.set("General", "EnableDiscordPresence", "false");
		toml.set("General", "PauseWhenUnfocused", "false");

		toml.set("General.PathOverrides", "BackupMemory", tomlString((saves / "backup").string()));
		toml.set("General.PathOverrides", "CDBlockROMImages", tomlString((roms / "cdb").string() + "/"));
		toml.set("General.PathOverrides", "Dumps", tomlString((saves / "dumps").string()));
		toml.set("General.PathOverrides", "ExportedBackups", tomlString((saves / "backup" / "exported").string()));
		toml.set("General.PathOverrides", "IPLROMImages", tomlString(ctx.paths.bios.string()));
		toml.set("General.PathOverrides", "PersistentState", tomlString((home / "state").string() + "/"));
		toml.set("General.PathOverrides", "ROMCartImages", tomlString(roms.string()));
		toml.set("General.PathOverrides", "SaveStates", tomlString(saves.string()));
		toml.set("General.PathOverrides", "Screenshots", tomlString((ctx.paths.screenshots / "ymir").string()));

		toml.set("System", "AutoDetectRegion", "true");
		toml.set("System", "InternalBackupRAMPerGame", tomlBool(s.getBool("ymir_backup_ram_per_game", false)));

		toml.set("Cartridge", "AutoLoadGameCarts", "true");

		toml.set("Video", "AutoResizeWindow", "false");
		toml.set("Video", "DisplayVideoOutputInWindow", "false");
		toml.set("Video", "FullScreen", "true");
		toml.set("Video", "ForceAspectRatio", "true");
		toml.set("Video", "ForceIntegerScaling", tomlBool(s.getBool("ymir_integer_scaling", false)));
		toml.set("Video", "ForcedAspect", tomlString(aspectRatio(s.getString("ymir_aspect"))));
		toml.set("Video", "Rotation", tomlString(s.getString("ymir_rotation", "Normal")));
		toml.set("Video.SoftwareRenderer", "ThreadedVDP1", "true");
		toml.set("Video.SoftwareRenderer", "ThreadedVDP2", "true");
		toml.set("Video.SoftwareRenderer", "ThreadedDeinterlacer", "true");
		toml.set("Video.Enhancements", "Deinterlace", tomlBool(s.getBool("ymir_interlace", true)));
		toml.set("Video.Enhancements", "TransparentMeshes", tomlBool(s.getBool("ymir_meshes", false)));

		toml.set("Input.Gamepad", "AnalogToDigitalSensitivity", "0.20000000298023224");
		toml.set("Input.Gamepad", "LSDeadzone", "0.15000000596046448");
		toml.set("Input.Gamepad", "RSDeadzone", "0.15000000596046448");

		// ports 1 and 2, rewritten at each launch; a port without a player has no peripheral
		for (int port = 1; port <= 2; ++port)
		{
			std::string section = "Input.Port" + std::to_string(port);
			for (const Peripheral& peripheral : peripherals)
			{
				toml.removeSection(section + "." + peripheral.name + ".Binds");
				toml.removeSection(section + "." + peripheral.name);
			}
			toml.removeSection(section);
			toml.set(section, "PeripheralType", tomlString("None"));
		}

		for (const Controller& pad : ctx.controllers)
		{
			if (pad.player < 1 || pad.player > 2)
				continue;

			std::string section = "Input.Port" + std::to_string(pad.player);
			toml.set(section, "PeripheralType", tomlString(s.getString(peripheralOptions[pad.player - 1], "ControlPad")));
			if (!pad.devicePath.empty())
				toml.set(section, "DevicePath", tomlString(pad.devicePath));

			std::string suffix = "@" + std::to_string(std::max(pad.deviceIndex, 0));
			for (const Peripheral& peripheral : peripherals)
			{
				std::string table = section + "." + peripheral.name;
				if (std::string_view(peripheral.name) == "ArcadeRacer")
					toml.set(table, "Sensitivity", "0.5");

				// keyboard keys first, then the gamepad's, in the order of Ymir's own file
				std::vector<std::pair<std::string, std::vector<std::string>>> binds;
				auto entry = [&](const char* key) -> std::vector<std::string>& {
					for (auto& [name, values] : binds)
						if (name == key)
							return values;
					return binds.emplace_back(key, std::vector<std::string>{}).second;
				};
				for (const auto& [key, values] : peripheral.keyboard[pad.player - 1])
					for (const char* value : values)
						entry(key).emplace_back(value);
				for (const auto& [key, values] : peripheral.gamepad)
					for (const char* value : values)
						entry(key).push_back(value + suffix);

				for (const auto& [key, values] : binds)
					toml.set(table + ".Binds", key, tomlArray(values));
			}
		}
	}

	Command YmirGenerator::configure(const LaunchContext& ctx)
	{
		fs::path home = ctx.paths.configFor("ymir");
		fs::path saves = ctx.paths.saves / "saturn" / "ymir";

		auto program = util::emulatorCommand(ctx, "ymir", { "ymir", "ymir-sdl3" }, {
			home, saves, ctx.paths.bios, ctx.paths.screenshots, ctx.game.rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "Ymir is not installed: install it from its releases (github.com/StrikerX3/Ymir) or with your package manager.");

		IniFile toml = IniFile::load(settingsPath(ctx));
		writeSettings(ctx, toml);
		try
		{
			for (const fs::path& dir : { saves / "backup" / "exported", saves / "dumps", ctx.paths.screenshots / "ymir", home / "state" })
				fs::create_directories(dir);
			toml.save(settingsPath(ctx));
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the Ymir configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *program;
		cmd.argv.insert(cmd.argv.end(), { "-p", home.string(), ctx.game.rom.string() });

		cmd.closeOnHotkey = true;   // its hotkeys are on the keyboard
		util::setPadEnvironment(cmd, ctx.controllers);
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
