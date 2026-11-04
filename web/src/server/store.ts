import { mkdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { dirname } from "node:path";
import {
  CAMERA_MAX_EXPOSURE,
  CAMERA_MAX_GAIN,
  MAX_LEVEL,
  ZONE_NAMES,
} from "../shared/types.ts";
import type {
  CameraSettings,
  DashboardState,
  ScanEntry,
  ScannerStatus,
  ZoneState,
} from "../shared/types.ts";

const ONLINE_WINDOW_MS = 3000;
const MAX_SCANS = 50;

interface SavedState {
  readonly estop: boolean;
  readonly levels: readonly number[];
  readonly settings: CameraSettings;
}

export function isLevel(value: unknown): value is number {
  return Number.isInteger(value) && (value as number) >= 0 && (value as number) <= MAX_LEVEL;
}

function isOptionalInteger(value: unknown, max: number): value is number | null {
  return value === null || (Number.isInteger(value) && (value as number) >= 0 && (value as number) <= max);
}

function parseSaved(raw: string): SavedState | null {
  const data: unknown = JSON.parse(raw);
  if (typeof data !== "object" || data === null) {
    return null;
  }
  const { estop, levels, settings } = data as Partial<Record<keyof SavedState, unknown>>;
  if (typeof estop !== "boolean" || !Array.isArray(levels) || levels.length !== ZONE_NAMES.length || !levels.every(isLevel)) {
    return null;
  }
  if (typeof settings !== "object" || settings === null) {
    return null;
  }
  const { flash, exposure, gain } = settings as Partial<Record<keyof CameraSettings, unknown>>;
  if (
    typeof flash !== "boolean" ||
    !isOptionalInteger(exposure, CAMERA_MAX_EXPOSURE) ||
    !isOptionalInteger(gain, CAMERA_MAX_GAIN)
  ) {
    return null;
  }
  return { estop, levels, settings: { flash, exposure, gain } };
}

// Holds everything the website knows. Levels, E-stop and camera settings are
// saved to disk, so restarting the website cannot silently release an E-stop.
export class Store {
  private readonly statePath: string;
  private readonly now: () => number;
  private readonly listeners = new Set<() => void>();

  private estop = false;
  private levels: number[] = ZONE_NAMES.map(() => 0);
  private settings: CameraSettings = { flash: true, exposure: null, gain: null };

  private scans: ScanEntry[] = [];
  private nextScanId = 1;
  private pendingCommands = new Set<string>();
  private levelsKnown = false;
  private controllerSeenAt: number | null = null;
  private cameraSeenAt: number | null = null;
  private scanner: ScannerStatus | null = null;
  private frame: Buffer | null = null;
  private frameSeq = 0;
  private frameAt: number | null = null;

  constructor(statePath: string, now: () => number = Date.now) {
    this.statePath = statePath;
    this.now = now;
    this.load();
  }

  private load(): void {
    let raw: string;
    try {
      raw = readFileSync(this.statePath, "utf8");
    } catch {
      return; // First run.
    }
    try {
      const saved = parseSaved(raw);
      if (saved === null) {
        throw new Error("unexpected contents");
      }
      this.estop = saved.estop;
      this.levels = [...saved.levels];
      this.settings = saved.settings;
    } catch (error) {
      // The last stop request is unknown, so fail safe.
      this.estop = true;
      console.error(`Could not read ${this.statePath}, starting with E-stop ON:`, error);
    }
  }

  private save(): void {
    const saved: SavedState = { estop: this.estop, levels: this.levels, settings: this.settings };
    try {
      mkdirSync(dirname(this.statePath), { recursive: true });
      const temporary = `${this.statePath}.tmp`;
      writeFileSync(temporary, JSON.stringify(saved));
      renameSync(temporary, this.statePath);
    } catch (error) {
      console.error("Could not save state:", error);
    }
  }

  private changed(): void {
    for (const listener of this.listeners) {
      listener();
    }
  }

  subscribe(listener: () => void): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  setLevel(zone: string, level: number): boolean {
    const index = ZONE_NAMES.findIndex((name) => name === zone);
    if (index < 0) {
      return false;
    }
    this.levelsKnown = true;
    if (this.levels[index] !== level) {
      this.levels[index] = level;
      this.save();
      this.changed();
    }
    return true;
  }

  addScan(text: string): void {
    const zone = ZONE_NAMES.find((name) => name.toLowerCase() === text.toLowerCase()) ?? null;
    this.scans = [{ id: this.nextScanId++, text, at: this.now(), zone }, ...this.scans].slice(0, MAX_SCANS);
    this.changed();
  }

  setEstop(on: boolean): void {
    if (this.estop !== on) {
      this.estop = on;
      this.save();
      this.changed();
    }
  }

  resetZone(index: number): boolean {
    if (!Number.isInteger(index) || index < 0 || index >= ZONE_NAMES.length) {
      return false;
    }
    this.levels[index] = 0;
    this.pendingCommands.add(`RESET:${index}`);
    this.save();
    this.changed();
    return true;
  }

  resetAll(): void {
    this.levels = ZONE_NAMES.map(() => 0);
    this.pendingCommands = new Set(["RESET_ALL"]);
    this.save();
    this.changed();
  }

  // Called each time the S3 polls.
  takeCommands(): { commands: string[]; estop: boolean; needLevels: boolean } {
    const wasOnline = this.isOnline(this.controllerSeenAt);
    this.controllerSeenAt = this.now();
    const commands = [...this.pendingCommands];
    this.pendingCommands.clear();
    if (!wasOnline || commands.length > 0) {
      this.changed();
    }
    return { commands, estop: this.estop, needLevels: !this.levelsKnown };
  }

  setScanner(status: ScannerStatus): void {
    this.cameraSeenAt = this.now();
    this.scanner = status;
    this.changed();
  }

  setFrame(jpeg: Buffer): void {
    this.frame = jpeg;
    this.frameSeq += 1;
    this.frameAt = this.now();
    this.cameraSeenAt = this.frameAt;
    this.changed();
  }

  getFrame(): Buffer | null {
    return this.frame;
  }

  updateSettings(change: Partial<CameraSettings>): void {
    this.settings = { ...this.settings, ...change };
    this.save();
    this.changed();
  }

  getSettings(): CameraSettings {
    return this.settings;
  }

  private isOnline(seenAt: number | null): boolean {
    return seenAt !== null && this.now() - seenAt < ONLINE_WINDOW_MS;
  }

  snapshot(): DashboardState {
    const zones: ZoneState[] = ZONE_NAMES.map((name, index) => ({ name, level: this.levels[index] ?? 0 }));
    return {
      serverTime: this.now(),
      estop: this.estop,
      zones,
      scans: this.scans,
      controller: { online: this.isOnline(this.controllerSeenAt), lastSeenAt: this.controllerSeenAt },
      camera: { online: this.isOnline(this.cameraSeenAt), lastSeenAt: this.cameraSeenAt },
      scanner: this.scanner,
      settings: this.settings,
      frameSeq: this.frameSeq,
      frameAt: this.frameAt,
    };
  }
}
