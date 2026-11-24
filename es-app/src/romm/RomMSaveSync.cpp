#include "romm/RomMSaveSync.h"

#include "romm/RomMClient.h"
#include "romm/RomMLibrary.h"

#include "HttpReq.h"
#include "Log.h"
#include "Paths.h"
#include "Settings.h"
#include "scrapers/RomMScraperUrl.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/TimeUtil.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <sys/stat.h>

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
	struct GameEntry
	{
		int romId = 0;
		std::string baseSystem;
		std::string gamePath;
		std::string gameStem;
	};

	struct ServerSave
	{
		int id = 0;
		std::string fileName;
		std::string extension;
		std::string emulator;
		std::string slot;
		std::string hash;
		int64_t updated = 0;
	};

	struct ServerState
	{
		int id = 0;
		std::string fileName;
		std::string extension;
		std::string emulator;
		std::string core;
		std::string hash;
		int64_t updated = 0;
	};

	struct LocalState
	{
		std::string path;
		std::string suffix;
	};

	struct Baseline
	{
		int serverId = 0;
		int64_t serverUpdated = 0;
		std::string serverHash;
		int64_t localMtime = 0;
		std::string localHash;
	};

	enum class Action
	{
		None,
		Upload,
		Download,
		Conflict,
	};

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

		return 0;
	}

	int64_t parseTime(const std::string& iso)
	{
		if (iso.size() < 19)
			return 0;

		return static_cast<int64_t>(Utils::Time::stringToTime(iso.substr(0, 19), "%Y-%m-%dT%H:%M:%S"));
	}

	int64_t fileMtime(const std::string& path)
	{
		struct stat st;
		if (stat(path.c_str(), &st) != 0)
			return 0;

		return static_cast<int64_t>(st.st_mtime);
	}

	std::string fileMd5(const std::string& path)
	{
		return Utils::String::toLower(Utils::FileSystem::getFileMd5(path));
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

	std::string baselinePath()
	{
		return RomMLibrary::libraryRoot() + "/sync.json";
	}

	std::map<std::string, Baseline> loadBaseline()
	{
		std::map<std::string, Baseline> result;

		std::ifstream in(baselinePath(), std::ios::binary);
		if (!in)
			return result;

		std::stringstream content;
		content << in.rdbuf();

		rapidjson::Document document;
		document.Parse(content.str().c_str());
		if (document.HasParseError() || !document.IsObject())
			return result;

		for (auto it = document.MemberBegin(); it != document.MemberEnd(); ++it)
		{
			if (!it->name.IsString() || !it->value.IsObject())
				continue;

			Baseline baseline;
			baseline.serverId = intValue(it->value, "server_id");
			baseline.serverUpdated = int64Value(it->value, "server_updated");
			baseline.serverHash = stringValue(it->value, "server_hash");
			baseline.localMtime = int64Value(it->value, "local_mtime");
			baseline.localHash = stringValue(it->value, "local_hash");
			result[it->name.GetString()] = baseline;
		}

		return result;
	}

	void saveBaseline(const std::map<std::string, Baseline>& baseline)
	{
		rapidjson::Document document;
		document.SetObject();
		rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

		for (const auto& [key, value] : baseline)
		{
			rapidjson::Value entry(rapidjson::kObjectType);
			entry.AddMember("server_id", value.serverId, allocator);
			entry.AddMember("server_updated", value.serverUpdated, allocator);
			entry.AddMember("server_hash", rapidjson::Value(value.serverHash.c_str(), allocator), allocator);
			entry.AddMember("local_mtime", value.localMtime, allocator);
			entry.AddMember("local_hash", rapidjson::Value(value.localHash.c_str(), allocator), allocator);
			document.AddMember(rapidjson::Value(key.c_str(), allocator), entry, allocator);
		}

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		document.Accept(writer);
		writeTextFile(baselinePath(), buffer.GetString());
	}

	std::string conflictsPath()
	{
		return RomMLibrary::libraryRoot() + "/conflicts.json";
	}

	std::vector<RomMConflict> loadConflictList()
	{
		std::vector<RomMConflict> result;

		std::ifstream in(conflictsPath(), std::ios::binary);
		if (!in)
			return result;

		std::stringstream content;
		content << in.rdbuf();

		rapidjson::Document document;
		document.Parse(content.str().c_str());
		if (document.HasParseError() || !document.IsArray())
			return result;

		for (const rapidjson::Value& value : document.GetArray())
		{
			if (!value.IsObject())
				continue;

			RomMConflict conflict;
			conflict.id = stringValue(value, "id");
			conflict.romId = intValue(value, "rom_id");
			conflict.game = stringValue(value, "game");
			conflict.baseSystem = stringValue(value, "base_system");
			conflict.kind = stringValue(value, "kind");
			conflict.suffix = stringValue(value, "suffix");
			conflict.localPath = stringValue(value, "local_path");
			conflict.localHash = stringValue(value, "local_hash");
			conflict.localMtime = int64Value(value, "local_mtime");
			conflict.serverId = intValue(value, "server_id");
			conflict.serverUpdated = int64Value(value, "server_updated");
			conflict.serverHash = stringValue(value, "server_hash");
			conflict.serverExtension = stringValue(value, "server_extension");
			conflict.serverFileName = stringValue(value, "server_file_name");
			conflict.serverEmulator = stringValue(value, "server_emulator");

			if (!conflict.id.empty() && conflict.romId > 0)
				result.push_back(conflict);
		}

		return result;
	}

	void saveConflicts(const std::vector<RomMConflict>& conflicts)
	{
		rapidjson::Document document;
		document.SetArray();
		rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

		for (const RomMConflict& conflict : conflicts)
		{
			rapidjson::Value entry(rapidjson::kObjectType);
			entry.AddMember("id", rapidjson::Value(conflict.id.c_str(), allocator), allocator);
			entry.AddMember("rom_id", conflict.romId, allocator);
			entry.AddMember("game", rapidjson::Value(conflict.game.c_str(), allocator), allocator);
			entry.AddMember("base_system", rapidjson::Value(conflict.baseSystem.c_str(), allocator), allocator);
			entry.AddMember("kind", rapidjson::Value(conflict.kind.c_str(), allocator), allocator);
			entry.AddMember("suffix", rapidjson::Value(conflict.suffix.c_str(), allocator), allocator);
			entry.AddMember("local_path", rapidjson::Value(conflict.localPath.c_str(), allocator), allocator);
			entry.AddMember("local_hash", rapidjson::Value(conflict.localHash.c_str(), allocator), allocator);
			entry.AddMember("local_mtime", conflict.localMtime, allocator);
			entry.AddMember("server_id", conflict.serverId, allocator);
			entry.AddMember("server_updated", conflict.serverUpdated, allocator);
			entry.AddMember("server_hash", rapidjson::Value(conflict.serverHash.c_str(), allocator), allocator);
			entry.AddMember("server_extension", rapidjson::Value(conflict.serverExtension.c_str(), allocator), allocator);
			entry.AddMember("server_file_name", rapidjson::Value(conflict.serverFileName.c_str(), allocator), allocator);
			entry.AddMember("server_emulator", rapidjson::Value(conflict.serverEmulator.c_str(), allocator), allocator);
			document.PushBack(entry, allocator);
		}

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		document.Accept(writer);
		writeTextFile(conflictsPath(), buffer.GetString());
	}

	std::map<std::string, RomMConflict> conflictMap()
	{
		std::map<std::string, RomMConflict> result;

		for (const RomMConflict& conflict : loadConflictList())
			result[conflict.id] = conflict;

		return result;
	}

	bool readSidecarRomId(const fs::path& path, int& romId)
	{
		std::ifstream in(path, std::ios::binary);
		if (!in)
			return false;

		std::stringstream content;
		content << in.rdbuf();

		rapidjson::Document document;
		document.Parse(content.str().c_str());
		if (document.HasParseError() || !document.IsObject())
			return false;

		romId = intValue(document, "rom_id");
		return romId > 0;
	}

	std::vector<GameEntry> libraryGames()
	{
		std::vector<GameEntry> result;
		std::error_code ec;

		const fs::path root = RomMLibrary::libraryRoot();
		if (!fs::exists(root, ec))
			return result;

		for (const fs::directory_entry& directory : fs::directory_iterator(root, ec))
		{
			if (!directory.is_directory())
				continue;

			const std::string directoryName = directory.path().filename().string();
			if (!Utils::String::startsWith(directoryName, "romm-"))
				continue;

			const std::string baseSystem = directoryName.substr(5);
			if (baseSystem.empty())
				continue;

			for (const fs::directory_entry& file : fs::directory_iterator(directory.path(), ec))
			{
				if (!file.is_regular_file())
					continue;

				const std::string fileName = file.path().filename().string();
				const std::string suffix = ".romm.json";
				if (fileName.size() <= suffix.size() || fileName.compare(fileName.size() - suffix.size(), suffix.size(), suffix) != 0)
					continue;

				const std::string gameName = fileName.substr(0, fileName.size() - suffix.size());
				const fs::path gamePath = directory.path() / gameName;
				if (!fs::exists(gamePath, ec))
					continue;

				int romId = 0;
				if (!readSidecarRomId(file.path(), romId))
					continue;

				GameEntry game;
				game.romId = romId;
				game.baseSystem = baseSystem;
				game.gamePath = gamePath.string();
				game.gameStem = Utils::FileSystem::getStem(game.gamePath);
				result.push_back(game);
			}
		}

		return result;
	}

	std::vector<std::string> m3uStems(const std::string& gamePath)
	{
		std::vector<std::string> result;

		if (Utils::String::toLower(Utils::FileSystem::getExtension(gamePath)) != ".m3u")
			return result;

		std::ifstream in(gamePath, std::ios::binary);
		if (!in)
			return result;

		std::string line;
		while (std::getline(in, line))
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

			result.push_back(Utils::FileSystem::getStem(line));
		}

		return result;
	}

	std::vector<std::string> gameStems(const GameEntry& game)
	{
		std::vector<std::string> stems = { game.gameStem };

		for (const std::string& stem : m3uStems(game.gamePath))
			if (std::find(stems.begin(), stems.end(), stem) == stems.end())
				stems.push_back(stem);

		return stems;
	}

	std::string localSavePath(const GameEntry& game)
	{
		const fs::path root = fs::path(Paths::getSavesPath()) / game.baseSystem;
		static const char* extensions[] = {
			".srm", ".sav", ".ps2", ".mcd", ".bcr", ".mem", ".smp", ".rtc", ".eep", ".fla", ".sra"
		};

		for (const std::string& stem : gameStems(game))
			for (const char* extension : extensions)
			{
				const fs::path path = root / (stem + extension);
				std::error_code ec;
				if (fs::exists(path, ec))
					return path.string();
			}

		return "";
	}

	std::vector<LocalState> localStates(const GameEntry& game)
	{
		std::vector<LocalState> result;
		const fs::path root = fs::path(Paths::getSavesPath()) / game.baseSystem;
		std::error_code ec;

		if (!fs::exists(root, ec))
			return result;

		const std::vector<std::string> stems = gameStems(game);

		for (const fs::directory_entry& entry : fs::directory_iterator(root, ec))
		{
			if (!entry.is_regular_file())
				continue;

			const std::string name = entry.path().filename().string();

			for (const std::string& stem : stems)
			{
				if (!Utils::String::startsWith(name, stem))
					continue;

				const std::string suffix = name.substr(stem.size());
				const bool autoState = suffix == ".state";
				const bool numberedState = suffix.size() == 7 && suffix.compare(0, 6, ".state") == 0 &&
					suffix[6] >= '1' && suffix[6] <= '9';

				if (!autoState && !numberedState)
					continue;

				result.push_back({ entry.path().string(), suffix });
				break;
			}
		}

		return result;
	}

	bool fetchServerSaves(const std::string& server, int romId, std::vector<ServerSave>& saves, std::string& error)
	{
		rapidjson::Document document;
		if (!RomMClient::getJson(server, "/api/saves?rom_id=" + std::to_string(romId), document, error))
			return false;

		if (!document.IsArray())
		{
			error = "Invalid saves response";
			return false;
		}

		for (const rapidjson::Value& value : document.GetArray())
		{
			if (!value.IsObject())
				continue;

			ServerSave save;
			save.id = intValue(value, "id");
			save.fileName = stringValue(value, "file_name");
			save.extension = Utils::String::toLower(stringValue(value, "file_extension"));
			save.emulator = stringValue(value, "emulator");
			save.slot = stringValue(value, "slot");
			save.hash = Utils::String::toLower(stringValue(value, "content_hash"));
			save.updated = parseTime(stringValue(value, "updated_at"));

			if (save.id > 0 && !save.slot.empty())
				saves.push_back(save);
		}

		return true;
	}

	bool fetchServerStates(const std::string& server, int romId, std::vector<ServerState>& states, std::string& error)
	{
		rapidjson::Document document;
		if (!RomMClient::getJson(server, "/api/states?rom_id=" + std::to_string(romId), document, error))
			return false;

		if (!document.IsArray())
		{
			error = "Invalid states response";
			return false;
		}

		for (const rapidjson::Value& value : document.GetArray())
		{
			if (!value.IsObject())
				continue;

			ServerState state;
			state.id = intValue(value, "id");
			state.fileName = stringValue(value, "file_name");
			state.extension = Utils::String::toLower(stringValue(value, "file_extension"));
			state.emulator = stringValue(value, "emulator");
			state.core = stringValue(value, "core");
			state.hash = Utils::String::toLower(stringValue(value, "content_hash"));
			state.updated = parseTime(stringValue(value, "updated_at"));

			if (state.id > 0)
				states.push_back(state);
		}

		return true;
	}

	const ServerSave* pickSave(const std::vector<ServerSave>& saves)
	{
		const ServerSave* autosave = nullptr;
		const ServerSave* defaultSave = nullptr;
		const ServerSave* newest = nullptr;

		for (const ServerSave& save : saves)
		{
			if (save.slot == "autosave" && (autosave == nullptr || save.updated > autosave->updated))
				autosave = &save;

			if (save.slot == "default" && (defaultSave == nullptr || save.updated > defaultSave->updated))
				defaultSave = &save;

			if (newest == nullptr || save.updated > newest->updated)
				newest = &save;
		}

		return autosave != nullptr ? autosave : (defaultSave != nullptr ? defaultSave : newest);
	}

	bool isStateExtension(const std::string& extension)
	{
		return extension == "state" ||
			(extension.size() == 6 && extension.compare(0, 5, "state") == 0 && extension[5] >= '1' && extension[5] <= '9');
	}

	std::map<std::string, const ServerState*> pickStates(const std::vector<ServerState>& states)
	{
		std::map<std::string, const ServerState*> result;
		std::map<std::string, int64_t> updated;

		for (const ServerState& state : states)
		{
			if (!isStateExtension(state.extension))
				continue;

			auto it = updated.find(state.extension);
			if (it == updated.end() || state.updated > it->second)
			{
				updated[state.extension] = state.updated;
				result[state.extension] = &state;
			}
		}

		return result;
	}

	Action decide(bool localExists, bool serverExists, const std::string& localHash, int64_t localMtime,
		const std::string& serverHash, int64_t serverUpdated, const Baseline* baseline)
	{
		if (!localExists && !serverExists)
			return Action::None;
		if (localExists && !serverExists)
			return Action::Upload;
		if (!localExists && serverExists)
			return Action::Download;

		if (!localHash.empty() && !serverHash.empty() && localHash == serverHash)
			return Action::None;

		if (baseline != nullptr)
		{
			const bool localChanged = !localHash.empty() && !baseline->localHash.empty()
				? localHash != baseline->localHash
				: localMtime != baseline->localMtime;
			const bool serverChanged = !serverHash.empty() && !baseline->serverHash.empty()
				? serverHash != baseline->serverHash
				: serverUpdated != baseline->serverUpdated;

			if (localChanged && !serverChanged)
				return Action::Upload;
			if (serverChanged && !localChanged)
				return Action::Download;
			if (localChanged && serverChanged)
				return Action::Conflict;

			return Action::None;
		}

		if (localMtime > serverUpdated + 2)
			return Action::Upload;
		if (serverUpdated > localMtime + 2)
			return Action::Download;

		return Action::Conflict;
	}

	std::string safeFileName(std::string name)
	{
		std::replace(name.begin(), name.end(), '"', '_');
		std::replace(name.begin(), name.end(), '\r', '_');
		std::replace(name.begin(), name.end(), '\n', '_');
		return name;
	}

	bool uploadFile(const std::string& url, const std::string& field, const std::string& filePath,
		rapidjson::Document& response, std::string& error, const std::string& fileNameOverride = "")
	{
		const std::vector<char> data = Utils::FileSystem::readAllBytes(filePath);
		if (data.empty())
		{
			error = "The file is empty: " + filePath;
			return false;
		}

		if (data.size() > 64 * 1024 * 1024)
		{
			error = "The file is too large: " + filePath;
			return false;
		}

		const std::string boundary = "----EmulationStationRE" + std::to_string(Utils::Time::now());
		const std::string fileName = safeFileName(fileNameOverride.empty()
			? Utils::FileSystem::getFileName(filePath) : fileNameOverride);

		std::string body;
		body.reserve(data.size() + 512);
		body += "--" + boundary + "\r\n";
		body += "Content-Disposition: form-data; name=\"" + field + "\"; filename=\"" + fileName + "\"\r\n";
		body += "Content-Type: application/octet-stream\r\n\r\n";
		body.append(data.data(), data.size());
		body += "\r\n--" + boundary + "--\r\n";

		HttpReqOptions options;
		options.dataToPost = body;
		options.userAgent = "EmulationStation-RE";
		options.useCookieManager = false;
		options.customHeaders.push_back("Content-Type: multipart/form-data; boundary=" + boundary);

		const std::string auth = RomMClient::authHeader();
		if (!auth.empty())
			options.customHeaders.push_back("Authorization: " + auth);

		HttpReq request(url, &options);
		if (!request.wait())
		{
			error = request.getErrorMsg();
			if (error.empty())
				error = "HTTP " + std::to_string(static_cast<int>(request.status()));
			return false;
		}

		response.Parse(request.getContent().c_str());
		if (response.HasParseError())
		{
			error = "Invalid upload response";
			return false;
		}

		return true;
	}

	bool uploadSave(const std::string& server, const GameEntry& game, const std::string& filePath,
		const std::string& localHash, ServerSave& uploaded, std::string& error)
	{
		std::string url = server + "/api/saves?rom_id=" + std::to_string(game.romId) +
			"&emulator=" + HttpReq::urlEncode(game.baseSystem) +
			"&slot=autosave&overwrite=true&content_hash=" + HttpReq::urlEncode(localHash);

		rapidjson::Document response;
		if (!uploadFile(url, "saveFile", filePath, response, error))
			return false;

		uploaded.id = intValue(response, "id");
		uploaded.hash = Utils::String::toLower(stringValue(response, "content_hash"));
		uploaded.updated = parseTime(stringValue(response, "updated_at"));
		if (uploaded.hash.empty())
			uploaded.hash = localHash;

		if (uploaded.id <= 0)
		{
			error = "The server did not return a save ID";
			return false;
		}

		return true;
	}

	bool uploadState(const std::string& server, const GameEntry& game, const std::string& filePath,
		const std::string& localHash, ServerState& uploaded, std::string& error,
		const std::string& fileNameOverride = "", const std::string& emulatorOverride = "")
	{
		const std::string emulator = emulatorOverride.empty() ? game.baseSystem : emulatorOverride;
		std::string url = server + "/api/states?rom_id=" + std::to_string(game.romId) +
			"&emulator=" + HttpReq::urlEncode(emulator);

		rapidjson::Document response;
		if (!uploadFile(url, "stateFile", filePath, response, error, fileNameOverride))
			return false;

		uploaded.id = intValue(response, "id");
		uploaded.hash = Utils::String::toLower(stringValue(response, "content_hash"));
		uploaded.updated = parseTime(stringValue(response, "updated_at"));
		uploaded.extension = Utils::String::toLower(stringValue(response, "file_extension"));
		if (uploaded.hash.empty())
			uploaded.hash = localHash;

		if (uploaded.id <= 0)
		{
			error = "The server did not return a state ID";
			return false;
		}

		return true;
	}

	bool downloadFile(const std::string& server, const std::string& urlPath, const fs::path& destination, std::string& error)
	{
		std::error_code ec;
		fs::create_directories(destination.parent_path(), ec);

		const fs::path temporary = destination.string() + ".romm.new";
		if (!RomMClient::download(server + urlPath, temporary.string(), nullptr, error))
		{
			fs::remove(temporary, ec);
			return false;
		}

		fs::rename(temporary, destination, ec);
		if (ec)
		{
			fs::copy_file(temporary, destination, fs::copy_options::overwrite_existing, ec);
			fs::remove(temporary, ec);
			if (ec)
			{
				error = "Cannot write " + destination.string();
				return false;
			}
		}

		return true;
	}

	void remember(Baseline& baseline, int serverId, int64_t serverUpdated, const std::string& serverHash,
		const std::string& localPath, const std::string& localHash)
	{
		baseline.serverId = serverId;
		baseline.serverUpdated = serverUpdated;
		baseline.serverHash = serverHash;
		baseline.localMtime = fileMtime(localPath);
		baseline.localHash = localHash;
	}
}

