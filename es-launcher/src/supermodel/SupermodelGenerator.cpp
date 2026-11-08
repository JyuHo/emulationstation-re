// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.supermodel), Copyright (c) 2026 The Batocera Team.

#include "supermodel/SupermodelGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Gamepad.h"

#include <algorithm>
#include <system_error>

namespace es::launcher::supermodel
{
	namespace fs = std::filesystem;
	using util::IniFile;

	ES_LAUNCHER_REGISTER_GENERATOR("supermodel", SupermodelGenerator)

	namespace
	{
		fs::path homeDir(const LaunchContext& ctx) { return ctx.paths.configFor("supermodel"); }
		fs::path savesDir(const LaunchContext& ctx) { return ctx.paths.saves / "model3"; }

		// "JOY1_BUTTON1": a control of the SDL gamepad, numbered from 1 in SDL's order
		std::string joy(const Controller& pad, const char* control)
		{
			return "JOY" + std::to_string(std::max(pad.deviceIndex, 0) + 1) + "_" + control;
		}

		// the keyboard key, then the pad's control
		std::string both(const char* key, const std::string& pad)
		{
			return std::string(key) + "," + pad;
		}

		const Controller* player(const LaunchContext& ctx, int number)
		{
			auto it = std::ranges::find_if(ctx.controllers, [number](const Controller& pad) { return pad.player == number; });
			return it == ctx.controllers.end() ? nullptr : &*it;
		}

		void bindPlayer1(IniFile& ini, const Controller& pad)
		{
			auto set = [&ini](const char* key, const std::string& value) { ini.set("Global", key, value); };
			std::string start = joy(pad, "BUTTON8"), select = joy(pad, "BUTTON7");
			std::string up = joy(pad, "POV1_UP"), down = joy(pad, "POV1_DOWN"), left = joy(pad, "POV1_LEFT"), right = joy(pad, "POV1_RIGHT");
			std::string south = joy(pad, "BUTTON1"), east = joy(pad, "BUTTON2"), west = joy(pad, "BUTTON3"), north = joy(pad, "BUTTON4");
			std::string l1 = joy(pad, "BUTTON5"), r1 = joy(pad, "BUTTON6"), l3 = joy(pad, "BUTTON9"), r3 = joy(pad, "BUTTON10");
			std::string l2 = joy(pad, "ZAXIS_POS"), r2 = joy(pad, "RZAXIS_POS");
			std::string leftX = joy(pad, "XAXIS"), leftY = joy(pad, "YAXIS"), rightX = joy(pad, "RXAXIS"), rightY = joy(pad, "RYAXIS");

			set("InputStart1", both("KEY_1", start));
			set("InputCoin1", both("KEY_3", select));
			set("InputServiceA", both("KEY_5", l3));
			set("InputTestA", both("KEY_6", r3));

			set("InputJoyUp", both("KEY_UP", up));
			set("InputJoyDown", both("KEY_DOWN", down));
			set("InputJoyLeft", both("KEY_LEFT", left));
			set("InputJoyRight", both("KEY_RIGHT", right));

			// fighting games (Virtua Fighter 3, Fighting Vipers 2)
			set("InputPunch", both("KEY_A", west));
			set("InputKick", both("KEY_S", north));
			set("InputGuard", both("KEY_D", south));
			set("InputEscape", both("KEY_F", east));

			// Spikeout
			set("InputShift", both("KEY_A", south));
			set("InputBeat", both("KEY_S", west));
			set("InputCharge", both("KEY_D", north));
			set("InputJump", both("KEY_F", east));

			// Virtua Striker
			set("InputShortPass", both("KEY_A", south));
			set("InputLongPass", both("KEY_S", west));
			set("InputShoot", both("KEY_D", east));

			// driving games: the stick steers, the triggers are the pedals
			set("InputSteering", leftX);
			set("InputAccelerator", both("KEY_UP", r2));
			set("InputBrake", both("KEY_DOWN", l2));
			set("InputGearShiftUp", both("KEY_Y", r1));
			set("InputGearShiftDown", both("KEY_H", l1));
			// the right stick is the 4 speed H-pattern
			set("InputGearShift1", both("KEY_Q", joy(pad, "RXAXIS_NEG")));
			set("InputGearShift2", both("KEY_W", joy(pad, "RYAXIS_POS")));
			set("InputGearShift3", both("KEY_E", joy(pad, "RYAXIS_NEG")));
			set("InputGearShift4", both("KEY_R", joy(pad, "RXAXIS_POS")));
			set("InputVR1", both("KEY_A", up));
			set("InputVR2", both("KEY_S", down));
			set("InputVR3", both("KEY_D", left));
			set("InputVR4", both("KEY_F", right));
			set("InputViewChange", both("KEY_A", south));
			set("InputHandBrake", both("KEY_S", east));
			set("InputRearBrake", both("KEY_S", east));
			set("InputMusicSelect", both("KEY_D", north));

			// analog joysticks and guns: the sticks, else the mouse
			set("InputAnalogJoyX", both("MOUSE_XAXIS", leftX));
			set("InputAnalogJoyY", both("MOUSE_YAXIS", leftY));
			set("InputGunX", both("MOUSE_XAXIS", rightX));
			set("InputGunY", both("MOUSE_YAXIS", rightY));
			set("InputAnalogGunX", both("MOUSE_XAXIS", rightX));
			set("InputAnalogGunY", both("MOUSE_YAXIS", rightY));
			set("InputAnalogJoyTrigger", both("KEY_A,MOUSE_LEFT_BUTTON", r2));
			set("InputTrigger", both("KEY_A,MOUSE_LEFT_BUTTON", r2));
			set("InputAnalogTriggerLeft", both("KEY_A,MOUSE_LEFT_BUTTON", r2));
			set("InputAnalogJoyEvent", both("KEY_S,MOUSE_RIGHT_BUTTON", l2));
			set("InputOffscreen", both("KEY_S,MOUSE_RIGHT_BUTTON", l2));
			set("InputAnalogTriggerRight", both("KEY_S,MOUSE_RIGHT_BUTTON", l2));
		}

