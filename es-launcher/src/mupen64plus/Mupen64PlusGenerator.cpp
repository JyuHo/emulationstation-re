// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch_mupen64plus), Copyright (c) 2026 The Batocera Team.

#include "mupen64plus/Mupen64PlusGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Gamepad.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace es::launcher::mupen64plus
{
	namespace fs = std::filesystem;
	using util::IniFile;

	ES_LAUNCHER_REGISTER_GENERATOR("mupen64plus", Mupen64PlusGenerator)

	namespace
	{
		using Layout = std::map<std::string, std::string, std::less<>>;

		// batocera-launch's input.xml: ES input -> N64 control, for any pad...
		const Layout defaultLayout = {
			{ "l3", "Mempak switch" }, { "r3", "Rumblepak switch" },
			{ "a", "C Button R" }, { "b", "A Button" }, { "x", "C Button U" }, { "y", "B Button" },
			{ "start", "Start" }, { "pageup", "L Trig" }, { "pagedown", "R Trig" }, { "l2", "Z Trig" },
			{ "up", "DPad U" }, { "down", "DPad D" }, { "right", "DPad R" }, { "left", "DPad L" },
			{ "joystick1up", "Y Axis" }, { "joystick1down", "Y Axis" }, { "joystick1left", "X Axis" }, { "joystick1right", "X Axis" },
			{ "joystick2up", "C Button U" }, { "joystick2down", "C Button D" }, { "joystick2left", "C Button L" }, { "joystick2right", "C Button R" },
		};

		// ... and for Nintendo 64 pads, or when the player picked the N64 layout
		const Layout n64Layout = {
			{ "l3", "Mempak switch" }, { "r3", "Rumblepak switch" },
			{ "a", "B Button" }, { "b", "A Button" }, { "x", "C Button U" }, { "y", "C Button L" },
			{ "start", "Start" }, { "select", "Z Trig" }, { "pageup", "L Trig" }, { "pagedown", "R Trig" },
			{ "l2", "C Button D" }, { "r2", "C Button R" },
			{ "up", "DPad U" }, { "down", "DPad D" }, { "right", "DPad R" }, { "left", "DPad L" },
			{ "joystick1up", "Y Axis" }, { "joystick1down", "Y Axis" }, { "joystick1left", "X Axis" }, { "joystick1right", "X Axis" },
		};

		// Every control of [Input-SDL-ControlN], cleared before binding
		constexpr std::array controls = {
			"Mempak switch", "Rumblepak switch", "A Button", "B Button", "Start", "L Trig", "R Trig", "Z Trig",
			"DPad U", "DPad D", "DPad R", "DPad L", "X Axis", "Y Axis",
			"C Button U", "C Button D", "C Button L", "C Button R",
		};

		bool isN64Pad(const Controller& pad)
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

		const char* hatDirection(int value)
		{
			switch (value)
			{
			case 1: return "Up";
			case 2: return "Right";
			case 4: return "Down";
			case 8: return "Left";
			default: return nullptr;
			}
		}

		const char* hatOpposite(int value)
		{
			switch (value)
			{
			case 1: return "Down";
			case 2: return "Left";
			case 4: return "Up";
			case 8: return "Right";
			default: return nullptr;
			}
		}

		double settingDouble(const Settings& s, std::string_view key, double fallback)
		{
			auto value = s.get(key);
			if (!value)
				return fallback;
			try
			{
				return std::stod(*value);
			}
			catch (const std::exception&)
			{
				return fallback;
			}
		}

		constexpr int analogPeak = 32768;

		// per player options, players 1 to 4
		constexpr std::array controllerOptions = { "mupen64-controller1", "mupen64-controller2", "mupen64-controller3", "mupen64-controller4" };
		constexpr std::array sensitivityOptions = { "mupen64-sensitivity1", "mupen64-sensitivity2", "mupen64-sensitivity3", "mupen64-sensitivity4" };
		constexpr std::array deadzoneOptions = { "mupen64-deadzone1", "mupen64-deadzone2", "mupen64-deadzone3", "mupen64-deadzone4" };

		const char* playerOption(const std::array<const char*, 4>& options, int player)
		{
			return player >= 1 && player <= 4 ? options[player - 1] : "";
		}

		// a higher peak lowers the sensitivity: mirror the multiplier around the default peak
		std::string peak(const Settings& s, int player)
		{
			double multiplier = settingDouble(s, playerOption(sensitivityOptions, player), 1.0);
			double adjusted = analogPeak * multiplier;
			long value = std::lround(analogPeak - (adjusted - analogPeak));
			return std::to_string(value) + "," + std::to_string(value);
		}

		std::string deadzone(const Settings& s, int player)
		{
			long value = std::lround(analogPeak * settingDouble(s, playerOption(deadzoneOptions, player), 0.01));
			return std::to_string(value) + "," + std::to_string(value);
		}

		// Mupen64Plus's binding of one ES input to one N64 control, empty when it is skipped
		std::string binding(const Controller& pad, const std::string& name, const InputBinding& input, const std::string& control)
		{
			bool axisControl = control == "X Axis" || control == "Y Axis";
			std::string id = std::to_string(input.id);

			switch (input.type)
			{
			case InputBinding::Type::Button:
			{
				if (!axisControl)
					return "button(" + id + ")";

				// a button on a stick axis: bind both directions on the first one
				const char* reverse = name == "up" || name == "joystick1up" ? "down" : name == "left" || name == "joystick1left" ? "right" : nullptr;
				if (reverse == nullptr)
					return {};
				const InputBinding* other = pad.find(name.starts_with("joystick1") ? std::string("joystick1") + reverse : reverse);
				if (other != nullptr && other->type == InputBinding::Type::Button)
					return "button(" + id + ", " + std::to_string(other->id) + ")";
				return "button(" + id + ")";
			}
			case InputBinding::Type::Hat:
			{
				const char* direction = hatDirection(input.value);
				if (direction == nullptr)
					return {};
				if (!axisControl)
					return "hat(" + id + " " + direction + ")";
				// both directions on the first one (up, left)
				if (input.value == 1 || input.value == 8)
					return "hat(" + id + " " + direction + " " + hatOpposite(input.value) + ")";
				return {};
			}
			case InputBinding::Type::Axis:
			{
				bool negative = input.value < 0;
				if (axisControl)
				{
					// ES records up and left: both directions from the one input
					if (name != "up" && name != "left" && !name.ends_with("up") && !name.ends_with("left"))
						return {};
					return negative ? "axis(" + id + "-," + id + "+)" : "axis(" + id + "+," + id + "-)";
				}
				return "axis(" + id + (negative ? "-)" : "+)");
			}
			case InputBinding::Type::Key:
				break;
			}
			return {};
		}

		const char* boolString(bool value) { return value ? "True" : "False"; }
	}

	fs::path Mupen64PlusGenerator::settingsPath(const LaunchContext& ctx)
	{
		return ctx.paths.configFor("mupen64plus") / "mupen64plus.cfg";
	}

	std::map<std::string, std::string> Mupen64PlusGenerator::padBindings(const LaunchContext& ctx, const Controller& pad)
	{
		bool n64 = isN64Pad(pad) || ctx.settings.getString(playerOption(controllerOptions, pad.player), "retropad") != "retropad";
		Layout layout = n64 ? n64Layout : defaultLayout;

		// Z matters: without a left trigger, put it on L1
		if (pad.find("l2") == nullptr)
			layout["pageup"] = layout["l2"];

		// no left stick: the d-pad drives the stick, which more games use
		if (pad.find("joystick1up") == nullptr)
		{
			layout["up"] = layout["joystick1up"];
			layout["down"] = layout["joystick1down"];
			layout["left"] = layout["joystick1left"];
			layout["right"] = layout["joystick1right"];
		}

		// ES records one direction per stick: the right stick's other directions are the opposite sign
		std::map<std::string, InputBinding, std::less<>> inputs(pad.inputs.begin(), pad.inputs.end());
		for (auto [stick, opposite] : { std::pair{ "joystick2up", "joystick2down" }, std::pair{ "joystick2left", "joystick2right" } })
		{
			auto it = inputs.find(stick);
			if (it != inputs.end() && it->second.type == InputBinding::Type::Axis && !inputs.contains(opposite))
			{
				InputBinding reversed = it->second;
				reversed.value = -reversed.value;
				inputs[opposite] = reversed;
			}
		}

		std::map<std::string, std::string> result;
		for (const auto& [name, input] : inputs)
		{
			auto it = layout.find(name);
			if (it == layout.end() || it->second.empty())
				continue;

			std::string value = binding(pad, name, input, it->second);
			if (value.empty())
				continue;

			std::string& slot = result[it->second];
			if (!slot.empty())
				slot += ' ';
			slot += value;
		}
		return result;
	}

	void Mupen64PlusGenerator::writeSettings(const LaunchContext& ctx, IniFile& ini)
	{
		const Settings& s = ctx.settings;
		fs::path saves = ctx.paths.saves / "n64";
		fs::path home = ctx.paths.configFor("mupen64plus");

		// pad hotkeys are ES's (hotkey + start closes the game), not Mupen64Plus's
		ini.set("CoreEvents", "Version", "1");
		for (const char* event : { "Stop", "Save State", "Load State", "Screenshot", "Increment Slot", "Fast Forward", "Reset", "Pause" })
			ini.set("CoreEvents", std::string("Joy Mapping ") + event, "");

		// Version matters: without it Mupen64Plus drops the section
		ini.set("Core", "Version", "1.01");
		ini.set("Core", "ScreenshotPath", ctx.paths.screenshots.string());
		ini.set("Core", "SaveStatePath", saves.string());
		ini.set("Core", "SaveSRAMPath", saves.string());
		ini.set("Core", "SharedDataPath", home.string());
		ini.set("Core", "SaveFilenameFormat", "1000");   // states named after the ROM
		ini.set("Core", "DisableExtraMem", boolString(s.getBool("mupen64plus_DisableExtraMem")));
		ini.set("Core", "AutoStateSlotIncrement", boolString(s.getBool("incrementalsavestates", true)));
		if (ctx.saveState.slot)
			ini.set("Core", "CurrentStateSlot", std::to_string(*ctx.saveState.slot));

		// audio: synchronization off by default, it causes issues
		ini.set("Audio-SDL", "AUDIO_SYNC", boolString(s.getBool("mupen64plus_AudioSync")));
		std::string buffer = s.getString("mupen64plus_AudioBuffer", "Medium");
		const char* primary = "8192";
		const char* target = "2048";
		const char* secondary = "1024";
		if (buffer == "Very High")
			primary = "16384", target = "4096", secondary = "2048";
		else if (buffer == "High")
			primary = "16384";
		else if (buffer == "Low")
			primary = "4096", target = "1024", secondary = "512";
		ini.set("Audio-SDL", "PRIMARY_BUFFER_SIZE", primary);
		ini.set("Audio-SDL", "PRIMARY_BUFFER_TARGET", target);
		ini.set("Audio-SDL", "SECONDARY_BUFFER_SIZE", secondary);

		// the window: the screen ES runs on, in landscape
		ini.set("Video-General", "Version", "1");
		if (ctx.screenWidth > 0 && ctx.screenHeight > 0)
		{
			ini.set("Video-General", "ScreenWidth", std::max(ctx.screenWidth, ctx.screenHeight));
			ini.set("Video-General", "ScreenHeight", std::min(ctx.screenWidth, ctx.screenHeight));
		}
		ini.set("Video-General", "Fullscreen", "True");
		ini.set("Video-General", "VerticalSync", "True");
		ini.set("Video-Rice", "Version", "1");
		ini.set("Video-Glide64mk2", "Version", "1");

		// aspect ratio: Mupen64Plus's own option, else the ES ratio
		std::string ratio = s.getString("mupen64plus_ratio", s.getString("ratio"));
		const char* adjust = "-1";
		const char* glideAspect = "-1";
		const char* glidenAspect = "3";
		if (ratio == "16/9")
			adjust = "1", glideAspect = "1", glidenAspect = "2";
		else if (ratio == "4/3")
			adjust = "0", glideAspect = "0", glidenAspect = "1";
		ini.set("Video-Glide64mk2", "adjust_aspect", adjust);
		ini.set("Video-Glide64mk2", "aspect", glideAspect);
		ini.set("Video-GLideN64", "AspectRatio", glidenAspect);

		// texture filtering: Rice's mip-mapping mode, translated for Glide64mk2
		std::string mipmapping = s.getString("mupen64plus_Mipmapping", "0");
		ini.set("Video-Rice", "Mipmapping", mipmapping);
		ini.set("Video-Glide64mk2", "filtering", mipmapping == "1" ? "2" : mipmapping == "2" || mipmapping == "3" ? "1" : "-1");

		std::string anisotropic = s.getString("mupen64plus_Anisotropic", "0");
		ini.set("Video-Rice", "AnisotropicFiltering", anisotropic);
		ini.set("Video-Glide64mk2", "wrpAnisotropic", boolString(anisotropic != "0"));
		ini.set("Video-GLideN64", "anisotropy", anisotropic);

		std::string antialiasing = s.getString("mupen64plus_AntiAliasing", "0");
		ini.set("Video-Rice", "MultiSampling", antialiasing);
		ini.set("Video-Glide64mk2", "wrpAntiAliasing", antialiasing);
		ini.set("Video-GLideN64", "MultiSampling", antialiasing);

		// GLideN64: 0 renders at the screen resolution, else a multiple of the N64's
		ini.set("Video-GLideN64", "UseNativeResolutionFactor", s.getString("mupen64plus_NativeResFactor", "0"));

		bool hires = s.getBool("mupen64plus_LoadHiResTextures");
		ini.set("Video-Rice", "LoadHiResTextures", boolString(hires));
		ini.set("Video-Glide64mk2", "ghq_hirs", hires ? "1" : "0");
		ini.set("Video-GLideN64", "txHiresEnable", boolString(hires));

		ini.set("Video-Rice", "TextureEnhancement", s.getString("mupen64plus_TextureEnhancement", "0"));

		// Glide64mk2 frame skipping: "automatic" skips up to 5 frames when needed
		std::string frameskip = s.getString("mupen64plus_frameskip", "0");
		ini.set("Video-Glide64mk2", "autoframeskip", frameskip == "automatic" ? "1" : "0");
		ini.set("Video-Glide64mk2", "maxframeskip", frameskip == "automatic" ? "5" : frameskip);
		ini.set("Video-Glide64mk2", "fb_read_always", s.getString("mupen64plus_fb_read_always", "-1"));

		bool fps = s.getBool("showFPS");
		ini.set("Video-Rice", "ShowFPS", boolString(fps));
		ini.set("Video-Glide64mk2", "show_fps", fps ? "4" : "8");

		// 64DD
		ini.set("64DD", "IPL-ROM", ctx.game.system == "n64dd" ? (ctx.paths.bios / "64DD_IPL.bin").string() : "");
		ini.set("64DD", "Disk", "");

		// pads, in player order; the other ports unplugged
		int port = 0;
		for (const Controller& pad : ctx.controllers)
		{
			if (++port > 4)
				break;

			std::string section = "Input-SDL-Control" + std::to_string(port);
			ini.set(section, "Version", "2");
			ini.set(section, "mode", "0");   // fully manual
			ini.set(section, "device", std::max(pad.deviceIndex, 0));
			std::string name;
			std::ranges::copy_if(pad.name, std::back_inserter(name), [](char c) { return static_cast<unsigned char>(c) < 0x80; });
			ini.set(section, "name", "\"" + name + "\"");
			ini.set(section, "plugged", "True");
			ini.set(section, "plugin", "2");   // memory pak
			ini.set(section, "mouse", "False");
			ini.set(section, "AnalogDeadzone", deadzone(s, pad.player));
			ini.set(section, "AnalogPeak", peak(s, pad.player));

			auto bindings = padBindings(ctx, pad);
			for (const char* control : controls)
			{
				auto it = bindings.find(control);
				ini.set(section, control, it == bindings.end() ? "" : "\"" + it->second + "\"");
			}
		}
		for (++port; port <= 4; ++port)
		{
			std::string section = "Input-SDL-Control" + std::to_string(port);
			ini.set(section, "Version", "2");
			ini.set(section, "plugged", "False");
		}

		// mupen64plus.<section>.<key> = value sets any key of mupen64plus.cfg
		for (const auto& [sectionKey, value] : s.withPrefix("mupen64plus."))
		{
			size_t dot = sectionKey.find('.');
			if (dot != std::string::npos && dot > 0 && dot + 1 < sectionKey.size())
				ini.set(sectionKey.substr(0, dot), sectionKey.substr(dot + 1), value);
		}
	}

	Command Mupen64PlusGenerator::configure(const LaunchContext& ctx)
	{
		fs::path home = ctx.paths.configFor("mupen64plus");
		fs::path saves = ctx.paths.saves / "n64";

		auto program = util::emulatorCommand(ctx, "mupen64plus", "mupen64plus", {
			home, saves, ctx.paths.bios, ctx.paths.screenshots, ctx.game.rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "Mupen64Plus is not installed: install the mupen64plus package of your distribution.");

		IniFile ini = IniFile::load(settingsPath(ctx));
		writeSettings(ctx, ini);
		try
		{
			for (const fs::path& dir : { saves, ctx.paths.screenshots })
				fs::create_directories(dir);
			ini.save(settingsPath(ctx));
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the Mupen64Plus configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *program;
		cmd.argv.insert(cmd.argv.end(), { "--configdir", home.string(), "--datadir", home.string() });

		// the video plugin chosen as the core: Batocera's names are the plugins' own
		if (!ctx.core.empty() && ctx.core != "mupen64plus")
			cmd.argv.insert(cmd.argv.end(), { "--gfx", "mupen64plus-video-" + ctx.core });

		if (!ctx.saveState.file.empty())
			cmd.argv.insert(cmd.argv.end(), { "--savestate", ctx.saveState.file.string() });

		std::string cheats = ctx.settings.getString("mupen64plus_cheats");
		if (!cheats.empty())
			cmd.argv.insert(cmd.argv.end(), { "--cheats", cheats });

		cmd.argv.push_back(ctx.game.rom.string());

		cmd.closeOnHotkey = true;   // its own hotkeys are on the keyboard
		util::setPadEnvironment(cmd, ctx.controllers);
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
