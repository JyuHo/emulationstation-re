#include "guis/GuiRomMConflicts.h"

#include "guis/GuiLoading.h"
#include "guis/GuiMsgBox.h"
#include "LocaleES.h"

#include <string>

GuiRomMConflicts::GuiRomMConflicts(Window* window) : GuiSettings(window, _("ROMM CONFLICTS"))
{
	reload();
}

void GuiRomMConflicts::reload()
{
	clear();

	mConflicts = RomMSaveSync::loadConflicts();

	addGroup(_("CONFLICTS") + " (" + std::to_string(mConflicts.size()) + ")");

	if (mConflicts.empty())
	{
		addEntry(_("No save or state conflict"), false, nullptr);
		return;
	}

	for (const RomMConflict& conflict : mConflicts)
	{
		const std::string label = conflict.game + " (" +
			(conflict.kind == "save" ? _("save") : conflict.suffix) + ")";

		addEntry(label, true, [this, conflict] { choose(conflict); });
	}
}

void GuiRomMConflicts::choose(const RomMConflict& conflict)
{
	Window* window = mWindow;

	std::string message = conflict.game + "\r\n";
	message += (conflict.kind == "save" ? _("Automatic save") : _("State") + " " + conflict.suffix);
	message += "\r\n";
	message += _("Keep the local file and upload it, or keep the server file and download it?");

	window->pushGui(new GuiMsgBox(window, message,
		_("KEEP LOCAL"), [this, conflict] { resolve(conflict, true); },
		_("KEEP SERVER"), [this, conflict] { resolve(conflict, false); },
		_("CANCEL"), nullptr, GuiMsgBoxIcon::ICON_QUESTION));
}

void GuiRomMConflicts::resolve(const RomMConflict& conflict, bool keepLocal)
{
	Window* window = mWindow;

	window->pushGui(new GuiLoading<RomMResolveResult>(window, _("RESOLVING ROMM CONFLICT"),
		[conflict, keepLocal](IGuiLoadingHandler* handler)
		{
			return RomMSaveSync::resolveConflict(conflict, keepLocal,
				[handler](const std::string& text) { handler->setText(text); });
		},
		[this, window](const RomMResolveResult& result)
		{
			if (!result.success)
			{
				window->pushGui(new GuiMsgBox(window, _("ROMM CONFLICT RESOLUTION FAILED") + ":\r\n" + result.error,
					_("OK"), nullptr, GuiMsgBoxIcon::ICON_ERROR));
				return;
			}

			reload();
		}));
}
