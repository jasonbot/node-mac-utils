declare module "node-mac-utils" {
  export type ProcessInfo = {
    processName: string;
    processId?: number;
    deviceName: string;
    isActive: boolean;
  };

  export type ResultSuccessWithProcess<T> = {
    success: true;
    error: null;
    processes: T[];
  };

  export type ResultErrorWithProcess<T> = {
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

  // Windows-only
  export type InstalledApp = {
    type: string;
    name: string;
    id: string;
    version: string;
  };
  export function getRunningProcesses(): string[];
  export function getRunningAppIDs(): string[];
  export function listInstalledApps(): InstalledApp[];
  export function currentInstalledApp(): InstalledApp | undefined;
  export function installMSIXAndRestart(fileUri: string): void;

  // Mac-only
  export function makeKeyAndOrderFront(windowID: number): void;
  export function startMonitoringMic(): void;
  export function stopMonitoringMic(): void;
}
