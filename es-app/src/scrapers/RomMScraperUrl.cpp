#include "scrapers/RomMScraperUrl.h"

#include <cctype>
#include <map>

namespace
{
	bool isSpace(char c)
	{
		return c == ' ' || c == '\t' || c == '\r' || c == '\n';
	}

	std::string encodeSpaces(const std::string& url)
	{
		std::string result;
		result.reserve(url.size());

		for (char c : url)
			if (c == ' ')
				result += "%20";
			else
				result += c;

		return result;
	}

	int platformGroup(const std::string& name)
	{
		// Names that RomM, Batocera and ES-RE spell differently. The values are
		// group ids: two names in the same group are the same platform.
		static const std::map<std::string, int> groups =
		{
			{ "psx", 1 }, { "ps", 1 }, { "playstation", 1 },
			{ "ngc", 2 }, { "gc", 2 }, { "gamecube", 2 },
			{ "genesis", 3 }, { "megadrive", 3 }, { "md", 3 }, { "megadrivejp", 3 }, { "msumd", 3 }, { "megadrivemsu", 3 },
			{ "snes", 4 }, { "sfc", 4 }, { "sfam", 4 }, { "supernintendo", 4 }, { "snesmsu1", 4 },
			{ "nes", 5 }, { "famicom", 5 },
			{ "sms", 6 }, { "mastersystem", 6 }, { "mark3", 6 },
			{ "dc", 7 }, { "dreamcast", 7 },
			{ "tg16", 8 }, { "pcengine", 8 },
			{ "turbografxcd", 9 }, { "pcenginecd", 9 }, { "tgcd", 9 },
			{ "segacd", 10 }, { "megacd", 10 }, { "megacdjp", 10 },
			{ "3ds", 11 }, { "n3ds", 11 },
			{ "64dd", 12 }, { "n64dd", 12 },
			{ "wii", 13 }, { "wiiware", 13 },
			{ "win", 14 }, { "pc", 14 }, { "windows", 14 },
			{ "amiga", 15 }, { "amiga500", 15 }, { "amiga600", 15 }, { "amiga1200", 15 }, { "amiga4000", 15 },
			{ "amigacd", 16 }, { "amigacdtv", 16 },
			{ "amigacd32", 17 }, { "cd32", 17 },
			{ "rpgmaker", 18 }, { "easyrpg", 18 },
			{ "doom", 19 }, { "gzdoom", 19 }, { "prboom", 19 },
			{ "arcade", 20 }, { "mame", 20 }, { "fbneo", 20 }, { "fba", 20 },
			{ "neogeo", 21 }, { "neogeoaes", 21 },
			{ "neogeocd", 22 },
			{ "ngp", 23 }, { "neogeopocket", 23 },
			{ "ngpc", 24 }, { "neogeopocketcolor", 24 },
			{ "atarijaguar", 25 }, { "jaguar", 25 },
			{ "atarijaguarcd", 26 },
			{ "atarilynx", 27 }, { "lynx", 27 },
			{ "atarist", 28 },
			{ "atarixe", 29 }, { "xegs", 29 },
			{ "amstradcpc", 30 }, { "acpc", 30 },
			{ "zxspectrum", 31 }, { "zxs", 31 },
			{ "c64", 32 }, { "commodore64", 32 },
			{ "cplus4", 33 }, { "plus4", 33 },
			{ "vic20", 34 },
			{ "cdi", 35 }, { "cdimono1", 35 }, { "philipscdi", 35 },
			{ "pcfx", 36 },
			{ "pc88", 37 }, { "pc8800series", 37 },
			{ "pc98", 38 }, { "pc9800series", 38 },
			{ "x68000", 39 },
			{ "virtualboy", 40 },
			{ "supergrafx", 41 },
			{ "saturn", 42 }, { "segasaturn", 42 },
			{ "gamegear", 43 }, { "gg", 43 },
			{ "sega32x", 44 }, { "32x", 44 },
			{ "pico", 45 }, { "pico8", 45 },
			{ "scummvm", 46 },
			{ "tic80", 47 },
			{ "wonderswan", 48 }, { "wswan", 48 },
			{ "wonderswancolor", 49 }, { "wswanc", 49 },
			{ "msx", 50 }, { "msx1", 50 },
			{ "msx2", 51 },
			{ "msx2plus", 52 },
			{ "msxturbor", 53 },
			{ "apple2", 54 }, { "appleii", 54 },
			{ "apple2gs", 55 }, { "appleiigs", 55 },
			{ "macintosh", 56 }, { "mac", 56 },
			{ "archimedes", 57 }, { "acornarchimedes", 57 },
			{ "bbc", 58 }, { "bbcmicro", 58 },
			{ "electron", 59 }, { "acornelectron", 59 },
			{ "oricatmos", 60 }, { "atmos", 60 },
			{ "ti99", 61 },
			{ "coco", 62 }, { "trs80colorcomputer", 62 },
			{ "vectrex", 63 },
			{ "videopac", 64 }, { "odyssey2", 64 },
			{ "intellivision", 65 },
			{ "colecovision", 66 },
			{ "channelf", 67 },
			{ "astrocde", 68 }, { "astrocade", 68 },
		};

		auto it = groups.find(name);
		return it == groups.end() ? -1 : it->second;
	}

