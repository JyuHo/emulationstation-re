#pragma once

#include "scrapers/Scraper.h"
#include "utils/TimeUtil.h"

#include <string>
#include <vector>

class RomMScraper : public Scraper
{
public:
	void generateRequests(const ScraperSearchParams& params,
		std::queue<std::unique_ptr<ScraperRequest>>& requests,
		std::vector<ScraperSearchResult>& results) override;

	bool isSupportedPlatform(SystemData* system) override;

	const std::set<ScraperMediaSource>& getSupportedMedias() override;

private:
	struct Platform
	{
		int id = 0;
		std::string slug;
		std::string fsSlug;
		std::string name;
	};

	static std::string authHeader();
	bool loadPlatforms(const std::string& server, const std::string& auth);
	int platformId(const std::string& systemName) const;

	std::vector<Platform> mPlatforms;
	std::string mPlatformsServer;
	Utils::Time::DateTime mPlatformsDate;
};

class RomMRequest : public ScraperHttpRequest
{
public:
	RomMRequest(std::vector<ScraperSearchResult>& resultsWrite, const std::string& url, HttpReqOptions* options,
		const std::string& server, const std::string& systemName, bool filterPlatform)
		: ScraperHttpRequest(resultsWrite, url, options), mServer(server), mSystemName(systemName),
		  mFilterPlatform(filterPlatform)
	{
	}

protected:
	bool process(const std::string& response, std::vector<ScraperSearchResult>& results) override;

private:
	std::string mServer;
	std::string mSystemName;
	bool mFilterPlatform;
};
