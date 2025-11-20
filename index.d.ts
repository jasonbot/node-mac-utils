declare module "node-mac-utils" {
  export function getRunningInputAudioProcesses(): string[];

  type ProcessInfo = {
    processName: string;
    processId?: number;
    deviceName: string;
    isActive: boolean;
  };

  type ResultSuccessWithProcess<T> = {
    success: true;
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

  export function getProcessesAccessingMicrophoneWithResult(): ResultWithProcesses<string>;
  export function getProcessesAccessingMicrophoneDebouncedWithResult(): ResultWithProcesses<string>;
  export function getProcessesAccessingSpeakersWithResult(): ResultWithProcesses<ProcessInfo>;
}
