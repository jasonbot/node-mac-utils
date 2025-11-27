#import <sys/sysctl.h>
#import <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include <string>
#include <vector>
#include <libproc.h>

#include "ProcessUtils.h"

extern CFStringRef const kCFBundleNameKey;
extern CFStringRef const kCFBundleVersionKey;

std::vector<std::string> GetRunningProcesses() {
    std::vector<std::string> processes;

    static int maxArgumentSize = 0;
	if (maxArgumentSize == 0) {
		size_t size = sizeof(maxArgumentSize);
		if (sysctl((int[]){ CTL_KERN, KERN_ARGMAX }, 2, &maxArgumentSize, &size, NULL, 0) == -1) {
			perror("sysctl argument size");
			maxArgumentSize = 4096; // Default
		}
	}
	int mib[3] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL};
	struct kinfo_proc *info;
	size_t length;
	int count;

	if (sysctl(mib, 3, NULL, &length, NULL, 0) < 0) {
		return processes;
	}

	if (!(info = (kinfo_proc *)malloc(length))) {
		return processes;
	}

	if (sysctl(mib, 3, info, &length, NULL, 0) < 0) {
		free(info);
		return processes;
	}

	count = length / sizeof(struct kinfo_proc);
	for (int i = 0; i < count; i++) {
		pid_t pid = info[i].kp_proc.p_pid;
		if (pid == 0) {
			continue;
		}

        char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
        int ret = proc_pidpath(pid, pathbuf, sizeof(pathbuf));
        if (ret > 0) {
            processes.push_back(pathbuf);
        }
	}
	free(info);

	return processes;
}

static InstalledApp fromBundle(NSBundle *bundle) {
  InstalledApp installedApp;
  @autoreleasepool {
    installedApp.AppType = "application";
    installedApp.AppName =
        [[bundle objectForInfoDictionaryKey:(id)kCFBundleNameKey] UTF8String];
    installedApp.Id = [[bundle bundleIdentifier] UTF8String];
    installedApp.Version = [[bundle
        objectForInfoDictionaryKey:(id)kCFBundleVersionKey] UTF8String];
  }

  return installedApp;
}

std::vector<InstalledApp> ListInstalledApps() {
  std::vector<InstalledApp> apps;

  @autoreleasepool {
    auto defaultManager = [NSFileManager defaultManager];

    auto directoryURL =
        [NSURL fileURLWithPath:@"/Applications"]; // Replace with your actual
                                                  // directory path
    NSError *error = nil;
    auto contents = [defaultManager
          contentsOfDirectoryAtURL:directoryURL
        includingPropertiesForKeys:nil
                           options:NSDirectoryEnumerationSkipsHiddenFiles
                             error:&error];

    for (const id appPath : contents) {
      auto bundle = [NSBundle bundleWithPath:appPath];

      InstalledApp installedApp;
      installedApp.AppType = "application";
      installedApp.AppName =
          [[bundle objectForInfoDictionaryKey:(id)kCFBundleNameKey] UTF8String];
      installedApp.Id = [[bundle bundleIdentifier] UTF8String];
      installedApp.Version = [[bundle
          objectForInfoDictionaryKey:(id)kCFBundleVersionKey] UTF8String];
    }
  }

  return apps;
}

std::vector<std::string> ListRunningAppIds() {
  std::vector<std::string> appIds;

  @autoreleasepool {
    auto workspace = [NSWorkspace sharedWorkspace];
    auto apps = [workspace runningApplications];
    for (const id app : apps) {
      auto bundle = [app bundle];

      if (bundle != nullptr) {
        appIds.push_back(std::string([[bundle
            objectForInfoDictionaryKey:(id)kCFBundleNameKey] UTF8String]));
      }
    }
  }

  return appIds;
}

std::unique_ptr<InstalledApp> CurrentApp() {
  @autoreleasepool {
    auto mainBundle = [NSBundle mainBundle];

    if (mainBundle != nullptr) {
      return std::unique_ptr<InstalledApp>(
          new InstalledApp(fromBundle(mainBundle)));
    }
  }

  return nullptr;
}
