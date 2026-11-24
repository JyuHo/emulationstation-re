#pragma once

#include "guis/GuiSettings.h"

class GuiRomM : public GuiSettings
{
public:
	GuiRomM(Window* window);

private:
	void sync();
	void syncNow();
	void syncSaves();
	void clearDownloads();
	void removeLibrary();
};
