let platform_utils;

const noopPlatformUtils = {
  getRunningInputAudioProcesses: () => {
    return [];
  },
  getProcessesAccessingMicrophoneWithResult: () => {
    return {
      success: true,
      error: null,
      processes: [],
    };
  },
  getProcessesAccessingMicrophoneDebouncedWithResult: () => {
    return {
      success: true,
      error: null,
      processes: [],
    };
  },
  getProcessesAccessingSpeakersWithResult: () => {
    return {
      success: true,
      error: null,
      processes: [],
    };
  },
  getRunningProcesses: () => [],
  getRunningAppIDs: () => [],
  listInstalledApps: () => [],
  currentInstalledApp: () => null,
  installMSIXAndRestart: () => {},
};

if (process.platform === "darwin") {
  platform_utils = require("bindings")("mac_utils.node");
} else if (process.platform === "win32") {
  platform_utils = require("bindings")("win_utils.node");
} else if (process.platform === "linux") {
  try {
    platform_utils = require("./pipewire-implementation");
  } catch (e) {
    console.log(`node-mac-utils Failed to import pipewire for Linux: ${e}`);
    platform_utils = noopPlatformUtils;
  }
} else {
  console.log("node-mac-utils Unsupported platform:", process.platform);
  platform_utils = noopPlatformUtils;
}

module.exports = {
  ...noopPlatformUtils,
  // Common exports that work on all platforms
  getRunningInputAudioProcesses: platform_utils.getRunningInputAudioProcesses,
  getProcessesAccessingMicrophoneWithResult:
    platform_utils.getProcessesAccessingMicrophoneWithResult,
  getProcessesAccessingMicrophoneDebouncedWithResult:
    platform_utils.getProcessesAccessingMicrophoneDebouncedWithResult,
  getProcessesAccessingSpeakersWithResult:
    platform_utils.getProcessesAccessingSpeakersWithResult,
  INFO_ERROR_CODE: 1,
  ERROR_DOMAIN: "com.MicrophoneUsageMonitor",

  // Mac-specific exports
  ...(process.platform === "darwin"
    ? {
        makeKeyAndOrderFront: platform_utils.makeKeyAndOrderFront,
        startMonitoringMic: platform_utils.startMonitoringMic,
        stopMonitoringMic: platform_utils.stopMonitoringMic,
      }
    : {}),
  // Windows-specific exports
  ...(process.platform === "win32"
    ? {
        getRunningProcesses: platform_utils.getRunningProcesses,
        getRunningAppIDs: platform_utils.getRunningAppIDs,
        listInstalledApps: platform_utils.listInstalledApps,
        currentInstalledApp: platform_utils.listInstalledApps,
        installMSIXAndRestart: platform_utils.installMSIXAndRestart,
      }
    : {}),
};
