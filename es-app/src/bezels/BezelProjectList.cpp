#include "bezels/BezelProjectList.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

namespace bezelproject
{
	namespace
	{
		std::string trim(const std::string& s)
		{
			size_t begin = s.find_first_not_of(" \t\r\n");
			if (begin == std::string::npos)
				return "";
			size_t end = s.find_last_not_of(" \t\r\n");
			return s.substr(begin, end - begin + 1);
		}

		std::string lower(std::string s)
		{
			std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
			return s;
		}

		std::string withoutExtension(const std::string& fileName)
		{
			size_t dot = fileName.find_last_of('.');
			return dot == std::string::npos || dot == 0 ? fileName : fileName.substr(0, dot);
		}

		std::string extension(const std::string& fileName)
		{
			size_t dot = fileName.find_last_of('.');
			return dot == std::string::npos ? "" : lower(fileName.substr(dot));
		}

		std::vector<std::string> lines(const std::string& text)
		{
			std::vector<std::string> result;
			std::istringstream stream(text);
			std::string line;
			while (std::getline(stream, line))
			{
				line = trim(line);
				if (!line.empty() && line[0] != '#')
					result.push_back(line);
			}
			return result;
		}

		bool isHttps(const std::string& url)
		{
			return url.rfind("https://", 0) == 0 && url.find_first_of(" \t\"'") == std::string::npos;
		}

		// No path in a name that ends up in a file path or a URL
		bool isPlainName(const std::string& name)
		{
			return !name.empty() && name != "." && name != ".." && name.find('/') == std::string::npos && name.find('\\') == std::string::npos;
		}
	}

	std::vector<System> parseSystems(const std::string& text)
	{
		std::vector<System> systems;
		for (const auto& line : lines(text))
		{
			std::istringstream fields(line);
			System system;
			fields >> system.name >> system.repository >> system.folder >> system.systemBezel;

			if (!isPlainName(system.name) || !isHttps(system.repository) || !isPlainName(system.folder))
				continue;
			if (!system.systemBezel.empty() && !isPlainName(system.systemBezel))
				system.systemBezel.clear();

			while (!system.repository.empty() && system.repository.back() == '/')
				system.repository.pop_back();

			systems.push_back(system);
		}
		return systems;
	}

	std::vector<Bezel> parseIndex(const std::string& text)
	{
		std::vector<Bezel> index;
		for (const auto& line : lines(text))
		{
			size_t space = line.find_first_of(" \t");
			if (space == std::string::npos)
				continue;

			Bezel bezel;
			bezel.md5 = lower(line.substr(0, space));
			bezel.file = trim(line.substr(space + 1));

			if (bezel.md5.size() != 32 || bezel.md5.find_first_not_of("0123456789abcdef") != std::string::npos)
				continue;
			if (!isPlainName(bezel.file) || extension(bezel.file) != ".png")
				continue;

			index.push_back(bezel);
		}
		return index;
	}

	std::string matchKey(const std::string& fileName)
	{
		std::string name = withoutExtension(fileName);

		for (const char* cut : { " [", " (" })
		{
			size_t pos = name.find(cut);
			if (pos != std::string::npos)
				name = name.substr(0, pos);
		}

		return lower(trim(name));
	}

	std::map<std::string, Bezel> byMatchKey(const std::vector<Bezel>& index)
	{
		std::map<std::string, Bezel> result;
		for (const auto& bezel : index)
			result.emplace(matchKey(bezel.file), bezel);
		return result;
	}

	static std::string overlayUrl(const System& system)
	{
		return system.repository + "/raw/master/retroarch/overlay/";
	}

	std::string bezelUrl(const System& system, const std::string& file)
	{
		std::string folder = system.folder == "ArcadeBezels" ? "ArcadeBezels/" : "GameBezels/" + urlEncode(system.folder) + "/";
		return overlayUrl(system) + folder + urlEncode(file);
	}

	std::string systemBezelUrl(const System& system)
	{
		if (system.systemBezel.empty() || extension(system.systemBezel) != ".png")
			return "";

		return overlayUrl(system) + urlEncode(system.systemBezel);
	}

	std::string urlEncode(const std::string& s)
	{
		static const char* hex = "0123456789ABCDEF";

		std::string result;
		for (unsigned char c : s)
		{
			if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
				result += (char)c;
			else
			{
				result += '%';
				result += hex[c >> 4];
				result += hex[c & 15];
			}
		}
		return result;
	}

	bool isIgnoredFile(const std::string& fileName)
	{
		static const std::set<std::string> ignored = {
			".jpg", ".jpeg", ".png", ".bmp", ".psd", ".tga", ".gif", ".hdr", ".pic", ".ppm", ".pgm",
			".mkv", ".pdf", ".mp4", ".avi", ".cbz", ".txt", ".xml", ".cfg", ".srm", ".state", ".sav" };

		if (fileName.empty() || fileName[0] == '.' || fileName[0] == '_')
			return true;
		if (lower(fileName).rfind("gamelist", 0) == 0)
			return true;

		return ignored.count(extension(fileName)) != 0;
	}
}
