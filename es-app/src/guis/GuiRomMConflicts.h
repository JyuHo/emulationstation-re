#pragma once

#include "guis/GuiSettings.h"
#include "romm/RomMSaveSync.h"

#include <vector>

class GuiRomMConflicts : public GuiSettings
{
public:
	explicit GuiRomMConflicts(Window* window);

private:
	void reload();
	void choose(const RomMConflict& conflict);
	void resolve(const RomMConflict& conflict, bool keepLocal);

	std::vector<RomMConflict> mConflicts;
};