	bool samePlatform(const std::string& a, const std::string& b)
	{
		if (a.empty() || b.empty())
			return false;

		if (a == b)
			return true;

		const int groupA = platformGroup(a);
		return groupA != -1 && groupA == platformGroup(b);
	}
}

namespace RomMScraperUrl
{
	std::string normalizeServer(const std::string& server)
	{
		std::string result = server;

		while (!result.empty() && isSpace(result.front()))
			result.erase(result.begin());
		while (!result.empty() && isSpace(result.back()))
			result.pop_back();

		if (result.empty())
			return "";

		if (result.find("://") == std::string::npos)
			result = "http://" + result;

		const auto scheme = result.find("://");
		if (scheme == std::string::npos || scheme + 3 >= result.size())
			return "";

		while (!result.empty() && result.back() == '/')
			result.pop_back();

		if (result.size() >= 4 && result.compare(result.size() - 4, 4, "/api") == 0)
		{
			result.resize(result.size() - 4);
			while (!result.empty() && result.back() == '/')
				result.pop_back();
		}

		return result;
	}

	std::string normalizeName(const std::string& name)
	{
		std::string result;
		result.reserve(name.size());

		for (unsigned char c : name)
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
				result.push_back(static_cast<char>(std::tolower(c)));

		return result;
	}

	bool platformMatches(const std::string& systemName, const std::string& slug, const std::string& fsSlug)
	{
		const std::string system = normalizeName(systemName);
		if (system.empty())
			return false;

		return samePlatform(system, normalizeName(slug)) || samePlatform(system, normalizeName(fsSlug));
	}

	std::string resourceUrl(const std::string& server, const std::string& pathOrUrl)
	{
		if (pathOrUrl.empty())
			return "";

		// Gamelist media that RomM did not copy to its resources: a file on the
		// server, which this scraper cannot download.
		if (pathOrUrl.rfind("file://", 0) == 0)
			return "";

		if (pathOrUrl.rfind("http://", 0) == 0 || pathOrUrl.rfind("https://", 0) == 0)
			return encodeSpaces(pathOrUrl);

		if (pathOrUrl.front() == '/')
			return encodeSpaces(server + pathOrUrl);

		return encodeSpaces(server + "/assets/romm/resources/" + pathOrUrl);
	}

	std::string extensionOf(const std::string& url)
	{
		std::string path = url;

		const auto query = path.find_first_of("?#");
		if (query != std::string::npos)
			path.resize(query);

		const auto slash = path.find_last_of('/');
		const auto dot = path.find_last_of('.');

		if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
			return "";

		return path.substr(dot);
	}
}
