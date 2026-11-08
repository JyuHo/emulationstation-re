// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch_libretro/cores/), Copyright (c) 2026 The Batocera Team.
//
// Profiles of libretro cores, one class per core; coreProfile() lists the names each one
// answers to (Batocera's and the library's). Light gun and wheel handling is left out for now.

#include "retroarch/Cores.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <map>
#include <memory>
#include <string_view>
#include <system_error>
#include <vector>

namespace es::launcher::retroarch
{
	std::string settingOr(const LaunchContext& ctx, std::string_view settingKey, std::string_view fallback)
	{
		return ctx.settings.getString(settingKey, fallback);
	}

	// no decoration around the game: the widescreen hacks need the whole screen
	static bool noBezel(const LaunchContext& ctx)
	{
		auto bezel = ctx.settings.get("bezel");
		return !bezel || *bezel == "none";
	}

	// "none", "h" or "both" -> horizontal and vertical overscan crop; the default crops 8 lines
	static std::pair<std::string, std::string> nesOverscan(const std::string& crop)
	{
		if (crop == "none")
			return { "0", "0" };
		if (crop == "h")
			return { "8", "0" };
		if (crop == "both")
			return { "8", "8" };
		return { "0", "8" };
	}

	// core option key = setting with the same name, else fallback
	static void option(const LaunchContext& ctx, ConfigFile& options, std::string_view key, std::string_view fallback)
	{
		options.set(key, settingOr(ctx, key, fallback));
	}

	// core option key = settingKey, else fallback
	static void option(const LaunchContext& ctx, ConfigFile& options, std::string_view key, std::string_view settingKey, std::string_view fallback)
	{
		options.set(key, settingOr(ctx, settingKey, fallback));
	}

	int Core::analogDpadMode(const LaunchContext& /*ctx*/, const Controller& pad) const
	{
		// pads whose d-pad is made of buttons or a hat get the stick on the d-pad as well
		for (const char* direction : { "up", "down", "left", "right" })
		{
			if (const InputBinding* input = pad.find(direction))
			{
				if (input->type == InputBinding::Type::Button || input->type == InputBinding::Type::Hat)
					return 1;
			}
		}
		return 0;
	}

	// Cores whose games read the analog stick themselves (N64, Dreamcast)
	class AnalogCore : public Core
	{
	public:
		int analogDpadMode(const LaunchContext&, const Controller&) const override { return 0; }
	};

	// ---------------------------------------------------------------------------------------------

