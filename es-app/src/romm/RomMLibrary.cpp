#include "romm/RomMLibrary.h"

#include "romm/RomMClient.h"

#include "HttpReq.h"
#include "Log.h"
#include "Paths.h"
#include "Settings.h"
#include "SystemData.h"
#include "scrapers/RomMScraperUrl.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/TimeUtil.h"
#include "utils/ZipFile.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <pugixml.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace
{
	std::string stringValue(const rapidjson::Value& object, const char* key)
	{
		if (!object.IsObject() || !object.HasMember(key) || !object[key].IsString())
			return "";

		return object[key].GetString();
	}

	int intValue(const rapidjson::Value& object, const char* key)
	{
		if (!object.IsObject() || !object.HasMember(key) || !object[key].IsInt())
			return 0;

		return object[key].GetInt();
	}

	int64_t int64Value(const rapidjson::Value& object, const char* key)
	{
		if (!object.IsObject() || !object.HasMember(key))
			return 0;

		const rapidjson::Value& value = object[key];
		if (value.IsInt64())
			return value.GetInt64();
		if (value.IsInt())
			return value.GetInt();
		if (value.IsUint64())
			return static_cast<int64_t>(value.GetUint64());

		return 0;
	}

	double numberValue(const rapidjson::Value& object, const char* key)
	{
		if (!object.IsObject() || !object.HasMember(key) || !object[key].IsNumber())
			return 0.0;

		return object[key].GetDouble();
	}

	const rapidjson::Value* objectValue(const rapidjson::Value& object, const char* key)
	{
		if (!object.IsObject() || !object.HasMember(key) || !object[key].IsObject())
			return nullptr;

		return &object[key];
	}

	std::string joinedList(const rapidjson::Value& object, const char* key)
	{
		if (!object.IsObject() || !object.HasMember(key))
			return "";

		const rapidjson::Value& value = object[key];
		if (value.IsString())
			return value.GetString();

		if (!value.IsArray())
			return "";

		std::string result;
		for (const rapidjson::Value& item : value.GetArray())
		{
			if (!item.IsString() || item.GetString()[0] == 0)
				continue;

			if (!result.empty())
				result += ", ";

			result += item.GetString();
		}

		return result;
	}

	std::string firstListValue(const rapidjson::Value& object, const char* key)
	{
		if (!object.IsObject() || !object.HasMember(key) || !object[key].IsArray())
			return "";

		for (const rapidjson::Value& item : object[key].GetArray())
			if (item.IsString() && item.GetString()[0] != 0)
				return item.GetString();

		return "";
	}

	std::string providerValue(const rapidjson::Value& rom, const char* provider, const char* key)
	{
		const rapidjson::Value* metadata = objectValue(rom, provider);
		return metadata == nullptr ? "" : stringValue(*metadata, key);
	}

	std::string providerMedia(const rapidjson::Value& rom, const char* provider, const char* pathKey, const char* urlKey = nullptr)
	{
		std::string path = providerValue(rom, provider, pathKey);
		if (!path.empty() || urlKey == nullptr)
			return path;

		return providerValue(rom, provider, urlKey);
	}

	std::string firstMedia(const std::initializer_list<std::string>& values)
	{
		for (const std::string& value : values)
			if (!value.empty())
				return value;

		return "";
	}

	std::string screenshotPath(const rapidjson::Value& rom)
	{
		if (rom.IsObject() && rom.HasMember("merged_screenshots") && rom["merged_screenshots"].IsArray())
		{
			for (const rapidjson::Value& item : rom["merged_screenshots"].GetArray())
				if (item.IsString() && item.GetString()[0] != 0)
					return item.GetString();
		}

		return firstMedia({
			providerMedia(rom, "ss_metadata", "screenshot_path", "screenshot_url"),
			providerMedia(rom, "gamelist_metadata", "screenshot_path", "screenshot_url"),
			providerMedia(rom, "launchbox_metadata", "screenshot_path", "screenshot_url"),
		});
	}

	std::string coverPath(const rapidjson::Value& rom)
	{
		std::string cover = stringValue(rom, "path_cover_large");
		if (cover.empty())
			cover = stringValue(rom, "path_cover_small");
		if (cover.empty())
			cover = providerMedia(rom, "ss_metadata", "box2d_path", "box2d_url");
		if (cover.empty())
			cover = providerMedia(rom, "gamelist_metadata", "box2d_path", "box2d_url");
		if (cover.empty())
			cover = providerValue(rom, "gamelist_metadata", "image_url");
		if (cover.empty())
			cover = providerMedia(rom, "launchbox_metadata", "box2d_path", "box2d_url");
		if (cover.empty())
			cover = screenshotPath(rom);

		return cover;
	}

	std::string mediaExtension(const std::string& url, const std::string& fallback = ".png")
	{
		const std::string extension = RomMScraperUrl::extensionOf(url);
		return extension.empty() ? fallback : extension;
	}

	std::string mediaFileName(const std::string& stem, const char* suffix, const std::string& url)
	{
		return stem + "-" + suffix + mediaExtension(url);
	}

	bool writeTextFile(const fs::path& path, const std::string& content)
	{
		std::error_code ec;
		fs::create_directories(path.parent_path(), ec);

		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out)
			return false;

		out.write(content.data(), static_cast<std::streamsize>(content.size()));
		return out.good();
	}

	bool saveXml(const fs::path& path, pugi::xml_document& document)
	{
		std::ostringstream out;
		document.save(out, "  ");
		return writeTextFile(path, out.str());
	}

	void readSystemFile(const std::string& path, const std::string& name, std::string& result)
	{
		pugi::xml_document document;
		if (!document.load_file(path.c_str()))
			return;

		pugi::xml_node systems = document.child("systemList");
		if (!systems)
			return;

		for (pugi::xml_node system : systems.children("system"))
		{
			if (name != system.child("name").text().get())
				continue;

			std::ostringstream out;
			system.print(out, "  ");
			result = out.str();
		}
	}

	std::string systemNodeXml(const std::string& name)
	{
		std::string result;

		const std::string mainConfig = SystemData::getConfigPath();
		readSystemFile(mainConfig, name, result);

		std::vector<std::string> roots = { Paths::getConfigPath(), Paths::getEmulationStationPath() };
		roots.insert(roots.end(), Paths::getSystemConfigPaths().cbegin(), Paths::getSystemConfigPaths().cend());
		roots.insert(roots.end(), Paths::getSystemDataPaths().cbegin(), Paths::getSystemDataPaths().cend());

		std::set<std::string> visited;
		for (const std::string& root : roots)
		{
			if (root.empty() || !visited.insert(root).second)
				continue;

			for (const std::string& file : Utils::FileSystem::getDirContent(root, false, false))
			{
				if (file == mainConfig)
					continue;

				if (Utils::FileSystem::getExtension(file) != ".cfg")
					continue;

				if (!Utils::String::startsWith(Utils::FileSystem::getFileName(file), "es_systems_"))
					continue;

				readSystemFile(file, name, result);
			}
		}

		return result;
	}

	SystemData* findLocalSystem(const std::string& slug, const std::string& fsSlug)
	{
		for (SystemData* system : SystemData::sSystemVector)
		{
			if (system == nullptr || system->isCollection() || system->isGroupSystem() || !system->isGameSystem())
				continue;

			if (!system->getSystemMetadata().baseSystem.empty())
				continue;

			if (system->getExtensions().empty())
				continue;

			if (RomMScraperUrl::platformMatches(system->getName(), slug, fsSlug))
				return system;
		}

		return nullptr;
	}

	bool downloadMediaFile(const std::string& server, const std::string& pathOrUrl,
		const std::string& destination, RomMSyncResult& result,
		const std::function<void(const std::string&)>& setText)
	{
		if (pathOrUrl.empty())
			return false;

		std::error_code ec;
		if (fs::exists(destination, ec) && fs::file_size(destination, ec) > 0)
			return true;

		fs::create_directories(fs::path(destination).parent_path(), ec);

		if (setText)
			setText("Downloading " + fs::path(destination).filename().string());

		std::string error;
		if (!RomMClient::downloadMedia(server, pathOrUrl, destination, error))
		{
			LOG(LogWarning) << "RomM: cannot download " << pathOrUrl << ": " << error;
			return false;
		}

		result.media++;
		return true;
	}

	struct RomFileInfo
	{
		int id = 0;
		std::string name;
		std::string path;
		std::string relative;
		std::string extension;
		std::string category;
		int64_t size = 0;
	};

	bool isDescriptorExtension(const std::string& extension)
	{
		return extension == ".cue" || extension == ".gdi" || extension == ".ccd" || extension == ".mds";
	}

	bool isCompanionExtension(const std::string& extension)
	{
		return extension == ".bin" || extension == ".raw" || extension == ".img" || extension == ".sub" ||
			extension == ".mdf" || extension == ".wav" || extension == ".ogg" || extension == ".flac" || extension == ".mp3";
	}

	bool isDiscExtension(const std::string& extension)
	{
		return isDescriptorExtension(extension) || isCompanionExtension(extension) ||
			extension == ".chd" || extension == ".iso" || extension == ".cdi" || extension == ".gcm" ||
			extension == ".rvz" || extension == ".gcz";
	}

	bool isDiscLikeGame(const std::vector<RomFileInfo>& files)
	{
		for (const RomFileInfo& file : files)
			if (!isDiscExtension(file.extension))
				return false;

		return true;
	}

	bool supportsPlaylist(SystemData* local)
	{
		const std::string& name = local->getName();

		// Redream does not open .m3u playlists; keep the first disc for it.
		if (name == "dreamcast")
			return false;

		return local->getExtensions().find(".m3u") != local->getExtensions().end() ||
			name == "psx" || name == "ps2" || name == "saturn" || name == "segacd" || name == "megacd" ||
			name == "gamecube" || name == "wii";
	}

	std::string sanitizeFolderName(const std::string& name)
	{
		std::string result = name;

		for (char& c : result)
			if (c == '/' || c == '\\' || c == '\0')
				c = '_';

		if (result == "." || result == "..")
			result = "Game";

		while (!result.empty() && (result.front() == ' ' || result.front() == '.'))
			result.erase(result.begin());

		return result.empty() ? "Game" : result;
	}

	bool isRootFile(const RomFileInfo& file)
	{
		return file.relative.find('/') == std::string::npos;
	}

	std::vector<RomFileInfo> baseGameFiles(const rapidjson::Value& rom, const std::string& romPath, const std::string& romName)
	{
		std::vector<RomFileInfo> result;

		if (!rom.IsObject() || !rom.HasMember("files") || !rom["files"].IsArray())
			return result;

		std::string base = romPath;
		if (!base.empty() && !romName.empty())
			base += "/" + romName;

		for (const rapidjson::Value& file : rom["files"].GetArray())
		{
			if (!file.IsObject())
				continue;

			if (file.HasMember("missing_from_fs") && file["missing_from_fs"].IsBool() && file["missing_from_fs"].GetBool())
				continue;

			const std::string category = stringValue(file, "category");
			if (!category.empty() && category != "game")
				continue;

			std::string name = stringValue(file, "file_name");
			if (name.empty())
				continue;

			const std::string extension = Utils::String::toLower(Utils::FileSystem::getExtension(name));
			if (extension == ".m3u")
				continue;

			const std::string path = stringValue(file, "file_path");
			std::string relative = name;

			if (!base.empty())
			{
				if (path == base)
					relative = name;
				else if (path.rfind(base + "/", 0) == 0)
					relative = path.substr(base.size() + 1) + "/" + name;
			}

			RomFileInfo info;
			info.id = intValue(file, "id");
			info.name = name;
			info.path = path;
			info.relative = relative;
			info.extension = extension;
			info.category = category;
			info.size = int64Value(file, "file_size_bytes");

			if (info.id > 0)
				result.push_back(info);
		}

		std::sort(result.begin(), result.end(), [](const RomFileInfo& a, const RomFileInfo& b) { return a.name < b.name; });
		return result;
	}

	bool addGame(const std::string& server, SystemData* local, const fs::path& systemDir,
		const rapidjson::Value& rom, pugi::xml_node gameList, RomMSyncResult& result,
		const std::function<void(const std::string&)>& setText)
	{
		const std::string romPath = stringValue(rom, "fs_path");
		const std::string romName = stringValue(rom, "fs_name");
		const std::vector<RomFileInfo> base = baseGameFiles(rom, romPath, romName);

		if (base.empty())
		{
			LOG(LogWarning) << "RomM: no game file for " << romName;
			return false;
		}

		const bool archive = base.size() > 1;
		if (archive && !isDiscLikeGame(base))
		{
			LOG(LogWarning) << "RomM: unsupported multi-file game " << romName;
			return false;
		}

		bool descriptor = false;
		for (const RomFileInfo& file : base)
			if (isDescriptorExtension(file.extension))
			{
				descriptor = true;
				break;
			}

		const std::string archiveName = romName.empty() ? Utils::FileSystem::getStem(base[0].name) : romName;
		const std::string romStem = Utils::FileSystem::getStem(archiveName);
		const bool playlist = archive && supportsPlaylist(local) && (descriptor || base.size() > 1);

		std::string gameName;
		std::string folder;

		if (playlist)
		{
			folder = sanitizeFolderName(archiveName);
			gameName = romStem + ".m3u";
		}
		else if (archive)
		{
			const RomFileInfo* root = nullptr;

			for (const RomFileInfo& file : base)
				if (isRootFile(file) && isDescriptorExtension(file.extension))
				{
					root = &file;
					break;
				}

			if (root == nullptr)
				for (const RomFileInfo& file : base)
					if (isRootFile(file))
					{
						root = &file;
						break;
					}

			if (root == nullptr)
			{
				LOG(LogWarning) << "RomM: no root file for " << romName;
				return false;
			}

			gameName = root->name;

			const std::string extension = Utils::String::toLower(Utils::FileSystem::getExtension(gameName));
			if (local->getExtensions().find(extension) == local->getExtensions().end())
			{
				LOG(LogWarning) << "RomM: " << local->getName() << " does not handle " << gameName;
				return false;
			}

			LOG(LogWarning) << "RomM: " << local->getName() << " has no playlist support; using " << gameName;
		}
		else
		{
			gameName = base[0].name;

			const std::string extension = Utils::String::toLower(Utils::FileSystem::getExtension(gameName));
			if (local->getExtensions().find(extension) == local->getExtensions().end())
			{
				LOG(LogWarning) << "RomM: " << local->getName() << " does not handle " << gameName;
				return false;
			}
		}

		std::error_code ec;
		fs::create_directories(systemDir, ec);

		const fs::path gamePath = systemDir / gameName;
		if (!fs::exists(gamePath, ec) || fs::file_size(gamePath, ec) == 0)
		{
			std::ofstream stub(gamePath, std::ios::binary | std::ios::trunc);
			if (!stub)
			{
				LOG(LogWarning) << "RomM: cannot write the stub " << gamePath;
				return false;
			}
		}

		// The sidecar holds everything the on-demand download needs, including
		// the server, so a later settings change does not break old stubs.
		rapidjson::Document sidecar;
		sidecar.SetObject();
		rapidjson::Document::AllocatorType& allocator = sidecar.GetAllocator();
		sidecar.AddMember("server", rapidjson::Value(server.c_str(), allocator), allocator);
		sidecar.AddMember("rom_id", intValue(rom, "id"), allocator);

		if (archive)
		{
			std::string ids;
			for (const RomFileInfo& file : base)
			{
				if (!ids.empty())
					ids += ",";
				ids += std::to_string(file.id);
			}

			sidecar.AddMember("archive", true, allocator);
			sidecar.AddMember("file_name", rapidjson::Value(archiveName.c_str(), allocator), allocator);
			sidecar.AddMember("file_ids", rapidjson::Value(ids.c_str(), allocator), allocator);
			sidecar.AddMember("game_name", rapidjson::Value(gameName.c_str(), allocator), allocator);
			sidecar.AddMember("playlist", playlist, allocator);
			if (playlist)
				sidecar.AddMember("folder", rapidjson::Value(folder.c_str(), allocator), allocator);
		}
		else
		{
			sidecar.AddMember("file_id", base[0].id, allocator);
			sidecar.AddMember("file_name", rapidjson::Value(base[0].name.c_str(), allocator), allocator);
			sidecar.AddMember("size", base[0].size, allocator);
		}

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		sidecar.Accept(writer);
		if (!writeTextFile(RomMLibrary::sidecarPath(gamePath.string()), buffer.GetString()))
		{
			LOG(LogWarning) << "RomM: cannot write the sidecar of " << gamePath;
			return false;
		}

		const std::string stem = Utils::FileSystem::getStem(gameName);
		const std::string cover = coverPath(rom);
		std::string imageRel;
		std::string thumbRel;
		std::string marqueeRel;

		if (!cover.empty())
		{
			const std::string imageName = mediaFileName(stem, "image", cover);
			const std::string thumbName = mediaFileName(stem, "thumb", cover);

			if (downloadMediaFile(server, cover, (systemDir / "images" / imageName).string(), result, setText))
				imageRel = "./images/" + imageName;

			if (downloadMediaFile(server, cover, (systemDir / "images" / thumbName).string(), result, setText))
				thumbRel = "./images/" + thumbName;
		}

		std::string logo = providerMedia(rom, "ss_metadata", "logo_path", "logo_url");
		if (logo.empty())
			logo = providerMedia(rom, "ss_metadata", "marquee_path", "marquee_url");

		if (!logo.empty())
		{
			const std::string marqueeName = mediaFileName(stem, "marquee", logo);
			if (downloadMediaFile(server, logo, (systemDir / "images" / marqueeName).string(), result, setText))
				marqueeRel = "./images/" + marqueeName;
		}

		pugi::xml_node game = gameList.append_child("game");
		game.append_child("path").text().set(("./" + gameName).c_str());

		std::string name = stringValue(rom, "name");
		if (name.empty())
			name = stringValue(rom, "fs_name_no_ext");
		game.append_child("name").text().set(name.c_str());

		const std::string summary = stringValue(rom, "summary");
		if (!summary.empty())
			game.append_child("desc").text().set(summary.c_str());

		const rapidjson::Value* metadatum = objectValue(rom, "metadatum");
		const rapidjson::Value* ss = objectValue(rom, "ss_metadata");

		const std::string developers = metadatum == nullptr ? "" : joinedList(*metadatum, "developers");
		const std::string publishers = metadatum == nullptr ? "" : joinedList(*metadatum, "publishers");
		const std::string genres = metadatum == nullptr ? "" : joinedList(*metadatum, "genres");
		const std::string families = metadatum == nullptr ? "" : firstListValue(*metadatum, "franchises");
		std::string players = metadatum == nullptr ? "" : stringValue(*metadatum, "player_count");
		if (players == "null" || players == "None")
			players.clear();

		if (!developers.empty())
			game.append_child("developer").text().set(developers.c_str());
		else if (ss != nullptr && !joinedList(*ss, "developers").empty())
			game.append_child("developer").text().set(joinedList(*ss, "developers").c_str());

		if (!publishers.empty())
			game.append_child("publisher").text().set(publishers.c_str());
		else if (ss != nullptr && !joinedList(*ss, "publishers").empty())
			game.append_child("publisher").text().set(joinedList(*ss, "publishers").c_str());

		if (!genres.empty())
			game.append_child("genre").text().set(genres.c_str());
		else if (ss != nullptr && !joinedList(*ss, "genres").empty())
			game.append_child("genre").text().set(joinedList(*ss, "genres").c_str());

		if (!families.empty())
			game.append_child("family").text().set(families.c_str());

		if (!players.empty())
			game.append_child("players").text().set(players.c_str());

		double timestamp = metadatum == nullptr ? 0.0 : numberValue(*metadatum, "first_release_date");
		if (timestamp <= 0.0 && ss != nullptr)
			timestamp = numberValue(*ss, "first_release_date");

		if (timestamp > 0.0)
		{
			const Utils::Time::DateTime date(static_cast<time_t>(timestamp / 1000.0));
			game.append_child("releasedate").text().set(date.getIsoString().c_str());
		}

		const double rating = metadatum == nullptr ? 0.0 : numberValue(*metadatum, "average_rating");
		if (rating > 0.0)
		{
			std::ostringstream value;
			value << (rating / 100.0);
			game.append_child("rating").text().set(value.str().c_str());
		}

		const std::string regions = joinedList(rom, "regions");
		if (!regions.empty())
			game.append_child("region").text().set(regions.c_str());

		const std::string languages = joinedList(rom, "languages");
		if (!languages.empty())
			game.append_child("lang").text().set(languages.c_str());

		if (!imageRel.empty())
			game.append_child("image").text().set(imageRel.c_str());

		if (!thumbRel.empty())
			game.append_child("thumbnail").text().set(thumbRel.c_str());

		if (!marqueeRel.empty())
			game.append_child("marquee").text().set(marqueeRel.c_str());

		pugi::xml_node scrap = game.append_child("scrap");
		scrap.append_attribute("name") = "RomM";
		scrap.append_attribute("date") = Utils::Time::DateTime::now().getIsoString().c_str();

		return true;
	}
}

