const pipewire = require("node-pipewire");

pipewire.createPwThread();

function allInfo() {
  const outputNodes = Object.fromEntries(
    pipewire.getOutputNodes().map((p) => [
      p.id,
      {
        name: p.name,
        type: p.node_type,
        isApp: !!JSON.parse(p.props)["application.name"],
        isDevice: !!JSON.parse(p.props)["node.description"],
      },
    ])
  );

  const inputNodes = Object.fromEntries(
    pipewire.getInputNodes().map((p) => [
      p.id,
      {
        name: p.name,
        type: p.node_type,
        isApp: !!JSON.parse(p.props)["application.name"],
        isDevice: !!JSON.parse(p.props)["node.description"],
      },
    ])
  );

  const links = pipewire.getLinks().map((link) => {
    return {
      input: inputNodes[link.input_node_id],
      output: outputNodes[link.output_node_id],
    };
  });

  return links;
}

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
    1000
  ),
  getProcessesAccessingSpeakersWithResult,
};
