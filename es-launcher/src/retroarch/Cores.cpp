// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch_libretro/cores/), Copyright (c) 2026 The Batocera Team.
//
// Profiles for the cores of the first systems: gambatte and vba-m (gb, gbc, gba), mgba (gba),
// melondsds and melonds (nds), mesen, fceumm and nestopia (nes), snes9x and bsnes (snes), mupen64plus-next
// and parallel_n64 (n64), desmume (nds), vb (virtualboy), pokemini, swanstation, mednafen_psx and
// pcsx_rearmed (psx), flycast (dreamcast). Light gun and wheel handling is left out for now.

#include "retroarch/Cores.h"

#include <algorithm>
#include <array>
#include <map>
#include <memory>

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
