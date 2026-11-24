#pragma once

#include <functional>
#include <string>
#include <vector>

struct RomMSaveSyncResult
{
	bool success = false;
	int uploaded = 0;
	int downloaded = 0;
	int conflicts = 0;
	std::vector<std::string> conflictNames;
	std::string error;
};

namespace RomMSaveSync
{
	// Sync the saves and states of the RomM library games. Only the files the
	// emulators write under ES's saves path are considered; a game whose save
	// lives in an emulator's own folder is left alone.
	RomMSaveSyncResult sync(const std::function<void(const std::string&)>& setText);
}
