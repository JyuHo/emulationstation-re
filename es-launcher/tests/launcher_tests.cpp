// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.

#include "Check.h"
#include "es-launcher/EvdevLayout.h"
#include "es-launcher/Launcher.h"

#include <algorithm>
#include <linux/input.h>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace es::launcher;

namespace
{
	int cleanups = 0;

	class FakeGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override
		{
			if (ctx.game.rom.empty())
				throw LaunchError(ErrorCode::BadArguments, "no rom");

			Command cmd;
			cmd.argv = { "fake-emu", "--fullscreen", ctx.game.rom.string() };
			cmd.env["FAKE_CONFIG"] = ctx.paths.configFor("fake").string();
			return cmd;
		}

		void cleanup(const LaunchContext&) override { ++cleanups; }
	};

	class EmptyGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext&) override { return {}; }
	};
}

ES_LAUNCHER_REGISTER_GENERATOR("fake", FakeGenerator)
ES_LAUNCHER_REGISTER_GENERATOR("empty", EmptyGenerator)

static void testShellQuote()
{
	CHECK(shellQuote("abc") == "'abc'");
	CHECK(shellQuote("it's") == "'it'\\''s'");
	CHECK(shellQuote("") == "''");
	CHECK(shellQuote("a b;$(rm -rf /)") == "'a b;$(rm -rf /)'");
}

static void testShellLine()
{
	Command cmd;
	cmd.argv = { "dolphin-emu", "-e", "/roms/My Game's.iso" };
	cmd.env["A"] = "1";
	cmd.workingDirectory = "/tmp/x y";
	CHECK(cmd.toShellLine() == "cd '/tmp/x y' && A='1' 'dolphin-emu' '-e' '/roms/My Game'\\''s.iso'");
}

static void testSettings()
{
	Settings s({ { "ratio", "4/3" }, { "smooth", "1" }, { "res", "2" }, { "shader", "crt" } },
	           { { "ratio", "16/9" }, { "smooth", "auto" }, { "res", "x" } },
	           { { "ratio", "" }, { "shader", "none" } });

	CHECK(s.getString("ratio") == "16/9");        // empty game value falls through to the system
	CHECK(s.getBool("smooth") == true);           // "auto" falls through to the global value
	CHECK(s.getString("shader") == "none");       // game wins
	CHECK(s.getInt("res", 7) == 7);               // system value is not a number: fallback
	CHECK(!s.isSet("missing"));
	CHECK(s.getString("missing", "def") == "def");
	CHECK(s.getInt("missing", 3) == 3);
}

static void testRegistry()
{
	auto& registry = Registry::instance();
	CHECK(registry.contains("fake"));
	CHECK(!registry.contains("nope"));
	CHECK(!registry.add("fake", [] { return std::unique_ptr<Generator>(new EmptyGenerator()); }));
	CHECK(registry.create("nope") == nullptr);

	auto names = registry.names();
	CHECK(std::ranges::is_sorted(names));
	CHECK(std::ranges::find(names, "fake") != names.end());
	CHECK(registry.contains("libretro"));
	CHECK(registry.contains("retroarch"));
}

static void testLaunch()
{
	LaunchContext ctx;
	ctx.emulator = "fake";
	ctx.game.rom = "/roms/psx/game.chd";
	ctx.paths.emulatorConfigs = "/cfg/emulators";

	cleanups = 0;
	{
		Launch launch(ctx);
		CHECK(launch.handled());
		Command cmd = launch.prepare();
		CHECK(cmd.argv.size() == 3);
		CHECK(cmd.argv[2] == "/roms/psx/game.chd");
		CHECK(cmd.env.at("FAKE_CONFIG") == "/cfg/emulators/fake");
		launch.finish();
		launch.finish();   // cleanup runs once
	}
	CHECK(cleanups == 1);

	{
		Launch launch(ctx);
		launch.prepare();
	}   // the destructor cleans up
	CHECK(cleanups == 2);

	{
		Launch launch(ctx);
	}   // never prepared: no cleanup
	CHECK(cleanups == 2);
}

static void testLaunchErrors()
{
	auto expectError = [](LaunchContext ctx, ErrorCode expected) {
		Launch launch(std::move(ctx));
		try
		{
			launch.prepare();
			CHECK(!"prepare() should have thrown");
		}
		catch (const LaunchError& e)
		{
			CHECK(e.code() == expected);
		}
	};

	LaunchContext unknown;
	unknown.emulator = "nope";
	CHECK(!Launch(unknown).handled());
	expectError(unknown, ErrorCode::UnknownEmulator);

	LaunchContext noRom;
	noRom.emulator = "fake";
	expectError(noRom, ErrorCode::BadArguments);

	LaunchContext empty;
	empty.emulator = "empty";
	empty.game.rom = "/x";
	expectError(empty, ErrorCode::InvalidConfiguration);

	CHECK(LaunchError(ErrorCode::CoreMissing, "x").exitCode() == 205);
}

