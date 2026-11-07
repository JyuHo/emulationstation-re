#include "HttpFileAccess.h"
#include "utils/StringUtil.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace HttpFileAccess
{

static bool isInside(const fs::path& root, const fs::path& path)
{
	std::string r = root.native();
	const std::string& p = path.native();

	if (p == r)
		return true;

	if (r.empty() || r.back() != '/')
		r += '/';

	return p.compare(0, r.size(), r) == 0;
}

bool resolvePath(const std::string& rootPath, const std::string& relativePath, std::string& resolved, bool mustExist)
{
	if (rootPath.empty() || relativePath.find('\0') != std::string::npos)
		return false;

	std::error_code ec;
	fs::path root = fs::canonical(rootPath, ec);
	if (ec)
		return false;

	fs::path target = root;

	for (auto part : Utils::String::splitAny(relativePath, "/\\"))
	{
		if (part.empty() || part == ".")
			continue;

		if (part == "..")
			return false;

		target /= part;
	}

	if (target == root)
	{
		resolved = root.string();
		return true;
	}

	if (fs::exists(fs::symlink_status(target, ec)))
	{
		// Follows every symbolic link: the real location must stay in the root
		fs::path real = fs::canonical(target, ec);
		if (!ec)
		{
			if (!isInside(root, real))
				return false;

			resolved = target.string(); // keep the link itself, so that deleting it does not delete its target
			return true;
		}
		// A dangling link: only its folder is checked, as for a new file
	}

	if (mustExist)
		return false;

	fs::path parent = fs::canonical(target.parent_path(), ec);
	if (ec || !isInside(root, parent))
		return false;

	resolved = (parent / target.filename()).string();
	return true;
}

static bool isLocalAddress(const std::string& addr)
{
	return addr == "127.0.0.1" || addr == "::1" || addr == "::ffff:127.0.0.1";
}

static std::string hostWithoutPort(std::string host)
{
	host = Utils::String::toLower(Utils::String::trim(host));

	if (!host.empty() && host[0] == '[')
	{
		auto end = host.find(']');
		return end == std::string::npos ? host : host.substr(1, end - 1);
	}

	auto colon = host.find(':');
	if (colon != std::string::npos)
		host = host.substr(0, colon);

	return host;
}

static std::string base64Decode(const std::string& in)
{
	static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	std::string out;
	int val = 0, bits = -8;

	for (unsigned char c : in)
	{
		if (c == '=')
			break;

		auto pos = chars.find(c);
		if (pos == std::string::npos)
			continue;

		val = (val << 6) + (int)pos;
		bits += 6;
		if (bits >= 0)
		{
			out.push_back(char((val >> bits) & 0xFF));
			bits -= 8;
		}
	}

	return out;
}

static bool sameSecret(const std::string& a, const std::string& b)
{
	// Same duration whatever the first different character
	unsigned char diff = a.size() == b.size() ? 0 : 1;
	for (size_t i = 0; i < a.size(); i++)
		diff |= (unsigned char)(a[i] ^ (i < b.size() ? b[i] : 0));

	return diff == 0;
}

Access check(const std::string& remoteAddr, const std::string& hostHeader, const std::string& authorizationHeader, const std::string& password)
{
	if (isLocalAddress(remoteAddr))
	{
		// A page of another site resolved to 127.0.0.1 (DNS rebinding) would send its own host name
		auto host = hostWithoutPort(hostHeader);
		if (host == "localhost" || host == "127.0.0.1" || host == "::1")
			return Access::Granted;
	}

	if (password.empty())
		return Access::NoPassword;

	if (!Utils::String::startsWith(authorizationHeader, "Basic "))
		return Access::MissingPassword;

	auto credentials = base64Decode(authorizationHeader.substr(6));
	auto colon = credentials.find(':');
	if (colon != std::string::npos && sameSecret(credentials.substr(colon + 1), password))
		return Access::Granted;

	return Access::WrongPassword;
}

}
