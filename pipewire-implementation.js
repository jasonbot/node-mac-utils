import * as pipewire from "node-pipewire";

pipewire.createPwThread();

export default {
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
};
