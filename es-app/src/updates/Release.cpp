#include "updates/Release.h"

#include <rapidjson/document.h>

#include <algorithm>
#include <cctype>
#include <regex>
#include <vector>

namespace selfupdate
{
	namespace
	{
		std::string str(const rapidjson::Value& obj, const char* name)
		{
			auto it = obj.FindMember(name);
			return it != obj.MemberEnd() && it->value.IsString() ? it->value.GetString() : "";
		}

		bool fail(std::string* error, const std::string& why)
		{
			if (error)
				*error = why;
			return false;
		}

		struct Parsed
		{
			std::vector<long> numbers;
			int kind = 0;          // -1 pre-release, 0 release, 1 after the release (git describe)
			long commits = 0;      // kind 1: commits after the tag
			std::string suffix;    // kind -1: compared as text (rc1 < rc2)
		};

		Parsed parse(std::string v)
		{
			Parsed p;
			if (!v.empty() && (v[0] == 'v' || v[0] == 'V'))
				v.erase(0, 1);

			size_t i = 0;
			while (i < v.size() && std::isdigit((unsigned char)v[i]))
			{
				long n = 0;
				while (i < v.size() && std::isdigit((unsigned char)v[i]))
					n = n * 10 + (v[i++] - '0');
				p.numbers.push_back(n);

				if (i + 1 < v.size() && v[i] == '.' && std::isdigit((unsigned char)v[i + 1]))
					++i;
				else
					break;
			}

			std::string rest = v.substr(i);
			if (!rest.empty() && (rest[0] == '-' || rest[0] == '+' || rest[0] == '.' || rest[0] == '~'))
				rest.erase(0, 1);

			static const std::regex describe("^([0-9]+)-g[0-9a-f]+(-dirty)?$");
			std::smatch m;
			if (rest.empty())
				p.kind = 0;
			else if (std::regex_match(rest, m, describe))
			{
				p.kind = 1;
				p.commits = std::stol(m[1].str());
			}
			else
			{
				p.kind = -1;
				p.suffix = rest;
			}
			return p;
		}
	}

	int compareVersions(const std::string& a, const std::string& b)
	{
		auto pa = parse(a);
		auto pb = parse(b);

		size_t count = std::max(pa.numbers.size(), pb.numbers.size());
		for (size_t i = 0; i < count; ++i)
		{
			long na = i < pa.numbers.size() ? pa.numbers[i] : 0;
			long nb = i < pb.numbers.size() ? pb.numbers[i] : 0;
			if (na != nb)
				return na < nb ? -1 : 1;
		}

		if (pa.kind != pb.kind)
			return pa.kind < pb.kind ? -1 : 1;
		if (pa.kind == 1 && pa.commits != pb.commits)
			return pa.commits < pb.commits ? -1 : 1;
		if (pa.kind == -1 && pa.suffix != pb.suffix)
			return pa.suffix < pb.suffix ? -1 : 1;
		return 0;
	}

	std::optional<Release> parseRelease(const std::string& json, std::string* error)
	{
		rapidjson::Document doc;
		doc.Parse(json.c_str());
		if (doc.HasParseError() || !doc.IsObject())
		{
			fail(error, "not a JSON object");
			return std::nullopt;
		}

		auto format = doc.FindMember("format");
		if (format == doc.MemberEnd() || !format->value.IsInt() || format->value.GetInt() != FORMAT)
		{
			fail(error, "unknown format");
			return std::nullopt;
		}

		Release release;
		release.version = str(doc, "version");
		if (!release.version.empty() && release.version[0] == 'v')
			release.version.erase(0, 1);
		release.tag = str(doc, "tag");
		release.date = str(doc, "date");
		release.url = str(doc, "url");

		if (release.version.empty() || !std::isdigit((unsigned char)release.version[0]))
		{
			fail(error, "no version");
			return std::nullopt;
		}

		auto packages = doc.FindMember("packages");
		if (packages != doc.MemberEnd() && packages->value.IsObject())
		{
			static const std::regex sha256("^[0-9a-f]{64}$");
			for (auto& item : packages->value.GetObject())
			{
				if (!item.name.IsString() || !item.value.IsObject())
					continue;

				Package package;
				package.file = str(item.value, "file");
				package.sha256 = str(item.value, "sha256");
				auto size = item.value.FindMember("size");
				if (size != item.value.MemberEnd() && size->value.IsUint64())
					package.size = size->value.GetUint64();

				if (package.file.empty() || package.file.find('/') != std::string::npos || !std::regex_match(package.sha256, sha256))
					continue;

				release.packages[item.name.GetString()] = package;
			}
		}

		return release;
	}
}
