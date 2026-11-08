#pragma once

#include "Window.h"
#include <thread>
#include <condition_variable>
#include <mutex>
#include <list>

#include "InputManager.h"
#include "ApiSystem.h"
#include "watchers/WatchersManager.h"

class CheckPadsBatteryLevelComponent : public IWatcher
{
public:
	CheckPadsBatteryLevelComponent();

	std::vector<PadInfo>& getPadsInfo() { return mPadsInfo; }

protected:
	bool enabled() override { return mEnabled; };
	int  updateTime() override { return 15 * 1000; } // 15 seconds

	bool check() override;

private:
	std::vector<PadInfo> mPadsInfo;
	bool mEnabled;
};

class CheckCheevosTokenComponent : public IWatcher
{
public:
	std::string& getLastToken() { return mLastToken; }

protected:
	bool enabled() override;
	int  updateTime() override { return 120 * 60 * 1000; } // 120 minutes
	int  initialUpdateTime() override { return 100; } // 100ms
	bool check() override;

private:
	std::string mLastToken;
};

// Looks for a newer EmulationStation-RE release (updates/SelfUpdater.h) shortly after startup,
// then once a day. Silent when there is none or the network fails: check() only returns true
// for a release newer than the running version that was not notified yet.
class CheckSelfUpdateComponent : public IWatcher
{
public:
	const std::string& getVersion() { return mNotifiedVersion; }

protected:
	bool enabled() override;
	int  updateTime() override { return 24 * 60 * 60 * 1000; } // 24 hours
	int  initialUpdateTime() override { return 10 * 1000; } // 10 seconds, once the UI is up
	bool check() override;

private:
	std::string mNotifiedVersion;
};

class NetworkThread : public IJoystickChangedEvent, public IWatcherNotify
{
public:
	NetworkThread(Window * window);

public:
	void onJoystickChanged() override;

private:
	void OnWatcherChanged(IWatcher* component) override;

	CheckPadsBatteryLevelComponent					mCheckPadsBatteryLevelComponent;
	CheckCheevosTokenComponent						mCheckCheevosTokenComponent;
	CheckSelfUpdateComponent						mCheckSelfUpdateComponent;
	Window* mWindow;
};