namespace RomMSaveSync
{
	RomMSaveSyncResult sync(const std::function<void(const std::string&)>& setText)
	{
		RomMSaveSyncResult result;

		const std::string server = RomMScraperUrl::normalizeServer(Settings::getInstance()->getString("RomMServer"));
		if (server.empty())
		{
			result.error = "No RomM server URL configured";
			return result;
		}

		const std::vector<GameEntry> games = libraryGames();
		if (games.empty())
		{
			result.error = "No RomM library game found; sync the library first";
			return result;
		}

		std::map<std::string, Baseline> baseline = loadBaseline();
		std::map<std::string, RomMConflict> pending = conflictMap();
		const fs::path savesRoot = Paths::getSavesPath();

		auto persist = [&]()
		{
			saveBaseline(baseline);

			std::vector<RomMConflict> conflicts;
			conflicts.reserve(pending.size());
			for (const auto& [id, conflict] : pending)
				conflicts.push_back(conflict);

			saveConflicts(conflicts);
		};

		for (const GameEntry& game : games)
		{
			if (setText)
				setText("Syncing " + game.gameStem + "...");

			std::error_code ec;
			std::string error;
			const fs::path saveRoot = savesRoot / game.baseSystem;
			fs::create_directories(saveRoot, ec);

			// Saves
			{
				const std::string key = "save:" + std::to_string(game.romId);
				const std::string localPath = localSavePath(game);
				const bool localExists = !localPath.empty();
				const std::string localHash = localExists ? fileMd5(localPath) : "";
				const int64_t localMtime = localExists ? fileMtime(localPath) : 0;

				std::vector<ServerSave> saves;
				if (!fetchServerSaves(server, game.romId, saves, error))
				{
					result.error = error;
					persist();
					return result;
				}

				const ServerSave* serverSave = pickSave(saves);
				const std::string serverHash = serverSave != nullptr ? serverSave->hash : "";
				const int64_t serverUpdated = serverSave != nullptr ? serverSave->updated : 0;

				auto baselineIt = baseline.find(key);
				const Baseline* previous = baselineIt == baseline.end() ? nullptr : &baselineIt->second;
				const Action action = decide(localExists, serverSave != nullptr, localHash, localMtime,
					serverHash, serverUpdated, previous);

				if (action == Action::Upload)
				{
					if (setText)
						setText("Uploading " + game.gameStem + " save...");

					ServerSave uploaded;
					if (!uploadSave(server, game, localPath, localHash, uploaded, error))
					{
						result.error = error;
						persist();
						return result;
					}

					result.uploaded++;
					remember(baseline[key], uploaded.id, uploaded.updated, uploaded.hash, localPath, localHash);
				}
				else if (action == Action::Download)
				{
					const std::string extension = serverSave->extension.empty() ? "srm" : serverSave->extension;
					const fs::path destination = localPath.empty() ? saveRoot / (game.gameStem + "." + extension) : fs::path(localPath);

					if (setText)
						setText("Downloading " + game.gameStem + " save...");

					if (!downloadFile(server, "/api/saves/" + std::to_string(serverSave->id) + "/content", destination, error))
					{
						result.error = error;
						persist();
						return result;
					}

					result.downloaded++;
					remember(baseline[key], serverSave->id, serverSave->updated, serverHash,
						destination.string(), fileMd5(destination.string()));
				}
				else if (action == Action::Conflict)
				{
					result.conflicts++;
					result.conflictNames.push_back(game.gameStem + " (save)");

					RomMConflict conflict;
					conflict.id = key;
					conflict.romId = game.romId;
					conflict.game = game.gameStem;
					conflict.baseSystem = game.baseSystem;
					conflict.kind = "save";
					conflict.localPath = localPath;
					conflict.localHash = localHash;
					conflict.localMtime = localMtime;
					conflict.serverId = serverSave->id;
					conflict.serverUpdated = serverSave->updated;
					conflict.serverHash = serverHash;
					conflict.serverExtension = serverSave->extension;
					conflict.serverFileName = serverSave->fileName;
					conflict.serverEmulator = serverSave->emulator;
					pending[key] = conflict;
				}
				else if (serverSave != nullptr)
				{
					remember(baseline[key], serverSave->id, serverSave->updated, serverHash, localPath, localHash);
				}

				if (action != Action::Conflict)
					pending.erase(key);
			}

			// States
			{
				std::vector<ServerState> states;
				if (!fetchServerStates(server, game.romId, states, error))
				{
					result.error = error;
					persist();
					return result;
				}

				const std::map<std::string, const ServerState*> serverStates = pickStates(states);
				const std::vector<LocalState> local = localStates(game);
				std::set<std::string> handled;

				for (const LocalState& state : local)
				{
					const std::string extension = state.suffix.substr(1);
					const std::string key = "state:" + std::to_string(game.romId) + ":" + extension;
					const std::string localHash = fileMd5(state.path);
					const int64_t localMtime = fileMtime(state.path);

					auto serverIt = serverStates.find(extension);
					const ServerState* serverState = serverIt == serverStates.end() ? nullptr : serverIt->second;
					const std::string serverHash = serverState != nullptr ? serverState->hash : "";
					const int64_t serverUpdated = serverState != nullptr ? serverState->updated : 0;

					auto baselineIt = baseline.find(key);
					const Baseline* previous = baselineIt == baseline.end() ? nullptr : &baselineIt->second;
					const Action action = decide(true, serverState != nullptr, localHash, localMtime,
						serverHash, serverUpdated, previous);

					if (action == Action::Upload)
					{
						if (setText)
							setText("Uploading " + game.gameStem + " state...");

						ServerState uploaded;
						if (!uploadState(server, game, state.path, localHash, uploaded, error))
						{
							result.error = error;
							persist();
							return result;
						}

						result.uploaded++;
						remember(baseline[key], uploaded.id, uploaded.updated, uploaded.hash, state.path, localHash);
					}
					else if (action == Action::Download)
					{
						const fs::path destination = saveRoot / (game.gameStem + state.suffix);

						if (setText)
							setText("Downloading " + game.gameStem + " state...");

						if (!downloadFile(server, "/api/states/" + std::to_string(serverState->id) + "/content", destination, error))
						{
							result.error = error;
							persist();
							return result;
						}

						result.downloaded++;
						remember(baseline[key], serverState->id, serverState->updated, serverHash,
							destination.string(), fileMd5(destination.string()));
					}
					else if (action == Action::Conflict)
					{
						result.conflicts++;
						result.conflictNames.push_back(game.gameStem + " (" + state.suffix + ")");

						RomMConflict conflict;
						conflict.id = key;
						conflict.romId = game.romId;
						conflict.game = game.gameStem;
						conflict.baseSystem = game.baseSystem;
						conflict.kind = "state";
						conflict.suffix = state.suffix;
						conflict.localPath = state.path;
						conflict.localHash = localHash;
						conflict.localMtime = localMtime;
						conflict.serverId = serverState->id;
						conflict.serverUpdated = serverState->updated;
						conflict.serverHash = serverHash;
						conflict.serverExtension = serverState->extension;
						conflict.serverFileName = serverState->fileName;
						conflict.serverEmulator = serverState->emulator;
						pending[key] = conflict;
					}
					else if (serverState != nullptr)
					{
						remember(baseline[key], serverState->id, serverState->updated, serverHash, state.path, localHash);
					}

					if (action != Action::Conflict)
						pending.erase(key);

					handled.insert(extension);
				}

				for (const auto& [extension, serverState] : serverStates)
				{
					if (handled.find(extension) != handled.end())
						continue;

					const fs::path destination = saveRoot / (game.gameStem + "." + extension);

					if (setText)
						setText("Downloading " + game.gameStem + " state...");

					if (!downloadFile(server, "/api/states/" + std::to_string(serverState->id) + "/content", destination, error))
					{
						result.error = error;
						persist();
						return result;
					}

					result.downloaded++;
					const std::string key = "state:" + std::to_string(game.romId) + ":" + extension;
					remember(baseline[key], serverState->id, serverState->updated, serverState->hash,
						destination.string(), fileMd5(destination.string()));
					pending.erase(key);
				}
			}
		}

		persist();
		result.success = true;
		return result;
	}

