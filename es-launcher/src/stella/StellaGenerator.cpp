// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.stella), Copyright (c) 2026 The Batocera Team.

#include "stella/StellaGenerator.h"

#include "es-launcher/LaunchError.h"
#include "util/EmulatorCommand.h"
#include "util/Gamepad.h"

namespace es::launcher::stella
{
	ES_LAUNCHER_REGISTER_GENERATOR("stella", StellaGenerator)

	Command StellaGenerator::configure(const LaunchContext& ctx)
	{
		auto program = util::emulatorCommand(ctx, "stella", "stella", { ctx.game.rom.parent_path() });
		if (!program)
			throw LaunchError(ErrorCode::EmulatorMissing, "Stella is not installed: install it with your package manager (stella).");

		Command cmd;
		cmd.argv = *program;
		cmd.argv.insert(cmd.argv.end(), { "-fullscreen", "1", ctx.game.rom.string() });

		cmd.closeOnHotkey = true;   // its hotkeys are on the keyboard
		util::setPadEnvironment(cmd, ctx.controllers);
		util::forwardEnvironment(cmd);
		return cmd;
	}
}
