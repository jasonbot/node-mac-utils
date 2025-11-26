#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <cstdint>
#include <winbase.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.System.Diagnostics.h>
#include <winrt/Windows.System.Inventory.h>
#include <winrt/Windows.Management.Deployment.Preview.h>


class InstalledApp {
public:
	std::string AppType;
	std::string AppName;
	std::string Id;
	std::string Version;

	InstalledApp(const winrt::Windows::ApplicationModel::Package&);
	InstalledApp(const winrt::Windows::System::Inventory::InstalledDesktopApp&);
};

std::vector<InstalledApp> ListInstalledApps();
std::vector<std::string> ListRunningAppIds();
void InstallMSIXAndRestart(std::string);
std::unique_ptr<InstalledApp> CurrentApp();