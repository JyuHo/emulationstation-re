#pragma once

#include <functional>
#include <string>
#include <vector>

namespace httplib
{
	class Server;
	struct Request;
	struct Response;
}

// File manager API of the web server: ROMs, BIOS, saves and configuration files.
//
// GET    /files/roots                                  -> the folders the API can reach, as JSON
// GET    /files/list?root={id}&path={relative}         -> content of a folder, as JSON
// GET    /files/download?root={id}&path={relative}     -> file bytes (add &inline=1 to display it instead of saving it)
// PUT    /files/upload?root={id}&path={relative}       -> body = file bytes. Add &overwrite=1 to replace an existing file
// PUT    /files/mkdir?root={id}&path={relative}        -> creates a folder
// PUT    /files/rename?root={id}&path={relative}&to={relative}  -> moves a file or folder inside the same root
// DELETE /files?root={id}&path={relative}              -> deletes a file, or a folder with &recursive=1
//
// Every path is relative to its root and is refused if it leaves it ("..", or a symbolic link pointing outside).
// From the network (public web access), the API needs the password set in the menu, sent as HTTP Basic auth,
// and stays closed while no password is set. From this computer, it only checks that the Host header is local,
// so a web page opened in a browser cannot reach it through DNS rebinding.
namespace HttpFileApi
{
	struct Root
	{
		std::string id;    // "bios", "saves", "config", "roms/snes"...
		std::string name;  // label for the UI
		std::string path;  // absolute path on disk
	};

	std::vector<Root> getRoots();

	void registerRoutes(httplib::Server* server, const std::function<bool(const httplib::Request&, httplib::Response&)>& isAllowed);
}
