#include "updates/SelfUpdater.h"

#include "ApiSystem.h"
#include "HttpReq.h"
#include "LocaleES.h"
#include "Log.h"
#include "utils/StringUtil.h"

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
}

std::string SelfUpdater::latestUrl() { return std::string(ES_RELEASES_URL) + "/latest/download/latest.json"; }
std::string SelfUpdater::helperPath() { return ES_UPDATE_HELPER; }

std::string SelfUpdater::platform()
{
	if (access(helperPath().c_str(), X_OK) != 0)
		return "";

	std::string result;
	ApiSystem::getInstance()->executeScript(helperPath() + " platform 2>/dev/null", [&result](const std::string& line)
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
	auto result = ApiSystem::getInstance()->executeScript("pkexec " + helperPath() + " install 2>&1", [&](const std::string& line)
	{
		last = line;
		LOG(LogInfo) << "Updates: " << line;
		if (progress)
			progress(line);
	});

	// pkexec: 126 when the authorization was refused, 127 when it could not authenticate
	if (result.second == 126 || result.second == 127)
		return Result(_("The update was not authorized"), result.second);

	return Result(last, result.second);
}
