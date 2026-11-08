// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#include "Check.h"
#include "Helpers.h"
#include "es-launcher/Launcher.h"
#include "retroarch/ConfigFile.h"
#include "retroarch/Cores.h"
#include "retroarch/RetroArchGenerator.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

using namespace es::launcher;
using namespace es::launcher::retroarch;
namespace fs = std::filesystem;

using namespace tests;

static void testConfigFile()
{
	TempDir tmp;
	fs::path file = tmp.path / "a" / "retroarch.cfg";
	writeFile(file, "# comment\nvideo_driver = \"gl\"\nmenu_driver=ozone\n\nbroken line\naudio_volume = \"-3.0\"\n");

	ConfigFile config = ConfigFile::load(file);
	CHECK(config.size() == 3);
	CHECK(config.get("video_driver") == "gl");
	CHECK(config.get("menu_driver") == "ozone");
	CHECK(config.get("audio_volume") == "-3.0");

	config.set("video_driver", "glcore");      // keeps its place
	config.set("video_fullscreen", true);      // appended
	config.remove("menu_driver");
	config.save(file);

	CHECK(readFile(file) == "video_driver = \"glcore\"\naudio_volume = \"-3.0\"\nvideo_fullscreen = \"true\"\n");
	CHECK(!fs::exists(file.string() + ".tmp"));

	CHECK(ConfigFile::load(tmp.path / "missing.cfg").size() == 0);
}

static void testCoreNames()
{
	CHECK(coreLibraryName("mupen64plus-next") == "mupen64plus_next_libretro.so");
	CHECK(coreLibraryName("snes9x") == "snes9x_libretro.so");
}

static void testControllers()
{
	LaunchContext ctx;
	ctx.core = "snes9x";
	ctx.game.system = "snes";
	ctx.controllers = { makePad(1, 0), makePad(2, 3) };

	ConfigFile config;
	config.set("input_player5_a_btn", "9");    // a pad from a previous launch
	config.set("input_player1_a", "x");        // keyboard binding, forgotten too
	RetroArchGenerator::setControllers(ctx, config);

	CHECK(!config.get("input_player5_a_btn"));
	CHECK(!config.get("input_player1_a"));
	CHECK(config.get("input_player1_a_btn") == "0");
	CHECK(config.get("input_player1_l_btn") == "4");
	CHECK(config.get("input_player1_l2_axis") == "+2");
	CHECK(config.get("input_player1_up_btn") == "h0up");
	CHECK(config.get("input_player1_right_btn") == "h0right");
	CHECK(config.get("input_player1_l_y_minus_axis") == "-1");
	CHECK(config.get("input_player1_l_y_plus_axis") == "+1");
	CHECK(config.get("input_player1_joypad_index") == "0");
	CHECK(config.get("input_player2_joypad_index") == "3");
	CHECK(config.get("input_player1_analog_dpad_mode") == "1");   // hat d-pad: stick drives it too
	CHECK(config.get("input_enable_hotkey_btn") == "8");
	CHECK(config.get("input_exit_emulator_btn") == "7");
	CHECK(config.get("input_menu_toggle_btn") == "1");
	CHECK(config.get("input_libretro_device_p1") == "1");
	CHECK(config.get("input_libretro_device_p2") == "1");

	// fight stick layout
	ctx.settings = Settings({ { "altlayout", "fightstick" } }, {}, {});
	RetroArchGenerator::setControllers(ctx, config);
	CHECK(config.get("input_player1_l2_btn") == "4");
	CHECK(config.get("input_player1_l_btn") == "5");

	// N64: analog stick stays analog, Z moves to L1 on pads without a right trigger
	LaunchContext n64;
	n64.core = "mupen64plus-next";
	n64.game.system = "n64";
	Controller pad = makePad(1, 0);
	pad.inputs.erase("r2");
	n64.controllers = { pad };
	ConfigFile c2;
	RetroArchGenerator::setControllers(n64, c2);
	CHECK(c2.get("input_player1_analog_dpad_mode") == "0");
	CHECK(c2.get("input_player1_l2_btn") == "4");
}

static void testVideoAndStates()
{
	LaunchContext ctx;
	ctx.architecture = Architecture::AArch64;
	ctx.settings = Settings({ { "ratio", "16/9" }, { "smooth", "1" }, { "autosave", "0" } }, { { "runahead", "2" } }, {});
	ctx.saveState.slot = 3;

	ConfigFile config;
	RetroArchGenerator::setVideoAndAudio(ctx, config);
	RetroArchGenerator::setSaveStates(ctx, config);

	CHECK(config.get("video_driver") == "gl");
	CHECK(config.get("aspect_ratio_index") == "1");
	CHECK(config.get("video_smooth") == "true");
	CHECK(config.get("run_ahead_enabled") == "true");
	CHECK(config.get("run_ahead_frames") == "2");
	CHECK(config.get("state_slot") == "3");
	CHECK(config.get("savestate_auto_load") == "false");

	ctx.settings = Settings({ { "ratio", "nonsense" } }, {}, {});
	ctx.architecture = Architecture::X86_64;
	RetroArchGenerator::setVideoAndAudio(ctx, config);
	CHECK(config.get("aspect_ratio_index") == "22");   // core ratio
	CHECK(config.get("video_driver") == "glcore");

	// cores that need another GL driver, unless the player chose one
	ctx.core = "yabasanshiro";
	RetroArchGenerator::setVideoAndAudio(ctx, config);
	CHECK(config.get("video_driver") == "gl");
	ctx.core = "kronos";
	ctx.architecture = Architecture::AArch64;
	RetroArchGenerator::setVideoAndAudio(ctx, config);
	CHECK(config.get("video_driver") == "glcore");
	ctx.settings = Settings({ { "gfxbackend", "vulkan" } }, {}, {});
	RetroArchGenerator::setVideoAndAudio(ctx, config);
	CHECK(config.get("video_driver") == "vulkan");

	// no rewind nor run-ahead for PicoDrive's 32X
	ctx.core = "picodrive";
	ctx.game.system = "sega32x";
	ctx.settings = Settings({ { "rewind", "1" }, { "runahead", "1" } }, {}, {});
	RetroArchGenerator::setVideoAndAudio(ctx, config);
	CHECK(config.get("rewind_enable") == "false");
	CHECK(config.get("run_ahead_frames") == "0");
	ctx.game.system = "megadrive";
	RetroArchGenerator::setVideoAndAudio(ctx, config);
	CHECK(config.get("rewind_enable") == "true");
}

