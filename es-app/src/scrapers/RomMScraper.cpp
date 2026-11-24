#include "scrapers/RomMScraper.h"

#include "FileData.h"
#include "Log.h"
#include "Settings.h"
#include "SystemData.h"
#include "scrapers/RomMScraperUrl.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <initializer_list>
#include <sstream>

namespace httplib { namespace detail { extern std::string base64_encode(const std::string& in); } }

namespace
{
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

	const rapidjson::Value* objectValue(const rapidjson::Value& object, const char* key)
	{
		if (!object.IsObject() || !object.HasMember(key) || !object[key].IsObject())
			return nullptr;

		return &object[key];
	}

	std::string joinedList(const rapidjson::Value& object, const char* key)
	{
		if (!object.IsObject() || !object.HasMember(key))
			return "";

		const rapidjson::Value& value = object[key];
		if (value.IsString())
			return value.GetString();

		if (!value.IsArray())
			return "";

		std::string result;
		for (const rapidjson::Value& item : value.GetArray())
		{
			if (!item.IsString() || item.GetString()[0] == 0)
				continue;

			if (!result.empty())
				result += ", ";

			result += item.GetString();
		}

		return result;
	}

	std::string firstListValue(const rapidjson::Value& object, const char* key)
	{
		if (!object.IsObject() || !object.HasMember(key) || !object[key].IsArray())
			return "";

		for (const rapidjson::Value& item : object[key].GetArray())
			if (item.IsString() && item.GetString()[0] != 0)
				return item.GetString();

		return "";
	}

	std::string providerValue(const rapidjson::Value& rom, const char* provider, const char* key)
	{
		const rapidjson::Value* metadata = objectValue(rom, provider);
		return metadata == nullptr ? "" : stringValue(*metadata, key);
	}

	std::string providerMedia(const rapidjson::Value& rom, const char* provider, const char* pathKey, const char* urlKey = nullptr)
	{
		std::string path = providerValue(rom, provider, pathKey);
		if (!path.empty() || urlKey == nullptr)
			return path;

		return providerValue(rom, provider, urlKey);
	}

	double numberValue(const rapidjson::Value& object, const char* key)
	{
		if (!object.IsObject() || !object.HasMember(key) || !object[key].IsNumber())
			return 0.0;

		return object[key].GetDouble();
	}

	std::string coverPath(const rapidjson::Value& rom)
	{
		std::string cover = stringValue(rom, "path_cover_large");
		if (cover.empty())
			cover = stringValue(rom, "path_cover_small");
		if (cover.empty())
			cover = providerMedia(rom, "gamelist_metadata", "box2d_path", "box2d_url");
		if (cover.empty())
			cover = providerValue(rom, "gamelist_metadata", "image_url");
		if (cover.empty())
			cover = providerMedia(rom, "gamelist_metadata", "thumbnail_path", "thumbnail_url");
		if (cover.empty())
			cover = providerMedia(rom, "launchbox_metadata", "box2d_path", "box2d_url");

		return cover;
	}

	std::string firstMedia(const std::initializer_list<std::string>& values)
	{
		for (const std::string& value : values)
			if (!value.empty())
				return value;

		return "";
	}

	std::string screenshotPath(const rapidjson::Value& rom)
	{
		if (rom.IsObject() && rom.HasMember("merged_screenshots") && rom["merged_screenshots"].IsArray())
		{
			for (const rapidjson::Value& item : rom["merged_screenshots"].GetArray())
				if (item.IsString() && item.GetString()[0] != 0)
					return item.GetString();
		}

		return firstMedia({
			providerMedia(rom, "ss_metadata", "screenshot_path", "screenshot_url"),
			providerMedia(rom, "gamelist_metadata", "screenshot_path", "screenshot_url"),
			providerMedia(rom, "launchbox_metadata", "screenshot_path", "screenshot_url"),
		});
	}

	std::string videoPath(const rapidjson::Value& rom)
	{
		std::string video = stringValue(rom, "path_video");
		if (!video.empty())
			return video;

		for (const char* key : { "video_normalized_path", "video_path", "video_normalized_url", "video_url" })
		{
			video = providerValue(rom, "ss_metadata", key);
			if (!video.empty())
				return video;
		}

		return firstMedia({
			providerMedia(rom, "gamelist_metadata", "video_path", "video_url"),
			providerMedia(rom, "launchbox_metadata", "video_path", "video_url"),
		});
	}

