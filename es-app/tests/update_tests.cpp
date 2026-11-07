// Unit tests of the release model of ES's updates (updates/Release).
// Built with -DES_TESTS=ON, run with ctest.

#include "updates/Release.h"

#include <cstdio>
#include <string>

using namespace selfupdate;

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

	const std::string SHA(64, 'a');

	void testVersions()
	{
		CHECK(compareVersions("1.2.0", "1.2.0") == 0);
		CHECK(compareVersions("v1.2.0", "1.2.0") == 0);
		CHECK(compareVersions("1.2", "1.2.0") == 0);
		CHECK(compareVersions("1.10.0", "1.9.0") > 0);
		CHECK(compareVersions("1.2.0", "1.2.1") < 0);
		CHECK(compareVersions("2.0.0", "1.99.99") > 0);
		CHECK(compareVersions("1.2.0-rc1", "1.2.0") < 0);
		CHECK(compareVersions("1.2.0-rc1", "1.2.0-rc2") < 0);
		CHECK(compareVersions("1.2.0-rc1", "1.1.9") > 0);
		CHECK(compareVersions("1.2.0-3-gabcdef0", "1.2.0") > 0);
		CHECK(compareVersions("1.2.0-3-gabcdef0", "1.2.0-12-g1234567") < 0);
		CHECK(compareVersions("1.2.0-3-gabcdef0-dirty", "1.2.1") < 0);
		CHECK(compareVersions("0.0.0-dev", "0.1.0") < 0);
		CHECK(compareVersions("0.1.0", "0.0.0-dev") > 0);
	}

	void testParse()
	{
		std::string json = R"({ "format": 1, "version": "v1.2.0", "tag": "v1.2.0", "date": "2026-11-07",
		  "url": "https://example.org/releases/tag/v1.2.0",
		  "packages": {
		    "arch-x86_64":  { "file": "emulationstation-re-1.2.0-1-x86_64.pkg.tar.zst", "sha256": ")" + SHA + R"(", "size": 42 },
		    "debian-arm64": { "file": "emulationstation-re_1.2.0_arm64.deb", "sha256": ")" + SHA + R"(" },
		    "bad-sha":      { "file": "x.deb", "sha256": "nothex" },
		    "bad-file":     { "file": "../x.deb", "sha256": ")" + SHA + R"(" } } })";

		auto release = parseRelease(json);
		CHECK(release.has_value());
		if (!release)
			return;
		CHECK(release->version == "1.2.0");
		CHECK(release->tag == "v1.2.0");
		CHECK(release->date == "2026-11-07");
		CHECK(release->url == "https://example.org/releases/tag/v1.2.0");
		CHECK(release->packages.size() == 2);
		CHECK(release->packages["arch-x86_64"].size == 42);
		CHECK(release->packages["debian-arm64"].file == "emulationstation-re_1.2.0_arm64.deb");
		CHECK(release->packages.count("bad-sha") == 0);
		CHECK(release->packages.count("bad-file") == 0);
	}

	void testRejects()
	{
		std::string error;
		CHECK(!parseRelease("not json", &error) && !error.empty());
		CHECK(!parseRelease(R"({ "format": 2, "version": "1.0.0" })", &error) && error == "unknown format");
		CHECK(!parseRelease(R"({ "format": 1 })", &error) && error == "no version");
		CHECK(!parseRelease(R"({ "format": 1, "version": "latest" })"));
		CHECK(parseRelease(R"({ "format": 1, "version": "1.0.0" })").has_value());
	}
}

int main()
{
	testVersions();
	testParse();
	testRejects();

	if (failures)
		std::fprintf(stderr, "%d check(s) failed\n", failures);
	else
		std::printf("update tests passed\n");
	return failures ? 1 : 0;
}