namespace RomMLibrary
{
	std::string libraryRoot()
	{
		return Paths::getDataPath() + "/romm/library";
	}

	std::string systemConfigPath()
	{
		return Paths::getConfigPath() + "/es_systems_romm.cfg";
	}

	std::string sidecarPath(const std::string& gamePath)
	{
		return gamePath + ".romm.json";
	}

	bool needsDownload(const std::string& gamePath)
	{
		std::error_code ec;
		const std::string sidecar = sidecarPath(gamePath);

		if (!fs::exists(sidecar, ec))
			return false;

		std::ifstream in(sidecar, std::ios::binary);
		if (!in)
			return true;

		std::stringstream content;
		content << in.rdbuf();

		rapidjson::Document document;
		document.Parse(content.str().c_str());
		if (document.HasParseError() || !document.IsObject())
			return true;

		if (document.HasMember("archive") && document["archive"].IsBool() && document["archive"].GetBool())
		{
			if (!fs::exists(gamePath, ec))
				return true;

			return fs::file_size(gamePath, ec) == 0;
		}

		if (document.HasMember("files") && document["files"].IsArray())
		{
			const fs::path parent = fs::path(gamePath).parent_path();

			for (const rapidjson::Value& file : document["files"].GetArray())
			{
				const std::string name = stringValue(file, "file_name");
				if (name.empty())
					continue;

				const fs::path path = parent / name;
				if (!fs::exists(path, ec) || fs::file_size(path, ec) == 0)
					return true;
			}

			return false;
		}

		if (!fs::exists(gamePath, ec))
			return true;

		return fs::file_size(gamePath, ec) == 0;
	}

