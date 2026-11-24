#include "guis/GuiRomM.h"

#include "guis/GuiLoading.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiRomMConflicts.h"
#include "romm/RomMLibrary.h"
#include "romm/RomMSaveSync.h"
#include "views/ViewController.h"
#include "LocaleES.h"

#include <string>

GuiRomM::GuiRomM(Window* window) : GuiSettings(window, _("ROMM"))
{
	addGroup(_("SERVER"));
	addInputTextConfigRow(_("SERVER URL"), "RomMServer", false, true);
	addInputTextConfigRow(_("USERNAME"), "RomMUser", false, true);
	addInputTextConfigRow(_("PASSWORD"), "RomMPassword", true, true);
	addInputTextConfigRow(_("API TOKEN"), "RomMToken", true, true);

	addGroup(_("SAVES"));
	addSwitch(_("SYNC AFTER EACH GAME"), "RomMAutoSyncSaves", true);

	addGroup(_("LIBRARY"));
	addEntry(_("SYNC LIBRARY"), true, [this] { sync(); });
	addEntry(_("SYNC SAVES & STATES"), true, [this] { syncSaves(); });
	addEntry(_("RESOLVE CONFLICTS"), true, [this] { mWindow->pushGui(new GuiRomMConflicts(mWindow)); });
	addEntry(_("CLEAR DOWNLOADED GAMES"), true, [this] { clearDownloads(); });
	addEntry(_("REMOVE ROMM LIBRARY"), true, [this] { removeLibrary(); });
}

void GuiRomM::sync()
{
	Window* window = mWindow;

	window->pushGui(new GuiMsgBox(window,
		_("DOWNLOAD THE ROMM LIBRARY AND ITS IMAGES NOW?") + "\r\n" + RomMLibrary::libraryRoot(),
		_("START"), [this] { syncNow(); },
		_("CANCEL"), nullptr));
}

void GuiRomM::syncNow()
{
	Window* window = mWindow;

	window->pushGui(new GuiLoading<RomMSyncResult>(window, _("SYNCING ROMM LIBRARY"),
		[](IGuiLoadingHandler* handler)
		{
			return RomMLibrary::sync([handler](const std::string& text) { handler->setText(text); });
		},
		[window](RomMSyncResult result)
		{
			if (!result.success)
			{
				window->pushGui(new GuiMsgBox(window, _("ROMM SYNC FAILED") + ":\r\n" + result.error,
					_("OK"), nullptr, GuiMsgBoxIcon::ICON_ERROR));
				return;
			}

			const std::string message = "RomM: " + std::to_string(result.platforms) + " platforms, " +
				std::to_string(result.games) + " games, " + std::to_string(result.media) + " media.";

			window->pushGui(new GuiMsgBox(window, message, _("OK"), [window]
			{
				window->postToUiThread([window]
				{
					if (ViewController::get() != nullptr)
						ViewController::get()->ReloadAndGoToStart();
				});
			}, GuiMsgBoxIcon::ICON_INFORMATION));
		}));
}

void GuiRomM::syncSaves()
{
	Window* window = mWindow;

	window->pushGui(new GuiMsgBox(window, _("SYNC SAVES AND STATES WITH ROMM?"),
		_("START"), [this]
		{
			Window* w = mWindow;

			w->pushGui(new GuiLoading<RomMSaveSyncResult>(w, _("SYNCING ROMM SAVES"),
				[](IGuiLoadingHandler* handler)
				{
					return RomMSaveSync::sync([handler](const std::string& text) { handler->setText(text); });
				},
				[w](RomMSaveSyncResult result)
				{
					if (!result.success)
					{
						w->pushGui(new GuiMsgBox(w, _("ROMM SAVE SYNC FAILED") + ":\r\n" + result.error,
							_("OK"), nullptr, GuiMsgBoxIcon::ICON_ERROR));
						return;
					}

					std::string message = std::to_string(result.uploaded) + " " + _("uploaded") + ", " +
						std::to_string(result.downloaded) + " " + _("downloaded") + ".";

					if (result.conflicts > 0)
					{
						message += "\r\n" + std::to_string(result.conflicts) + " " + _("conflicts kept on both sides:") + "\r\n";
						for (size_t i = 0; i < result.conflictNames.size() && i < 8; ++i)
							message += " - " + result.conflictNames[i] + "\r\n";

						message += "\r\n" + _("Use RESOLVE CONFLICTS to keep the local or the server file.");
					}

					w->pushGui(new GuiMsgBox(w, message, _("OK"), nullptr, GuiMsgBoxIcon::ICON_INFORMATION));
				}));
		},
		_("CANCEL"), nullptr));
}

void GuiRomM::clearDownloads()
{	Window* window = mWindow;

	window->pushGui(new GuiMsgBox(window, _("EMPTY EVERY DOWNLOADED ROMM GAME?") + "\r\n" +
		_("The games stay in the library and are downloaded again when launched."),
		_("YES"), [window]
		{
			const int cleared = RomMLibrary::clearDownloads();
			window->pushGui(new GuiMsgBox(window, std::to_string(cleared) + " " + _("games cleared."), _("OK"), nullptr));
		},
		_("NO"), nullptr, GuiMsgBoxIcon::ICON_QUESTION));
}

void GuiRomM::removeLibrary()
{
	Window* window = mWindow;

	window->pushGui(new GuiMsgBox(window, _("REMOVE THE ROMM LIBRARY AND ITS SYSTEMS?") + "\r\n" +
		_("The next sync downloads the library again."),
		_("YES"), [window]
		{
			RomMLibrary::removeLibrary();
			window->postToUiThread([window]
			{
				if (ViewController::get() != nullptr)
					ViewController::get()->ReloadAndGoToStart();
			});
		},
		_("NO"), nullptr, GuiMsgBoxIcon::ICON_WARNING));
}
