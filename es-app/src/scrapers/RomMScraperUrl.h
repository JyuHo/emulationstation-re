#pragma once

// The parts of the RomM scraper that need no Settings, SystemData or HTTP:
// the server URL, the platform matching and the resource URLs.

#include <string>

namespace RomMScraperUrl
{
	// "http://host:8080/" -> "http://host:8080". A missing scheme gets
	// "http://", and a trailing "/api" is removed, so that both the server
	// root and the API URL can be pasted in the settings.
	std::string normalizeServer(const std::string& server);

	// Lower case, letters and digits only: "Super Nintendo" -> "supernintendo".
	std::string normalizeName(const std::string& name);

	// Whether a RomM platform matches an ES-RE system name, directly or
	// through the Batocera/ES-RE aliases ("ps" and "psx", "gc" and "ngc"...).
	bool platformMatches(const std::string& systemName, const std::string& slug, const std::string& fsSlug);

	// A RomM resource path as an absolute URL: "roms/1/cover.png" becomes
	// "<server>/assets/romm/resources/roms/1/cover.png". An absolute URL is
	// returned unchanged.
	std::string resourceUrl(const std::string& server, const std::string& pathOrUrl);

	// The extension of the URL's path, with its dot, ignoring the query
	// string: "http://host/cover.png?ts=1" -> ".png".
	std::string extensionOf(const std::string& url);
}