static void testGameOptions()
{
	TempDir tmp;
	writeFile(tmp.path / "user" / "crt.filt", "");
	writeFile(tmp.path / "system" / "crt.filt", "");
	writeFile(tmp.path / "system" / "Scanline2x.filt", "");

	LaunchContext ctx;
	ctx.paths.videoFilters = { tmp.path / "user", tmp.path / "system" };
	ctx.controllers = { makePad(1, 0) };

	// video filters: the player's folder first, "none" clears the one set in RetroArch, auto leaves it
	ConfigFile config;
	ctx.settings = Settings({ { "videofilters", "Scanline2x" } }, {}, { { "videofilters", "crt" } });
	RetroArchGenerator::setVideoAndAudio(ctx, config);
	CHECK(config.get("video_filter") == (tmp.path / "user" / "crt.filt").string());

	ctx.settings = Settings({ { "videofilters", "Scanline2x" } }, {}, {});
	RetroArchGenerator::setVideoAndAudio(ctx, config);
	CHECK(config.get("video_filter") == (tmp.path / "system" / "Scanline2x.filt").string());

	ctx.settings = Settings({ { "videofilters", "none" } }, {}, {});
	RetroArchGenerator::setVideoAndAudio(ctx, config);
	CHECK(config.get("video_filter") == "");

	ctx.settings = Settings({}, {}, {});
	RetroArchGenerator::setVideoAndAudio(ctx, config);
	CHECK(!config.get("video_filter"));

	// shader presets: the .slangp for glcore, the .glslp for gl, the other one when it is all there is
	writeFile(tmp.path / "shaders" / "crt" / "crt-royale.slangp", "");
	writeFile(tmp.path / "shaders" / "crt" / "crt-royale.glslp", "");
	writeFile(tmp.path / "shaders" / "scanlines.glslp", "");
	ctx.paths.shaders = { tmp.path / "shaders" };
	ctx.architecture = Architecture::X86_64;

	ctx.settings = Settings({ { "shaderset", "crt/crt-royale" } }, {}, {});
	CHECK(RetroArchGenerator::shaderPreset(ctx) == tmp.path / "shaders" / "crt" / "crt-royale.slangp");
	RetroArchGenerator::setVideoAndAudio(ctx, config);
	CHECK(config.get("video_shader_enable") == "true");

	ctx.settings = Settings({ { "shaderset", "crt/crt-royale" }, { "gfxbackend", "gl" } }, {}, {});
	CHECK(RetroArchGenerator::shaderPreset(ctx) == tmp.path / "shaders" / "crt" / "crt-royale.glslp");
	ctx.settings = Settings({ { "shaderset", "scanlines" } }, {}, {});
	CHECK(RetroArchGenerator::shaderPreset(ctx) == tmp.path / "shaders" / "scanlines.glslp");

	ctx.settings = Settings({ { "shaderset", "crt/crt-royale" } }, {}, { { "shaderset", "none" } });
	CHECK(!RetroArchGenerator::shaderPreset(ctx));
	RetroArchGenerator::setVideoAndAudio(ctx, config);
	CHECK(config.get("video_shader_enable") == "false");

	// decorations: the game's overlay, then the system's; a bare picture gets a generated .cfg
	writeFile(tmp.path / "deco" / "thebezelproject" / "GameBezels" / "SNES" / "Super Metroid (USA).cfg", "overlays = 1\n");
	writeFile(tmp.path / "deco" / "thebezelproject" / "GameBezels" / "SNES" / "Super Metroid (USA).png", "");
	writeFile(tmp.path / "deco" / "thebezelproject" / "snes.png", "");
	writeFile(tmp.path / "deco" / "thebezelproject" / "Genesis" / "Sonic (USA).png", "");
	ctx.paths.decorations = { tmp.path / "deco" };
	ctx.paths.emulatorConfigs = tmp.path / "emulators";
	ctx.game.system = "snes";

	ctx.game.rom = "/roms/snes/Super Metroid (USA).sfc";
	ctx.settings = Settings({ { "bezel", "thebezelproject" } }, {}, {});
	CHECK(RetroArchGenerator::decorationOverlay(ctx) == tmp.path / "deco" / "thebezelproject" / "GameBezels" / "SNES" / "Super Metroid (USA).cfg");
	ConfigFile deco;
	RetroArchGenerator::setDecoration(ctx, deco);
	CHECK(deco.get("input_overlay_enable") == "true");
	CHECK(deco.get("input_overlay") == (tmp.path / "deco" / "thebezelproject" / "GameBezels" / "SNES" / "Super Metroid (USA).cfg").string());
	CHECK(deco.get("aspect_ratio_index") == "0");

	ctx.game.rom = "/roms/snes/Zelda.sfc";
	RetroArchGenerator::setDecoration(ctx, deco);
	CHECK(deco.get("input_overlay") == (tmp.path / "emulators" / "retroarch" / "decoration.cfg").string());
	CHECK(readFile(tmp.path / "emulators" / "retroarch" / "decoration.cfg").find((tmp.path / "deco" / "thebezelproject" / "snes.png").string()) != std::string::npos);

	ctx.game.system = "megadrive";
	ctx.game.rom = "/roms/megadrive/Sonic (USA).md";
	CHECK(RetroArchGenerator::decorationOverlay(ctx) == tmp.path / "deco" / "thebezelproject" / "Genesis" / "Sonic (USA).png");

	ctx.game.rom = "/roms/megadrive/Other.md";
	CHECK(!RetroArchGenerator::decorationOverlay(ctx));
	RetroArchGenerator::setDecoration(ctx, deco);
	CHECK(deco.get("input_overlay_enable") == "false");

	ctx.settings = Settings({ { "bezel", "thebezelproject" } }, {}, { { "bezel", "none" } });
	ctx.game.system = "snes";
	CHECK(!RetroArchGenerator::decorationOverlay(ctx));

	// what the Updates & downloads menu installs, Batocera's layout: games/<system>/<rom>.png, systems/<system>.png
	writeFile(tmp.path / "deco2" / "thebezelproject" / "games" / "nes" / "Zelda (USA).png", "");
	writeFile(tmp.path / "deco2" / "thebezelproject" / "systems" / "nes.png", "");
	ctx.paths.decorations = { tmp.path / "deco2" };
	ctx.settings = Settings({ { "bezel", "thebezelproject" } }, {}, {});
	ctx.game.system = "nes";
	ctx.game.rom = "/roms/nes/Zelda (USA).nes";
	CHECK(RetroArchGenerator::decorationOverlay(ctx) == tmp.path / "deco2" / "thebezelproject" / "games" / "nes" / "Zelda (USA).png");
	ctx.game.rom = "/roms/nes/Metroid.nes";
	CHECK(RetroArchGenerator::decorationOverlay(ctx) == tmp.path / "deco2" / "thebezelproject" / "systems" / "nes.png");

	// AI translation: the target language goes into the service URL
	ctx.settings = Settings({ { "ai_service_enabled", "1" }, { "ai_target_lang", "Fr" }, { "ai_service_url", "http://localhost:4404/" }, { "ai_service_pause", "1" } }, {}, {});
	RetroArchGenerator::setTranslation(ctx, config);
	CHECK(config.get("ai_service_enable") == "true");
	CHECK(config.get("ai_service_url") == "http://localhost:4404/?target_lang=Fr");
	CHECK(config.get("ai_service_pause") == "true");

	ctx.settings = Settings({ { "ai_service_enabled", "1" }, { "ai_service_url", "http://host/translate?key=abc" } }, {}, {});
	RetroArchGenerator::setTranslation(ctx, config);
	CHECK(config.get("ai_service_url") == "http://host/translate?key=abc&target_lang=En");

	ctx.settings = Settings({}, {}, {});
	RetroArchGenerator::setTranslation(ctx, config);
	CHECK(config.get("ai_service_enable") == "false");

	// automatic controller configuration off: the player's own bindings stay
	ConfigFile pads;
	pads.set("input_player1_a_btn", "5");
	ctx.settings = Settings({ { "disableautocontrollers", "1" } }, {}, {});
	RetroArchGenerator::setControllers(ctx, pads);
	CHECK(pads.get("input_player1_a_btn") == "5");
	CHECK(pads.get("input_autodetect_enable") == "true");
}

