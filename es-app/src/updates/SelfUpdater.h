#pragma once

// Updates EmulationStation-RE itself from its GitHub releases (see updates/Release.h).
//
// ES reads latest.json to show what is available; the install is done by es-re-update, run as
// root through pkexec (packaging/update). That helper takes no argument besides "install": it
// fetches the latest release's package for this machine itself and checks it against the
// release's SHA256SUMS, so ES never hands a file to a root process.

#include "updates/Release.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>

class SelfUpdater
{
public:
	using Progress = std::function<void(const std::string&)>;
	using Result = std::pair<std::string, int>; // message, 0 on success (like ApiSystem)

	static std::string latestUrl();
	static std::string helperPath();

	// Release package that fits this machine (arch-x86_64, debian-arm64...), "not-packaged" when
	// ES was not installed from the package, "unsupported" or "" (no helper) otherwise
	static std::string platform();
	static bool canInstall(const std::string& platform);

	static std::optional<selfupdate::Release> fetchLatest(std::string* error = nullptr);

	// Runs pkexec es-re-update install; progress gets each line it prints
	static Result install(const Progress& progress = nullptr);
};
