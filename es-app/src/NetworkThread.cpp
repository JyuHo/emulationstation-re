#include "NetworkThread.h"
#include "ApiSystem.h"
#include "SystemConf.h"
#include "guis/GuiMsgBox.h"
#include "LocaleES.h"
#include "Log.h"
#include <chrono>
#include <SDL3/SDL.h>

#include "watchers/BatteryLevelWatcher.h"
#include "watchers/NetworkStateWatcher.h"
#include "RetroAchievements.h"
#include "Settings.h"
#include "updates/SelfUpdater.h"
#include "utils/StringUtil.h"

NetworkThread::NetworkThread(Window* window) : mWindow(window)
{
	WatchersManager* mgr = WatchersManager::getInstance();

	mgr->RegisterComponent(&mCheckCheevosTokenComponent);
	mgr->RegisterComponent(&mCheckSelfUpdateComponent);
	mgr->RegisterComponent(new BatteryLevelWatcher());
	mgr->RegisterComponent(new NetworkStateWatcher());

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::PADSINFO))
		mgr->RegisterComponent(&mCheckPadsBatteryLevelComponent);

	mgr->RegisterNotify(this);

	LOG(LogDebug) << "NetworkThread : Starting";
	InputManager::joystickChanged += this;
}

CheckPadsBatteryLevelComponent::CheckPadsBatteryLevelComponent()
{
	mEnabled = ApiSystem::getInstance()->isScriptingSupported(ApiSystem::PADSINFO);
}

bool CheckPadsBatteryLevelComponent::check()
{
	bool changed = false;

	auto padsInfo = ApiSystem::getInstance()->getPadsInfo();

	if (mPadsInfo.size() != padsInfo.size())
		changed = true;
	else
	{
		for (int i = 0; i < (int)padsInfo.size(); i++)
		{
			if (padsInfo[i].status == "Charging")
				padsInfo[i].battery = -1;

			if (mPadsInfo[i].battery != padsInfo[i].battery || mPadsInfo[i].status != padsInfo[i].status || mPadsInfo[i].id != padsInfo[i].id || mPadsInfo[i].name != padsInfo[i].name || mPadsInfo[i].path != padsInfo[i].path || mPadsInfo[i].device != padsInfo[i].device)
			{
				changed = true;
				break;
			}
		}
	}

	mPadsInfo = padsInfo;
	return changed;
}

bool CheckCheevosTokenComponent::enabled()
{
	return SystemConf::getInstance()->getBool("global.retroachievements");
};

bool CheckCheevosTokenComponent::check()
{
	if (!enabled())
		return false;

	std::string cheevosUsername = SystemConf::getInstance()->get("global.retroachievements.username");
	std::string cheevosPassword = SystemConf::getInstance()->get("global.retroachievements.password");
	
	if (cheevosUsername.empty() || cheevosPassword.empty())
		return false;

	std::string tokenOrError;
	if (RetroAchievements::testAccount(cheevosUsername, cheevosPassword, tokenOrError))
	{
		if (tokenOrError == SystemConf::getInstance()->get("global.retroachievements.token"))
		{
			LOG(LogInfo) << "[CheckCheevosTokenComponent] Cheevos token is unchanged.";
		}
		else
		{
			LOG(LogInfo) << "[CheckCheevosTokenComponent] Generated a new cheevos token, saving.";
			mLastToken = tokenOrError;
			return true;
		}
	}
	else
	{
		LOG(LogError) << "[CheckCheevosTokenComponent] Failed to generate a new cheevos token: " << tokenOrError;		
	}

	return false;
}

bool CheckSelfUpdateComponent::enabled()
{
	return Settings::getInstance()->getBool("CheckUpdatesAtStartup");
}

bool CheckSelfUpdateComponent::check()
{
	if (ApiSystem::getInstance()->getIpAddress() == "NOT CONNECTED")
		return false;

	auto release = SelfUpdater::fetchLatest(); // logs its errors, nothing is shown
	if (!release || release->version.empty() || release->version == mNotifiedVersion)
		return false;

	auto current = ApiSystem::getInstance()->getVersion();
	if (selfupdate::compareVersions(release->version, current) <= 0)
		return false;

	LOG(LogInfo) << "Updates: version " << release->version << " is available (running " << current << ")";
	mNotifiedVersion = release->version;
	return true;
}

void NetworkThread::onJoystickChanged()
{
	WatchersManager::getInstance()->ResetComponent(&mCheckPadsBatteryLevelComponent);
}

void NetworkThread::OnWatcherChanged(IWatcher* component)
{
	if (component == &mCheckPadsBatteryLevelComponent)
	{
		auto pads = mCheckPadsBatteryLevelComponent.getPadsInfo();

		mWindow->postToUiThread([pads]() { for (auto pad : pads) InputManager::getInstance()->updateBatteryLevel(pad.id, pad.device, pad.path, pad.battery); });
		return;
	}

	if (component == &mCheckSelfUpdateComponent)
	{
		auto text = Utils::String::format(_("UPDATE %s AVAILABLE").c_str(), mCheckSelfUpdateComponent.getVersion().c_str());
		mWindow->displayCornerNotificationMessage(_U("\uF019  ") + text, 8000);
		return;
	}

	if (component == &mCheckCheevosTokenComponent)
	{
		auto token = mCheckCheevosTokenComponent.getLastToken();

		mWindow->postToUiThread([token]() 
			{ 
				SystemConf::getInstance()->set("global.retroachievements.token", token);
				SystemConf::getInstance()->saveSystemConf();
			});

		return;
	}
}