static void testFullLaunch()
{
	TempDir tmp;
	fs::path bin = tmp.path / "bin";
	fs::path cores = tmp.path / "cores";
	writeFile(bin / "retroarch", "#!/bin/sh\n", true);
	writeFile(cores / "gambatte_libretro.so", "");

	std::string oldPath = std::getenv("PATH") ? std::getenv("PATH") : "";
	setenv("PATH", bin.c_str(), 1);
	setenv("ES_LIBRETRO_CORES_DIR", cores.c_str(), 1);

	LaunchContext ctx;
	ctx.emulator = "libretro";
	ctx.core = "gambatte";
	ctx.game.system = "gb";
	ctx.game.rom = "/roms/gb/Tetris (World).gb";
	ctx.paths.bios = tmp.path / "bios";
	ctx.paths.saves = tmp.path / "saves";
	ctx.paths.screenshots = tmp.path / "shots";
	ctx.paths.cache = tmp.path / "cache";
	ctx.paths.emulatorConfigs = tmp.path / "emulators";
	ctx.settings = Settings({ { "retroarch.menu_driver", "rgui" }, { "retroarchcore.gambatte_up_down_allowed", "enabled" } },
	                        { { "gb_colorization", "GB - SmartColor" } }, {});
	ctx.controllers = { makePad(1, 0) };

	fs::path configDir = tmp.path / "emulators" / "retroarch";
	writeFile(configDir / "retroarch.cfg", "menu_driver = \"ozone\"\nvideo_vsync = \"false\"\n");
	writeFile(configDir / "gb.cfg", "video_shader_enable = \"true\"\n");

	Launch launch(ctx);
	CHECK(launch.handled());
	Command cmd = launch.prepare();

	CHECK(cmd.argv.size() == 8);
	CHECK(cmd.argv[0] == (bin / "retroarch").string());
	CHECK(cmd.argv[1] == "-L");
	CHECK(cmd.argv[2] == (cores / "gambatte_libretro.so").string());
	CHECK(cmd.argv[4] == (configDir / "retroarch.cfg").string());
	CHECK(cmd.argv[5] == "--appendconfig");
	CHECK(cmd.argv[6] == (configDir / "gb.cfg").string());
	CHECK(cmd.argv[7] == "/roms/gb/Tetris (World).gb");

	ConfigFile written = ConfigFile::load(configDir / "retroarch.cfg");
	CHECK(written.get("video_vsync") == "false");        // the player's own setting survives
	CHECK(written.get("menu_driver") == "rgui");         // retroarch.* option wins
	CHECK(written.get("savefile_directory") == (tmp.path / "saves" / "gb").string());
	CHECK(written.get("system_directory") == (tmp.path / "bios").string());
	CHECK(written.get("libretro_directory") == cores.string());
	CHECK(fs::is_directory(tmp.path / "saves" / "gb"));

	ConfigFile options = ConfigFile::load(configDir / "retroarch-core-options.cfg");
	CHECK(options.get("gambatte_gb_colorization") == "auto");
	CHECK(options.get("gambatte_up_down_allowed") == "enabled");

	// a shader preset is passed on the command line, before the game
	LaunchContext shaded = ctx;
	writeFile(tmp.path / "shaders" / "crt.slangp", "");
	shaded.paths.shaders = { tmp.path / "shaders" };
	shaded.settings = Settings({ { "shaderset", "crt" } }, {}, {});
	Command shadedCmd = Launch(shaded).prepare();
	CHECK(std::ranges::find(shadedCmd.argv, "--set-shader") != shadedCmd.argv.end());
	CHECK(shadedCmd.argv[shadedCmd.argv.size() - 2] == (tmp.path / "shaders" / "crt.slangp").string());

	// missing core
	LaunchContext missing = ctx;
	missing.core = "mesen";
	try
	{
		Launch(missing).prepare();
		CHECK(!"a missing core must throw");
	}
	catch (const LaunchError& e)
	{
		CHECK(e.code() == ErrorCode::CoreMissing);
	}

	// netplay client and a save state to load
	LaunchContext net = ctx;
	net.netplay = Netplay{ Netplay::Mode::Client, "192.168.1.20", 55436, "pw", "" };
	net.saveState.slot = 2;
	net.saveState.file = "/saves/gb/Tetris.state2";
	Command netCmd = Launch(net).prepare();
	auto has = [&](std::initializer_list<std::string> seq) {
		return std::ranges::search(netCmd.argv, seq).begin() != netCmd.argv.end();
	};
	CHECK(has({ "--connect", "192.168.1.20" }));
	CHECK(has({ "--port", "55436" }));
	CHECK(has({ "-e", "2" }));
	CHECK(netCmd.argv.back() == "/roms/gb/Tetris (World).gb");

	// RetroArch not installed
	setenv("PATH", (tmp.path / "nothing").c_str(), 1);
	try
	{
		Launch(ctx).prepare();
		CHECK(!"a missing RetroArch must throw");
	}
	catch (const LaunchError& e)
	{
		CHECK(e.code() == ErrorCode::EmulatorMissing);
	}

	setenv("PATH", oldPath.c_str(), 1);
	unsetenv("ES_LIBRETRO_CORES_DIR");
}

