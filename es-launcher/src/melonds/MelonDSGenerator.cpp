// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.melonds), Copyright (c) 2026 The Batocera Team.

#include "melonds/MelonDSGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Gamepad.h"

namespace es::launcher::melonds
{
	namespace fs = std::filesystem;
	using util::IniFile;
	using util::tomlString;

	ES_LAUNCHER_REGISTER_GENERATOR("melonds", MelonDSGenerator)

	namespace
	{
		// melonDS key -> ES input
		constexpr std::pair<const char*, const char*> buttons[] = {
			{ "A", "a" }, { "B", "b" }, { "X", "x" }, { "Y", "y" }, { "Select", "select" }, { "Start", "start" },
			{ "Up", "up" }, { "Down", "down" }, { "Left", "left" }, { "Right", "right" },
			{ "L", "pageup" }, { "R", "pagedown" },
		};

		const char* tomlBool(bool value) { return value ? "true" : "false"; }

		// the DS BIOS and firmware, all three or none
		bool hasDSBios(const LaunchContext& ctx)
		{
			std::error_code ec;
			for (const char* file : { "bios7.bin", "bios9.bin", "firmware.bin" })
				if (!fs::is_regular_file(ctx.paths.bios / file, ec))
					return false;
			return true;
		}
	}

	fs::path MelonDSGenerator::settingsPath(const LaunchContext& ctx)
	{
		return ctx.paths.configFor("melonds") / "melonDS" / "melonDS.toml";
	}

	int MelonDSGenerator::joystickValue(const InputBinding& input)
	{
		switch (input.type)
		{
		case InputBinding::Type::Button:
			return input.id;
		case InputBinding::Type::Hat:
			return 0x100 | (input.id << 4) | (input.value & 0xF);
		default:
			return -1;
		}
	}

