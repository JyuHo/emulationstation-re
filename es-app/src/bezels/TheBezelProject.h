#pragma once

// The Bezel Project from the "Updates & downloads" menu, as Batocera does it: for a system, the
// bezels of the games the player has are downloaded from The Bezel Project's GitHub repository
// into the "thebezelproject" decoration set:
//   <decorations>/thebezelproject/games/<system>/<rom name>.png   one per game
//   <decorations>/thebezelproject/systems/<system>.png            the system's own bezel
// es-launcher shows them as RetroArch overlays when the system's decoration set is
// "thebezelproject".
//
// The lists (see BezelProjectList.h) are Batocera's, at updates.batocera.org. A copy of them in
// ~/.config/emulationstation/thebezelproject/ (tbp_systems.txt, tbp_<system>.txt) is used
// instead when it exists.

#include "bezels/BezelProjectList.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace bezelproject
{
	struct Entry
	{
		System system;
		bool installed = false;
	};

	using Progress = std::function<void(const std::string&)>;

	std::string setPath();     // <decorations>/thebezelproject

	// The systems of the list that ES has; empty with *error set when the list cannot be read
	std::vector<Entry> list(std::string* error = nullptr);

	// <message, 0> when done, <message, 1> on failure (ApiSystem's script convention)
	std::pair<std::string, int> install(const std::string& system, const Progress& progress = nullptr);
	std::pair<std::string, int> remove(const std::string& system);
}
