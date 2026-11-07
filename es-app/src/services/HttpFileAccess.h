#pragma once

#include <string>

// Path and access checks of the file manager of the web API (HttpFileApi), kept apart from the server
// so that the unit tests can reach them.
namespace HttpFileAccess
{
	// Resolves a path relative to rootPath. Returns false if it leaves the root, through ".." or through a symbolic link.
	// With mustExist false, the last component may not exist yet, but its parent must.
	bool resolvePath(const std::string& rootPath, const std::string& relativePath, std::string& resolved, bool mustExist);

	enum class Access
	{
		Granted,
		NoPassword,      // request from the network while no password is set
		MissingPassword, // request from the network without credentials
		WrongPassword
	};

	// From this computer, only checks that the Host header is local, so that a web page cannot reach the API through
	// DNS rebinding. From the network, needs the password as HTTP Basic auth (any user name).
	Access check(const std::string& remoteAddr, const std::string& hostHeader, const std::string& authorizationHeader, const std::string& password);
}
