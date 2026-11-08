#include "updates/SelfUpdater.h"

#include "HttpReq.h"
#include "LocaleES.h"
#include "Log.h"
#include "utils/StringUtil.h"

#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

#ifndef ES_RELEASES_URL
#define ES_RELEASES_URL "https://github.com/JyuHo/emulationstation-re/releases"
#endif
#ifndef ES_UPDATE_HELPER
#define ES_UPDATE_HELPER "/usr/libexec/emulationstation-re/es-re-update"
#endif

namespace
{
	bool inPath(const std::string& program)
	{
		return system(("command -v " + program + " >/dev/null 2>&1").c_str()) == 0;
	}

	// Runs a shell command, gives each line it prints to onLine; returns its exit code
	int run(const std::string& command, const std::function<void(const std::string&)>& onLine)
	{
		LOG(LogInfo) << "Updates: running " << command;

		FILE* pipe = popen(command.c_str(), "r");
		if (pipe == nullptr)
			return -1;

		char buffer[1024];
		std::string line;
		while (fgets(buffer, sizeof(buffer), pipe))
		{
			line += buffer;
			if (!line.empty() && line.back() == '\n')
			{
				line.pop_back();
				onLine(line);
				line.clear();
			}
		}
		if (!line.empty())
			onLine(line);

		int status = pclose(pipe);
		return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	}
}

// Built with append(): GCC 14 at -O2 reports a false -Warray-bounds on string + literal here
std::string SelfUpdater::latestUrl()
{
	std::string url(ES_RELEASES_URL);
	url.append("/latest/download/latest.json");
	return url;
}
std::string SelfUpdater::helperPath() { return ES_UPDATE_HELPER; }

std::string SelfUpdater::platform()
{
	if (access(helperPath().c_str(), X_OK) != 0)
		return "";

	std::string command = helperPath();
	command.append(" platform 2>/dev/null");

	std::string result;
	run(command, [&result](const std::string& line)
	{
		if (result.empty())
			result = Utils::String::trim(line);
	});
	return result;
}

bool SelfUpdater::canInstall(const std::string& platform)
{
	return !platform.empty() && platform != "not-packaged" && platform != "unsupported" && inPath("pkexec");
}

std::optional<selfupdate::Release> SelfUpdater::fetchLatest(std::string* error)
{
	auto url = latestUrl();
	HttpReq request(url);
	if (!request.wait())
	{
		LOG(LogError) << "Updates: cannot download " << url << ": " << request.getErrorMsg();
		if (error)
			*error = request.getErrorMsg();
		return std::nullopt;
	}

	std::string why;
	auto release = selfupdate::parseRelease(request.getContent(), &why);
	if (!release)
	{
		LOG(LogError) << "Updates: " << url << " is not usable: " << why;
		if (error)
			*error = why;
	}
	return release;
}

SelfUpdater::Result SelfUpdater::install(const Progress& progress)
{
	std::string last;
	int code = run("pkexec " + helperPath() + " install 2>&1", [&](const std::string& line)
	{
		last = line;
		LOG(LogInfo) << "Updates: " << line;
		if (progress)
			progress(line);
	});

	// pkexec: 126 when the authorization was refused, 127 when it could not authenticate
	if (code == 126 || code == 127)
		return Result(_("The update was not authorized"), code);

	return Result(last, code);
}