// RetroArch and a core installed by ES's emulator catalog (installed.json, cores/)
static void testCatalogInstall()
{
	TempDir tmp;
	fs::path bin = tmp.path / "bin";
	fs::path catalogCores = tmp.path / "data" / "cores";
	fs::path appImage = tmp.path / "data" / "emulators" / "retroarch" / "retroarch.AppImage";
	writeFile(bin / "retroarch", "#!/bin/sh\n", true);        // the distribution's, must lose
	writeFile(bin / "flatpak", "#!/bin/sh\n", true);
	writeFile(appImage, "#!/bin/sh\n", true);
	writeFile(catalogCores / "snes9x_libretro.so", "");
	writeFile(catalogCores / "info" / "snes9x_libretro.info", "");

	std::string oldPath = std::getenv("PATH") ? std::getenv("PATH") : "";
	setenv("PATH", bin.c_str(), 1);
	unsetenv("ES_LIBRETRO_CORES_DIR");

	LaunchContext ctx;
	ctx.emulator = "libretro";
	ctx.core = "snes9x";
	ctx.game.system = "snes";
	ctx.game.rom = "/roms/snes/Game.sfc";
	ctx.paths.bios = tmp.path / "bios";
	ctx.paths.saves = tmp.path / "saves";
	ctx.paths.screenshots = tmp.path / "shots";
	ctx.paths.cache = tmp.path / "cache";
	ctx.paths.emulatorConfigs = tmp.path / "emulators";
	ctx.paths.cores = catalogCores;
	ctx.installedEmulators["retroarch"] = { appImage.string() };

	fs::path configDir = tmp.path / "emulators" / "retroarch";

	// the AppImage of the catalog, the core of the catalog, its .info files
	Command cmd = Launch(ctx).prepare();
	CHECK(cmd.argv[0] == appImage.string());
	CHECK(cmd.argv[1] == "-L");
	CHECK(cmd.argv[2] == (catalogCores / "snes9x_libretro.so").string());
	ConfigFile written = ConfigFile::load(configDir / "retroarch.cfg");
	CHECK(written.get("libretro_info_path") == (catalogCores / "info").string());
	CHECK(written.get("libretro_directory") == catalogCores.string());

	// a Flatpak gets access to what it reads and writes, before the application id
	ctx.installedEmulators["retroarch"] = { "flatpak", "run", "org.libretro.RetroArch" };
	cmd = Launch(ctx).prepare();
	auto ref = std::ranges::find(cmd.argv, std::string("org.libretro.RetroArch"));
	CHECK(cmd.argv[0] == (bin / "flatpak").string());
	CHECK(cmd.argv[1] == "run");
	CHECK(ref != cmd.argv.end());
	auto shared = [&](const fs::path& path) {
		return std::find(cmd.argv.begin(), ref, "--filesystem=" + path.lexically_normal().string()) != ref;
	};
	CHECK(shared(configDir));
	CHECK(shared(tmp.path / "saves"));
	CHECK(shared(tmp.path / "bios"));
	CHECK(shared(catalogCores));
	CHECK(shared("/roms/snes"));
	CHECK(ref + 1 != cmd.argv.end() && *(ref + 1) == "-L");

	// removed by hand: back to the one in PATH
	ctx.installedEmulators["retroarch"] = { (tmp.path / "gone.AppImage").string() };
	cmd = Launch(ctx).prepare();
	CHECK(cmd.argv[0] == (bin / "retroarch").string());

	setenv("PATH", oldPath.c_str(), 1);
}

