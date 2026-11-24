#include "romm/RomMClient.h"

#include "HttpReq.h"
#include "Log.h"
#include "Settings.h"
#include "scrapers/RomMScraperUrl.h"

#include <chrono>
#include <thread>

namespace httplib { namespace detail { extern std::string base64_encode(const std::string& in); } }

namespace
{
	HttpReqOptions requestOptions()
	{
		HttpReqOptions options;
		options.userAgent = "EmulationStation-RE";
		options.useCookieManager = false;

		const std::string auth = RomMClient::authHeader();
		if (!auth.empty())
			options.customHeaders.push_back("Authorization: " + auth);

		return options;
	}

	std::string httpError(HttpReq& request)
	{
		std::string error = request.getErrorMsg();
		if (error.empty())
			error = "HTTP " + std::to_string(static_cast<int>(request.status()));
		return error;
	}
}

namespace RomMClient
{
	std::string authHeader()
	{
		const std::string token = Settings::getInstance()->getString("RomMToken");
		if (!token.empty())
			return "Bearer " + token;

		const std::string user = Settings::getInstance()->getString("RomMUser");
		const std::string password = Settings::getInstance()->getString("RomMPassword");
		if (user.empty() || password.empty())
			return "";

		return "Basic " + httplib::detail::base64_encode(user + ":" + password);
	}

	bool getJson(const std::string& server, const std::string& path, rapidjson::Document& document, std::string& error)
	{
		HttpReqOptions options = requestOptions();
		HttpReq request(server + path, &options);

		if (!request.wait())
		{
			error = httpError(request);
			return false;
		}

		document.Parse(request.getContent().c_str());
		if (document.HasParseError())
		{
			error = "Invalid JSON from " + server + path;
			return false;
		}

		return true;
	}

	bool download(const std::string& url, const std::string& destination,
		const std::function<void(int)>& progress, std::string& error)
	{
		HttpReqOptions options = requestOptions();
		options.outputFilename = destination;

		HttpReq request(url, &options);
		HttpReq::Status status = request.status();

		while (status == HttpReq::REQ_IN_PROGRESS)
		{
			if (progress)
				progress(request.getPercent());

			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			status = request.status();
		}

		if (status != HttpReq::REQ_SUCCESS)
		{
			error = httpError(request);
			return false;
		}

		if (progress)
			progress(100);

		return true;
	}

	bool downloadMedia(const std::string& server, const std::string& pathOrUrl,
		const std::string& destination, std::string& error)
	{
		const std::string url = RomMScraperUrl::resourceUrl(server, pathOrUrl);
		if (url.empty())
		{
			error = "Unsupported media URL";
			return false;
		}

		return download(url, destination, nullptr, error);
	}
}