	bool downloadGame(const std::string& gamePath, const std::function<void(const std::string&)>& setText, std::string& error)
	{
		const std::string sidecar = sidecarPath(gamePath);

		std::ifstream in(sidecar, std::ios::binary);
		if (!in)
		{
			error = "No RomM sidecar for " + gamePath;
			return false;
		}

		std::stringstream content;
		content << in.rdbuf();

		rapidjson::Document document;
		document.Parse(content.str().c_str());
		if (document.HasParseError() || !document.IsObject())
		{
			error = "Invalid RomM sidecar for " + gamePath;
			return false;
		}

		std::string server = stringValue(document, "server");
		if (server.empty())
			server = RomMScraperUrl::normalizeServer(Settings::getInstance()->getString("RomMServer"));

		if (server.empty())
		{
			error = "No RomM server URL configured";
			return false;
		}

		const int romId = intValue(document, "rom_id");
		const fs::path gameFile(gamePath);
		const fs::path parent = gameFile.parent_path();
		std::error_code ec;

		const bool archive = document.HasMember("archive") && document["archive"].IsBool() && document["archive"].GetBool();
		if (archive)
		{
			const std::string fileName = stringValue(document, "file_name");
			const std::string fileIds = stringValue(document, "file_ids");
			const std::string gameName = stringValue(document, "game_name");
			const std::string folder = stringValue(document, "folder");
			const bool playlist = document.HasMember("playlist") && document["playlist"].IsBool() && document["playlist"].GetBool();

			if (romId <= 0 || fileName.empty() || fileIds.empty() || gameName.empty())
			{
				error = "Incomplete RomM archive sidecar for " + gamePath;
				return false;
			}

			const fs::path zipPath = parent / (Utils::FileSystem::getStem(gamePath) + ".romm.zip");
			const std::string url = server + "/api/roms/" + std::to_string(romId) + "/content/" +
				HttpReq::urlEncode(fileName) + "?file_ids=" + fileIds;

			auto truncateGame = [&]
			{
				std::ofstream stub(gamePath, std::ios::binary | std::ios::trunc);
			};

			if (setText)
				setText("Downloading " + gameName + "...");

			std::string downloadError;
			if (!RomMClient::download(url, zipPath.string(), [&](int percent)
			{
				if (setText && percent >= 0)
					setText("Downloading " + gameName + " ... " + std::to_string(percent) + "%");
			}, downloadError))
			{
				error = downloadError;
				fs::remove(zipPath, ec);
				truncateGame();
				return false;
			}

			Utils::Zip::ZipFile zip;
			if (!zip.load(zipPath.string()))
			{
				error = "Invalid RomM archive for " + gamePath;
				fs::remove(zipPath, ec);
				truncateGame();
				return false;
			}

			const fs::path root = playlist && !folder.empty() ? parent / folder : parent;
			fs::create_directories(root, ec);

			std::string m3uMember;
			if (playlist)
			{
				const std::string preferred = fileName + ".m3u";

				for (const std::string& member : zip.namelist())
				{
					if (member == preferred)
					{
						m3uMember = member;
						break;
					}

					if (m3uMember.empty() && member.size() > 4 && member.compare(member.size() - 4, 4, ".m3u") == 0)
						m3uMember = member;
				}

				if (m3uMember.empty())
				{
					error = "No playlist in the RomM archive for " + gamePath;
					fs::remove(zipPath, ec);
					truncateGame();
					return false;
				}
			}

			if (setText)
				setText("Extracting " + gameName + "...");

			bool extracted = true;
			for (const std::string& member : zip.namelist())
			{
				if (member.empty() || member.back() == '/')
					continue;

				if (member.find("..") != std::string::npos || fs::path(member).is_absolute())
					continue;

				const fs::path destination = root / member;
				fs::create_directories(destination.parent_path(), ec);

				if (!zip.extract(member, root.string(), false))
				{
					extracted = false;
					break;
				}
			}

			if (!extracted)
			{
				error = "Cannot extract the RomM archive for " + gamePath;
				fs::remove(zipPath, ec);
				truncateGame();
				return false;
			}

			if (playlist)
			{
				std::string playlistContent;
				auto append = [](void* opaque, uint64_t, const void* data, size_t size) -> size_t
				{
					static_cast<std::string*>(opaque)->append(static_cast<const char*>(data), size);
					return size;
				};

				if (!zip.readBuffered(m3uMember, append, &playlistContent))
				{
					error = "Cannot read the RomM playlist for " + gamePath;
					fs::remove(zipPath, ec);
					truncateGame();
					return false;
				}

				const std::string prefix = folder.empty() ? "" : folder + "/";
				std::ostringstream written;
				std::istringstream lines(playlistContent);
				std::string line;

				while (std::getline(lines, line))
				{
					if (!line.empty() && line.back() == '\r')
						line.pop_back();

					const size_t first = line.find_first_not_of(" \t");
					if (first == std::string::npos)
						continue;

					const size_t last = line.find_last_not_of(" \t");
					line = line.substr(first, last - first + 1);

					if (line.empty() || line[0] == '#')
						continue;

					std::replace(line.begin(), line.end(), '\\', '/');
					if (line[0] == '/')
						written << line << "\n";
					else
						written << prefix << line << "\n";
				}

				if (written.str().empty() || !writeTextFile(gamePath, written.str()))
				{
					error = "Cannot write the RomM playlist for " + gamePath;
					fs::remove(zipPath, ec);
					truncateGame();
					return false;
				}
			}

			fs::remove(zipPath, ec);

			if (!fs::exists(gamePath, ec) || fs::file_size(gamePath, ec) == 0)
			{
				error = "The downloaded game is empty";
				truncateGame();
				return false;
			}

			return true;
		}

		if (document.HasMember("files") && document["files"].IsArray())
		{
			for (const rapidjson::Value& file : document["files"].GetArray())
			{
				const int fileId = intValue(file, "id");
				const std::string fileName = stringValue(file, "file_name");
				if (fileId <= 0 || fileName.empty())
					continue;

				const fs::path destination = parent / fileName;
				if (fs::exists(destination, ec) && fs::file_size(destination, ec) > 0)
					continue;

				const std::string url = server + "/api/roms/" + std::to_string(fileId) +
					"/files/content/" + HttpReq::urlEncode(fileName);

				if (setText)
					setText("Downloading " + fileName + "...");

				if (!RomMClient::download(url, destination.string(), [&](int percent)
				{
					if (setText && percent >= 0)
						setText("Downloading " + fileName + " ... " + std::to_string(percent) + "%");
				}, error))
					return false;
			}

			if (!fs::exists(gamePath, ec) || fs::file_size(gamePath, ec) == 0)
			{
				error = "The downloaded game is empty";
				return false;
			}

			return true;
		}

		const int fileId = intValue(document, "file_id");
		const std::string fileName = stringValue(document, "file_name");

		if (fileId <= 0 || fileName.empty())
		{
			error = "Incomplete RomM sidecar for " + gamePath;
			return false;
		}

		const std::string url = server + "/api/roms/" + std::to_string(fileId) +
			"/files/content/" + HttpReq::urlEncode(fileName);

		if (setText)
			setText("Downloading " + fileName + "...");

		const bool downloaded = RomMClient::download(url, gamePath, [&](int percent)
		{
			if (setText && percent >= 0)
				setText("Downloading " + fileName + " ... " + std::to_string(percent) + "%");
		}, error);

		if (!downloaded)
			return false;

		if (!fs::exists(gamePath, ec) || fs::file_size(gamePath, ec) == 0)
		{
			error = "The downloaded game is empty";
			return false;
		}

		return true;
	}