	class Gambatte : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "gambatte_gb_bootloader", "gb_bootloader", "enabled");
			option(ctx, o, "gambatte_mix_frames", "gb_mix_frames", "disabled");

			const std::string& system = ctx.game.system;
			if (system == "gbc")
				option(ctx, o, "gambatte_gbc_color_correction", "gbc_color_correction", "disabled");
			else if (system == "gb")
				o.set("gambatte_gbc_color_correction", "disabled");

			if (system != "gb")
				return;

			// Colorization of Game Boy games
			std::string choice = settingOr(ctx, "gb_colorization", "GB - DMG");
			std::string colorization = "internal";
			std::string palette = "Special 1";

			if (choice == "GB - Disabled")
				colorization = "disabled";
			else if (choice == "GB - SmartColor")
				colorization = "auto";
			else if (choice == "GBC - Game Specific")
				colorization = "GBC";
			else if (choice == "custom")
				colorization = "custom";
			else if (choice != "none")
				palette = choice;

			o.set("gambatte_gb_colorization", colorization);
			o.set("gambatte_gb_internal_palette", palette);
		}
	};

	class Mgba : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			o.set("mgba_skip_bios", ctx.settings.getBool("skip_bios_mgba") ? "ON" : "OFF");
			o.set("mgba_force_gbp", ctx.settings.getBool("rumble_gain", true) ? "OFF" : "ON");

			if (ctx.game.system == "gba")
			{
				option(ctx, o, "mgba_solar_sensor_level", "solar_sensor_level", "0");
				option(ctx, o, "mgba_frameskip", "frameskip_mgba", "0");
			}

			if (ctx.game.system == "sgb")
			{
				o.set("mgba_gb_model", "Super Game Boy");
				o.set("mgba_sgb_borders", ctx.settings.getBool("sgb_borders", true) ? "ON" : "OFF");
			}
			else
				o.set("mgba_gb_model", "Autodetect");
		}
	};

	class VbaM : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			// the core's automatic hardware mode picks the wrong one: use the system
			const std::string& system = ctx.game.system;
			if (system == "gb" || system == "gbc" || system == "gba")
				o.set("vbam_gbHardware", system);

			if (system == "gb")
			{
				std::string palette = settingOr(ctx, "palettes", "black and white");
				o.set("vbam_palettes", palette == "weird colors" ? "wierd colors" : palette);
				option(ctx, o, "vbam_lcdfilter", "gbcoloroption_gb", "disabled");
			}
			else if (system == "gbc")
				option(ctx, o, "vbam_lcdfilter", "gbcoloroption_gbc", "disabled");
			else if (system == "gba")
			{
				option(ctx, o, "vbam_solarsensor", "solarsensor", "0");
				option(ctx, o, "vbam_gyro_sensitivity", "gyro_sensitivity", "10");
				option(ctx, o, "vbam_tilt_sensitivity", "tilt_sensitivity", "10");
			}
		}
	};

	class MelonDSDS : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "melonds_console_mode", "melondsds_console_mode", "ds");
			option(ctx, o, "melonds_render_mode", "melondsds_render_mode", "software");
			option(ctx, o, "melonds_opengl_resolution", "melondsds_resolution", "1");
			option(ctx, o, "melonds_opengl_better_polygons", "melondsds_poygon", "disabled");
			option(ctx, o, "melonds_opengl_filtering", "melondsds_filtering", "nearest");
			option(ctx, o, "melonds_show_cursor", "melondsds_cursor", "timeout");
			option(ctx, o, "melonds_cursor_timeout", "melondsds_cursor_timeout", "3");
			option(ctx, o, "melonds_touch_mode", "melondsds_touchmode", "auto");

			// one layout, both screens stacked
			o.set("melonds_number_of_screen_layouts", "1");
			o.set("melonds_screen_gap", "0");
			o.set("melonds_screen_layout1", "top-bottom");

			option(ctx, o, "melonds_firmware_language", "melondsds_language", "default");
			option(ctx, o, "melonds_firmware_favorite_color", "melondsds_colour", "default");
			option(ctx, o, "melonds_firmware_birth_month", "melondsds_month", "default");
			option(ctx, o, "melonds_firmware_birth_day", "melondsds_day", "default");

			option(ctx, o, "melonds_show_unsupported_features", "melondsds_show_unsupported", "disabled");
			option(ctx, o, "melonds_show_bios_warnings", "melondsds_show_bios", "disabled");
			option(ctx, o, "melonds_show_current_layout", "melondsds_show_layout", "disabled");
			option(ctx, o, "melonds_show_mic_state", "melondsds_show_mic", "disabled");
			option(ctx, o, "melonds_show_camera_state", "melondsds_show_camera", "disabled");
			option(ctx, o, "melonds_show_lid_state", "melondsds_show_lid", "disabled");
		}
	};

	class Desmume : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			o.set("desmume_pointer_device_r", "emulated");   // the stylus on the right stick
			option(ctx, o, "desmume_internal_resolution", "internal_resolution_desmume", "256x192");
			option(ctx, o, "desmume_gfx_multisampling", "multisampling", "disabled");
			option(ctx, o, "desmume_gfx_texture_smoothing", "texture_smoothing", "disabled");
			option(ctx, o, "desmume_gfx_texture_scaling", "texture_scaling", "1");
			option(ctx, o, "desmume_frameskip", "frameskip_desmume", "0");
			option(ctx, o, "desmume_screens_layout", "screens_layout", "top/bottom");
		}
	};

	// the older melonDS core (melonDS DS is MelonDSDS)
	class MelonDS : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "melonds_console_mode", "DS");
			option(ctx, o, "melonds_language", "English");
			option(ctx, o, "melonds_use_fw_settings", "disabled");
			o.set("melonds_threaded_renderer", "enabled");
			option(ctx, o, "melonds_touch_mode", "Joystick");
			option(ctx, o, "melonds_boot_directly", "enabled");

			// "Hybrid Top-Ratio3": the layout and the size of the large screen
			std::string layout = settingOr(ctx, "melonds_screen_layout", "Top/Bottom");
			std::string ratio = "2";
			if (layout.starts_with("Hybrid ") && layout.find("-Ratio") != std::string::npos)
			{
				ratio = layout.substr(layout.find("-Ratio") + 6);
				layout = layout.substr(0, layout.find("-Ratio"));
			}
			o.set("melonds_screen_layout", layout);
			o.set("melonds_hybrid_ratio", ratio);
		}
	};

	class VirtualBoy : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "vb_color_mode", "2d_color_mode", "black & red");
			option(ctx, o, "vb_anaglyph_preset", "3d_color_mode", "disabled");
		}
	};

	class PokeMini : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "pokemini_lcdfilter", "dotmatrix");
			option(ctx, o, "pokemini_lcdmode", "analog");
		}
	};

	class Mesen : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "mesen_region", "Auto");
			option(ctx, o, "mesen_screenrotation", "None");
			option(ctx, o, "mesen_ntsc_filter", "Disabled");
			option(ctx, o, "mesen_nospritelimit", "disabled");
			option(ctx, o, "mesen_palette", "Default");
			option(ctx, o, "mesen_hdpacks", "enabled");
			option(ctx, o, "mesen_fdsautoinsertdisk", "disabled");
			option(ctx, o, "mesen_fdsfastforwardload", "disabled");
			option(ctx, o, "mesen_ramstate", "All 0s (Default)");
			option(ctx, o, "mesen_overclock", "None");
			option(ctx, o, "mesen_overclock_type", "Before NMI (Recommended)");
		}
	};

	class Fceumm : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			o.set("fceumm_zapper_mode", "mouse");
			option(ctx, o, "fceumm_show_crosshair", "disabled");
			option(ctx, o, "fceumm_nospritelimit", "enabled");

			auto [h, v] = nesOverscan(settingOr(ctx, "fceumm_cropoverscan", ""));

			o.set("fceumm_overscan_h_left", h);
			o.set("fceumm_overscan_h_right", h);
			o.set("fceumm_overscan_v_top", v);
			o.set("fceumm_overscan_v_bottom", v);

			std::string palette = settingOr(ctx, "fceumm_palette", "default");
			o.set("fceumm_palette", palette == "nintendo-vc" ? "wii-vc" : palette);

			option(ctx, o, "fceumm_ntsc_filter", "disabled");
			option(ctx, o, "fceumm_sndquality", "Low");
			option(ctx, o, "fceumm_overclocking", "disabled");
		}

		std::optional<std::string> deviceType(const LaunchContext& ctx, int player) const override
		{
			if (player == 1)
				return settingOr(ctx, "controller1_nes", "1");
			if (player == 2)
				return settingOr(ctx, "controller2_nes", "1");
			return std::nullopt;
		}
	};

	class Nestopia : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			o.set("nestopia_zapper_device", "mouse");
			option(ctx, o, "nestopia_show_crosshair", "disabled");
			option(ctx, o, "nestopia_nospritelimit", "enabled");

			auto [h, v] = nesOverscan(settingOr(ctx, "nestopia_cropoverscan", ""));
			o.set("nestopia_overscan_h_left", h);
			o.set("nestopia_overscan_h_right", h);
			o.set("nestopia_overscan_v_top", v);
			o.set("nestopia_overscan_v_bottom", v);

			option(ctx, o, "nestopia_palette", "consumer");
			option(ctx, o, "nestopia_blargg_ntsc_filter", "disabled");

			std::string adapter = settingOr(ctx, "nestopia_select_adapter", "automatic");
			o.set("nestopia_select_adapter", adapter == "automatic" ? "auto" : adapter);
		}
	};

	class Snes9x : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "snes9x_reduce_sprite_flicker", "reduce_sprite_flicker", "enabled");
			option(ctx, o, "snes9x_overclock_cycles", "reduce_slowdown", "disabled");
			option(ctx, o, "snes9x_overclock_superfx", "overclock_superfx", "100%");
			option(ctx, o, "snes9x_hires_blend", "hires_blend", "disabled");
			option(ctx, o, "snes9x_blargg", "snes9x_blargg_filter", "disabled");
		}

		std::optional<std::string> deviceType(const LaunchContext& ctx, int player) const override
		{
			if (player == 1)
				return settingOr(ctx, "controller1_snes9x", "1");
			if (player == 2)
			{
				// more than two pads: plug the multitap in port 2
				std::string fallback = ctx.controllers.size() > 2 ? "257" : "1";
				return settingOr(ctx, "controller2_snes9x", fallback);
			}
			if (player == 3)
				return settingOr(ctx, "controller3_snes9x", "1");
			return std::nullopt;
		}
	};

	class Bsnes : public Core
	{
	public:
		explicit Bsnes(bool hd) : mHd(hd) { }

		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			const std::string& system = ctx.game.system;

			if (!mHd)
				option(ctx, o, "bsnes_video_filter", "None");

			// HD Mode 7 (bsnes_hd only)
			if (mHd && (system == "snes" || system == "snes-msu1"))
			{
				option(ctx, o, "bsnes_mode7_scale", "disable");
				option(ctx, o, "bsnes_mode7_supersample", "none");
				option(ctx, o, "bsnes_mode7_wsMode", "none");
			}

			if (system == "sgb-msu1")
			{
				option(ctx, o, "bsnes_sgb_bios", "SGB1.sfc");
				if (!mHd)
					option(ctx, o, "bsnes_hide_sgb_border", "OFF");
			}
		}

	private:
		bool mHd;
	};

	// Nintendo 64 pads (Switch Online, 8BitDo) whose buttons RetroArch must remap
	static bool isN64Pad(const Controller& pad)
	{
		static constexpr std::array guids = {
			"050000007e0500001920000001800000", "05000000c82d00006928000000010000", "030000007e0500001920000011810000",
			"05000000c82d00001930000001000000", "03000000c82d00001930000011010000",
		};
		static constexpr std::array names = {
			"N64 Controller", "Nintendo Co., Ltd. N64 Controller", "8BitDo N64 Modkit", "8BitDo 64 BT",
			"8BitDo 8BitDo 64 Bluetooth Controller",
		};
		return std::ranges::find(guids, pad.guid) != guids.end() && std::ranges::find(names, pad.name) != names.end();
	}

	// Mupen64Plus-Next and ParallelN64: Z on L1 for pads without a right trigger, and the buttons
	// moved around for Nintendo 64 pads or when the player picked an N64 controller layout
	class N64Core : public AnalogCore
	{
	public:
		explicit N64Core(std::string controllerOption) : mControllerOption(std::move(controllerOption)) { }

		void adjustButtons(const LaunchContext& ctx, const Controller& pad, ButtonMap& buttons) const override
		{
			// Z matters: without a right trigger, put it on L1
			if (ctx.game.system == "n64" && pad.find("r2") == nullptr)
			{
				for (auto& [es, ra] : buttons)
				{
					if (es == "pageup")
						ra = "l2";
					else if (es == "l2")
						ra = "l";
				}
			}
		}

		void setConfig(const LaunchContext& ctx, ConfigFile& config) const override
		{
			static const std::pair<const char*, const char*> remap[] = {
				{ "btn_a", "1" }, { "btn_b", "0" }, { "btn_x", "23" }, { "btn_y", "21" },
				{ "btn_l2", "22" }, { "btn_r2", "20" }, { "btn_select", "12" },
			};

			for (const Controller& pad : ctx.controllers)
			{
				if (pad.player > 4)
					break;

				if (isN64Pad(pad) || settingOr(ctx, mControllerOption + std::to_string(pad.player), "retropad") != "retropad")
				{
					for (const auto& [button, value] : remap)
						config.set("input_player" + std::to_string(pad.player) + "_" + button, value);
				}
			}
		}

	private:
		std::string mControllerOption;   // "<core>-controller", followed by the player number
	};

	class Mupen64PlusNext : public N64Core
	{
	public:
		Mupen64PlusNext() : N64Core("mupen64plus-controller") { }

		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			o.set("mupen64plus-ThreadedRenderer", "True");
			o.set("mupen64plus-txHiresEnable", "True");
			option(ctx, o, "mupen64plus-43screensize", "320x240");
			option(ctx, o, "mupen64plus-169screensize", "640x360");

			// widescreen hack only when the player asked for it and for a 16/9 ratio
			bool widescreen = settingOr(ctx, "mupen64plus-aspect", "") == "16:9 adjusted" && settingOr(ctx, "ratio", "") == "16/9";
			o.set("mupen64plus-aspect", widescreen ? "16:9 adjusted" : "4:3");

			option(ctx, o, "mupen64plus-BilinearMode", "standard");
			option(ctx, o, "mupen64plus-MultiSampling", "0");
			option(ctx, o, "mupen64plus-txFilterMode", "None");
			option(ctx, o, "mupen64plus-txEnhancementMode", "None");

			for (int pak = 1; pak <= 4; ++pak)
			{
				std::string key = "mupen64plus-pak" + std::to_string(pak);
				std::string fallback = pak == 1 ? "memory" : "none";
				std::string value = settingOr(ctx, key, fallback);
				if (value == "auto_rumble")
					value = fallback;   // needs the game database, not available yet
				o.set(key, value);
			}

			option(ctx, o, "mupen64plus-rdp-plugin", "mupen64plus-rdpPlugin", "gliden64");
			option(ctx, o, "mupen64plus-rsp-plugin", "mupen64plus-rspPlugin", "hle");
			option(ctx, o, "mupen64plus-cpucore", "mupen64plus-cpuCore", "dynamic_recompiler");
			option(ctx, o, "mupen64plus-Framerate", "Original");
			option(ctx, o, "mupen64plus-parallel-rdp-upscaling", "1x");
			option(ctx, o, "mupen64plus-astick-deadzone", "mupen64plus-deadzone", "15");
			option(ctx, o, "mupen64plus-astick-sensitivity", "mupen64plus-sensitivity", "100");
		}
	};

	class ParallelN64 : public N64Core
	{
	public:
		ParallelN64() : N64Core("parallel-n64-controller") { }

		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			bool dd = ctx.game.system == "n64dd";
			o.set("parallel-n64-64dd-hardware", dd ? "enabled" : "disabled");
			o.set("parallel-n64-boot-device", dd ? "64DD IPL" : "Default");

			option(ctx, o, "parallel-n64-gfxplugin", settingOr(ctx, "gfxbackend", "") == "vulkan" ? "parallel" : "gliden64");
			option(ctx, o, "parallel-n64-screensize", "320x240");
			option(ctx, o, "parallel-n64-upscaling", "1x");

			// widescreen hack only when the player asked for it, in 16/9 and without decoration
			bool wide = settingOr(ctx, "parallel-n64-aspectratiohint", "") == "widescreen" && settingOr(ctx, "ratio", "") == "16/9" && noBezel(ctx);
			o.set("parallel-n64-aspectratiohint", wide ? "widescreen" : "normal");

			option(ctx, o, "parallel-n64-filtering", "automatic");

			for (int pak = 1; pak <= 4; ++pak)
			{
				std::string key = "parallel-n64-pak" + std::to_string(pak);
				std::string fallback = pak == 1 ? "memory" : "none";
				std::string value = settingOr(ctx, key, fallback);
				if (value == "auto_rumble")
					value = fallback;   // needs the game database, not available yet
				o.set(key, value);
			}

			option(ctx, o, "parallel-n64-astick-deadzone", "parallel-n64-deadzone", "15");
			option(ctx, o, "parallel-n64-astick-sensitivity", "parallel-n64-sensitivity", "100");
		}
	};

	class Flycast : public AnalogCore
	{
	public:
		void adjustButtons(const LaunchContext& ctx, const Controller& pad, ButtonMap& buttons) const override
		{
			// triggers matter: without analog triggers, use the shoulder buttons
			if (ctx.game.system == "dreamcast" && pad.find("r2") == nullptr)
			{
				for (auto& [es, ra] : buttons)
				{
					if (es == "pageup")
						ra = "l2";
					else if (es == "l2")
						ra = "l";
					else if (es == "pagedown")
						ra = "r2";
					else if (es == "r2")
						ra = "r";
				}
			}

			// with the service buttons on, L3 is the service button and R3 the test button
			if (isArcade(ctx) && settingOr(ctx, "reicast_allow_service_buttons", "") == "enabled")
			{
				for (auto& [es, ra] : buttons)
				{
					if (es == "l3")
						ra = "r3";
					else if (es == "r3")
						ra = "l3";
				}
			}
		}

		void setConfig(const LaunchContext& ctx, ConfigFile& config) const override
		{
			if (ctx.game.system != "dreamcast")
				config.set("input_player1_select", "num5");   // coin on arcade boards

			for (int player = 1; player <= 4; ++player)
			{
				std::string p = std::to_string(player);
				std::string device = settingOr(ctx, "controller" + p + "_dc", "1");
				if (device == "5")   // gamepad driven by the left stick
				{
					config.set("input_libretro_device_p" + p, "1");
					config.set("input_player" + p + "_analog_dpad_mode", "3");
				}
				else
				{
					config.set("input_libretro_device_p" + p, device);
					config.set("input_player" + p + "_analog_dpad_mode", "0");
				}
			}
		}

		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			// one VMU file per game, in the saves folder
			o.set("reicast_per_content_vmus", "All VMUs");
			option(ctx, o, "reicast_enable_dsp", "reicast_dsp", "disabled");
			o.set("reicast_threaded_rendering", "enabled");
			option(ctx, o, "reicast_internal_resolution", "640x480");
			option(ctx, o, "reicast_mipmapping", "disabled");
			option(ctx, o, "reicast_anisotropic_filtering", "off");
			option(ctx, o, "reicast_alpha_sorting", "per-triangle (normal)");
			option(ctx, o, "reicast_texupscale", "1");
			option(ctx, o, "reicast_frame_skipping", "disabled");

			bool wide = settingOr(ctx, "ratio", "") == "16/9";
			o.set("reicast_widescreen_cheats", wide && settingOr(ctx, "reicast_widescreen_cheats", "") == "enabled" ? "enabled" : "disabled");
			o.set("reicast_widescreen_hack", wide && settingOr(ctx, "reicast_widescreen_hack", "") == "enabled" ? "enabled" : "disabled");

			option(ctx, o, "reicast_language", "Default");
			option(ctx, o, "reicast_region", "Default");
			option(ctx, o, "reicast_native_depth_interpolation", "disabled");

			// arcade boards: the screen orientation and the service and test buttons
			std::string rotation = "horizontal";
			if (ctx.game.system == "atomiswave")
				rotation = settingOr(ctx, "screen_rotation_atomiswave", rotation);
			else if (ctx.game.system == "naomi")
				rotation = settingOr(ctx, "screen_rotation_naomi", rotation);
			o.set("reicast_screen_rotation", rotation);
			option(ctx, o, "reicast_allow_service_buttons", "disabled");
		}

		static bool isArcade(const LaunchContext& ctx)
		{
			return ctx.game.system == "atomiswave" || ctx.game.system == "naomi" || ctx.game.system == "naomi2";
		}
	};

	// PlayStation controller type chosen for player 1 (setting key1) or 2 (key2)
	static std::optional<std::string> psxDevice(const LaunchContext& ctx, std::string_view key1, std::string_view key2, int player)
	{
		if (player == 1)
			return ctx.settings.get(key1);
		if (player == 2)
			return ctx.settings.get(key2);
		return std::nullopt;
	}

	// the digital pad ("1") takes the left stick on its d-pad, the analog ones read the sticks
	static int psxAnalogDpadMode(const std::string& device)
	{
		return device == "1" ? 1 : 0;
	}

	class Swanstation : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			std::string renderer = "Auto";
			if (ctx.settings.getBool("gpu_software"))
				renderer = "Software";
			else if (auto backend = ctx.settings.get("gfxbackend"))
			{
				if (*backend == "vulkan")
					renderer = "Vulkan";
				else if (*backend == "gl" || *backend == "glcore")
					renderer = "OpenGL";
			}
			o.set("swanstation_GPU_Renderer", renderer);

			option(ctx, o, "swanstation_BIOS_PatchFastBoot", "swanstation_PatchFastBoot", "false");
			option(ctx, o, "swanstation_GPU_ResolutionScale", "swanstation_resolution_scale", "1");
			option(ctx, o, "swanstation_GPU_PGXPEnable", "swanstation_pgxp", "true");
			option(ctx, o, "swanstation_GPU_MSAA", "swanstation_antialiasing", "1");
			option(ctx, o, "swanstation_GPU_TextureFilter", "swanstation_texture_filtering", "Nearest");

			bool wide = settingOr(ctx, "swanstation_widescreen_hack", "") == "true" && settingOr(ctx, "ratio", "") == "16/9" && noBezel(ctx);
			o.set("swanstation_GPU_WidescreenHack", wide ? "true" : "false");
			o.set("swanstation_Display_AspectRatio", wide ? "16:9" : "4:3");

			option(ctx, o, "swanstation_Display_CropMode", "swanstation_CropMode", "Overscan");
			option(ctx, o, "swanstation_Controller_ShowCrosshair", "false");
		}

		int analogDpadMode(const LaunchContext& ctx, const Controller& pad) const override
		{
			// the analog pads of the core (261, 517) take the left stick on the d-pad
			if (auto device = deviceType(ctx, pad.player))
				return *device == "261" || *device == "517" ? 1 : 0;
			return Core::analogDpadMode(ctx, pad);
		}

		std::optional<std::string> deviceType(const LaunchContext& ctx, int player) const override
		{
			auto device = psxDevice(ctx, "swanstation_Controller1", "swanstation_Controller2", player);
			if (!device && (player == 1 || player == 2))
				return "1";
			return device;
		}
	};

	class MednafenPsx : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			// the software build of the core reads the same options without "_hw"
			auto set = [&](const std::string& name, std::string_view value) {
				o.set("beetle_psx_hw_" + name, value);
				o.set("beetle_psx_" + name, value);
			};
			auto setFromSetting = [&](const std::string& name, std::string_view fallback) {
				set(name, settingOr(ctx, "beetle_psx_hw_" + name, fallback));
			};

			setFromSetting("cpu_freq_scale", "100%");
			setFromSetting("skip_bios", "disabled");
			setFromSetting("internal_resolution", "1x(native)");

			bool wide = settingOr(ctx, "beetle_psx_hw_widescreen_hack", "") == "enabled" && settingOr(ctx, "ratio", "") == "16/9" && noBezel(ctx);
			set("widescreen_hack", wide ? "enabled" : "disabled");

			setFromSetting("frame_duping", "disabled");
			setFromSetting("cpu_dynarec", "disabled");
			setFromSetting("dynarec_invalidate", "full");
			setFromSetting("pgxp_mode", "disabled");
			set("analog_calibration", "enabled");

			std::string multitap = settingOr(ctx, "multitap_mednafen", "");
			bool port1 = multitap == "port1" || multitap == "port12";
			bool port2 = multitap == "port2" || multitap == "port12";
			set("enable_multitap_port1", port1 ? "enabled" : "disabled");
			set("enable_multitap_port2", port2 ? "enabled" : "disabled");
		}

		int analogDpadMode(const LaunchContext& ctx, const Controller& pad) const override
		{
			if (auto device = psxDevice(ctx, "beetle_psx_hw_Controller1", "beetle_psx_hw_Controller2", pad.player))
				return psxAnalogDpadMode(*device);
			return Core::analogDpadMode(ctx, pad);
		}

		std::optional<std::string> deviceType(const LaunchContext& ctx, int player) const override
		{
			return psxDevice(ctx, "beetle_psx_hw_Controller1", "beetle_psx_hw_Controller2", player);
		}
	};

	class PcsxRearmed : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			o.set("pcsx_rearmed_show_input_settings", "enabled");
			o.set("pcsx_rearmed_vibration", "enabled");
			option(ctx, o, "pcsx_rearmed_show_bios_bootlogo", "show_bios_bootlogo", "disabled");

			std::string frameskip = settingOr(ctx, "frameskip_pcsx", "0");
			if (frameskip == "0")
				o.set("pcsx_rearmed_frameskip_type", "disabled");
			else
			{
				o.set("pcsx_rearmed_frameskip_type", "fixed_interval");
				o.set("pcsx_rearmed_frameskip_interval", frameskip);
			}

			// higher resolution on ARM (NEON renderer), optionally without the main pass
			std::string neon = settingOr(ctx, "neon_enhancement", "");
			bool enhanced = neon == "enabled" || neon == "enabled_with_speedhack";
			o.set("pcsx_rearmed_neon_enhancement_enable", enhanced ? "enabled" : "disabled");
			o.set("pcsx_rearmed_neon_enhancement_no_main", neon == "enabled_with_speedhack" ? "enabled" : "disabled");

			option(ctx, o, "pcsx_rearmed_multitap", "disabled");
			option(ctx, o, "pcsx_rearmed_crosshair1", "disabled");
			option(ctx, o, "pcsx_rearmed_crosshair2", "disabled");
		}

		int analogDpadMode(const LaunchContext& ctx, const Controller& pad) const override
		{
			if (auto device = psxDevice(ctx, "controller1_pcsx", "controller2_pcsx", pad.player))
				return psxAnalogDpadMode(*device);
			return Core::analogDpadMode(ctx, pad);
		}

		std::optional<std::string> deviceType(const LaunchContext& ctx, int player) const override
		{
			return psxDevice(ctx, "controller1_pcsx", "controller2_pcsx", player);
		}
	};

	// Mega Drive pads (8BitDo M30, Retro-Bit Saturn) whose buttons RetroArch must move: the six
	// face buttons in the Mega Drive order
	static bool isMegaDrivePad(const Controller& pad)
	{
		static constexpr std::array guids = {
			"05000000c82d00005106000000010000", "03000000c82d00000650000011010000", "050000005e0400008e02000030110000",
			"03000000c82d00000150000011010000", "05000000c82d00000151000000010000", "0500000049190000020400001b010000",
		};
		static constexpr std::array names = {
			"8BitDo M30 gamepad", "8Bitdo  8BitDo M30 gamepad", "8BitDo M30 Modkit", "8Bitdo  8BitDo M30 Modkit",
			"Retro Bit Bluetooth Controller",
		};
		return std::ranges::find(guids, pad.guid) != guids.end() && std::ranges::find(names, pad.name) != names.end();
	}

	// Genesis Plus GX and PicoDrive: the Mega Drive layout for those pads, or when the player
	// picked it (<prefix>_controller<N>_mapping, players 1 to 4)
	class MegaDriveCore : public Core
	{
	public:
		explicit MegaDriveCore(std::array<const char*, 4> mappingOptions) : mMappingOptions(mappingOptions) { }

		void setConfig(const LaunchContext& ctx, ConfigFile& config) const override
		{
			static const std::pair<const char*, const char*> remap[] = {
				{ "btn_a", "0" }, { "btn_b", "1" }, { "btn_x", "9" }, { "btn_y", "10" }, { "btn_l", "11" }, { "btn_r", "8" },
			};

			for (const Controller& pad : ctx.controllers)
			{
				if (pad.player < 1 || pad.player > 4)
					continue;

				if (isMegaDrivePad(pad) || settingOr(ctx, mMappingOptions[pad.player - 1], "retropad") != "retropad")
				{
					for (const auto& [button, value] : remap)
						config.set("input_player" + std::to_string(pad.player) + "_" + button, value);
				}
			}
		}

	private:
		std::array<const char*, 4> mMappingOptions;
	};

	static bool isMegaDrive(const LaunchContext& ctx)
	{
		return ctx.game.system == "megadrive" || ctx.game.system == "genesis";
	}

	static bool isMegaCD(const LaunchContext& ctx)
	{
		return ctx.game.system == "megacd" || ctx.game.system == "segacd";
	}

	class GenesisPlusGX : public MegaDriveCore
	{
	public:
		explicit GenesisPlusGX(bool masterSystemPads)
			: MegaDriveCore({ "gx_controller1_mapping", "gx_controller2_mapping", "gx_controller3_mapping", "gx_controller4_mapping" })
			, mMasterSystemPads(masterSystemPads)
		{
		}

		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			const std::string& system = ctx.game.system;
			bool msu = system == "megadrive-msu";

			o.set("genesis_plus_gx_system_bram", "per game");   // one backup RAM file per game
			option(ctx, o, "genesis_plus_gx_region_detect", "gpgx_region", "auto");
			option(ctx, o, "genesis_plus_gx_no_sprite_limit", "gpgx_no_sprite_limit", "disabled");

			std::string ntsc = "disabled";
			if (isMegaDrive(ctx))
				ntsc = settingOr(ctx, "gpgx_blargg_filter_md", "disabled");
			else if (system == "mastersystem")
				ntsc = settingOr(ctx, "gpgx_blargg_filter_ms", "disabled");
			o.set("genesis_plus_gx_blargg_ntsc_filter", ntsc);

			std::string cursor = "disabled";
			if (isMegaDrive(ctx))
				cursor = settingOr(ctx, "gun_cursor_md", "disabled");
			else if (system == "mastersystem")
				cursor = settingOr(ctx, "gun_cursor_ms", "disabled");
			o.set("genesis_plus_gx_gun_cursor", cursor);

			option(ctx, o, "genesis_plus_gx_ym2612", "gpgx_fm", "mame (ym2612)");
			std::string ym2413 = settingOr(ctx, "ym2413", "automatic");
			o.set("genesis_plus_gx_ym2413", ym2413 == "automatic" ? "auto" : ym2413);

			option(ctx, o, "genesis_plus_gx_lcd_filter", "lcd_filter", "disabled");
			option(ctx, o, "genesis_plus_gx_gg_extra", "gg_extra", "disabled");

			// MSU-MD needs the Mega-CD add-on, and its CD audio is louder than the Mega-CD's
			option(ctx, o, "genesis_plus_gx_add_on", "gpgx_cd_add_on", msu ? "sega/mega cd" : "auto");
			option(ctx, o, "genesis_plus_gx_cdda_volume", "gpgx_cdda_volume", msu ? "70" : "100");
		}

		std::optional<std::string> deviceType(const LaunchContext& ctx, int player) const override
		{
			if (player != 1 && player != 2)
				return std::nullopt;
			if (isMegaDrive(ctx))   // 513: six button pad
				return settingOr(ctx, player == 1 ? "controller1_md" : "controller2_md", "513");
			if (mMasterSystemPads && ctx.game.system == "mastersystem")
				return settingOr(ctx, player == 1 ? "controller1_ms" : "controller2_ms", "769");
			return std::nullopt;
		}

	private:
		bool mMasterSystemPads;   // the Master System controller types: not the expanded build
	};

	class PicoDrive : public MegaDriveCore
	{
	public:
		PicoDrive() : MegaDriveCore({ "pd_controller1_mapping", "pd_controller2_mapping", "pd_controller3_mapping", "pd_controller4_mapping" }) { }

		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "picodrive_sprlim", "enabled");
			option(ctx, o, "picodrive_renderer", "accurate");
			option(ctx, o, "picodrive_input1", "picodrive_controller1", "6 button pad");
			option(ctx, o, "picodrive_input2", "picodrive_controller2", "6 button pad");
			// the Backup RAM cartridge, for the Mega-CD games that save on it (Shining Force CD)
			o.set("picodrive_ramcart", isMegaCD(ctx) ? "enabled" : "disabled");
		}

		bool supportsRewindAndRunAhead(const LaunchContext& ctx) const override
		{
			return ctx.game.system != "sega32x";
		}
	};

	// Saturn controller type of player 1 or 2. Beetle Saturn's 3D pad is 261, YabaSanshiro's 5:
	// the option is shared, so each core reads the other's value too.
	static std::optional<std::string> saturnDevice(const LaunchContext& ctx, int player, bool yabasanshiro)
	{
		if (ctx.game.system != "saturn" || (player != 1 && player != 2))
			return std::nullopt;
		std::string device = settingOr(ctx, player == 1 ? "controller1_saturn" : "controller2_saturn", "1");
		if (yabasanshiro && device == "261")
			return "5";
		if (!yabasanshiro && device == "5")
			return "261";
		return device;
	}

	class BeetleSaturn : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "beetle_saturn_virtuagun_crosshair", "beetle-saturn_crosshair", "Off");
			o.set("beetle_saturn_analog_stick_deadzone", "15%");
		}

		std::optional<std::string> deviceType(const LaunchContext& ctx, int player) const override
		{
			return saturnDevice(ctx, player, false);
		}
	};

	// two ports of 6: none, port 1, port 2 or both
	static std::pair<const char*, const char*> multitapPorts(const std::string& value)
	{
		bool port1 = value == "port1" || value == "port12";
		bool port2 = value == "port2" || value == "port12";
		return { port1 ? "enabled" : "disabled", port2 ? "enabled" : "disabled" };
	}

	class YabaSanshiro : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "yabasanshiro_resolution_mode", "resolution_mode", "original");
			option(ctx, o, "yabasanshiro_rbg_resolution_mode", "yabasanshiro_rbg_resolution", "original");
			auto [port1, port2] = multitapPorts(settingOr(ctx, "multitap_yabasanshiro", ""));
			o.set("yabasanshiro_multitap_port1", port1);
			o.set("yabasanshiro_multitap_port2", port2);
			option(ctx, o, "yabasanshiro_system_language", "yabasanshiro_language", "english");
		}

		// the core swaps the shoulder buttons
		void adjustButtons(const LaunchContext& /*ctx*/, const Controller& /*pad*/, ButtonMap& buttons) const override
		{
			for (auto& [es, ra] : buttons)
			{
				if (es == "pageup")
					ra = "r";
				else if (es == "pagedown")
					ra = "l";
			}
		}

		// it needs the compatibility GL driver
		std::string defaultVideoDriver(const std::string& generic) const override
		{
			return generic == "glcore" ? "gl" : generic;
		}

		std::optional<std::string> deviceType(const LaunchContext& ctx, int player) const override
		{
			return saturnDevice(ctx, player, true);
		}
	};

	class Kronos : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "kronos_resolution_mode", "kronos_resolution", "original");
			option(ctx, o, "kronos_meshmode", "disabled");
			option(ctx, o, "kronos_bandingmode", "disabled");
			option(ctx, o, "kronos_use_beetle_saves", "enabled");
			auto [port1, port2] = multitapPorts(settingOr(ctx, "kronos_multitap", ""));
			o.set("kronos_multitap_port1", port1);
			o.set("kronos_multitap_port2", port2);
			option(ctx, o, "kronos_language_id", "English");
		}

		// it needs the core GL driver
		std::string defaultVideoDriver(const std::string& generic) const override
		{
			return generic == "gl" ? "glcore" : generic;
		}
	};

	class Ppsspp : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "ppsspp_internal_resolution", "ppsspp_resolution", "480x272");
		}

		// it needs the compatibility GL driver
		std::string defaultVideoDriver(const std::string& generic) const override
		{
			return generic == "glcore" ? "gl" : generic;
		}
	};

	// Beetle PCE and Beetle PCE Fast: same options under their own prefix
	class Pce : public Core
	{
	public:
		explicit Pce(bool fast) : m_fast(fast) { }

		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			// no 16 sprites per line limit
			o.set(m_fast ? "pce_fast_nospritelimit" : "pce_nospritelimit", settingOr(ctx, "pce_nospritelimit", "enabled"));
		}

		std::optional<std::string> deviceType(const LaunchContext& ctx, int player) const override
		{
			if (player != 1)
				return std::nullopt;
			return settingOr(ctx, "controller1_pce", "1");
		}

	private:
		bool m_fast;
	};

	class MednafenSupergrafx : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			// no 16 sprites per line limit
			option(ctx, o, "sgx_nospritelimit", "enabled");
		}
	};

	class MednafenWswan : public Core
	{
	public:
		// the player's choice, else manual (Batocera picks portrait for the games it decorates at 90°)
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "wswan_rotate_display", "manual");
		}

		void setConfig(const LaunchContext& ctx, ConfigFile& config) const override
		{
			option(ctx, config, "wswan_rotate_display", "manual");
		}
	};

	// Arcade cores: the coin of player 1 is also on the 5 of the numeric keypad, as in MAME
	class ArcadeCore : public Core
	{
	public:
		void setConfig(const LaunchContext& /*ctx*/, ConfigFile& config) const override
		{
			config.set("input_player1_select", "num5");
		}
	};

	class Fbneo : public ArcadeCore
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			o.set("fbneo-diagnostic-input", "Start + L + R");
			// RetroAchievements in hardcore mode need the original romsets
			o.set("fbneo-allow-patched-romsets", "disabled");
			option(ctx, o, "fbneo-cpu-speed-adjust", "100%");

			std::string frameskip = settingOr(ctx, "fbneo-frameskip", "0");
			o.set("fbneo-frameskip-type", frameskip != "0" ? "Fixed" : "disabled");
			o.set("fbneo-fixed-frameskip", frameskip);

			// light guns are left out: the games that can use one get the joystick
			std::string game = ctx.game.rom.stem().string();
			o.set("fbneo-dipswitch-" + game + "-Controls", "Joystick");

			if (ctx.game.system == "neogeo")
			{
				// the Universe BIOS, unless the player chose a console or a region
				static const std::map<std::string, std::string, std::less<>> bios = {
					{ "MVS Asia/Europe", "MVS Asia/Europe ver. 5 (1 slot)" },
					{ "MVS USA", "MVS USA ver. 5 (2 slot)" },
					{ "MVS Japan", "MVS Japan ver. 5 (? slot)" },
					{ "AES World", "AES World" },
					{ "AES Asia", "AES World" },
					{ "AES Japan", "AES Japan" },
				};
				auto it = bios.find(settingOr(ctx, "fbneo-neogeo-mode-switch", ""));
				if (it != bios.end())
				{
					o.set("fbneo-neogeo-mode", "DIPSWITCH");
					o.set("fbneo-dipswitch-" + game + "-BIOS", it->second);
				}
				else
					o.set("fbneo-neogeo-mode", "UNIBIOS");
				option(ctx, o, "fbneo-memcard-mode", "per-game");
			}
		}

		std::vector<std::string> commandArguments(const LaunchContext& ctx) const override
		{
			if (ctx.game.system == "neogeocd")
				return { "--subsystem", "neocd" };
			return {};
		}
	};

	class Mame2003Plus : public ArcadeCore
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			o.set("mame2003-plus_skip_disclaimer", "enabled");
			o.set("mame2003-plus_skip_warnings", "enabled");
			option(ctx, o, "mame2003-plus_frameskip", "disabled");
			option(ctx, o, "mame2003-plus_vector_resolution", "1024x768");
			option(ctx, o, "mame2003-plus_input_interface", "retropad");
			option(ctx, o, "mame2003-plus_tate_mode", "disabled");
			option(ctx, o, "mame2003-plus_neogeo_bios", "unibios33");
			// light guns are left out: the mouse drives the guns
			o.set("mame2003-plus_xy_device", "mouse");
		}
	};

	class Neocd : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "neocd_region", "Japan");
			option(ctx, o, "neocd_bios", "neocd_z.rom (CDZ)");
			o.set("neocd_per_content_saves", ctx.settings.getBool("neocd_per_content_saves", true) ? "On" : "Off");
		}
	};

	class Stella : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "stella_console", "auto");
			option(ctx, o, "stella_palette", "standard");
			option(ctx, o, "stella_filter", "disabled");
			option(ctx, o, "stella_crop_hoverscan", "disabled");
			option(ctx, o, "stella_crop_voverscan", "0");
			option(ctx, o, "stella_ntsc_aspect", "par");
			option(ctx, o, "stella_pal_aspect", "par");
			option(ctx, o, "stella_stereo", "auto");
			option(ctx, o, "stella_phosphor", "auto");
			option(ctx, o, "stella_phosphor_blend", "60");
			option(ctx, o, "stella_paddle_mouse_sensitivity", "10");
			option(ctx, o, "stella_paddle_joypad_sensitivity", "3");
			option(ctx, o, "stella_paddle_analog_sensitivity", "20");
			option(ctx, o, "stella_paddle_analog_deadzone", "15");
			option(ctx, o, "stella_paddle_analog_absolute", "disabled");
			option(ctx, o, "stella_reload", "off");
		}
	};

	// hatariB, the newer Atari ST core
	class Hatarib : public Core
	{
		static std::string extension(const LaunchContext& ctx)
		{
			std::string ext = ctx.game.rom.extension().string();
			std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return ext;
		}

		static bool hardDisk(const LaunchContext& ctx)
		{
			std::string ext = extension(ctx);
			return ext == ".hd" || ext == ".gemdos";
		}

	public:
		// a hard disk is not given as the content: the core finds it in its system folder
		std::filesystem::path romArgument(const LaunchContext& ctx) const override
		{
			return hardDisk(ctx) ? std::filesystem::path() : ctx.game.rom;
		}

		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			o.set("hatarib_statusbar", "0");
			o.set("hatarib_fast_floppy", "1");
			o.set("hatarib_show_welcome", "0");
			o.set("hatarib_tos", "<etos1024k>");
			option(ctx, o, "hatarib_machine", "0");
			option(ctx, o, "hatarib_emutos_region", "hatarib_language", "-1");
			option(ctx, o, "hatarib_monitor", "1");
			option(ctx, o, "hatarib_cpu", "-1");
			option(ctx, o, "hatarib_cpu_clock", "-1");
			option(ctx, o, "hatarib_memory", "1024");
			option(ctx, o, "hatarib_pause_osk", "hatarib_pause", "2");
			option(ctx, o, "hatarib_aspect", "hatarib_ratio", "0");
			option(ctx, o, "hatarib_borders", "0");

			// RetroArch only lets the core open hard disks in its system folder (the BIOS
			// folder): bios/hatarib/hdd is a link to the game
			std::filesystem::path link = ctx.paths.bios / "hatarib" / "hdd";
			std::error_code ec;
			std::filesystem::remove(link, ec);
			if (hardDisk(ctx))
			{
				std::filesystem::create_directories(link.parent_path(), ec);
				std::filesystem::create_symlink(ctx.game.rom, link, ec);
				o.set("hatarib_hardimg", "hatarib/hdd");
				o.set("hatarib_hardboot", "1");
				if (extension(ctx) == ".hd")
				{
					std::string drive = settingOr(ctx, "hatarib_drive", "");
					o.set("hatarib_hardtype", drive == "ACSI" ? "2" : drive == "SCSI" ? "3" : "4");
					o.set("hatarib_hard_readonly", "1");
				}
				else
				{
					o.set("hatarib_hardtype", "0");
					o.set("hatarib_hard_readonly", "0");
				}
			}
			else
			{
				o.remove("hatarib_hardimg");
				o.set("hatarib_hardtype", "0");
				o.set("hatarib_hardboot", "0");
				o.set("hatarib_hard_readonly", "1");
			}
		}
	};

	class Handy : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& /*ctx*/, ConfigFile& o) const override
		{
			// a rotated start crashes the core
			o.set("handy_rot", "None");
		}
	};

	class VirtualJaguar : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "virtualjaguar_usefastblitter", "usefastblitter", "enabled");
			option(ctx, o, "virtualjaguar_bios", "bios_vj", "enabled");
			option(ctx, o, "virtualjaguar_internal_resolution", "1x");
		}
	};

	class Opera : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			o.set("opera_dsp_threaded", "enabled");
			option(ctx, o, "opera_high_resolution", "high_resolution", "enabled");
			option(ctx, o, "opera_cpu_overclock", "cpu_overclock", "1.0x (12.50Mhz)");
			option(ctx, o, "opera_active_devices", "active_devices", "1");

			// games on several discs share their NVRAM
			std::string storage = settingOr(ctx, "opera_nvram_storage", "");
			if (storage.empty())
			{
				std::string rom = ctx.game.rom.string();
				std::ranges::transform(rom, rom.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				storage = rom.find("disc") != std::string::npos ? "shared" : "per game";
			}
			o.set("opera_nvram_storage", storage);
		}
	};

	// Atari 800 and 5200
	class Atari800 : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			if (ctx.game.system != "atari800")
			{
				o.set("atari800_system", "5200");
				option(ctx, o, "atari800_opt2", "disabled");   // joystick hack (Robotron)
				return;
			}
			option(ctx, o, "atari800_system", "800XL (64K)");
			option(ctx, o, "atari800_ntscpal", "NTSC");
			option(ctx, o, "atari800_sioaccel", "enabled");
			std::string legacy = settingOr(ctx, "atari800_artifacting", "") == "enabled" ? "blue/brown 1" : "none";
			option(ctx, o, "atari800_artifacting_mode", legacy);
			if (auto resolution = ctx.settings.get("atari800_resolution"))
				o.set("atari800_resolution", *resolution);
			option(ctx, o, "atari800_internalbasic", "disabled");
		}

		// the Atari joystick
		std::optional<std::string> deviceType(const LaunchContext& /*ctx*/, int player) const override
		{
			if (player == 1 || player == 2)
				return "513";
			return std::nullopt;
		}
	};

	// DOSBox Pure
	class DosboxPure : public Core
	{
	public:
		// a .pc or .dos folder starts with the batch file named after it, else dosbox.bat
		std::filesystem::path romArgument(const LaunchContext& ctx) const override
		{
			const std::filesystem::path& rom = ctx.game.rom;
			std::string extension = rom.extension().string();
			if (ctx.game.system == "dos" && (extension == ".dos" || extension == ".pc"))
			{
				std::error_code ec;
				std::string stem = rom.stem().string();
				std::filesystem::path stemBat = rom / (stem + ".bat");
				bool hasStemBat = std::filesystem::exists(stemBat, ec);
				if (hasStemBat && stem.find(' ') == std::string::npos)
					return stemBat;
				if (!hasStemBat && std::filesystem::exists(rom / "dosbox.bat", ec))
					return rom / "dosbox.bat";
			}
			return rom;
		}

		void setConfig(const LaunchContext& ctx, ConfigFile& config) const override
		{
			// 3: the joystick drives the d-pad; the other types read the stick
			if (auto device = ctx.settings.get("controller1_dosbox_pure"))
				config.set("input_player1_analog_dpad_mode", *device == "3" ? "3" : "0");
			if (auto device = ctx.settings.get("controller2_dosbox_pure"))
				config.set("input_player2_analog_dpad_mode", *device == "3" ? "3" : "0");
		}

		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			// a dosbox.conf in the game folder is read
			o.set("dosbox_pure_conf", "inside");
			auto autoOption = [&](const char* key, const char* settingKey) {
				std::string value = settingOr(ctx, settingKey, "automatic");
				o.set(key, value == "automatic" ? "auto" : value);
			};
			autoOption("dosbox_pure_cpu_type", "pure_cpu_type");
			autoOption("dosbox_pure_cpu_core", "pure_cpu_core");
			autoOption("dosbox_pure_cycles", "pure_cycles");
			option(ctx, o, "dosbox_pure_machine", "pure_machine", "svga");
			option(ctx, o, "dosbox_pure_memory_size", "pure_memory_size", "16");
			option(ctx, o, "dosbox_pure_savestate", "pure_savestate", "on");
			option(ctx, o, "dosbox_pure_keyboard_layout", "pure_keyboard_layout", "us");
			option(ctx, o, "dosbox_pure_auto_mapping", "pure_auto_mapping", "true");
			option(ctx, o, "dosbox_pure_joystick_analog_deadzone", "pure_joystick_analog_deadzone", "15");
			option(ctx, o, "dosbox_pure_joystick_timed", "pure_joystick_timed", "true");
			option(ctx, o, "dosbox_pure_sblaster_type", "pure_sblaster_type", "sb16");
			option(ctx, o, "dosbox_pure_gus", "pure_gravis", "false");
			option(ctx, o, "dosbox_pure_midi", "pure_midi", "disabled");
			option(ctx, o, "dosbox_pure_bootos_ramdisk", "pure_bootos_ramdisk", "false");
		}

		std::optional<std::string> deviceType(const LaunchContext& ctx, int player) const override
		{
			if (player == 1)
				return ctx.settings.get("controller1_dosbox_pure");
			if (player == 2)
				return ctx.settings.get("controller2_dosbox_pure");
			return std::nullopt;
		}
	};

	class ScummVM : public Core
	{
	public:
		// an empty "game.scummvm" file names the game "game"
		std::filesystem::path romArgument(const LaunchContext& ctx) const override
		{
			std::error_code ec;
			if (std::filesystem::is_regular_file(ctx.game.rom, ec) && std::filesystem::file_size(ctx.game.rom, ec) == 0)
				return std::filesystem::path(ctx.game.rom).replace_extension();
			return ctx.game.rom;
		}

		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			option(ctx, o, "scummvm_analog_deadzone", "15");
			option(ctx, o, "scummvm_gamepad_cursor_speed", "1.0");
		}
	};

	class BlueMSX : public Core
	{
	public:
		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			static const std::map<std::string, std::string, std::less<>> machines = {
				{ "colecovision", "ColecoVision" }, { "msx1", "MSX" }, { "msx2", "MSX2" }, { "msx2+", "MSX2+" },
				{ "msxturbor", "MSXturboR" }, { "spectravideo", "SVI - Spectravideo SVI-328" },
			};
			auto it = machines.find(ctx.game.system);
			o.set("bluemsx_msxtype", it == machines.end() ? "Auto" : it->second);
			o.set("bluemsx_nospritelimits", ctx.settings.getBool("bluemsx_nospritelimits", true) ? "ON" : "OFF");
			bool msx1 = ctx.game.system == "colecovision" || ctx.game.system == "msx1";
			option(ctx, o, "bluemsx_overscan", msx1 ? "enabled" : "MSX2");
		}

		std::optional<std::string> deviceType(const LaunchContext& ctx, int player) const override
		{
			const std::string& s = ctx.game.system;
			if ((player == 1 || player == 2) && (s == "msx" || s == "msx1" || s == "msx2" || s == "colecovision"))
				return "1";
			return std::nullopt;
		}
	};

	// PUAE (Amiga 500, 1200, CD32, CDTV)
	class Puae : public Core
	{
	public:
		void setConfig(const LaunchContext& ctx, ConfigFile& config) const override
		{
			// the Kickstart ROMs are in the amiga folder of the BIOS
			config.set("system_directory", (ctx.paths.bios / "amiga").string() + "/");
			// the core reads the d-pad only: the stick drives it
			config.set("input_player1_analog_dpad_mode", "3");
			config.set("input_player2_analog_dpad_mode", "3");
		}

		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			const std::string& system = ctx.game.system;
			bool cd32 = system == "amigacd32" || settingOr(ctx, "controller1_puae", "") == "517"
				|| settingOr(ctx, "controller2_puae", "") == "517";
			// pad buttons to Amiga keys and mouse
			using Mapping = std::array<std::pair<const char*, const char*>, 26>;
			static constexpr Mapping keyboard = { {
				{ "aspect_ratio_toggle", "---" }, { "mouse_toggle", "RETROK_RCTRL" }, { "statusbar", "RETROK_F11" },
				{ "vkbd", "---" }, { "reset", "---" }, { "crop_toggle", "RETROK_F12" }, { "zoom_mode_toggle", "---" },
				{ "a", "---" }, { "b", "---" }, { "x", "RETROK_LALT" }, { "y", "RETROK_SPACE" }, { "l", "RETROK_ESCAPE" },
				{ "l2", "MOUSE_LEFT_BUTTON" }, { "l3", "SWITCH_JOYMOUSE" }, { "ld", "---" }, { "ll", "---" }, { "lr", "---" },
				{ "lu", "---" }, { "r", "RETROK_F1" }, { "r2", "MOUSE_RIGHT_BUTTON" }, { "r3", "TOGGLE_STATUSBAR" },
				{ "rd", "---" }, { "rl", "---" }, { "rr", "---" }, { "ru", "---" }, { "select", "TOGGLE_VKBD" },
			} };
			for (const auto& [key, value] : keyboard)
			{
				// the CD32 pad keeps its buttons for the game
				bool kept = std::string_view(value).starts_with("MOUSE_") || std::string_view(key) == "l3" || std::string_view(key) == "r3"
					|| std::string_view(key) == "mouse_toggle" || std::string_view(key) == "statusbar" || std::string_view(key) == "crop_toggle";
				o.set(std::string("puae_mapper_") + key, cd32 && !kept ? "---" : value);
			}
			o.set("puae_mapper_start", cd32 ? "---" : "RETROK_RETURN");

			o.set("puae_video_options_display", "enabled");
			// auto: A500 for floppies, A600 for hard disks
			static const std::map<std::string, std::string, std::less<>> models = {
				{ "amiga1200", "A1200" }, { "amigacd32", "CD32FR" }, { "amigacdtv", "CDTV" },
			};
			std::string model = settingOr(ctx, "puae_model", "automatic");
			if (model == "automatic")
			{
				auto it = models.find(system);
				model = it == models.end() ? "auto" : it->second;
			}
			o.set("puae_model", model);

			option(ctx, o, "puae_cpu_compatibility", "cpu_compatibility", "normal");
			option(ctx, o, "puae_cpu_throttle", "cpu_throttle", "0.0");
			o.set("puae_cpu_multiplier", "0");
			if (settingOr(ctx, "cpu_compatibility", "") == "exact")
			{
				o.set("puae_cpu_throttle", "0.0");
				option(ctx, o, "puae_cpu_multiplier", "cpu_multiplier", "0");
			}
			option(ctx, o, "puae_video_standard", "video_standard", "PAL auto");
			option(ctx, o, "puae_video_resolution", "video_resolution", "hires");
			std::string zoom = settingOr(ctx, "zoom_mode", "automatic");
			o.set("puae_crop", zoom == "automatic" ? "auto" : zoom);
			o.set("puae_zoom_mode", "deprecated");
			option(ctx, o, "puae_gfx_framerate", "gfx_framerate", "disabled");
			option(ctx, o, "puae_mouse_speed", "mouse_speed", "200");
			option(ctx, o, "puae_retropad_options", "pad_options", system == "amigacdtv" ? "disabled" : "jump");

			if (system == "amiga500" || system == "amiga1200")
			{
				option(ctx, o, "puae_floppy_speed", "100");
				option(ctx, o, "puae_keyrah_keypad_mappings", "keyrah_mapping", "enabled");
				option(ctx, o, "puae_use_whdload_prefs", "whdload", "config");
				option(ctx, o, "puae_physical_keyboard_pass_through", "disable_joystick", "disabled");
			}
			if (system == "amigacd32" || system == "amigacdtv")
			{
				option(ctx, o, "puae_cd_startup_delayed_insert", "disabled");
				option(ctx, o, "puae_cd_speed", "100");
			}
			if (system == "amigacd32")
				option(ctx, o, "puae_cd32pad_options", "disabled");
		}

		std::optional<std::string> deviceType(const LaunchContext& ctx, int player) const override
		{
			bool cd32 = ctx.game.system == "amigacd32";
			if (player == 1)
				return cd32 ? std::string("517") : settingOr(ctx, "controller1_puae", "1");   // 517: CD32 pad
			if (player == 2 && !cd32)
				return settingOr(ctx, "controller2_puae", "1");
			return std::nullopt;
		}
	};

	// VICE C64 (x64, and x64sc / xscpu64 without the d-pad setting)
	class ViceX64 : public Core
	{
	public:
		explicit ViceX64(bool forceDpad, bool scpu) : m_forceDpad(forceDpad), m_scpu(scpu) { }

		void setConfig(const LaunchContext& /*ctx*/, ConfigFile& config) const override
		{
			if (!m_forceDpad)
				return;
			config.set("input_player1_analog_dpad_mode", "3");
			config.set("input_player2_analog_dpad_mode", "3");
		}

		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			// JiffyDOS when its ROMs are all in bios/vice: the core turns off a drive without its ROM
			std::vector<std::string> roms = { "JiffyDOS_1541-II.bin", "JiffyDOS_1571_repl310654.bin", "JiffyDOS_1581.bin" };
			if (!m_scpu)
				roms.emplace_back(settingOr(ctx, "c64_model", "").starts_with("C64SX") ? "JiffyDOS_SX-64.bin" : "JiffyDOS_C64.bin");
			bool jiffy = std::ranges::all_of(roms, [&](const std::string& rom) {
				std::error_code ec;
				return std::filesystem::exists(ctx.paths.bios / "vice" / rom, ec);
			});
			o.set("vice_jiffydos", jiffy ? "enabled" : "disabled");

			o.set("vice_autoloadwarp", "enabled");
			o.set("vice_datasette_hotkeys", "disabled");
			o.set("vice_read_vicerc", "disabled");
			o.set("vice_Controller", "joystick");
			o.set("vice_turbo_fire", "disabled");
			static constexpr std::array<std::pair<const char*, const char*>, 26> mapping = { {
				{ "a", "---" }, { "aspect_ratio_toggle", "---" }, { "b", "---" }, { "joyport_switch", "RETROK_F10" },
				{ "l", "RETROK_ESCAPE" }, { "l2", "RETROK_F11" }, { "l3", "SWITCH_JOYPORT" }, { "ld", "---" }, { "ll", "---" },
				{ "lr", "---" }, { "lu", "---" }, { "r", "RETROK_PAGEUP" }, { "r2", "RETROK_LSHIFT" }, { "rd", "RETROK_F7" },
				{ "reset", "---" }, { "rl", "RETROK_F3" }, { "rr", "RETROK_F5" }, { "ru", "RETROK_F1" }, { "select", "TOGGLE_VKBD" },
				{ "start", "RETROK_RETURN" }, { "statusbar", "RETROK_F9" }, { "vkbd", "RETROK_F12" }, { "warp_mode", "RETROK_F11" },
				{ "turbo_fire_toggle", "RETROK_RCTRL" }, { "x", "RETROK_RCTRL" }, { "y", "RETROK_SPACE" },
			} };
			for (const auto& [key, value] : mapping)
				o.set(std::string("vice_mapper_") + key, value);

			option(ctx, o, "vice_c64_model", "c64_model", "C64 PAL auto");
			option(ctx, o, "vice_aspect_ratio", "pal");
			std::string zoom = settingOr(ctx, "vice_zoom_mode", "auto_disable");
			o.set("vice_crop", zoom == "automatic" ? "auto" : zoom);
			o.set("vice_zoom_mode", "deprecated");
			option(ctx, o, "vice_external_palette", "colodore");
			option(ctx, o, "vice_retropad_options", "jump");
			option(ctx, o, "vice_joyport", "2");
			option(ctx, o, "vice_joyport_type", "1");
			option(ctx, o, "vice_ram_expansion_unit", "none");
			option(ctx, o, "vice_physical_keyboard_pass_through", "vice_keyboard_pass_through", "disabled");
		}

	private:
		bool m_forceDpad;
		bool m_scpu;
	};

	// The other VICE cores: C128, VIC-20, Plus/4, PET. They differ in their model and palette
	// options, and in the joystick options the machine has.
	class ViceMachine : public Core
	{
	public:
		struct Options
		{
			const char* model;           // core option
			const char* modelSetting;
			const char* modelDefault;
			const char* palette;         // core option
			const char* paletteDefault;
			bool aspect;                 // vice_aspect_ratio
			bool retropad;               // vice_retropad_options
			bool joyport;                // vice_joyport (the machine has two joystick ports)
			bool joyportType;            // vice_joyport_type
			bool c128;                   // JiffyDOS and the RAM expansion of the C128
		};

		explicit ViceMachine(Options options) : m(options) { }

		void setCoreOptions(const LaunchContext& ctx, ConfigFile& o) const override
		{
			if (m.c128)
			{
				std::vector<std::string> roms = { "JiffyDOS_1541-II.bin", "JiffyDOS_1571_repl310654.bin", "JiffyDOS_1581.bin",
					"JiffyDOS_C64.bin", "JiffyDOS_C128.bin" };
				bool jiffy = std::ranges::all_of(roms, [&](const std::string& rom) {
					std::error_code ec;
					return std::filesystem::exists(ctx.paths.bios / "vice" / rom, ec);
				});
				o.set("vice_jiffydos", jiffy ? "enabled" : "disabled");
			}
			o.set("vice_autoloadwarp", "enabled");
			o.set("vice_datasette_hotkeys", "disabled");
			o.set("vice_read_vicerc", "disabled");
			o.set("vice_Controller", "joystick");
			o.set("vice_turbo_fire", "disabled");

			option(ctx, o, m.model, m.modelSetting, m.modelDefault);
			if (m.aspect)
				option(ctx, o, "vice_aspect_ratio", "pal");
			std::string zoom = settingOr(ctx, "vice_zoom_mode", "auto_disable");
			o.set("vice_crop", zoom == "automatic" ? "auto" : zoom);
			o.set("vice_zoom_mode", "deprecated");
			option(ctx, o, m.palette, m.paletteDefault);
			if (m.retropad)
				option(ctx, o, "vice_retropad_options", "disabled");
			if (m.joyport)
				option(ctx, o, "vice_joyport", "2");
			if (m.joyportType)
				option(ctx, o, "vice_joyport_type", "1");
			if (m.c128)
				option(ctx, o, "vice_c128_ram_expansion_unit", "none");
			option(ctx, o, "vice_physical_keyboard_pass_through", "vice_keyboard_pass_through", "disabled");
		}

	private:
		Options m;
	};

	// ---------------------------------------------------------------------------------------------

	const Core& coreProfile(std::string_view core)
	{
		static const std::map<std::string, std::unique_ptr<Core>, std::less<>> profiles = [] {
			std::map<std::string, std::unique_ptr<Core>, std::less<>> m;
			m.emplace("gambatte", std::make_unique<Gambatte>());
			m.emplace("mgba", std::make_unique<Mgba>());
			m.emplace("melondsds", std::make_unique<MelonDSDS>());
			m.emplace("mesen", std::make_unique<Mesen>());
			m.emplace("fceumm", std::make_unique<Fceumm>());
			m.emplace("snes9x", std::make_unique<Snes9x>());
			m.emplace("mupen64plus-next", std::make_unique<Mupen64PlusNext>());
			m.emplace("mupen64plus_next", std::make_unique<Mupen64PlusNext>());
			m.emplace("flycast", std::make_unique<Flycast>());
			m.emplace("nestopia", std::make_unique<Nestopia>());
			m.emplace("bsnes", std::make_unique<Bsnes>(false));
			m.emplace("bsnes_hd", std::make_unique<Bsnes>(true));
			m.emplace("bsnes_hd_beta", std::make_unique<Bsnes>(true));
			// Batocera calls it vba-m, the library is vbam_libretro.so
			m.emplace("vba-m", std::make_unique<VbaM>());
			m.emplace("vbam", std::make_unique<VbaM>());
			m.emplace("swanstation", std::make_unique<Swanstation>());
			// Beetle PSX: the hardware renderer build and the software one
			m.emplace("mednafen_psx_hw", std::make_unique<MednafenPsx>());
			m.emplace("mednafen_psx", std::make_unique<MednafenPsx>());
			m.emplace("pcsx_rearmed", std::make_unique<PcsxRearmed>());
			m.emplace("parallel_n64", std::make_unique<ParallelN64>());
			m.emplace("desmume", std::make_unique<Desmume>());
			m.emplace("melonds", std::make_unique<MelonDS>());
			// Batocera calls Beetle VB "vb", the library is mednafen_vb_libretro.so
			m.emplace("vb", std::make_unique<VirtualBoy>());
			m.emplace("mednafen_vb", std::make_unique<VirtualBoy>());
			m.emplace("pokemini", std::make_unique<PokeMini>());
			// Batocera's names, then the libraries' (genesis_plus_gx_libretro.so...)
			m.emplace("genesisplusgx", std::make_unique<GenesisPlusGX>(true));
			m.emplace("genesis_plus_gx", std::make_unique<GenesisPlusGX>(true));
			m.emplace("genesisplusgx-expanded", std::make_unique<GenesisPlusGX>(false));
			m.emplace("genesis_plus_gx_expanded", std::make_unique<GenesisPlusGX>(false));
			m.emplace("picodrive", std::make_unique<PicoDrive>());
			m.emplace("beetle-saturn", std::make_unique<BeetleSaturn>());
			m.emplace("mednafen_saturn", std::make_unique<BeetleSaturn>());
			m.emplace("yabasanshiro", std::make_unique<YabaSanshiro>());
			m.emplace("kronos", std::make_unique<Kronos>());
			m.emplace("ppsspp", std::make_unique<Ppsspp>());
			// Batocera calls Beetle PCE "pce" and "pce_fast", the libraries are mednafen_pce(_fast)
			m.emplace("pce", std::make_unique<Pce>(false));
			m.emplace("mednafen_pce", std::make_unique<Pce>(false));
			m.emplace("pce_fast", std::make_unique<Pce>(true));
			m.emplace("mednafen_pce_fast", std::make_unique<Pce>(true));
			m.emplace("mednafen_supergrafx", std::make_unique<MednafenSupergrafx>());
			m.emplace("mednafen_wswan", std::make_unique<MednafenWswan>());
			m.emplace("fbneo", std::make_unique<Fbneo>());
			// Batocera calls MAME 2003-Plus "mame078plus", the library is mame2003_plus_libretro.so
			m.emplace("mame078plus", std::make_unique<Mame2003Plus>());
			m.emplace("mame2003_plus", std::make_unique<Mame2003Plus>());
			m.emplace("neocd", std::make_unique<Neocd>());
			m.emplace("stella", std::make_unique<Stella>());
			m.emplace("hatarib", std::make_unique<Hatarib>());
			m.emplace("handy", std::make_unique<Handy>());
			m.emplace("virtualjaguar", std::make_unique<VirtualJaguar>());
			m.emplace("opera", std::make_unique<Opera>());
			m.emplace("atari800", std::make_unique<Atari800>());
			m.emplace("dosbox_pure", std::make_unique<DosboxPure>());
			m.emplace("scummvm", std::make_unique<ScummVM>());
			m.emplace("bluemsx", std::make_unique<BlueMSX>());
			m.emplace("puae", std::make_unique<Puae>());
			m.emplace("vice_x64", std::make_unique<ViceX64>(true, false));
			m.emplace("vice_x64sc", std::make_unique<ViceX64>(false, false));
			m.emplace("vice_xscpu64", std::make_unique<ViceX64>(false, true));
			m.emplace("vice_x128", std::make_unique<ViceMachine>(ViceMachine::Options {
				"vice_c128_model", "c128_model", "C128 PAL", "vice_external_palette", "colodore", true, true, true, true, true }));
			m.emplace("vice_xvic", std::make_unique<ViceMachine>(ViceMachine::Options {
				"vice_vic20_model", "vic20_model", "VIC20 PAL auto", "vice_vic20_external_palette", "colodore_vic", true, true, false, true, false }));
			m.emplace("vice_xplus4", std::make_unique<ViceMachine>(ViceMachine::Options {
				"vice_plus4_model", "plus4_model", "PLUS4 PAL", "vice_plus4_external_palette", "colodore_ted", true, true, true, true, false }));
			m.emplace("vice_xpet", std::make_unique<ViceMachine>(ViceMachine::Options {
				"vice_pet_model", "pet_model", "8032", "vice_pet_external_palette", "default", false, false, false, false, false }));
			return m;
		}();
		static const Core generic;

		auto it = profiles.find(core);
		return it == profiles.end() ? generic : *it->second;
	}

	std::string coreLibraryName(std::string_view core)
	{
		std::string name(core);
		std::ranges::replace(name, '-', '_');
		return name + "_libretro.so";
	}
}
