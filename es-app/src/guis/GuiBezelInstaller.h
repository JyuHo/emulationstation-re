#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "bezels/TheBezelProject.h"
#include "ContentInstaller.h"

template<typename T>
class OptionListComponent;

class GuiBezelProjectEntry : public ComponentGrid
{
public:
	GuiBezelProjectEntry(Window* window, bezelproject::Entry& entry);

	bool isInstallPending() { return mIsPending; }
	bezelproject::Entry& getEntry() { return mEntry; }
	virtual void setColor(unsigned int color);

private:
	std::shared_ptr<TextComponent>  mImage;
	std::shared_ptr<TextComponent>  mText;
	std::shared_ptr<TextComponent>  mSubstring;

	bezelproject::Entry mEntry;
	bool mIsPending;
};

class GuiBezelInstaller : public GuiComponent, IContentInstalledNotify
{
public:
	GuiBezelInstaller(Window* window);
	~GuiBezelInstaller();

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override;

	void OnContentInstalled(int contentType, std::string contentName, bool success) override;
private:
	void processBezel(bezelproject::Entry name);
	
	void loadBezelsAsync();
	void loadList();

	void centerWindow();

	int			  mReloadList;
	MenuComponent mMenu;	

	std::vector<bezelproject::Entry> mBezels;
};