		void bindPlayer2(IniFile& ini, const Controller& pad)
		{
			auto set = [&ini](const char* key, const std::string& value) { ini.set("Global", key, value); };
			std::string south = joy(pad, "BUTTON1"), east = joy(pad, "BUTTON2"), west = joy(pad, "BUTTON3"), north = joy(pad, "BUTTON4");

			set("InputStart2", both("KEY_2", joy(pad, "BUTTON8")));
			set("InputCoin2", both("KEY_4", joy(pad, "BUTTON7")));
			set("InputJoyUp2", joy(pad, "POV1_UP"));
			set("InputJoyDown2", joy(pad, "POV1_DOWN"));
			set("InputJoyLeft2", joy(pad, "POV1_LEFT"));
			set("InputJoyRight2", joy(pad, "POV1_RIGHT"));
			set("InputPunch2", west);
			set("InputKick2", north);
			set("InputGuard2", south);
			set("InputEscape2", east);
			set("InputShortPass2", south);
			set("InputLongPass2", west);
			set("InputShoot2", east);
		}

		// copies source over target when target is missing or older
		void refresh(const fs::path& source, const fs::path& target)
		{
			std::error_code ec;
			if (!fs::exists(target, ec) || fs::last_write_time(source, ec) > fs::last_write_time(target, ec))
				fs::copy_file(source, target, fs::copy_options::overwrite_existing);
		}

		// a folder of Supermodel's working directory that lives in the saves
		void linkToSaves(const fs::path& link, const fs::path& target)
		{
			fs::create_directories(target);
			std::error_code ec;
			if (!fs::exists(fs::symlink_status(link, ec)))
				fs::create_directory_symlink(target, link);
		}
	}

	std::vector<std::string> SupermodelGenerator::arguments(const LaunchContext& ctx)
	{
		const Settings& s = ctx.settings;
		bool arm = ctx.architecture == Architecture::AArch64;

		std::vector<std::string> args = { "-fullscreen", "-channels=" + s.getString("m3_audio_channels", "2") };

		// read by the builds that have them, ignored by the others
		if (auto backend = s.get("graphics_backend"); backend && !backend->empty())
			args.push_back("-graphics-backend=" + *backend);
		args.emplace_back("-new3d");
		if (auto scsp = s.getString("m3_scsp"); scsp == "legacy")
			args.emplace_back("-legacy-scsp");
		else if (!scsp.empty())
			args.emplace_back("-new-scsp");

		if (s.getBool("m3_wideScreen"))
			args.insert(args.end(), { "-wide-screen", "-wide-bg" });
		// too slow on the GLES of ARM boards
		if (!arm && s.getBool("quadRendering"))
			args.emplace_back("-quad-rendering");
		if (auto ss = s.getString("m3_supersampling"); !ss.empty() && ss != "1")
			args.push_back("-ss=" + ss);
		if (auto scale = s.getString("m3_renderscale", arm ? "1" : "0"); scale != "0")
			args.push_back("-render-scale=" + scale);
		if (s.getBool("m3_stretch"))
			args.emplace_back("-stretch");
		args.emplace_back(s.getBool("m3_vsync", true) ? "-vsync" : "-no-vsync");
		if (s.getBool("m3_true_hz"))
			args.emplace_back("-true-hz");
		if (s.getBool("m3_no_white_flash"))
			args.emplace_back("-no-white-flash");
		if (auto crosshairs = s.getString("crosshairs"); !crosshairs.empty())
			args.push_back("-crosshairs=" + crosshairs);
		// stated either way: the ini may turn it on
		args.emplace_back(s.getBool("forceFeedback") ? "-force-feedback" : "-no-force-feedback");
		if (auto freq = s.getString("ppcFreq"); !freq.empty())
			args.push_back("-ppc-frequency=" + freq);
		if (auto colors = s.getString("crt_colour"); !colors.empty())
			args.push_back("-crtcolors=" + colors);
		if (auto upscale = s.getString("upscale_mode"); !upscale.empty())
			args.push_back("-upscalemode=" + upscale);

		if (ctx.screenWidth > 0 && ctx.screenHeight > 0)
			args.push_back("-res=" + std::to_string(ctx.screenWidth) + "," + std::to_string(ctx.screenHeight));
		args.push_back("-log-output=" + (homeDir(ctx) / "Supermodel.log").string());
		args.push_back(ctx.game.rom.string());
		return args;
	}

