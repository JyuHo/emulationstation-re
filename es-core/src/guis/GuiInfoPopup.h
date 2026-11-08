#pragma once
#ifndef ES_APP_GUIS_GUI_INFO_POPUP_H
#define ES_APP_GUIS_GUI_INFO_POPUP_H

#include "GuiComponent.h"
#include "Window.h"

class ComponentGrid;
class NinePatchComponent;

class GuiInfoPopup : public GuiComponent
{
public:
	enum class Align { CENTER, RIGHT }; // horizontally, at the top of the screen

	GuiInfoPopup(Window* window, std::string message, int duration, Align align = Align::CENTER);

	void render(const Transform4x4f& parentTrans) override;	
	void update(int deltaTime) override;

	float getFadeOut() { return mFadeOut; }
	bool isRunning() { return mRunning; };

	std::string         getMessage() { return mMessage; }
	Align               getAlign() { return mAlign; }

private:
	Align               mAlign;
	std::string         mMessage;

	int                 mStartTime;
	int                 mDuration;
	bool                mRunning;	

	float				mFadeOut;

	ComponentGrid*      mGrid;
	NinePatchComponent* mFrame;
};

#endif // ES_APP_GUIS_GUI_INFO_POPUP_H
