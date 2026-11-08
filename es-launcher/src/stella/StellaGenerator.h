// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// Adapted from batocera-launch (batocera_launch.emulators.stella), Copyright (c) 2026 The Batocera Team.

#pragma once

#include "es-launcher/Generator.h"

namespace es::launcher::stella
{
	// Starts Stella (Atari 2600). Registered as "stella".
	//
	// Stella keeps its own settings (~/.config/stella); like batocera-launch, the generator only
	// starts it with the game, full screen, and gives its SDL the pads as ES mapped them.
	class StellaGenerator : public Generator
	{
	public:
		Command configure(const LaunchContext& ctx) override;
	};
}