	std::string manualPath(const rapidjson::Value& rom)
	{
		std::string manual = stringValue(rom, "path_manual");
		if (!manual.empty())
			return manual;

		manual = stringValue(rom, "url_manual");
		if (!manual.empty())
			return manual;

		return firstMedia({
			providerValue(rom, "ss_metadata", "manual_url"),
			providerValue(rom, "gamelist_metadata", "manual_url"),
		});
	}

	std::string chooseImage(const std::string& source, const std::string& cover, const std::string& screenshot,
		const std::string& title, const std::string& box2d, const std::string& box3d,
		const std::string& mix1, const std::string& mix2, const std::string& fanart)
	{
		if (source == "ss")
			return screenshot.empty() ? cover : screenshot;

		if (source == "sstitle")
			return !title.empty() ? title : (screenshot.empty() ? cover : screenshot);

		if (source == "mixrbv1")
			return !mix1.empty() ? mix1 : (box2d.empty() ? cover : box2d);

		if (source == "mixrbv2")
			return !mix2.empty() ? mix2 : (!mix1.empty() ? mix1 : (box2d.empty() ? cover : box2d));

		if (source == "box-2D")
			return box2d.empty() ? cover : box2d;

		if (source == "box-3D")
			return !box3d.empty() ? box3d : (box2d.empty() ? cover : box2d);

		if (source == "fanart")
			return fanart.empty() ? cover : fanart;

		if (source.empty())
			return "";

		return cover;
	}

	void addMedia(ScraperSearchResult& result, const std::string& server, MetaDataId id, const std::string& pathOrUrl)
	{
		const std::string url = RomMScraperUrl::resourceUrl(server, pathOrUrl);
		if (url.empty())
			return;

		result.urls[id] = ScraperSearchItem(url, RomMScraperUrl::extensionOf(url));
	}
}

void RomMScraper::generateRequests(const ScraperSearchParams& params,
	std::queue<std::unique_ptr<ScraperRequest>>& requests,
	std::vector<ScraperSearchResult>& results)
{
	const std::string server = RomMScraperUrl::normalizeServer(Settings::getInstance()->getString("RomMServer"));
	if (server.empty())
	{
		LOG(LogWarning) << "RomM: no server URL configured";
		return;
	}

	const std::string systemName = params.system == nullptr ? "" : params.system->getName();
	const std::string auth = authHeader();

	loadPlatforms(server, auth);
	const int platform = platformId(systemName);
	const bool filterPlatform = platform == 0;

	std::string name = params.nameOverride;
	if (name.empty() && params.game != nullptr)
		name = Utils::String::removeParenthesis(Utils::FileSystem::getStem(params.game->getPath()));
	name = Utils::String::trim(name);

	if (name.empty())
		return;

	std::string url = server + "/api/roms?search_term=" + HttpReq::urlEncode(name) +
		"&limit=25&with_provider_metadata=true";
	if (platform > 0)
		url += "&platform_ids=" + std::to_string(platform);

	HttpReqOptions options;
	options.userAgent = "EmulationStation-RE";
	if (!auth.empty())
		options.customHeaders.push_back("Authorization: " + auth);

	requests.push(std::unique_ptr<ScraperRequest>(new RomMRequest(results, url, &options, server, systemName, filterPlatform)));
}

bool RomMScraper::isSupportedPlatform(SystemData* system)
{
	return system != nullptr && !system->getName().empty();
}

const std::set<Scraper::ScraperMediaSource>& RomMScraper::getSupportedMedias()
{
	static const std::set<ScraperMediaSource> medias =
	{
		ScraperMediaSource::Screenshot,
		ScraperMediaSource::Box2d,
		ScraperMediaSource::Box3d,
		ScraperMediaSource::Mix,
		ScraperMediaSource::Wheel,
		ScraperMediaSource::Marquee,
		ScraperMediaSource::FanArt,
		ScraperMediaSource::TitleShot,
		ScraperMediaSource::Video,
		ScraperMediaSource::Manual,
		ScraperMediaSource::BoxBack,
		ScraperMediaSource::Bezel_16_9,
	};

	return medias;
}

std::string RomMScraper::authHeader()
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

