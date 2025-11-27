#import <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include <libproc.h>
#include <string>
#import <sys/sysctl.h>
#include <vector>

#include "ProcessUtils.h"

std::vector<std::string> GetRunningProcesses() {
  std::vector<std::string> processes;

  static int maxArgumentSize = 0;
  if (maxArgumentSize == 0) {
    size_t size = sizeof(maxArgumentSize);
    if (sysctl((int[]){CTL_KERN, KERN_ARGMAX}, 2, &maxArgumentSize, &size, NULL,
               0) == -1) {
      maxArgumentSize = 4096; // Default
    }
  }
  int mib[3] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL};
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
  char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
  for (int i = 0; i < count; i++) {
    pid_t pid = info[i].kp_proc.p_pid;
    if (pid == 0) {
      continue;
    }

    NSLog(@"I am now here, querying pid %i", pid);

    int ret = proc_pidpath(pid, pathbuf, sizeof(pathbuf));
    if (ret > 0) {
      processes.push_back(std::string(pathbuf));
    }
  }
  free(info);

  return processes;
}

static InstalledApp fromBundle(NSBundle *bundle) {
  InstalledApp installedApp;
  @autoreleasepool {
    NSString *bundleId = nil;

    if (bundle != nil) {
      bundleId = [bundle bundleIdentifier];
    }
    if (bundleId == nil) {
      installedApp.AppType = "invalid";
      return installedApp;
    }

    installedApp.Id = [bundleId UTF8String];

    NSString *displayName =
        [bundle objectForInfoDictionaryKey:@"CFBundleDisplayName"];
    if (displayName == nil) {
      displayName = [bundle objectForInfoDictionaryKey:@"CFBundleName"];
    }
    if (displayName == nil) {
      displayName = [bundle bundlePath];
    }

    installedApp.AppName = [displayName UTF8String];

    NSString *version = [bundle objectForInfoDictionaryKey:@"CFBundleVersion"];
    if (version == nil) {
      version =
          [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
    }
    if (version != nil) {
      installedApp.Version = [version UTF8String];
    }
  }

  return installedApp;
}

std::vector<InstalledApp> ListInstalledApps() {
  std::vector<InstalledApp> apps;

  @autoreleasepool {
    auto defaultManager = [NSFileManager defaultManager];
    NSError *error = nil;
    auto contents = [defaultManager
          contentsOfDirectoryAtURL:[NSURL fileURLWithPath:@"/Applications"]
        includingPropertiesForKeys:[NSArray arrayWithObject:NSURLNameKey]
                           options:NSDirectoryEnumerationSkipsHiddenFiles
                             error:&error];
    if (error == nil) {
      for (NSURL *appPath : contents) {
        auto bundle = [NSBundle bundleWithURL:appPath];
        NSLog(@"Path: %@ Bundle: %@", appPath, bundle);

        auto app(fromBundle(bundle));
        if (app.AppType != "invalid") {
          apps.push_back(app);
        }
      }
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
      auto bundleId = [app bundleIdentifier];
      if (bundleId == nil) {
        continue;
      }
      appIds.push_back([bundleId UTF8String]);
    }
  }

  return appIds;
}

std::unique_ptr<InstalledApp> CurrentApp() {
  @autoreleasepool {
    auto mainBundle = [NSBundle mainBundle];

    if (mainBundle != nullptr) {
      auto bundle(fromBundle(mainBundle));
      if (bundle.AppType != "invalid") {
        return std::unique_ptr<InstalledApp>(new InstalledApp());
      }
    }
  }

  return nullptr;
}
