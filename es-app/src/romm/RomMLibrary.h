#pragma once

#include <functional>
#include <string>

struct RomMSyncResult
{
	bool success = false;
	int platforms = 0;
	int games = 0;
	int media = 0;
	std::string error;
};

namespace RomMLibrary
{
	// The generated ES-RE library: <data>/romm/library/<romm-system>.
	std::string libraryRoot();

	// The generated es_systems_romm.cfg, loaded by SystemData::loadAdditionalConfig.
	std::string systemConfigPath();

	// The sidecar that says where a stub game comes from on the RomM server.
	std::string sidecarPath(const std::string& gamePath);

	// Whether a game is still a stub (its sidecar exists and the file is empty).
	bool needsDownload(const std::string& gamePath);

	// Download a stub game from its sidecar, replacing the empty stub in place.
	bool downloadGame(const std::string& gamePath, const std::function<void(const std::string&)>& setText, std::string& error);

	// Read the RomM library, write the stub files, gamelists, media and system
	// config. Runs on a background thread; setText updates the loading screen.
	RomMSyncResult sync(const std::function<void(const std::string&)>& setText);

	// Empty every downloaded ROM, keeping its stub and sidecar. Returns the count.
	int clearDownloads();

	// Remove the generated library and system config.
	bool removeLibrary();

	bool isGeneratedSystem(const std::string& systemName);
}
