declare module "node-mac-utils" {
  type ProcessInfo = {
    processName: string;
    processId?: number;
    deviceName: string;
    isActive: boolean;
  };

  type ResultSuccessWithProcess<T> = {
    success: true;
    error: null;
    processes: T[];
  };

  type ResultErrorWithProcess<T> = {
    success: false;
    error: string;
    code: number;
    domain: string;
  };

  export type ResultWithProcesses<T> =
    | ResultSuccessWithProcess<T>
    | ResultErrorWithProcess<T>;

  export function getRunningInputAudioProcesses(): string[];
  export function getProcessesAccessingMicrophoneWithResult(): ResultWithProcesses<string>;
  export function getProcessesAccessingMicrophoneDebouncedWithResult(): ResultWithProcesses<string>;

  // No-op on Mac
  export function getProcessesAccessingSpeakersWithResult(): ResultWithProcesses<ProcessInfo>;

  // Mac-only
  const makeKeyAndOrderFront: (windowID: number) => void | undefined;
  const startMonitoringMic: () => void | undefined;
  const stopMonitoringMic: () => void | undefined;
  export { makeKeyAndOrderFront, startMonitoringMic, stopMonitoringMic };
}