static void testController()
{
	Controller pad;
	pad.inputs["a"] = { InputBinding::Type::Button, 0, 1 };
	CHECK(pad.find("a") != nullptr && pad.find("a")->id == 0);
	CHECK(pad.find("b") == nullptr);
}

static void testSettingsPrefix()
{
	Settings s({ { "retroarch.video_driver", "gl" }, { "retroarch.menu_driver", "ozone" }, { "ratio", "4/3" } },
	           { { "retroarch.video_driver", "vulkan" }, { "retroarch.audio_driver", "auto" } },
	           { { "retroarch.menu_driver", "" }, { "retroarch.fps_show", "true" } });

	auto raw = s.withPrefix("retroarch.");
	CHECK(raw.size() == 3);
	CHECK(raw.at("video_driver") == "vulkan");   // system over global
	CHECK(raw.at("menu_driver") == "ozone");     // empty game value falls through
	CHECK(raw.at("fps_show") == "true");
	CHECK(!raw.contains("audio_driver"));        // "auto" is not set
}

static void testEvdevLayout()
{
	using es::launcher::EvdevLayout;
	using es::launcher::InputBinding;
	using Type = InputBinding::Type;

	// a pad with a button below BTN_JOYSTICK (BTN_0), a d-pad hat and an axis above ABS_MISC
	EvdevLayout layout = EvdevLayout::fromCapabilities("Pad",
		{ BTN_SOUTH, BTN_EAST, BTN_0 },
		{ { ABS_X, -32768, 32767 }, { ABS_Y, -32768, 32767 }, { ABS_HAT0X, -1, 1 }, { ABS_HAT0Y, -1, 1 }, { ABS_MISC, 0, 255 } });
	CHECK(!layout.sameNumbering());

	// SDL: south 0, east 1, BTN_0 2; udev: BTN_0 0, south 1, east 2
	CHECK(layout.sdlToUdev(InputBinding{ Type::Button, 0, 1 })->id == 1);
	CHECK(layout.sdlToUdev(InputBinding{ Type::Button, 2, 1 })->id == 0);
	CHECK(layout.udevToSdl(InputBinding{ Type::Button, 0, 1 })->id == 2);
	CHECK(!layout.sdlToUdev(InputBinding{ Type::Button, 3, 1 }));

	// axes: the same below ABS_MISC, none above for udev
	CHECK(layout.sdlToUdev(InputBinding{ Type::Axis, 1, -1 })->id == 1);
	CHECK(layout.sdlToUdev(InputBinding{ Type::Axis, 1, -1 })->value == -1);
	CHECK(!layout.sdlToUdev(InputBinding{ Type::Axis, 2, 1 }));

	// hat 0 both ways, keys unchanged
	CHECK(layout.sdlToUdev(InputBinding{ Type::Hat, 0, 4 })->id == 0);
	CHECK(layout.udevToSdl(InputBinding{ Type::Hat, 0, 4 })->value == 4);
	CHECK(layout.sdlToUdev(InputBinding{ Type::Key, 97, 1 })->id == 97);

	// a usual pad: both agree
	EvdevLayout usual = EvdevLayout::fromCapabilities("Pad", { BTN_SOUTH, BTN_EAST, BTN_START },
		{ { ABS_X, -32768, 32767 }, { ABS_HAT0X, -1, 1 }, { ABS_HAT0Y, -1, 1 } });
	CHECK(usual.sameNumbering());

	// hat 1 alone: SDL hat 0, udev hat 1
	EvdevLayout secondHat = EvdevLayout::fromCapabilities("Pad", { BTN_SOUTH }, { { ABS_HAT1X, -1, 1 }, { ABS_HAT1Y, -1, 1 } });
	CHECK(secondHat.sdlToUdev(InputBinding{ Type::Hat, 0, 1 })->id == 1);
	CHECK(secondHat.udevToSdl(InputBinding{ Type::Hat, 1, 1 })->id == 0);

	// a device ES cannot read
	CHECK(!EvdevLayout::read("/nonexistent"));
}

void tests::runLauncherTests()
{
	testEvdevLayout();
	testShellQuote();
	testShellLine();
	testSettings();
	testSettingsPrefix();
	testRegistry();
	testLaunch();
	testLaunchErrors();
	testController();
}
