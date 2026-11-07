#pragma once

// The latest EmulationStation-RE release, as ES's update menu sees it.
//
// The release workflow (.github/workflows/release.yml) attaches latest.json to every GitHub
// release; ES downloads it from <repo>/releases/latest/download/latest.json, which is not the
// GitHub API and so has no rate limit. This file only parses and compares, without I/O, so it
// is unit-tested on its own; SelfUpdater does the download and runs the installer.
//
// latest.json, format 1:
//   { "format": 1, "version": "1.2.0", "tag": "v1.2.0", "date": "2026-11-07",
//     "url": "https://github.com/.../releases/tag/v1.2.0",
//     "packages": { "arch-x86_64":  { "file": "emulationstation-re-1.2.0-1-x86_64.pkg.tar.zst", "sha256": "…", "size": 123 },
//                   "debian-arm64": { "file": "emulationstation-re_1.2.0_arm64.deb", "sha256": "…", "size": 123 } } }

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace selfupdate
{
	constexpr int FORMAT = 1;

	struct Package
	{
		std::string file;
		std::string sha256;
		uint64_t    size = 0;
	};

	struct Release
	{
		std::string version; // without the leading v
		std::string tag;
		std::string date;
		std::string url;     // release page
		std::map<std::string, Package> packages; // by platform: arch-x86_64, debian-amd64, debian-arm64
	};

	std::optional<Release> parseRelease(const std::string& json, std::string* error = nullptr);

	// < 0 when a is older than b, 0 when equal, > 0 when newer. Numbers are compared one by one
	// (1.10 > 1.9, a missing number counts as 0). After them, a pre-release suffix (1.2.0-rc1) is
	// older than the release, while a git describe suffix (1.2.0-3-gabcdef) is newer.
	int compareVersions(const std::string& a, const std::string& b);
}
