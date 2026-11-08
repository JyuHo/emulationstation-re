// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.shadps4), Copyright (c) 2026 The Batocera Team.

#include "shadps4/ShadPS4Generator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Files.h"
#include "util/Gamepad.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <system_error>

namespace es::launcher::shadps4
{
	namespace fs = std::filesystem;

	ES_LAUNCHER_REGISTER_GENERATOR("shadps4", ShadPS4Generator)

	namespace
	{
		using Json = rapidjson::Document;

		// The object `name` of parent, made one when it is missing or something else
		rapidjson::Value& section(Json& doc, const char* name)
		{
			auto it = doc.FindMember(name);
			if (it == doc.MemberEnd())
			{
				doc.AddMember(rapidjson::Value(name, doc.GetAllocator()), rapidjson::Value(rapidjson::kObjectType), doc.GetAllocator());
				return doc[name];
			}
			if (!it->value.IsObject())
				it->value.SetObject();
			return it->value;
		}

		void put(Json& doc, rapidjson::Value& object, const char* key, rapidjson::Value value)
		{
			auto it = object.FindMember(key);
			if (it == object.MemberEnd())
				object.AddMember(rapidjson::Value(key, doc.GetAllocator()), value, doc.GetAllocator());
			else
				it->value = value;
		}

		void putBool(Json& doc, rapidjson::Value& object, const char* key, bool value) { put(doc, object, key, rapidjson::Value(value)); }
		void putInt(Json& doc, rapidjson::Value& object, const char* key, int value) { put(doc, object, key, rapidjson::Value(value)); }
		void putString(Json& doc, rapidjson::Value& object, const char* key, const std::string& value)
		{
			put(doc, object, key, rapidjson::Value(value.c_str(), static_cast<rapidjson::SizeType>(value.size()), doc.GetAllocator()));
		}

		std::string lower(std::string text)
		{
			std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		}
	}

	fs::path ShadPS4Generator::gameFile(const fs::path& rom)
	{
		std::error_code ec;
		if (lower(rom.extension().string()) == ".zar")
			return rom;
		if (fs::is_directory(rom, ec))
			return rom / "eboot.bin";
		return rom.parent_path() / "eboot.bin";
	}

	std::string ShadPS4Generator::writeSettings(const LaunchContext& ctx, const std::string& text)
	{
		const Settings& s = ctx.settings;
		// the folder of the games: the one of the .zar, else the one of the game folder
		fs::path game = gameFile(ctx.game.rom);
		fs::path roms = game == ctx.game.rom ? game.parent_path() : game.parent_path().parent_path();

		Json doc;
		doc.Parse(text.c_str());
		if (doc.HasParseError() || !doc.IsObject())
			doc.SetObject();
		auto& a = doc.GetAllocator();

		rapidjson::Value& general = section(doc, "General");
		putBool(doc, general, "discord_rpc_enabled", false);
		putString(doc, general, "home_dir", (ctx.paths.saves / "ps4").string());
		putString(doc, general, "addon_install_dir", (roms / "DLC").string());
		rapidjson::Value dirs(rapidjson::kArrayType);
		rapidjson::Value dir(rapidjson::kObjectType);
		dir.AddMember("enabled", true, a);
		dir.AddMember("path", rapidjson::Value(roms.string().c_str(), a), a);
		dirs.PushBack(dir, a);
		put(doc, general, "install_dirs", std::move(dirs));
		putBool(doc, general, "show_fps_counter", s.getBool("shadps4_show_fps", false));
		putBool(doc, general, "neo_mode", s.getBool("shadps4_neo_mode", false));
		putInt(doc, general, "console_language", s.getInt("shadps4_console_lang", 1));

		rapidjson::Value& gpu = section(doc, "GPU");
		putBool(doc, gpu, "full_screen", true);
		putString(doc, gpu, "full_screen_mode", "Fullscreen (Borderless)");
		if (ctx.screenWidth > 0 && ctx.screenHeight > 0)
		{
			putInt(doc, gpu, "window_width", ctx.screenWidth);
			putInt(doc, gpu, "window_height", ctx.screenHeight);
		}
		putBool(doc, gpu, "fsr_enabled", s.getBool("shadps4_fsr", false));
		putBool(doc, gpu, "rcas_enabled", s.getBool("shadps4_rcas", true));
		putBool(doc, gpu, "hdr_allowed", s.getBool("shadps4_hdr", false));
		std::string present = s.getString("shadps4_present_mode", "Mailbox");
		putString(doc, gpu, "present_mode", present.empty() ? "Mailbox" : present);
		int vblank = s.getInt("shadps4_vblank_freq", 60);
		putInt(doc, gpu, "vblank_frequency", vblank > 0 ? vblank : 60);
		putBool(doc, gpu, "copy_gpu_buffers", s.getBool("shadps4_copy_gpu_buffers", false));
		putInt(doc, gpu, "readbacks_mode", s.getInt("shadps4_readbacks_mode", 0));
		putBool(doc, gpu, "direct_memory_access_enabled", s.getBool("shadps4_dma", false));

		rapidjson::Value& vulkan = section(doc, "Vulkan");
		putBool(doc, vulkan, "pipeline_cache_enabled", true);
		putBool(doc, vulkan, "pipeline_cache_archived", s.getBool("shadps4_pipeline_cache", false));

		rapidjson::Value& log = section(doc, "Log");
		putBool(doc, log, "enable", s.getBool("shadps4_logging", true));

		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		writer.SetIndent(' ', 2);
		doc.Accept(writer);
		return std::string(buffer.GetString(), buffer.GetSize()) + "\n";
	}

	Command ShadPS4Generator::configure(const LaunchContext& ctx)
	{
		if (ctx.architecture != Architecture::X86_64)
			throw LaunchError(ErrorCode::Generic, "shadPS4 only runs on x86_64 computers.");

		fs::path home = ctx.paths.configFor("shadps4");
		fs::path saves = ctx.paths.saves / "ps4";
		fs::path game = gameFile(ctx.game.rom);
		fs::path roms = game == ctx.game.rom ? game.parent_path() : game.parent_path().parent_path();

		std::error_code ec;
		if (!fs::exists(game, ec))
			throw LaunchError(ErrorCode::Generic, "shadPS4 starts a game from its folder: " + game.string() + " was not found.");

		auto program = util::emulatorCommand(ctx, "shadps4", { "shadps4", "shadPS4" }, { home, saves, roms });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "shadPS4 is not installed: install it from the emulator catalog.");

		fs::path configPath = home / "shadPS4" / "config.json";
		std::ifstream in(configPath);
		std::stringstream text;
		text << in.rdbuf();
		try
		{
			fs::create_directories(saves);
			// shadPS4 hangs when it has no input_config folder
			fs::create_directories(home / "shadPS4" / "input_config");
			util::writeFileAtomically(configPath, writeSettings(ctx, text.str()));
		}
		catch (const fs::filesystem_error& e)
		{
			throw LaunchError(ErrorCode::InvalidConfiguration, std::string("Cannot write the shadPS4 configuration: ") + e.what());
		}

		Command cmd;
		cmd.argv = *program;
		cmd.argv.insert(cmd.argv.end(), { "--game", game.string(), "--fullscreen", "true" });
		cmd.env["XDG_DATA_HOME"] = home.string();
		cmd.closeOnHotkey = true;
		util::setPadEnvironment(cmd, ctx.controllers);
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
