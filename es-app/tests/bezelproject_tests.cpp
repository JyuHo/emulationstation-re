// Unit tests of The Bezel Project lists (bezels/BezelProjectList).
// Built with -DES_TESTS=ON, run with ctest.

#include "bezels/BezelProjectList.h"

#include <cstdio>
#include <string>

using namespace bezelproject;

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

	void testSystems()
	{
		auto systems = parseSystems(
			"# The Bezel Project systems\n"
			"snes https://github.com/thebezelproject/bezelproject-SNES SNES Super-Nintendo-Entertainment-System.png\r\n"
			"\n"
			"fbneo\thttps://github.com/thebezelproject/bezelproject-MAME/\tArcadeBezels\tMAME-Horizontal.cfg\n"
			"nes https://github.com/thebezelproject/bezelproject-NES NES\n"
			"bad http://example.com/x NES\n"
			"../x https://github.com/x NES\n"
			"short https://github.com/x\n");

		CHECK(systems.size() == 3);
		CHECK(systems[0].name == "snes");
		CHECK(systems[0].repository == "https://github.com/thebezelproject/bezelproject-SNES");
		CHECK(systems[0].folder == "SNES");
		CHECK(systems[0].systemBezel == "Super-Nintendo-Entertainment-System.png");
		CHECK(systems[1].repository == "https://github.com/thebezelproject/bezelproject-MAME");
		CHECK(systems[2].systemBezel.empty());

		CHECK(systemBezelUrl(systems[0]) == "https://github.com/thebezelproject/bezelproject-SNES/raw/master/retroarch/overlay/Super-Nintendo-Entertainment-System.png");
		CHECK(systemBezelUrl(systems[1]).empty()); // a .cfg, not a picture
		CHECK(systemBezelUrl(systems[2]).empty());

		CHECK(bezelUrl(systems[0], "Wild Guns (USA).png") == "https://github.com/thebezelproject/bezelproject-SNES/raw/master/retroarch/overlay/GameBezels/SNES/Wild%20Guns%20%28USA%29.png");
		CHECK(bezelUrl(systems[1], "sf2.png") == "https://github.com/thebezelproject/bezelproject-MAME/raw/master/retroarch/overlay/ArcadeBezels/sf2.png");
	}

	void testIndex()
	{
		auto index = parseIndex(
			"1665E82431AAC0E897521575B84B2A83 Wild Guns (USA).png\n"
			"ac9a8203106c31d0efa60860f7364311 Incredible Hulk, The (USA).png\n"
			"031d5a4b8a0884d053cf0ca4836bee34 Wild Guns (Japan).png\n"
			"tooshort Foo.png\n"
			"c21d5f026624c0dfa098141527364a3c ../evil.png\n"
			"c21d5f026624c0dfa098141527364a3c Readme.txt\n");

		CHECK(index.size() == 3);
		CHECK(index[0].md5 == "1665e82431aac0e897521575b84b2a83");
		CHECK(index[1].file == "Incredible Hulk, The (USA).png");

		auto bezels = byMatchKey(index);
		CHECK(bezels.size() == 2);
		CHECK(bezels["wild guns"].file == "Wild Guns (USA).png"); // the first one wins
	}

	void testMatching()
	{
		CHECK(matchKey("Wild Guns (Europe).sfc") == "wild guns");
		CHECK(matchKey("Super Metroid [!].smc") == "super metroid");
		CHECK(matchKey("Super Metroid (USA) [b1].zip") == "super metroid");
		CHECK(matchKey("Mr. Do!.zip") == "mr. do!");
		CHECK(matchKey("sf2") == "sf2");

		CHECK(isIgnoredFile("gamelist.xml"));
		CHECK(isIgnoredFile(".hidden.sfc"));
		CHECK(isIgnoredFile("_info.sfc"));
		CHECK(isIgnoredFile("cover.PNG"));
		CHECK(!isIgnoredFile("Wild Guns (USA).sfc"));

		CHECK(urlEncode("a b(c)&é") == "a%20b%28c%29%26%C3%A9");
	}
}

int main()
{
	testSystems();
	testIndex();
	testMatching();

	if (failures == 0)
		std::printf("bezelproject tests: OK\n");
	return failures == 0 ? 0 : 1;
}
