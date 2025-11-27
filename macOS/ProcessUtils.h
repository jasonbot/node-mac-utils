#include <string>
#include <vector>

class InstalledApp {
public:
  std::string AppType;
  std::string AppName;
  std::string Id;
  std::string Version;
};

std::vector<std::string> GetRunningProcesses();
std::vector<InstalledApp> ListInstalledApps();
std::vector<std::string> ListRunningAppIds();
std::unique_ptr<InstalledApp> CurrentApp();