	void SupermodelGenerator::writeSettings(const LaunchContext& ctx, IniFile& ini)
	{
		const Settings& s = ctx.settings;
		fs::path home = homeDir(ctx);
		fs::path saves = savesDir(ctx);

		ini.set("Global", "AnalysisPath", (home / "Analysis").string());
		ini.set("Global", "NVRAMPath", (saves / "NVRAM").string());
		ini.set("Global", "SavesPath", (saves / "Saves").string());
		ini.set("Global", "ScreenshotsPath", (ctx.paths.screenshots / "supermodel").string());
		ini.set("Global", "AssetsPath", (home / "Assets").string());
		ini.set("Global", "LogPath", home.string());

		// SDL_GAMECONTROLLERCONFIG maps every pad onto the standard gamepad
		ini.set("Global", "InputSystem", "sdlgamepad");
		std::string game = ctx.game.rom.stem().string();
		if (ini.hasSection(game))
			ini.set(game, "InputSystem", "sdlgamepad");

		// outputs for cabinet lamps and force feedback programs (MAME's network protocol)
		if (s.getString("m3_outputs", "none") == "net")
		{
			ini.set("Global", "Outputs", "net");
			ini.set("Global", "OutputsWithLF", s.getBool("m3_outputs_lf") ? "true" : "false");
			ini.set("Global", "OutputsTCPPort", s.getString("m3_outputs_tcp", "8000"));
			ini.set("Global", "OutputsUDPBroadcastPort", s.getString("m3_outputs_udp", "8001"));
		}
		else
			ini.set("Global", "Outputs", "none");

		if (const Controller* pad = player(ctx, 1))
			bindPlayer1(ini, *pad);
		if (const Controller* pad = player(ctx, 2))
			bindPlayer2(ini, *pad);
	}

	Command SupermodelGenerator::configure(const LaunchContext& ctx)
	{
		fs::path home = homeDir(ctx);
		fs::path saves = savesDir(ctx);
		fs::path config = home / "Config";

		auto program = util::emulatorCommand(ctx, "supermodel", { "supermodel", "Supermodel" }, {
			home, saves, ctx.paths.screenshots, ctx.game.rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "Supermodel is not installed: install it with your package manager (supermodel).");

		try
		{
			fs::create_directories(config);
			fs::create_directories(home / "Analysis");
			fs::create_directories(ctx.paths.screenshots / "supermodel");
			linkToSaves(home / "NVRAM", saves / "NVRAM");
			linkToSaves(home / "Saves", saves / "Saves");

			// Games.xml and the stock ini, from the install
			std::vector<fs::path> shares = { "/usr/share/supermodel/Config", "/usr/share/supermodel", "/usr/local/share/supermodel/Config",
				"/usr/local/share/supermodel" };
			if (fs::path binary = (*program)[0]; binary.is_absolute())
				shares.insert(shares.begin(), binary.parent_path() / "Config");
			for (const fs::path& share : shares)
			{
				std::error_code ec;
				if (!fs::is_regular_file(share / "Games.xml", ec))
					continue;
				refresh(share / "Games.xml", config / "Games.xml");
				if (fs::is_regular_file(share / "Supermodel.ini", ec) && !fs::exists(config / "Supermodel.ini", ec))
					fs::copy_file(share / "Supermodel.ini", config / "Supermodel.ini");
				break;
			}
			if (!fs::exists(config / "Games.xml"))
				throw LaunchError(ErrorCode::InvalidConfiguration, "Supermodel's Games.xml was not found: copy it to " + config.string() + ".");

			IniFile ini = IniFile::load(config / "Supermodel.ini");
			writeSettings(ctx, ini);
			ini.save(config / "Supermodel.ini");
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the Supermodel configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *program;
		std::vector<std::string> args = arguments(ctx);
		cmd.argv.insert(cmd.argv.end(), args.begin(), args.end());
		cmd.workingDirectory = home;

		cmd.closeOnHotkey = true;   // its hotkeys are on the keyboard
		util::setPadEnvironment(cmd, ctx.controllers);
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