bool RomMScraper::loadPlatforms(const std::string& server, const std::string& auth)
{
	if (mPlatformsServer != server)
	{
		mPlatforms.clear();
		mPlatformsServer.clear();
		mPlatformsDate = Utils::Time::DateTime();
	}

	if (!mPlatforms.empty() && mPlatformsServer == server && mPlatformsDate.isValid() &&
		Utils::Time::DateTime::now().elapsedSecondsSince(mPlatformsDate) < 3600)
		return true;

	HttpReqOptions options;
	options.userAgent = "EmulationStation-RE";
	if (!auth.empty())
		options.customHeaders.push_back("Authorization: " + auth);

	HttpReq request(server + "/api/platforms", &options);
	if (!request.wait())
	{
		LOG(LogWarning) << "RomM: cannot read the platforms from " << server << ": " << request.getErrorMsg();
		return false;
	}

	rapidjson::Document doc;
	doc.Parse(request.getContent().c_str());

	if (doc.HasParseError() || !doc.IsArray())
	{
		LOG(LogWarning) << "RomM: invalid platforms response from " << server;
		return false;
	}

	mPlatforms.clear();
	for (const rapidjson::Value& item : doc.GetArray())
	{
		if (!item.IsObject())
			continue;

		Platform platform;
		platform.id = intValue(item, "id");
		platform.slug = stringValue(item, "slug");
		platform.fsSlug = stringValue(item, "fs_slug");
		platform.name = stringValue(item, "name");

		if (platform.id > 0)
			mPlatforms.push_back(platform);
	}

	mPlatformsServer = server;
	mPlatformsDate = Utils::Time::DateTime::now();
	return true;
}

int RomMScraper::platformId(const std::string& systemName) const
{
	for (const Platform& platform : mPlatforms)
		if (RomMScraperUrl::platformMatches(systemName, platform.slug, platform.fsSlug))
			return platform.id;

	return 0;
}

