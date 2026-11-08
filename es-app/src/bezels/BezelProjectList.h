#pragma once

// The lists that The Bezel Project installer works from, the same ones Batocera uses.
//
// Nothing of The Bezel Project is shipped with ES: the bezels are downloaded, one per game the
// player has, straight from The Bezel Project's GitHub repositories when the player asks for
// them, the way Batocera and RetroPie do.
//
// The systems list (tbp_systems.txt), one system per line, "#" for comments:
//   <system> <github repository> <bezels folder> [<system bezel file>]
//   snes https://github.com/thebezelproject/bezelproject-SNES SNES Super-Nintendo-Entertainment-System.png
// The bezels folder is the one under retroarch/overlay/GameBezels/ in the repository, or
// "ArcadeBezels" for retroarch/overlay/ArcadeBezels/.
//
// A system's index (tbp_<system>.txt), one bezel per line: "<md5> <file name>".
//   1665e82431aac0e897521575b84b2a83 Wild Guns (USA).png
//
// A ROM gets the bezel whose name, without its extension and from the first " (" or " [" on,
// is the ROM's (so "Wild Guns (Europe).sfc" gets "Wild Guns (USA).png").
//
// This file only parses and matches, without I/O, so it is unit-tested on its own;
// TheBezelProject does the downloads.

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace bezelproject
{
	struct System
	{
		std::string name;        // ES system name: snes
		std::string repository;  // https://github.com/thebezelproject/bezelproject-SNES
		std::string folder;      // SNES, or ArcadeBezels
		std::string systemBezel; // Super-Nintendo-Entertainment-System.png, may be empty
	};

	struct Bezel
	{
		std::string md5;
		std::string file;        // Wild Guns (USA).png
	};

	std::vector<System> parseSystems(const std::string& text);
	std::vector<Bezel> parseIndex(const std::string& text);

	// "Wild Guns (Europe) [!].sfc" -> "wild guns"
	std::string matchKey(const std::string& fileName);

	// The bezels of an index by match key; the first one of a key wins
	std::map<std::string, Bezel> byMatchKey(const std::vector<Bezel>& index);

	// Where a bezel of the index, or the system bezel, is downloaded from
	std::string bezelUrl(const System& system, const std::string& file);
	std::string systemBezelUrl(const System& system);

	// Percent-encoding of a file name in a URL ("Wild Guns (USA).png" -> "Wild%20Guns%20%28USA%29.png")
	std::string urlEncode(const std::string& s);

	// Files of a ROM folder that are not games (pictures, videos, manuals, gamelists...)
	bool isIgnoredFile(const std::string& fileName);
}
