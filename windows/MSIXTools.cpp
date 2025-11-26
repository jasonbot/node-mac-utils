#include "MSIXTools.h"

#pragma comment(lib, "RuntimeObject.lib")

#define IGNORE_HRESULT_ERROR(v) try { v } catch (winrt::hresult_error const& e) { }

InstalledApp::InstalledApp(const winrt::Windows::ApplicationModel::Package& app) : AppType("msix") {
	IGNORE_HRESULT_ERROR(this->AppName = winrt::to_string(app.DisplayName()););

	auto id(app.Id());
	IGNORE_HRESULT_ERROR(this->Id = winrt::to_string(id.FullName()););
	auto version(id.Version());
	std::stringstream versionstring;
	versionstring << version.Major << "." << version.Minor << "." << version.Build << "." << version.Revision;
	this->Version = versionstring.str();
}

InstalledApp::InstalledApp(const winrt::Windows::System::Inventory::InstalledDesktopApp& app) : AppType("desktop") {
	this->AppName = winrt::to_string(app.DisplayName());
	this->Id = winrt::to_string(app.Id());
	this->Version = winrt::to_string(app.DisplayVersion());
}

std::unique_ptr<InstalledApp> CurrentApp() {
	try {
		auto cp(winrt::Windows::ApplicationModel::Package::Current());
		std::unique_ptr<InstalledApp> returnVal(new InstalledApp(cp));
		return returnVal;
	}
	catch (winrt::hresult_error const& e) {
	}
	return nullptr;
}

std::vector<InstalledApp> ListInstalledApps() {
	winrt::init_apartment();

	std::vector<InstalledApp> installed_apps;

	try {
		for (const auto package : winrt::Windows::System::Inventory::InstalledDesktopApp::GetInventoryAsync().GetResults()) {
			installed_apps.push_back(InstalledApp(package));
		}
	}
	catch (winrt::hresult_error const& e) {
	}

	try {
		winrt::Windows::Management::Deployment::PackageManager pm;

		for (const auto package : pm.FindPackagesForUser(L"")) {
			installed_apps.push_back(InstalledApp(package));
		}
	}
	catch (winrt::hresult_error const& e) {
	}

	return installed_apps;
}

void InstallMSIXAndRestart(std::string msixPath) {
	winrt::init_apartment();

	try {
		winrt::Windows::Management::Deployment::PackageManager pm;
		winrt::Windows::Foundation::Uri uri(winrt::to_hstring(msixPath));

		RegisterApplicationRestart(nullptr, 0);
		pm.AddPackageAsync(uri, nullptr, winrt::Windows::Management::Deployment::DeploymentOptions::ForceApplicationShutdown).GetResults();
	}
	catch (winrt::hresult_error const& e) {
	}
}