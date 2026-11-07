#include "HttpFileApi.h"
#include "HttpFileAccess.h"
// httplib.h defines base64_encode out of line, so that HfsDBScraper can link to the copy of HttpServerThread.cpp.
// This second copy gets another name, or the link fails with a duplicate symbol.
#define base64_encode httpFileApi_base64_encode
#include "httplib.h"
#undef base64_encode
#include "Log.h"
#include "Paths.h"
#include "Settings.h"
#include "SystemData.h"
#include "FileData.h"
#include "utils/StringUtil.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace HttpFileApi
{

// ---------------------------------------------------------------------------------------------
// Roots and paths
// ---------------------------------------------------------------------------------------------

static bool isDirectory(const std::string& path)
{
	std::error_code ec;
	return !path.empty() && fs::is_directory(path, ec);
}

std::vector<Root> getRoots()
{
	std::vector<Root> roots;
	std::set<std::string> ids;

	auto add = [&roots, &ids](const std::string& id, const std::string& name, const std::string& path)
	{
		if (ids.find(id) != ids.cend() || !isDirectory(path))
			return;

		ids.insert(id);
		roots.push_back({ id, name, path });
	};

	std::vector<std::pair<std::string, std::string>> systems; // name, path
	std::set<std::string> parents;

	for (auto system : SystemData::sSystemVector)
	{
		if (system->isCollection() || system->isGroupSystem() || !system->isGameSystem() || system->getRootFolder() == nullptr)
			continue;

		std::string path = system->getRootFolder()->getPath();
		if (!isDirectory(path))
			continue;

		systems.push_back({ system->getName(), path });
		parents.insert(fs::path(path).lexically_normal().parent_path().string());
	}

	// All the systems in one folder (~/ROMs/nes, ~/ROMs/snes...): that folder is a root too, to create the folder of a new system
	if (parents.size() == 1)
		add("roms", "ROMs", *parents.cbegin());

	for (auto& system : systems)
		add("roms/" + system.first, "ROMs - " + system.first, system.second);

	add("bios", "BIOS", Paths::getBiosPath());
	add("saves", "Saves", Paths::getSavesPath());
	add("screenshots", "Screenshots", Paths::getScreenShotPath());
	add("config", "Configuration", Paths::getConfigPath());

	return roots;
}


// ---------------------------------------------------------------------------------------------
// Access control
// ---------------------------------------------------------------------------------------------

static void sendError(httplib::Response& res, int status, const std::string& message)
{
	res.status = status;
	res.set_content("{\"error\":\"" + message + "\"}", "application/json");
}

static bool checkAccess(const httplib::Request& req, httplib::Response& res)
{
	auto access = HttpFileAccess::check(req.remote_addr, req.get_header_value("Host"), req.get_header_value("Authorization"), Settings::getInstance()->getString("WebAccessPassword"));
	if (access == HttpFileAccess::Access::Granted)
		return true;

	if (access == HttpFileAccess::Access::NoPassword)
	{
		LOG(LogWarning) << "HttpFileApi : file access refused for " << req.remote_addr << ", no web access password is set";
		sendError(res, 403, "Set a web access password in the EmulationStation menu to manage files from the network");
		return false;
	}

	if (access == HttpFileAccess::Access::WrongPassword)
	{
		LOG(LogWarning) << "HttpFileApi : wrong password from " << req.remote_addr;
		std::this_thread::sleep_for(std::chrono::seconds(1)); // slows down password guessing
	}

	res.set_header("WWW-Authenticate", "Basic realm=\"EmulationStation\", charset=\"UTF-8\"");
	sendError(res, 401, "Password required");
	return false;
}

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

static std::string jsonEscape(const std::string& s)
{
	std::string out;
	out.reserve(s.size() + 2);

	for (unsigned char c : s)
	{
		switch (c)
		{
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (c < 0x20)
			{
				char buf[8];
				snprintf(buf, sizeof(buf), "\\u%04x", c);
				out += buf;
			}
			else
				out += (char)c;
		}
	}

	return out;
}

// RFC 5987 value for Content-Disposition: everything but unreserved characters is encoded
static std::string percentEncode(const std::string& s)
{
	std::string out;
	for (unsigned char c : s)
	{
		if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~')
			out += (char)c;
		else
		{
			char buf[4];
			snprintf(buf, sizeof(buf), "%%%02X", c);
			out += buf;
		}
	}

	return out;
}

static long long toUnixTime(const fs::file_time_type& time)
{
	auto sys = std::chrono::clock_cast<std::chrono::system_clock>(time);
	return std::chrono::duration_cast<std::chrono::seconds>(sys.time_since_epoch()).count();
}

// Finds the root and the path named by ?root= and ?path=, after the access check. Sends the error itself.
static bool getTarget(const httplib::Request& req, httplib::Response& res, Root& root, std::string& path, const char* pathParam = "path")
{
	if (!checkAccess(req, res))
		return false;

	std::string rootId = req.get_param_value("root");
	bool found = false;

	for (auto& r : getRoots())
	{
		if (r.id == rootId)
		{
			root = r;
			found = true;
			break;
		}
	}

	if (!found)
	{
		sendError(res, 404, "Unknown root");
		return false;
	}

	if (!HttpFileAccess::resolvePath(root.path, req.get_param_value(pathParam), path, false))
	{
		LOG(LogWarning) << "HttpFileApi : path outside of " << root.id << " refused : " << req.get_param_value(pathParam);
		sendError(res, 403, "Path outside of the root");
		return false;
	}

	return true;
}

static bool isRootPath(const Root& root, const std::string& path)
{
	std::error_code ec;
	return fs::equivalent(root.path, path, ec);
}

// ---------------------------------------------------------------------------------------------
// Routes
// ---------------------------------------------------------------------------------------------

void registerRoutes(httplib::Server* server, const std::function<bool(const httplib::Request&, httplib::Response&)>& isAllowed)
{
	server->Get("/files/roots", [isAllowed](const httplib::Request& req, httplib::Response& res)
	{
		if (!isAllowed(req, res) || !checkAccess(req, res))
			return;

		std::string json = "[";
		bool first = true;

		for (auto& root : getRoots())
		{
			if (!first)
				json += ",";

			json += "{\"id\":\"" + jsonEscape(root.id) + "\",\"name\":\"" + jsonEscape(root.name) + "\",\"path\":\"" + jsonEscape(root.path) + "\"}";
			first = false;
		}

		json += "]";
		res.set_content(json, "application/json");
	});

	server->Get("/files/list", [isAllowed](const httplib::Request& req, httplib::Response& res)
	{
		Root root;
		std::string path;
		if (!isAllowed(req, res) || !getTarget(req, res, root, path))
			return;

		std::error_code ec;
		if (!fs::is_directory(path, ec))
		{
			sendError(res, 404, "Folder not found");
			return;
		}

		std::vector<std::string> entries;

		for (auto it = fs::directory_iterator(path, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
		{
			std::error_code fileEc;
			bool dir = it->is_directory(fileEc);
			unsigned long long size = dir ? 0 : it->file_size(fileEc);
			if (fileEc)
				size = 0;

			long long mtime = 0;
			auto time = it->last_write_time(fileEc);
			if (!fileEc)
				mtime = toUnixTime(time);

			entries.push_back("{\"name\":\"" + jsonEscape(it->path().filename().string()) + "\",\"dir\":" + (dir ? "true" : "false") +
				",\"size\":" + std::to_string(size) + ",\"mtime\":" + std::to_string(mtime) + "}");
		}

		if (ec)
		{
			sendError(res, 500, "Cannot read the folder");
			return;
		}

		std::string relative = fs::path(path).lexically_relative(fs::canonical(root.path, ec)).generic_string();
		if (relative == ".")
			relative = "";

		std::string json = "{\"root\":\"" + jsonEscape(root.id) + "\",\"path\":\"" + jsonEscape(relative) + "\",\"entries\":[";
		for (size_t i = 0; i < entries.size(); i++)
			json += (i ? "," : "") + entries[i];

		json += "]}";
		res.set_content(json, "application/json");
	});

	server->Get("/files/download", [isAllowed](const httplib::Request& req, httplib::Response& res)
	{
		Root root;
		std::string path;
		if (!isAllowed(req, res) || !getTarget(req, res, root, path))
			return;

		std::error_code ec;
		if (!fs::is_regular_file(path, ec))
		{
			sendError(res, 404, "File not found");
			return;
		}

		auto size = fs::file_size(path, ec);
		auto file = std::make_shared<std::ifstream>(path, std::ios::binary);
		if (ec || !file->is_open())
		{
			sendError(res, 500, "Cannot read the file");
			return;
		}

		std::string name = fs::path(path).filename().string();

		if (req.get_param_value("inline") == "1")
			res.set_header("Content-Type", "text/plain; charset=utf-8"); // shown as text, never run as a page
		else
		{
			res.set_header("Content-Type", "application/octet-stream");
			res.set_header("Content-Disposition", "attachment; filename*=UTF-8''" + percentEncode(name));
		}

		res.set_header("X-Content-Type-Options", "nosniff");

		res.set_content_provider((size_t)size, [file](size_t offset, size_t length, httplib::DataSink& sink)
		{
			char buffer[64 * 1024];

			file->seekg((std::streamoff)offset);
			size_t toRead = std::min(length, sizeof(buffer));
			file->read(buffer, (std::streamsize)toRead);

			auto read = file->gcount();
			if (read <= 0)
				return false;

			sink.write(buffer, (size_t)read);
			return true;
		});
	});

	server->Put("/files/upload", [isAllowed](const httplib::Request& req, httplib::Response& res, const httplib::ContentReader& reader)
	{
		// A refused upload must still read its body: left in the connection, it would be parsed as the next request
		bool bodyRead = false;
		std::shared_ptr<void> drainOnExit(nullptr, [&](void*)
		{
			if (!bodyRead)
				reader([](const char*, size_t) { return true; });
		});

		Root root;
		std::string path;
		if (!isAllowed(req, res) || !getTarget(req, res, root, path))
			return;

		std::error_code ec;
		if (isRootPath(root, path) || fs::is_directory(path, ec))
		{
			sendError(res, 400, "The path must name a file");
			return;
		}

		if (fs::exists(path, ec) && req.get_param_value("overwrite") != "1")
		{
			sendError(res, 409, "The file already exists");
			return;
		}

		// Written next to the file, then renamed: an interrupted upload never leaves half a file under its real name
		fs::path target(path);
		std::string partial = (target.parent_path() / ("." + target.filename().string() + ".part")).string();

		bool ok;
		{
			std::ofstream out(partial, std::ios::binary | std::ios::trunc);
			if (!out.is_open())
			{
				sendError(res, 500, "Cannot write the file");
				return;
			}

			bodyRead = true;
			ok = reader([&out](const char* data, size_t length)
			{
				out.write(data, (std::streamsize)length);
				return out.good();
			});

			out.close();
			ok = ok && !out.fail();
		}

		if (!ok)
		{
			fs::remove(partial, ec);
			sendError(res, 500, "Upload interrupted");
			return;
		}

		fs::rename(partial, path, ec);
		if (ec)
		{
			fs::remove(partial, ec);
			sendError(res, 500, "Cannot write the file");
			return;
		}

		LOG(LogInfo) << "HttpFileApi : uploaded " << path;

		res.status = 201;
		res.set_content("{\"ok\":true}", "application/json");
	});

	server->Put("/files/mkdir", [isAllowed](const httplib::Request& req, httplib::Response& res)
	{
		Root root;
		std::string path;
		if (!isAllowed(req, res) || !getTarget(req, res, root, path))
			return;

		std::error_code ec;
		if (fs::exists(path, ec))
		{
			sendError(res, 409, "Already exists");
			return;
		}

		if (!fs::create_directory(path, ec))
		{
			sendError(res, 500, "Cannot create the folder");
			return;
		}

		res.status = 201;
		res.set_content("{\"ok\":true}", "application/json");
	});

	server->Put("/files/rename", [isAllowed](const httplib::Request& req, httplib::Response& res)
	{
		Root root;
		std::string path;
		if (!isAllowed(req, res) || !getTarget(req, res, root, path))
			return;

		std::string destination;
		if (!HttpFileAccess::resolvePath(root.path, req.get_param_value("to"), destination, false))
		{
			sendError(res, 403, "Path outside of the root");
			return;
		}

		std::error_code ec;
		if (isRootPath(root, path) || isRootPath(root, destination) || !fs::exists(fs::symlink_status(path, ec)))
		{
			sendError(res, 400, "Nothing to rename");
			return;
		}

		if (fs::exists(fs::symlink_status(destination, ec)))
		{
			sendError(res, 409, "The destination already exists");
			return;
		}

		fs::rename(path, destination, ec);
		if (ec)
		{
			sendError(res, 500, "Cannot rename");
			return;
		}

		res.set_content("{\"ok\":true}", "application/json");
	});

	server->Delete("/files", [isAllowed](const httplib::Request& req, httplib::Response& res)
	{
		Root root;
		std::string path;
		if (!isAllowed(req, res) || !getTarget(req, res, root, path))
			return;

		std::error_code ec;
		if (isRootPath(root, path))
		{
			sendError(res, 400, "A root cannot be deleted");
			return;
		}

		auto status = fs::symlink_status(path, ec);
		if (!fs::exists(status))
		{
			sendError(res, 404, "Not found");
			return;
		}

		if (fs::is_directory(status))
		{
			if (req.get_param_value("recursive") == "1")
				fs::remove_all(path, ec); // does not follow the links it finds inside
			else if (!fs::is_empty(path, ec))
			{
				sendError(res, 409, "The folder is not empty");
				return;
			}
			else
				fs::remove(path, ec);
		}
		else
			fs::remove(path, ec);

		if (ec)
		{
			sendError(res, 500, "Cannot delete");
			return;
		}

		LOG(LogInfo) << "HttpFileApi : deleted " << path;

		res.set_content("{\"ok\":true}", "application/json");
	});
}

}