	std::vector<RomMConflict> loadConflicts()
	{
		return loadConflictList();
	}

	RomMResolveResult resolveConflict(const RomMConflict& conflict, bool keepLocal,
		const std::function<void(const std::string&)>& setText)
	{
		RomMResolveResult result;

		const std::string server = RomMScraperUrl::normalizeServer(Settings::getInstance()->getString("RomMServer"));
		if (server.empty())
		{
			result.error = "No RomM server URL configured";
			return result;
		}

		if (conflict.id.empty() || conflict.romId <= 0 || conflict.kind.empty())
		{
			result.error = "Invalid conflict";
			return result;
		}

		GameEntry game;
		game.romId = conflict.romId;
		game.baseSystem = conflict.baseSystem;
		game.gamePath = conflict.localPath;
		game.gameStem = Utils::FileSystem::getStem(conflict.localPath);

		std::map<std::string, Baseline> baseline = loadBaseline();
		const fs::path savesRoot = Paths::getSavesPath();
		std::error_code ec;

		if (keepLocal)
		{
			if (conflict.localPath.empty() || !fs::exists(conflict.localPath, ec))
			{
				result.error = "The local file is missing: " + conflict.localPath;
				return result;
			}

			const std::string localHash = fileMd5(conflict.localPath);

			if (conflict.kind == "save")
			{
				if (setText)
					setText("Uploading " + conflict.game + " save...");

				ServerSave uploaded;
				if (!uploadSave(server, game, conflict.localPath, localHash, uploaded, result.error))
					return result;

				remember(baseline[conflict.id], uploaded.id, uploaded.updated, uploaded.hash,
					conflict.localPath, localHash);
			}
			else
			{
				if (setText)
					setText("Uploading " + conflict.game + " state...");

				ServerState uploaded;
				if (!uploadState(server, game, conflict.localPath, localHash, uploaded, result.error,
					conflict.serverFileName, conflict.serverEmulator))
					return result;

				remember(baseline[conflict.id], uploaded.id, uploaded.updated, uploaded.hash,
					conflict.localPath, localHash);
			}
		}
		else
		{
			fs::path destination = conflict.localPath;
			if (destination.empty())
			{
				const std::string extension = conflict.kind == "save"
					? (conflict.serverExtension.empty() ? "srm" : conflict.serverExtension)
					: conflict.suffix.substr(1);
				destination = savesRoot / conflict.baseSystem / (conflict.game + "." + extension);
			}

			if (setText)
				setText("Downloading " + conflict.game + (conflict.kind == "save" ? " save..." : " state..."));

			const std::string urlPath = (conflict.kind == "save" ? "/api/saves/" : "/api/states/") +
				std::to_string(conflict.serverId) + "/content";

			if (!downloadFile(server, urlPath, destination, result.error))
				return result;

			remember(baseline[conflict.id], conflict.serverId, conflict.serverUpdated, conflict.serverHash,
				destination.string(), fileMd5(destination.string()));
		}

		saveBaseline(baseline);

		std::vector<RomMConflict> conflicts = loadConflictList();
		conflicts.erase(std::remove_if(conflicts.begin(), conflicts.end(),
			[&conflict](const RomMConflict& other) { return other.id == conflict.id; }), conflicts.end());
		saveConflicts(conflicts);

		result.success = true;
		return result;
	}
}
