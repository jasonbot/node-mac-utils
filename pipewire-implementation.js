const pipewire = require("node-pipewire");

pipewire.createPwThread();

function debouncedResult(fn, delay) {
  let lastUpdate = 0;
  let lastResult = undefined;

  return function () {
    const now = performance.now();
    if (lastUpdate === 0 || now > lastUpdate + delay) {
      lastResult = fn();
      lastUpdate = now;
    }

    return lastResult;
  };
}

function getRunningInputAudioProcesses() {
  return [];
}

function getProcessesAccessingMicrophoneWithResult() {
  return {
    success: true,
    error: null,
    processes: [],
  };
}

function getProcessesAccessingSpeakersWithResult() {
  return {
    success: true,
    error: null,
    processes: [],
  };
}

module.exports = {
  getRunningInputAudioProcesses,
  getProcessesAccessingMicrophoneWithResult,
  getProcessesAccessingMicrophoneDebouncedWithResult: debouncedResult(
    getProcessesAccessingMicrophoneWithResult,
    1000,
  ),
  getProcessesAccessingSpeakersWithResult,
};