	RomMSyncResult sync(const std::function<void(const std::string&)>& setText)
	{
		RomMSyncResult result;

		const std::string server = RomMScraperUrl::normalizeServer(Settings::getInstance()->getString("RomMServer"));
		if (server.empty())
		{
			result.error = "No RomM server URL configured";
			return result;
		}

		if (setText)
			setText("Reading RomM platforms...");

		rapidjson::Document platforms;
		std::string error;
		if (!RomMClient::getJson(server, "/api/platforms", platforms, error) || !platforms.IsArray())
		{
			result.error = error.empty() ? "Invalid RomM platforms response" : error;
			return result;
		}

		std::error_code ec;
		const fs::path library = libraryRoot();
		fs::create_directories(library, ec);

		pugi::xml_document systemsDocument;
		pugi::xml_node systemsDeclaration = systemsDocument.append_child(pugi::node_declaration);
		systemsDeclaration.append_attribute("version") = "1.0";

		pugi::xml_node systems = systemsDocument.append_child("systemList");

		for (const rapidjson::Value& platform : platforms.GetArray())
		{
			if (!platform.IsObject())
				continue;

			const int platformId = intValue(platform, "id");
			if (platformId <= 0)
				continue;

			const std::string slug = stringValue(platform, "slug");
			const std::string fsSlug = stringValue(platform, "fs_slug");

			SystemData* local = findLocalSystem(slug, fsSlug);
			if (local == nullptr)
				continue;

			const std::string rommName = "romm-" + local->getName();
			const fs::path systemDir = library / rommName;
			fs::create_directories(systemDir, ec);

			pugi::xml_document gamelist;
			pugi::xml_node declaration = gamelist.append_child(pugi::node_declaration);
			declaration.append_attribute("version") = "1.0";

			pugi::xml_node gameList = gamelist.append_child("gameList");
			int games = 0;
			int offset = 0;

			while (true)
			{
				const std::string path = "/api/roms?platform_ids=" + std::to_string(platformId) +
					"&limit=100&offset=" + std::to_string(offset) +
					"&with_files=true&with_provider_metadata=true&with_char_index=false" +
					"&with_filter_values=false&with_rom_id_index=false";

				rapidjson::Document roms;
				if (!RomMClient::getJson(server, path, roms, error))
				{
					LOG(LogWarning) << "RomM: cannot read the " << slug << " roms: " << error;
					break;
				}

				if (!roms.IsObject() || !roms.HasMember("items") || !roms["items"].IsArray())
					break;

				const rapidjson::Value& items = roms["items"];
				for (const rapidjson::Value& rom : items.GetArray())
				{
					if (!rom.IsObject())
						continue;

					if (setText)
						setText("Reading " + stringValue(rom, "name") + "...");

					if (addGame(server, local, systemDir, rom, gameList, result, setText))
						games++;
				}

				if (items.Size() < 100)
					break;

				offset += static_cast<int>(items.Size());
				if (offset > 100000)
					break;
			}

			if (games == 0)
				continue;

			if (!saveXml(systemDir / "gamelist.xml", gamelist))
			{
				result.error = "Cannot write the RomM gamelist for " + rommName;
				return result;
			}

			const std::string nodeXml = systemNodeXml(local->getName());
			if (nodeXml.empty())
			{
				LOG(LogWarning) << "RomM: no es_systems.cfg entry for " << local->getName();
				continue;
			}

			pugi::xml_document nodeDocument;
			nodeDocument.load_string(nodeXml.c_str());
			pugi::xml_node system = nodeDocument.child("system");
			if (!system)
				continue;

			system.child("name").text().set(rommName.c_str());
			system.child("fullname").text().set(("RomM - " + local->getFullName()).c_str());
			system.child("path").text().set(systemDir.string().c_str());

			// The generated system also lists the playlists made for multi-disc
			// RomM games.
			std::string extensions = system.child("extension").text().get();
			if (extensions.find(".m3u") == std::string::npos)
			{
				if (!extensions.empty() && extensions.back() != ' ')
					extensions += " ";
				extensions += ".m3u";
				system.child("extension").text().set(extensions.c_str());
			}

			pugi::xml_node theme = system.child("theme");
			if (!theme)
				theme = system.append_child("theme");
			theme.text().set(local->getThemeFolder().c_str());

			pugi::xml_node baseSystem = system.child("rommBaseSystem");
			if (!baseSystem)
				baseSystem = system.append_child("rommBaseSystem");
			baseSystem.text().set(local->getName().c_str());

			pugi::xml_node group = system.child("group");
			if (group)
				system.remove_child(group);

			systems.append_copy(system);
			result.platforms++;
			result.games += games;
		}

		if (result.platforms == 0)
		{
			fs::remove(systemConfigPath(), ec);
			result.success = true;
			return result;
		}

		if (!saveXml(systemConfigPath(), systemsDocument))
		{
			result.error = "Cannot write " + systemConfigPath();
			return result;
		}

		result.success = true;
		return result;
	}

