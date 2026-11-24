// Unit tests of the RomM scraper helpers: server URLs, platform aliases and
// media resource URLs. Built with -DES_TESTS=ON, run with ctest.

#include "scrapers/RomMScraperUrl.h"

#include <cstdio>
#include <string>

namespace
{
	int failures = 0;

	void check(bool condition, const char* expression, const char* file, int line)
	{
		if (!condition)
		{
			std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", file, line, expression);
			++failures;
		}
	}

	#define CHECK(x) check((x), #x, __FILE__, __LINE__)
}

int main()
{
	using namespace RomMScraperUrl;

	// A server root or the API URL can be pasted in the settings.
	CHECK(normalizeServer("http://host:8080/") == "http://host:8080");
	CHECK(normalizeServer("host:8080") == "http://host:8080");
	CHECK(normalizeServer(" https://host/romm/api/ ") == "https://host/romm");
	CHECK(normalizeServer("https://host/romm/") == "https://host/romm");
	CHECK(normalizeServer("") == "");
	CHECK(normalizeServer("http://") == "");

	// Platform names match directly or through the Batocera/ES-RE aliases.
	CHECK(platformMatches("ps", "psx", "ps"));
	CHECK(platformMatches("gc", "ngc", "gamecube"));
	CHECK(platformMatches("snes", "sfam", "sfc"));
	CHECK(platformMatches("dreamcast", "dc", "dreamcast"));
	CHECK(platformMatches("n64", "n64", "n64"));
	CHECK(!platformMatches("snes", "genesis", "genesis"));
	CHECK(!platformMatches("", "snes", "snes"));
	CHECK(normalizeName("Super Nintendo") == "supernintendo");
	CHECK(normalizeName("N64DD") == "n64dd");

	// RomM returns paths under its resources root, or absolute provider URLs.
	CHECK(resourceUrl("http://host", "roms/1/cover.png") == "http://host/assets/romm/resources/roms/1/cover.png");
	CHECK(resourceUrl("http://host", "/assets/romm/resources/x.png?ts=1") == "http://host/assets/romm/resources/x.png?ts=1");
	CHECK(resourceUrl("http://host", "https://cdn.example/x.png") == "https://cdn.example/x.png");
	CHECK(resourceUrl("http://host", "file://roms/snes/covers/x.png") == "");
	CHECK(resourceUrl("http://host", "/assets/romm/resources/x.png?ts=2026-11-24 21:36:20") == "http://host/assets/romm/resources/x.png?ts=2026-11-24%2021:36:20");
	CHECK(resourceUrl("http://host", "") == "");

	// The media download keeps the extension but not the cache-busting query.
	CHECK(extensionOf("http://host/a/b.png?ts=1") == ".png");
	CHECK(extensionOf("roms/1/cover.JPG") == ".JPG");
	CHECK(extensionOf("http://host/a/b") == "");
	CHECK(extensionOf("http://host/a.b/c") == "");

	if (failures == 0)
		std::printf("RomM scraper tests passed\n");

	return failures == 0 ? 0 : 1;
}