bool RomMRequest::process(const std::string& response, std::vector<ScraperSearchResult>& results)
{
	rapidjson::Document doc;
	doc.Parse(response.c_str());

	if (doc.HasParseError())
	{
		const std::string error = std::string("RomM: invalid JSON response: ") + rapidjson::GetParseError_En(doc.GetParseError());
		LOG(LogWarning) << error;
		setError(error);
		return true;
	}

	if (!doc.IsObject() || !doc.HasMember("items") || !doc["items"].IsArray())
	{
		LOG(LogWarning) << "RomM: response has no items";
		return true;
	}

	for (const rapidjson::Value& rom : doc["items"].GetArray())
	{
		if (!rom.IsObject())
			continue;

		if (mFilterPlatform && !RomMScraperUrl::platformMatches(mSystemName,
			stringValue(rom, "platform_slug"), stringValue(rom, "platform_fs_slug")))
			continue;

		std::string name = stringValue(rom, "name");
		if (name.empty())
			name = stringValue(rom, "fs_name_no_ext");
		if (name.empty())
			continue;

		ScraperSearchResult result("RomM");

		result.mdl.set(MetaDataId::Name, name);
		result.mdl.set(MetaDataId::Desc, stringValue(rom, "summary"));

		const rapidjson::Value* metadatum = objectValue(rom, "metadatum");
		const rapidjson::Value* ss = objectValue(rom, "ss_metadata");

		if (metadatum != nullptr)
		{
			result.mdl.set(MetaDataId::Developer, joinedList(*metadatum, "developers"));
			result.mdl.set(MetaDataId::Publisher, joinedList(*metadatum, "publishers"));
			result.mdl.set(MetaDataId::Genre, joinedList(*metadatum, "genres"));
			result.mdl.set(MetaDataId::Family, firstListValue(*metadatum, "franchises"));
			result.mdl.set(MetaDataId::Players, stringValue(*metadatum, "player_count"));
		}

		if (result.mdl.get(MetaDataId::Developer).empty() && ss != nullptr)
			result.mdl.set(MetaDataId::Developer, joinedList(*ss, "developers"));

		if (result.mdl.get(MetaDataId::Publisher).empty() && ss != nullptr)
			result.mdl.set(MetaDataId::Publisher, joinedList(*ss, "publishers"));

		if (result.mdl.get(MetaDataId::Genre).empty() && ss != nullptr)
			result.mdl.set(MetaDataId::Genre, joinedList(*ss, "genres"));

		if (result.mdl.get(MetaDataId::Players).empty() && ss != nullptr)
			result.mdl.set(MetaDataId::Players, stringValue(*ss, "player_count"));

		double timestamp = metadatum == nullptr ? 0.0 : numberValue(*metadatum, "first_release_date");
		if (timestamp <= 0.0 && ss != nullptr)
			timestamp = numberValue(*ss, "first_release_date");

		if (timestamp > 0.0)
			result.mdl.set(MetaDataId::ReleaseDate, Utils::Time::DateTime(static_cast<time_t>(timestamp / 1000.0)));

		result.mdl.set(MetaDataId::Rating, "-1");
		if (Settings::getInstance()->getBool("ScrapeRatings"))
		{
			const double rating = metadatum == nullptr ? 0.0 : numberValue(*metadatum, "average_rating");
			if (rating > 0.0)
			{
				std::ostringstream value;
				value << (rating / 100.0);
				result.mdl.set(MetaDataId::Rating, value.str());
			}
		}

		result.mdl.set(MetaDataId::Region, joinedList(rom, "regions"));
		result.mdl.set(MetaDataId::Language, joinedList(rom, "languages"));

		const std::string cover = coverPath(rom);
		const std::string screenshot = screenshotPath(rom);
		const std::string title = firstMedia({
			providerMedia(rom, "ss_metadata", "title_screen_path", "title_screen_url"),
			providerMedia(rom, "gamelist_metadata", "title_screen_path", "title_screen_url"),
			providerMedia(rom, "launchbox_metadata", "title_screen_path", "title_screen_url"),
		});
		const std::string box2d = firstMedia({
			providerMedia(rom, "ss_metadata", "box2d_path", "box2d_url"),
			providerMedia(rom, "gamelist_metadata", "box2d_path", "box2d_url"),
			providerValue(rom, "gamelist_metadata", "image_url"),
			providerMedia(rom, "launchbox_metadata", "box2d_path", "box2d_url"),
		});
		const std::string box3d = firstMedia({
			providerMedia(rom, "ss_metadata", "box3d_path", "box3d_url"),
			providerMedia(rom, "gamelist_metadata", "box3d_path", "box3d_url"),
			providerMedia(rom, "launchbox_metadata", "box3d_path", "box3d_url"),
		});
		const std::string mix1 = firstMedia({
			providerMedia(rom, "ss_metadata", "miximage_path", "miximage_url"),
			providerMedia(rom, "gamelist_metadata", "miximage_path", "miximage_url"),
		});
		const std::string mix2 = firstMedia({
			providerMedia(rom, "ss_metadata", "miximage_v2_path", "miximage_v2_url"),
			providerMedia(rom, "gamelist_metadata", "miximage_v2_path", "miximage_v2_url"),
		});
		const std::string fanart = firstMedia({
			providerMedia(rom, "ss_metadata", "fanart_path", "fanart_url"),
			providerMedia(rom, "gamelist_metadata", "fanart_path", "fanart_url"),
		});
		const std::string logo = providerMedia(rom, "ss_metadata", "logo_path", "logo_url");
		const std::string marquee = firstMedia({
			providerMedia(rom, "ss_metadata", "marquee_path", "marquee_url"),
			providerMedia(rom, "gamelist_metadata", "marquee_path", "marquee_url"),
		});

		const std::string imageSource = Settings::getInstance()->getString("ScrapperImageSrc");
		const std::string image = chooseImage(imageSource, cover, screenshot, title,
			box2d, box3d, mix1, mix2, fanart);
		addMedia(result, mServer, MetaDataId::Image, image);

		const std::string thumbSource = Settings::getInstance()->getString("ScrapperThumbSrc");
		if (!thumbSource.empty() && thumbSource != imageSource)
			addMedia(result, mServer, MetaDataId::Thumbnail,
				thumbSource == "box-3D" ? box3d : (thumbSource == "box-2D" ? box2d : ""));

		const std::string logoSource = Settings::getInstance()->getString("ScrapperLogoSrc");
		if (!logoSource.empty())
			addMedia(result, mServer, MetaDataId::Marquee,
				logoSource == "marquee" ? (marquee.empty() ? logo : marquee) : logo);

		if (Settings::getInstance()->getBool("ScrapeTitleShot"))
			addMedia(result, mServer, MetaDataId::TitleShot, title);

		if (Settings::getInstance()->getBool("ScrapeVideos"))
			addMedia(result, mServer, MetaDataId::Video, videoPath(rom));

		if (Settings::getInstance()->getBool("ScrapeManual"))
			addMedia(result, mServer, MetaDataId::Manual, manualPath(rom));

		if (Settings::getInstance()->getBool("ScrapeFanart"))
			addMedia(result, mServer, MetaDataId::FanArt, fanart);

		if (Settings::getInstance()->getBool("ScrapeBoxBack"))
			addMedia(result, mServer, MetaDataId::BoxBack, firstMedia({
				providerMedia(rom, "ss_metadata", "box2d_back_path", "box2d_back_url"),
				providerMedia(rom, "gamelist_metadata", "box2d_back_path", "box2d_back_url"),
				providerMedia(rom, "launchbox_metadata", "box2d_back_path", "box2d_back_url"),
			}));

		if (Settings::getInstance()->getBool("ScrapeBezel"))
			addMedia(result, mServer, MetaDataId::Bezel,
				providerMedia(rom, "ss_metadata", "bezel_path", "bezel_url"));

		results.push_back(result);
	}

	return true;
}