	int clearDownloads()
	{
		int cleared = 0;
		std::error_code ec;

		if (!fs::exists(libraryRoot(), ec))
			return 0;

		for (const fs::directory_entry& entry : fs::recursive_directory_iterator(libraryRoot(), ec))
		{
			if (!entry.is_regular_file())
				continue;

			const fs::path path = entry.path();
			if (path.filename().string().ends_with(".romm.json"))
				continue;

			if (!fs::exists(sidecarPath(path.string()), ec))
				continue;

			if (fs::file_size(path, ec) == 0)
				continue;

			std::ifstream sidecarIn(sidecarPath(path.string()), std::ios::binary);
			if (sidecarIn)
			{
				std::stringstream sidecarContent;
				sidecarContent << sidecarIn.rdbuf();

				rapidjson::Document sidecar;
				sidecar.Parse(sidecarContent.str().c_str());
				if (!sidecar.HasParseError() && sidecar.IsObject() && sidecar.HasMember("folder") && sidecar["folder"].IsString())
				{
					const fs::path folder = path.parent_path() / sidecar["folder"].GetString();
					fs::remove_all(folder, ec);
				}
			}

			std::ofstream stub(path, std::ios::binary | std::ios::trunc);
			if (stub)
				cleared++;
		}

		return cleared;
	}

	bool removeLibrary()
	{
		std::error_code ec;
		fs::remove_all(libraryRoot(), ec);
		fs::remove(systemConfigPath(), ec);
		return true;
	}

	bool isGeneratedSystem(const std::string& systemName)
	{
		return Utils::String::startsWith(systemName, "romm-");
	}
}
