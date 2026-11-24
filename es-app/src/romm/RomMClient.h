#pragma once

#include <functional>
#include <string>

#include <rapidjson/document.h>

namespace RomMClient
{
	// The Authorization header of the RomM settings: a client token as Bearer,
	// else the username/password as Basic, else empty.
	std::string authHeader();

	// GET a JSON document from the RomM API. The server is the normalized base
	// URL, the path starts with "/api/".
	bool getJson(const std::string& server, const std::string& path, rapidjson::Document& document, std::string& error);

	// Download a URL to a file, with the Authorization header. The progress
	// callback receives the percentage, or -1 when the server sends no length.
	bool download(const std::string& url, const std::string& destination,
		const std::function<void(int)>& progress, std::string& error);

	// Download a RomM resource path or absolute media URL to a file.
	bool downloadMedia(const std::string& server, const std::string& pathOrUrl,
		const std::string& destination, std::string& error);
}