	void MelonDSGenerator::writeSettings(const LaunchContext& ctx, IniFile& toml)
	{
		const Settings& s = ctx.settings;
		fs::path bios = ctx.paths.bios;
		fs::path saves = ctx.paths.saves / "nds";

		// keys outside any table come first
		toml.set("", "MouseHide", "false");
		toml.set("", "MouseHideSeconds", "5");
		toml.set("", "PauseLostFocus", "false");
		toml.set("", "LastBIOSFolder", tomlString(bios.string()));
		toml.set("", "LastROMFolder", tomlString(ctx.game.rom.parent_path().string()));
		toml.set("", "LimitFPS", tomlBool(s.getBool("melonds_framerate", true)));

		toml.set("DS", "BIOS7Path", tomlString((bios / "bios7.bin").string()));
		toml.set("DS", "BIOS9Path", tomlString((bios / "bios9.bin").string()));
		toml.set("DS", "FirmwarePath", tomlString((bios / "firmware.bin").string()));

		toml.set("DLDI", "Enable", "true");
		toml.set("DLDI", "FolderPath", tomlString(saves.string()));
		toml.set("DLDI", "ImagePath", tomlString((saves / "dldi.bin").string()));

		toml.set("DSi", "FullBIOSBoot", "false");
		toml.set("DSi", "BIOS7Path", tomlString((bios / "dsi_bios7.bin").string()));
		toml.set("DSi", "BIOS9Path", tomlString((bios / "dsi_bios9.bin").string()));
		toml.set("DSi", "FirmwarePath", tomlString((bios / "dsi_firmware.bin").string()));
		toml.set("DSi", "NANDPath", tomlString((bios / "dsi_nand.bin").string()));
		toml.set("DSi.SD", "Enable", "true");
		toml.set("DSi.SD", "FolderPath", tomlString(saves.string()));
		toml.set("DSi.SD", "ImagePath", tomlString((saves / "dsisd.bin").string()));

		toml.set("Emu", "DirectBoot", "true");
		toml.set("Emu", "ExternalBIOSEnable", tomlBool(hasDSBios(ctx)));
		toml.set("Emu", "ConsoleType", std::to_string(s.getInt("melonds_console", 0)));   // 0 DS, 1 DSi

		// 0 software, 1 OpenGL, 2 OpenGL compute
		int renderer = s.getInt("melonds_renderer", 0);
		toml.set("3D", "Renderer", std::to_string(renderer));
		toml.set("Screen", "UseGL", tomlBool(renderer != 0));
		bool vsync = s.getBool("melonds_vsync", false);
		toml.set("Screen", "VSync", tomlBool(vsync));
		if (vsync)
			toml.set("Screen", "VSyncInterval", "1");

		toml.set("3D.GL", "ScaleFactor", s.getString("melonds_resolution", "5"));
		if (s.isSet("melonds_resolution"))
			toml.set("3D.GL", "HiresCoordinates", "true");
		toml.set("3D.GL", "BetterPolygons", tomlBool(s.getBool("melonds_polygons", false)));

		toml.set("Instance0", "SaveFilePath", tomlString(saves.string()));
		toml.set("Instance0", "SavestatePath", tomlString(saves.string()));
		toml.set("Instance0", "CheatFilePath", tomlString((saves / "cheats").string()));
		toml.set("Instance0", "EnableCheats", tomlBool(s.getBool("melonds_cheats", false)));

		toml.set("Instance0.Firmware", "OverrideSettings", tomlBool(s.getBool("melonds_use_fw_settings", false)));
		toml.set("Instance0.Firmware", "Language", std::to_string(s.getInt("melonds_language", 1)));
		toml.set("Instance0.Firmware", "BirthdayDay", std::to_string(s.getInt("melonds_day", 1)));
		toml.set("Instance0.Firmware", "BirthdayMonth", std::to_string(s.getInt("melonds_month", 1)));

		// one window, or each screen in its own window
		bool scaling = s.getBool("melonds_scaling", false);
		bool dual = s.getBool("melonds_dual_screen", false);
		toml.set("Instance0.Window0", "ShowOSD", tomlBool(s.getBool("melonds_osd", false)));
		toml.set("Instance0.Window0", "IntegerScaling", tomlBool(scaling));
		toml.set("Instance0.Window0", "ScreenRotation", dual ? "0" : std::to_string(s.getInt("melonds_rotation", 0)));
		toml.set("Instance0.Window0", "ScreenSwap", dual ? "false" : tomlBool(s.getBool("melonds_screenswap", false)));
		toml.set("Instance0.Window0", "ScreenLayout", dual ? "0" : std::to_string(s.getInt("melonds_layout", 0)));
		toml.set("Instance0.Window0", "ScreenSizing", dual ? "4" : std::to_string(s.getInt("melonds_screensizing", 0)));
		toml.set("Instance0.Window1", "Enabled", tomlBool(dual));
		toml.set("Instance0.Window1", "ScreenRotation", "0");
		toml.set("Instance0.Window1", "ScreenSwap", "false");
		toml.set("Instance0.Window1", "ScreenLayout", "0");
		toml.set("Instance0.Window1", "ScreenSizing", "5");
		toml.set("Instance0.Window1", "IntegerScaling", tomlBool(scaling));

		// player 1's pad, rewritten at each launch
		toml.removeSection("Instance0.Joystick");
		if (const Controller* pad = util::firstPlayer(ctx))
		{
			toml.set("Instance0", "JoystickID", std::to_string(std::max(pad->deviceIndex, 0)));
			for (const auto& [key, input] : buttons)
			{
				const InputBinding* binding = pad->find(input);
				int value = binding != nullptr ? joystickValue(*binding) : -1;
				if (value >= 0)
					toml.set("Instance0.Joystick", key, std::to_string(value));
			}
		}
	}

	Command MelonDSGenerator::configure(const LaunchContext& ctx)
	{
		fs::path home = ctx.paths.configFor("melonds");
		fs::path saves = ctx.paths.saves / "nds";

		auto program = util::emulatorCommand(ctx, "melonds", { "melonDS", "melonds" }, {
			home, saves, ctx.paths.bios, ctx.game.rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "melonDS is not installed: install it from the emulator catalog or with your package manager.");

		IniFile toml = IniFile::load(settingsPath(ctx));
		writeSettings(ctx, toml);
		try
		{
			fs::create_directories(saves / "cheats");
			toml.save(settingsPath(ctx));
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the melonDS configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *program;
		cmd.argv.insert(cmd.argv.end(), { "-f", ctx.game.rom.string() });

		cmd.env["XDG_CONFIG_HOME"] = home.string();
		cmd.env["XDG_DATA_HOME"] = saves.string();
		cmd.closeOnHotkey = true;   // its hotkeys are on the keyboard
		util::setPadEnvironment(cmd, ctx.controllers);
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
