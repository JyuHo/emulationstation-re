#pragma once

#include <cstdint>
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

struct RomMConflict
{
	std::string id;
	int romId = 0;
	std::string game;
	std::string baseSystem;
	std::string kind;       // "save" or "state"
	std::string suffix;     // "" for a save, ".state"/".state1" for a state
	std::string localPath;
	std::string localHash;
	int64_t localMtime = 0;
	int serverId = 0;
	int64_t serverUpdated = 0;
	std::string serverHash;
	std::string serverExtension;
	std::string serverFileName;
	std::string serverEmulator;
};

struct RomMResolveResult
{
	bool success = false;
	std::string error;
};

namespace RomMSaveSync
{
	// Sync the saves and states of the RomM library games. Only the files the
	// emulators write under ES's saves path are considered; a game whose save
	// lives in an emulator's own folder is left alone.
	RomMSaveSyncResult sync(const std::function<void(const std::string&)>& setText);

	// Sync only the saves and states of one library game, identified by the path
	// ES launched (the stub file or the top-level .m3u). Used after a game exits.
	RomMSaveSyncResult syncGame(const std::string& gamePath,
		const std::function<void(const std::string&)>& setText);

	// The conflicts the last sync kept on both sides, and their resolution.
	// Resolving keeps the chosen side and updates the baseline, so the next
	// sync sees both sides as equal.
	std::vector<RomMConflict> loadConflicts();
	RomMResolveResult resolveConflict(const RomMConflict& conflict, bool keepLocal,
		const std::function<void(const std::string&)>& setText);
}
