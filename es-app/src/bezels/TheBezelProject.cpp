#include "bezels/TheBezelProject.h"

#include "HttpReq.h"
#include "LocaleES.h"
#include "Log.h"
#include "Paths.h"
#include "SystemData.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <map>
#include <set>

namespace bezelproject
{
	namespace
	{
		const std::string LISTS_URL = "https://updates.batocera.org/";
		const std::string SET_NAME = "thebezelproject";
		const int MAX_FAILURES = 10;

		std::string localListPath(const std::string& name)
		{
			return Paths::getConfigPath() + "/thebezelproject/" + name;
		}

		std::string cachedListPath(const std::string& name)
		{
			return Paths::getCachePath() + "/thebezelproject/" + name;
		}

		// The player's own copy, else the hosted list (kept in the cache for when it cannot be downloaded)
		std::string readList(const std::string& name, const std::string& url, std::string* error)
		{
			std::string local = localListPath(name);
			if (Utils::FileSystem::exists(local, false))
				return Utils::FileSystem::readAllText(local);

			std::string cache = cachedListPath(name);

			HttpReq request(url);
			if (request.wait())
			{
				std::string text = request.getContent();
				Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(cache));
				Utils::FileSystem::writeAllText(cache, text);
				return text;
			}

			LOG(LogWarning) << "TheBezelProject: cannot download " << url << ": " << request.getErrorMsg();
			if (Utils::FileSystem::exists(cache, false))
				return Utils::FileSystem::readAllText(cache);

			if (error)
				*error = request.getErrorMsg();
			return "";
		}

		std::vector<System> readSystems(std::string* error)
		{
			std::string why;
			auto systems = parseSystems(readList("tbp_systems.txt", LISTS_URL + "tbp_systems.txt", &why));
			if (systems.empty() && error)
				*error = why.empty() ? _("The Bezel Project's systems list is empty") : why;
			return systems;
		}

		SystemData* findSystem(const std::string& name)
		{
			for (auto system : SystemData::sSystemVector)
				if (system->getName() == name && !system->isCollection())
					return system;
			return nullptr;
		}

		std::string gamesPath(const std::string& system) { return setPath() + "/games/" + system; }
		std::string systemBezelPath(const std::string& system) { return setPath() + "/systems/" + system + ".png"; }

		bool isGame(SystemData* system, const std::string& path)
		{
			std::string fileName = Utils::FileSystem::getFileName(path);
			if (isIgnoredFile(fileName) || !Utils::FileSystem::isRegularFile(path))
				return false;

			const auto& extensions = system->getExtensions();
			if (extensions.empty())
				return true;

			std::string ext = Utils::FileSystem::getExtension(fileName);
			return extensions.count(ext) != 0 || extensions.count(Utils::String::toLower(ext)) != 0;
		}

		bool download(const std::string& url, const std::string& file)
		{
			std::string part = file + ".part";
			Utils::FileSystem::removeFile(part);

			HttpReq request(url, part);
			if (!request.wait() || !Utils::FileSystem::exists(part, false))
			{
				LOG(LogWarning) << "TheBezelProject: cannot download " << url << ": " << request.getErrorMsg();
				Utils::FileSystem::removeFile(part);
				return false;
			}

			return Utils::FileSystem::renameFile(part, file);
		}
	}

	std::string setPath()
	{
		return Paths::getUserDecorationsPath() + "/" + SET_NAME;
	}

	std::vector<Entry> list(std::string* error)
	{
		std::vector<Entry> entries;
		std::set<std::string> seen;

		for (const auto& system : readSystems(error))
		{
			if (!seen.insert(system.name).second || findSystem(system.name) == nullptr)
				continue;

			Entry entry;
			entry.system = system;
			entry.installed = Utils::FileSystem::exists(gamesPath(system.name), false);
			entries.push_back(entry);
		}

		return entries;
	}

	std::pair<std::string, int> install(const std::string& name, const Progress& progress)
	{
		auto report = [&progress](const std::string& text) { if (progress) progress(text); };

		std::string error;
		System system;
		bool found = false;
		for (const auto& item : readSystems(&error))
		{
			if (item.name == name)
			{
				system = item;
				found = true;
				break;
			}
		}

		if (!found)
			return { error.empty() ? _("The Bezel Project has no bezels for this system") : error, 1 };

		SystemData* esSystem = findSystem(name);
		if (esSystem == nullptr)
			return { _("Unknown system") + std::string(" ") + name, 1 };

		report(_("Downloading the list of bezels"));
		auto index = parseIndex(readList("tbp_" + name + ".txt", LISTS_URL + "thebezelproject/tbp_" + name + ".txt", &error));
		if (index.empty())
			return { error.empty() ? _("The Bezel Project has no bezels for this system") : error, 1 };

		auto bezels = byMatchKey(index);

		std::string games = gamesPath(name);
		Utils::FileSystem::createDirectory(games);
		Utils::FileSystem::createDirectory(setPath() + "/systems");

		// The system's own bezel, for the games without one
		std::string systemUrl = systemBezelUrl(system);
		if (!systemUrl.empty() && !Utils::FileSystem::exists(systemBezelPath(name), false))
			download(systemUrl, systemBezelPath(name));

		// The ROMs that have a bezel
		std::vector<std::pair<std::string, Bezel>> wanted;
		std::set<std::string> names;
		for (const auto& path : Utils::FileSystem::getDirContent(esSystem->getStartPath(), true))
		{
			if (!isGame(esSystem, path))
				continue;

			std::string stem = Utils::FileSystem::getStem(path);
			auto it = bezels.find(matchKey(Utils::FileSystem::getFileName(path)));
			if (it != bezels.cend() && names.insert(stem).second)
				wanted.push_back({ stem, it->second });
		}

		int done = 0, copied = 0, failures = 0;
		std::map<std::string, std::string> downloaded; // bezel file -> where it already is

		for (const auto& [stem, bezel] : wanted)
		{
			done++;
			report(Utils::String::format("%d/%d", done, (int)wanted.size()) + "  " + stem);

			std::string target = games + "/" + stem + ".png";
			if (Utils::FileSystem::exists(target, false) && Utils::FileSystem::getFileMd5(target) == bezel.md5)
				continue;

			auto previous = downloaded.find(bezel.file);
			bool ok = previous != downloaded.cend()
				? Utils::FileSystem::copyFile(previous->second, target)
				: download(bezelUrl(system, bezel.file), target);

			if (ok)
			{
				copied++;
				downloaded[bezel.file] = target;
			}
			else if (++failures > MAX_FAILURES)
				return { _("Too many downloads failed"), 1 };
		}

		LOG(LogInfo) << "TheBezelProject: " << name << ": " << wanted.size() << " games with a bezel, " << copied << " downloaded, " << failures << " failed";

		if (wanted.empty() && !Utils::FileSystem::exists(systemBezelPath(name), false))
			return { _("None of your games has a bezel"), 1 };

		return { Utils::String::format(_("%d bezels").c_str(), (int)wanted.size()), 0 };
	}

	std::pair<std::string, int> remove(const std::string& name)
	{
		if (name.empty() || name.find('/') != std::string::npos || name == "." || name == "..")
			return { _("Unknown system"), 1 };

		std::string games = gamesPath(name);
		if (Utils::FileSystem::isDirectory(games))
			Utils::FileSystem::deleteDirectoryFiles(games, true);

		Utils::FileSystem::removeFile(systemBezelPath(name));

		return { "", Utils::FileSystem::exists(games, false) ? 1 : 0 };
	}
}