static void testCoreProfiles()
{
	LaunchContext ctx;
	Controller pad = makePad(1, 0);   // d-pad on a hat
	ctx.controllers = { pad };

	// Nestopia: overscan crop and 4 player adapter
	{
		ConfigFile o;
		ctx.game.system = "nes";
		ctx.settings = Settings({ { "nestopia_cropoverscan", "h" } }, {}, {});
		coreProfile("nestopia").setCoreOptions(ctx, o);
		CHECK(o.get("nestopia_overscan_h_left") == "8");
		CHECK(o.get("nestopia_overscan_v_top") == "0");
		CHECK(o.get("nestopia_select_adapter") == "auto");
		CHECK(o.get("nestopia_palette") == "consumer");
	}

	// VBA-M: the hardware follows the system, under both names of the core
	{
		ConfigFile o;
		ctx.game.system = "gb";
		ctx.settings = Settings({ { "palettes", "weird colors" } }, {}, {});
		coreProfile("vbam").setCoreOptions(ctx, o);
		CHECK(o.get("vbam_gbHardware") == "gb");
		CHECK(o.get("vbam_palettes") == "wierd colors");

		ctx.game.system = "gba";
		coreProfile("vba-m").setCoreOptions(ctx, o);
		CHECK(o.get("vbam_gbHardware") == "gba");
		CHECK(o.get("vbam_solarsensor") == "0");
	}

	// bsnes: video filter only for bsnes, HD mode 7 only for bsnes_hd
	{
		ConfigFile o;
		ctx.game.system = "snes";
		ctx.settings = Settings({}, {}, {});
		coreProfile("bsnes").setCoreOptions(ctx, o);
		CHECK(o.get("bsnes_video_filter") == "None");
		CHECK(!o.get("bsnes_mode7_scale"));

		ConfigFile hd;
		coreProfile("bsnes_hd").setCoreOptions(ctx, hd);
		CHECK(!hd.get("bsnes_video_filter"));
		CHECK(hd.get("bsnes_mode7_scale") == "disable");
	}

	// SwanStation: renderer from the video driver, widescreen only in 16/9 without decoration
	{
		ConfigFile o;
		ctx.game.system = "psx";
		ctx.settings = Settings({ { "gfxbackend", "vulkan" }, { "swanstation_widescreen_hack", "true" }, { "ratio", "16/9" } }, {}, {});
		const Core& core = coreProfile("swanstation");
		core.setCoreOptions(ctx, o);
		CHECK(o.get("swanstation_GPU_Renderer") == "Vulkan");
		CHECK(o.get("swanstation_GPU_WidescreenHack") == "true");
		CHECK(o.get("swanstation_Display_AspectRatio") == "16:9");
		CHECK(core.deviceType(ctx, 1) == "1");
		CHECK(core.deviceType(ctx, 3) == std::nullopt);
		CHECK(core.analogDpadMode(ctx, pad) == 0);

		ctx.settings = Settings({ { "swanstation_widescreen_hack", "true" }, { "ratio", "16/9" }, { "bezel", "thebezelproject" }, { "swanstation_Controller1", "261" } }, {}, {});
		core.setCoreOptions(ctx, o);
		CHECK(o.get("swanstation_GPU_Renderer") == "Auto");
		CHECK(o.get("swanstation_GPU_WidescreenHack") == "false");
		CHECK(core.deviceType(ctx, 1) == "261");
		CHECK(core.analogDpadMode(ctx, pad) == 1);
	}

	// Beetle PSX: the options of both builds, multitap, controller type
	{
		ConfigFile o;
		ctx.settings = Settings({ { "beetle_psx_hw_internal_resolution", "4x" }, { "multitap_mednafen", "port2" }, { "beetle_psx_hw_Controller1", "517" } }, {}, {});
		const Core& core = coreProfile("mednafen_psx_hw");
		core.setCoreOptions(ctx, o);
		CHECK(o.get("beetle_psx_hw_internal_resolution") == "4x");
		CHECK(o.get("beetle_psx_internal_resolution") == "4x");
		CHECK(o.get("beetle_psx_hw_enable_multitap_port1") == "disabled");
		CHECK(o.get("beetle_psx_hw_enable_multitap_port2") == "enabled");
		CHECK(core.deviceType(ctx, 1) == "517");
		CHECK(core.deviceType(ctx, 2) == std::nullopt);
		CHECK(core.analogDpadMode(ctx, pad) == 0);
	}

	// PCSX ReARMed: frame skipping, NEON enhancement
	{
		ConfigFile o;
		ctx.settings = Settings({ { "frameskip_pcsx", "2" }, { "neon_enhancement", "enabled_with_speedhack" }, { "controller1_pcsx", "1" } }, {}, {});
		const Core& core = coreProfile("pcsx_rearmed");
		core.setCoreOptions(ctx, o);
		CHECK(o.get("pcsx_rearmed_frameskip_type") == "fixed_interval");
		CHECK(o.get("pcsx_rearmed_frameskip_interval") == "2");
		CHECK(o.get("pcsx_rearmed_neon_enhancement_enable") == "enabled");
		CHECK(o.get("pcsx_rearmed_neon_enhancement_no_main") == "enabled");
		CHECK(core.deviceType(ctx, 1) == "1");
		CHECK(core.analogDpadMode(ctx, pad) == 1);
	}
	// ParallelN64: GLideN64 unless Vulkan, the 64DD boots its IPL
	{
		ConfigFile o;
		ctx.game.system = "n64dd";
		ctx.settings = Settings({ { "gfxbackend", "vulkan" } }, {}, {});
		coreProfile("parallel_n64").setCoreOptions(ctx, o);
		CHECK(o.get("parallel-n64-gfxplugin") == "parallel");
		CHECK(o.get("parallel-n64-boot-device") == "64DD IPL");
		CHECK(o.get("parallel-n64-pak1") == "memory");
		CHECK(coreProfile("parallel_n64").analogDpadMode(ctx, pad) == 0);
	}

	// DeSmuME, Beetle VB (under both names), PokeMini
	{
		ConfigFile o;
		ctx.settings = Settings({ { "screens_layout", "left/right" }, { "2d_color_mode", "black & white" } }, {}, {});
		coreProfile("desmume").setCoreOptions(ctx, o);
		CHECK(o.get("desmume_screens_layout") == "left/right");
		CHECK(o.get("desmume_pointer_device_r") == "emulated");
		coreProfile("mednafen_vb").setCoreOptions(ctx, o);
		CHECK(o.get("vb_color_mode") == "black & white");
		coreProfile("pokemini").setCoreOptions(ctx, o);
		ctx.settings = Settings({ { "melonds_screen_layout", "Hybrid Bottom-Ratio3" } }, {}, {});
		coreProfile("melonds").setCoreOptions(ctx, o);
		CHECK(o.get("melonds_screen_layout") == "Hybrid Bottom");
		CHECK(o.get("melonds_hybrid_ratio") == "3");
		CHECK(o.get("pokemini_lcdfilter") == "dotmatrix");
	}
	// Genesis Plus GX: six button pads on the Mega Drive, Master System pads but not in the
	// expanded build, the MSU-MD add-on
	{
		ConfigFile o;
		ctx.settings = Settings({}, {}, {});
		ctx.game.system = "megadrive";
		const Core& gx = coreProfile("genesis_plus_gx");
		gx.setCoreOptions(ctx, o);
		CHECK(o.get("genesis_plus_gx_system_bram") == "per game");
		CHECK(o.get("genesis_plus_gx_add_on") == "auto");
		CHECK(gx.deviceType(ctx, 1) == "513");
		ctx.game.system = "mastersystem";
		CHECK(gx.deviceType(ctx, 2) == "769");
		CHECK(coreProfile("genesisplusgx-expanded").deviceType(ctx, 2) == std::nullopt);
		ctx.game.system = "megadrive-msu";
		gx.setCoreOptions(ctx, o);
		CHECK(o.get("genesis_plus_gx_add_on") == "sega/mega cd");
		CHECK(o.get("genesis_plus_gx_cdda_volume") == "70");

		// the Mega Drive layout when asked
		ConfigFile config;
		ctx.settings = Settings({ { "gx_controller1_mapping", "megadrive" } }, {}, {});
		gx.setConfig(ctx, config);
		CHECK(config.get("input_player1_btn_x") == "9");
	}

	// PicoDrive: the Backup RAM cartridge on the Mega-CD
	{
		ConfigFile o;
		ctx.settings = Settings({}, {}, {});
		ctx.game.system = "segacd";
		coreProfile("picodrive").setCoreOptions(ctx, o);
		CHECK(o.get("picodrive_ramcart") == "enabled");
		CHECK(o.get("picodrive_input1") == "6 button pad");
	}

	// Saturn: the 3D pad is 261 for Beetle and 5 for YabaSanshiro, whichever value was chosen
	{
		ctx.game.system = "saturn";
		ctx.settings = Settings({ { "controller1_saturn", "5" }, { "controller2_saturn", "261" } }, {}, {});
		CHECK(coreProfile("mednafen_saturn").deviceType(ctx, 1) == "261");
		CHECK(coreProfile("yabasanshiro").deviceType(ctx, 2) == "5");

		ButtonMap buttons = { { "pageup", "l" }, { "pagedown", "r" } };
		coreProfile("yabasanshiro").adjustButtons(ctx, pad, buttons);
		CHECK(buttons[0].second == "r");
		CHECK(buttons[1].second == "l");

		ConfigFile o;
		ctx.settings = Settings({ { "kronos_multitap", "port12" } }, {}, {});
		coreProfile("kronos").setCoreOptions(ctx, o);
		CHECK(o.get("kronos_multitap_port1") == "enabled");
		CHECK(o.get("kronos_multitap_port2") == "enabled");
	}

	// PPSSPP, Beetle PCE (both builds, each with its prefix), SuperGrafx, WonderSwan
	{
		ConfigFile o;
		ctx.game.system = "psp";
		ctx.settings = Settings({ { "ppsspp_resolution", "960x544" } }, {}, {});
		coreProfile("ppsspp").setCoreOptions(ctx, o);
		CHECK(o.get("ppsspp_internal_resolution") == "960x544");
		CHECK(coreProfile("ppsspp").defaultVideoDriver("glcore") == "gl");

		ctx.game.system = "pcengine";
		ctx.settings = Settings({ { "pce_nospritelimit", "disabled" }, { "controller1_pce", "2" } }, {}, {});
		coreProfile("mednafen_pce_fast").setCoreOptions(ctx, o);
		CHECK(o.get("pce_fast_nospritelimit") == "disabled");
		CHECK(!o.get("pce_nospritelimit"));
		coreProfile("pce").setCoreOptions(ctx, o);
		CHECK(o.get("pce_nospritelimit") == "disabled");
		CHECK(coreProfile("pce_fast").deviceType(ctx, 1) == "2");
		CHECK(coreProfile("mednafen_pce").deviceType(ctx, 2) == std::nullopt);

		ctx.settings = Settings({}, {}, {});
		CHECK(coreProfile("mednafen_pce").deviceType(ctx, 1) == "1");
		coreProfile("mednafen_supergrafx").setCoreOptions(ctx, o);
		CHECK(o.get("sgx_nospritelimit") == "enabled");

		ConfigFile config;
		ctx.game.system = "wswan";
		ctx.settings = Settings({ { "wswan_rotate_display", "portrait" } }, {}, {});
		coreProfile("mednafen_wswan").setCoreOptions(ctx, o);
		coreProfile("mednafen_wswan").setConfig(ctx, config);
		CHECK(o.get("wswan_rotate_display") == "portrait");
		CHECK(config.get("wswan_rotate_display") == "portrait");
	}

	// Arcade: FBNeo (Neo Geo BIOS, Neo Geo CD subsystem), MAME 2003-Plus, NeoCD, Flycast boards
	{
		ConfigFile o;
		ConfigFile config;
		ctx.game.system = "neogeo";
		ctx.game.rom = "/roms/neogeo/mslug.zip";
		ctx.settings = Settings({ { "fbneo-neogeo-mode-switch", "AES Japan" }, { "fbneo-frameskip", "1" } }, {}, {});
		const Core& fbneo = coreProfile("fbneo");
		fbneo.setCoreOptions(ctx, o);
		fbneo.setConfig(ctx, config);
		CHECK(o.get("fbneo-neogeo-mode") == "DIPSWITCH");
		CHECK(o.get("fbneo-dipswitch-mslug-BIOS") == "AES Japan");
		CHECK(o.get("fbneo-frameskip-type") == "Fixed");
		CHECK(o.get("fbneo-memcard-mode") == "per-game");
		CHECK(config.get("input_player1_select") == "num5");
		CHECK(fbneo.commandArguments(ctx).empty());
		ctx.game.system = "neogeocd";
		CHECK((fbneo.commandArguments(ctx) == std::vector<std::string>{ "--subsystem", "neocd" }));

		ctx.settings = Settings({}, {}, {});
		ctx.game.system = "neogeo";
		fbneo.setCoreOptions(ctx, o);
		CHECK(o.get("fbneo-neogeo-mode") == "UNIBIOS");

		ctx.game.system = "mame";
		coreProfile("mame2003_plus").setCoreOptions(ctx, o);
		CHECK(o.get("mame2003-plus_skip_disclaimer") == "enabled");
		CHECK(o.get("mame2003-plus_neogeo_bios") == "unibios33");

		ctx.game.system = "neogeocd";
		coreProfile("neocd").setCoreOptions(ctx, o);
		CHECK(o.get("neocd_per_content_saves") == "On");

		ctx.game.system = "naomi";
		ctx.settings = Settings({ { "screen_rotation_naomi", "vertical" }, { "reicast_allow_service_buttons", "enabled" } }, {}, {});
		coreProfile("flycast").setCoreOptions(ctx, o);
		CHECK(o.get("reicast_screen_rotation") == "vertical");
		ButtonMap buttons = { { "l3", "l3" }, { "r3", "r3" } };
		coreProfile("flycast").adjustButtons(ctx, pad, buttons);
		CHECK(buttons[0].second == "r3");
		ctx.game.system = "dreamcast";
		coreProfile("flycast").setCoreOptions(ctx, o);
		CHECK(o.get("reicast_screen_rotation") == "horizontal");
	}

	// Atari and 3DO
	{
		ConfigFile o;
		ctx.settings = Settings({ { "stella_palette", "z26" }, { "bios_vj", "disabled" } }, {}, {});
		ctx.game.system = "atari2600";
		coreProfile("stella").setCoreOptions(ctx, o);
		CHECK(o.get("stella_palette") == "z26");
		CHECK(o.get("stella_console") == "auto");
		coreProfile("handy").setCoreOptions(ctx, o);
		CHECK(o.get("handy_rot") == "None");
		coreProfile("virtualjaguar").setCoreOptions(ctx, o);
		CHECK(o.get("virtualjaguar_bios") == "disabled");

		ctx.game.system = "3do";
		ctx.game.rom = "/roms/3do/Myst (Disc 1).chd";
		coreProfile("opera").setCoreOptions(ctx, o);
		CHECK(o.get("opera_nvram_storage") == "shared");
		ctx.game.rom = "/roms/3do/Gex.chd";
		coreProfile("opera").setCoreOptions(ctx, o);
		CHECK(o.get("opera_nvram_storage") == "per game");

		const Core& atari = coreProfile("atari800");
		ctx.game.system = "atari5200";
		atari.setCoreOptions(ctx, o);
		CHECK(o.get("atari800_system") == "5200");
		CHECK(atari.deviceType(ctx, 2) == "513");
		CHECK(atari.deviceType(ctx, 3) == std::nullopt);
		ctx.game.system = "atari800";
		ctx.settings = Settings({ { "atari800_artifacting", "enabled" } }, {}, {});
		atari.setCoreOptions(ctx, o);
		CHECK(o.get("atari800_system") == "800XL (64K)");
		CHECK(o.get("atari800_artifacting_mode") == "blue/brown 1");
		CHECK(!o.get("atari800_resolution"));
	}

	// Computers: DOSBox Pure, ScummVM, blueMSX, PUAE, VICE
	{
		TempDir tmp;
		ConfigFile o;
		ConfigFile config;
		ctx.paths.bios = tmp.path / "bios";

		// a .pc folder starts with its batch file
		fs::path game = tmp.path / "Doom.pc";
		writeFile(game / "Doom.bat", "doom\n");
		ctx.game.system = "dos";
		ctx.game.rom = game;
		ctx.settings = Settings({ { "pure_cycles", "automatic" }, { "controller1_dosbox_pure", "3" } }, {}, {});
		const Core& pure = coreProfile("dosbox_pure");
		CHECK(pure.romArgument(ctx) == game / "Doom.bat");
		pure.setCoreOptions(ctx, o);
		pure.setConfig(ctx, config);
		CHECK(o.get("dosbox_pure_cycles") == "auto");
		CHECK(o.get("dosbox_pure_machine") == "svga");
		CHECK(config.get("input_player1_analog_dpad_mode") == "3");
		CHECK(pure.deviceType(ctx, 1) == "3");
		CHECK(pure.deviceType(ctx, 2) == std::nullopt);

		// an empty .scummvm file stands for the game id
		fs::path scumm = tmp.path / "monkey.scummvm";
		writeFile(scumm, "");
		ctx.game.system = "scummvm";
		ctx.game.rom = scumm;
		CHECK(coreProfile("scummvm").romArgument(ctx) == tmp.path / "monkey");

		ctx.game.system = "msx2";
		ctx.settings = Settings({}, {}, {});
		coreProfile("bluemsx").setCoreOptions(ctx, o);
		CHECK(o.get("bluemsx_msxtype") == "MSX2");
		CHECK(o.get("bluemsx_nospritelimits") == "ON");
		CHECK(o.get("bluemsx_overscan") == "MSX2");
		CHECK(coreProfile("bluemsx").deviceType(ctx, 1) == "1");

		// PUAE: the Kickstarts folder, the keyboard mapping, the CD32 pad
		const Core& puae = coreProfile("puae");
		ctx.game.system = "amiga1200";
		puae.setConfig(ctx, config);
		puae.setCoreOptions(ctx, o);
		CHECK(config.get("system_directory") == (tmp.path / "bios" / "amiga").string() + "/");
		CHECK(o.get("puae_model") == "A1200");
		CHECK(o.get("puae_mapper_start") == "RETROK_RETURN");
		CHECK(o.get("puae_mapper_x") == "RETROK_LALT");
		CHECK(o.get("puae_use_whdload_prefs") == "config");
		ctx.game.system = "amigacd32";
		puae.setCoreOptions(ctx, o);
		CHECK(o.get("puae_mapper_x") == "---");
		CHECK(o.get("puae_mapper_l2") == "MOUSE_LEFT_BUTTON");
		CHECK(o.get("puae_mapper_mouse_toggle") == "RETROK_RCTRL");
		CHECK(o.get("puae_model") == "CD32FR");
		CHECK(puae.deviceType(ctx, 1) == "517");
		CHECK(puae.deviceType(ctx, 2) == std::nullopt);

		// VICE: JiffyDOS once all its ROMs are there
		ctx.game.system = "c64";
		coreProfile("vice_x64").setCoreOptions(ctx, o);
		CHECK(o.get("vice_jiffydos") == "disabled");
		for (const char* rom : { "JiffyDOS_1541-II.bin", "JiffyDOS_1571_repl310654.bin", "JiffyDOS_1581.bin", "JiffyDOS_C64.bin" })
			writeFile(tmp.path / "bios" / "vice" / rom, "");
		coreProfile("vice_x64").setCoreOptions(ctx, o);
		CHECK(o.get("vice_jiffydos") == "enabled");
		CHECK(o.get("vice_mapper_start") == "RETROK_RETURN");
		ConfigFile scConfig;
		coreProfile("vice_x64sc").setConfig(ctx, scConfig);
		CHECK(!scConfig.get("input_player1_analog_dpad_mode"));

		// the other VICE machines
		coreProfile("vice_xvic").setCoreOptions(ctx, o);
		CHECK(o.get("vice_vic20_model") == "VIC20 PAL auto");
		CHECK(o.get("vice_vic20_external_palette") == "colodore_vic");
		coreProfile("vice_x128").setCoreOptions(ctx, o);
		CHECK(o.get("vice_c128_model") == "C128 PAL");
		CHECK(o.get("vice_jiffydos") == "disabled");   // JiffyDOS_C128.bin is missing

		// hatariB: a floppy is the content, a hard disk is linked in the BIOS folder
		const Core& hatarib = coreProfile("hatarib");
		ctx.game.system = "atarist";
		ctx.game.rom = tmp.path / "roms" / "Game.st";
		hatarib.setCoreOptions(ctx, o);
		CHECK(hatarib.romArgument(ctx) == ctx.game.rom);
		CHECK(o.get("hatarib_tos") == "<etos1024k>");
		CHECK(o.get("hatarib_hardboot") == "0");
		ctx.game.rom = tmp.path / "roms" / "Disk.HD";
		writeFile(ctx.game.rom, "");
		hatarib.setCoreOptions(ctx, o);
		CHECK(hatarib.romArgument(ctx).empty());
		CHECK(o.get("hatarib_hardimg") == "hatarib/hdd");
		CHECK(o.get("hatarib_hardtype") == "4");
		CHECK(fs::read_symlink(tmp.path / "bios" / "hatarib" / "hdd") == ctx.game.rom);
	}
}

void tests::runRetroArchTests()
{
	testConfigFile();
	testCoreNames();
	testControllers();
	testVideoAndStates();
	testGameOptions();
	testCoreProfiles();
	testFullLaunch();
	testCatalogInstall();
}
