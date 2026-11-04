// Types and constants shared by the server and the dashboard.

export const ZONE_NAMES = [
  "Vamenta st.",
  "Lirio st.",
  "MaxSuniel st.",
  "P.N.Roa st.",
  "Gumamela st.",
  "Villarin st.",
] as const;

export const MAX_LEVEL = 3;

// The values in zone_cam/cam_config.h, used until changed on the website.
export const CAMERA_DEFAULT_EXPOSURE = 120;
export const CAMERA_DEFAULT_GAIN = 8;
export const CAMERA_MAX_EXPOSURE = 1200;
export const CAMERA_MAX_GAIN = 30;

export interface ZoneState {
  readonly name: string;
  readonly level: number;
}

export interface ScanEntry {
  readonly id: number;
  readonly text: string;
  readonly at: number;
  readonly zone: string | null;
}

export interface BoardLink {
  readonly online: boolean;
  readonly lastSeenAt: number | null;
}

export interface ScannerStatus {
  readonly ok: boolean;
  readonly fps: number;
  readonly frameMs: number;
  readonly shapes: number;
  readonly readError: string;
  readonly lastText: string;
  readonly lastAgoSeconds: number;
  readonly fastMemory: boolean;
}

export interface CameraSettings {
  readonly flash: boolean;
  readonly exposure: number | null;
  readonly gain: number | null;
}

export interface DashboardState {
  readonly serverTime: number;
  readonly estop: boolean;
  readonly zones: readonly ZoneState[];
  readonly scans: readonly ScanEntry[];
  readonly controller: BoardLink;
  readonly camera: BoardLink;
  readonly scanner: ScannerStatus | null;
  readonly settings: CameraSettings;
  readonly frameSeq: number;
  readonly frameAt: number | null;
}
