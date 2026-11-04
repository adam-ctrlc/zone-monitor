import { useEffect, useState } from "react";
import type { CSSProperties } from "react";
import {
  CAMERA_DEFAULT_EXPOSURE,
  CAMERA_DEFAULT_GAIN,
  CAMERA_MAX_EXPOSURE,
  CAMERA_MAX_GAIN,
} from "../../shared/types.ts";
import type { CameraSettings, ScannerStatus } from "../../shared/types.ts";
import { api } from "../api.ts";
import { useAction } from "../useAction.ts";
import { Panel } from "./Panel.tsx";

interface SliderProps {
  readonly label: string;
  readonly hint: string;
  readonly value: number;
  readonly max: number;
  readonly step: number;
  readonly disabled: boolean;
  readonly onCommit: (value: number) => void;
}

// Sends the value when the slider is let go, not on every step.
function Slider({ label, hint, value, max, step, disabled, onCommit }: SliderProps) {
  const [draft, setDraft] = useState(value);

  useEffect(() => setDraft(value), [value]);

  const commit = (): void => {
    if (draft !== value) onCommit(draft);
  };

  return (
    <label className="block">
      <span className="flex items-baseline gap-2 text-sm">
        <span>{label}</span>
        <span className="min-w-0 flex-1 truncate text-xs text-muted">{hint}</span>
        <span className="font-mono tabular-nums">{draft}</span>
      </span>
      <input
        type="range"
        min={0}
        max={max}
        step={step}
        value={draft}
        disabled={disabled}
        className="slider"
        style={{ "--fill": `${(draft / max) * 100}%` } as CSSProperties}
        onChange={(event) => setDraft(Number(event.target.value))}
        onPointerUp={commit}
        onKeyUp={commit}
        onBlur={commit}
      />
    </label>
  );
}

function Stat({ label, value }: { readonly label: string; readonly value: string }) {
  return (
    <div className="min-w-0 rounded-lg bg-raised px-2 py-1.5">
      <dt className="truncate text-[11px] text-muted">{label}</dt>
      <dd className="font-mono text-base tabular-nums">{value}</dd>
    </div>
  );
}

interface ScannerPanelProps {
  readonly settings: CameraSettings;
  readonly scanner: ScannerStatus | null;
  readonly connected: boolean;
  readonly flat?: boolean;
}

function FlashSwitch({ settings, disabled, onToggle }: { readonly settings: CameraSettings; readonly disabled: boolean; readonly onToggle: () => void }) {
  return (
    <button
      type="button"
      role="switch"
      aria-checked={settings.flash}
      disabled={disabled}
      className="flex h-9 items-center gap-2 text-sm text-muted disabled:opacity-40"
      onClick={onToggle}
    >
      Flash
      <span className={`relative h-5 w-9 rounded-full transition-colors ${settings.flash ? "bg-level-2" : "bg-edge-strong"}`}>
        <span className={`absolute top-0.5 left-0.5 size-4 rounded-full bg-white transition-transform ${settings.flash ? "translate-x-4" : ""}`} />
      </span>
    </button>
  );
}

function Sliders({ settings, disabled, onChange }: { readonly settings: CameraSettings; readonly disabled: boolean; readonly onChange: (change: Partial<CameraSettings>) => void }) {
  return (
    <>
      <Slider
        label="Exposure"
        hint="lower = less blur"
        value={settings.exposure ?? CAMERA_DEFAULT_EXPOSURE}
        max={CAMERA_MAX_EXPOSURE}
        step={10}
        disabled={disabled}
        onCommit={(exposure) => onChange({ exposure })}
      />
      <Slider
        label="Gain"
        hint="higher = brighter, noisier"
        value={settings.gain ?? CAMERA_DEFAULT_GAIN}
        max={CAMERA_MAX_GAIN}
        step={1}
        disabled={disabled}
        onCommit={(gain) => onChange({ gain })}
      />
    </>
  );
}

function BigStat({ label, value }: { readonly label: string; readonly value: string }) {
  return (
    <div className="flex min-h-0 min-w-0 items-center justify-between gap-3 rounded-xl bg-raised px-4 short:flex-col short:items-start short:justify-center short:gap-1 short:px-3">
      <dt className="text-sm text-muted short:text-xs">{label}</dt>
      <dd className="font-mono text-4xl font-medium tabular-nums short:text-lg">{value}</dd>
    </div>
  );
}

function Connection({ label, link }: { readonly label: string; readonly link: boolean }) {
  return (
    <div className="flex min-w-0 flex-col justify-center rounded-lg bg-raised px-2.5 py-2">
      <dt className="truncate text-xs text-muted">{label}</dt>
      <dd className={`flex items-center gap-1.5 text-sm ${link ? "text-ink" : "text-level-3"}`}>
        <span aria-hidden className={`size-1.5 shrink-0 rounded-full ${link ? "bg-online" : "bg-stop"}`} />
        {link ? "Online" : "Offline"}
      </dd>
    </div>
  );
}

interface ScannerPageProps extends ScannerPanelProps {
  readonly controllerOnline: boolean;
  readonly cameraOnline: boolean;
}

// The phone's Scanner tab. The number tiles take up the spare height.
export function ScannerPage({ settings, scanner, connected, controllerOnline, cameraOnline }: ScannerPageProps) {
  const { busy, error, run } = useAction();
  const disabled = busy || !connected;
  const update = (change: Partial<CameraSettings>): void => void run(() => api.updateCamera(change));

  return (
    <div className="flex min-h-0 flex-1 flex-col gap-2 landscape:grid landscape:grid-cols-2 landscape:grid-rows-[auto_minmax(0,1fr)]">
      <Panel
        title="Scanner"
        className="min-h-0 flex-1 landscape:row-span-2"
        action={<FlashSwitch settings={settings} disabled={disabled} onToggle={() => update({ flash: !settings.flash })} />}
      >
        <dl className="grid min-h-0 flex-1 auto-rows-fr grid-cols-1 gap-2 px-3 pb-3 short:grid-cols-3">
          <BigStat label="Frames per second" value={scanner === null ? "–" : String(scanner.fps)} />
          <BigStat label="ms per frame" value={scanner === null ? "–" : String(scanner.frameMs)} />
          <BigStat label="Memory" value={scanner === null ? "–" : scanner.fastMemory ? "Fast" : "PSRAM"} />
        </dl>
      </Panel>
      <Panel title="Picture" className="shrink-0">
        <div className="px-4 pb-2">
          {error !== null && <p role="alert" className="text-xs text-level-3">Not sent. Try again.</p>}
          <Sliders settings={settings} disabled={disabled} onChange={update} />
        </div>
      </Panel>
      <section aria-label="Connections" className="shrink-0 rounded-2xl border border-edge bg-panel p-2">
        <dl className="grid h-full grid-cols-3 gap-1.5">
          <Connection label="Website" link={connected} />
          <Connection label="S3" link={controllerOnline} />
          <Connection label="Camera" link={cameraOnline} />
        </dl>
      </section>
    </div>
  );
}

export function ScannerPanel({ settings, scanner, connected, flat = false }: ScannerPanelProps) {
  const { busy, error, run } = useAction();
  const disabled = busy || !connected;

  return (
    <Panel
      title="Scanner"
      className="shrink-0"
      flat={flat}
      action={<FlashSwitch settings={settings} disabled={disabled} onToggle={() => void run(() => api.updateCamera({ flash: !settings.flash }))} />}
    >
      <div className="px-3 pb-2">
        <dl className="grid grid-cols-3 gap-1.5">
          <Stat label="Frames/s" value={scanner === null ? "–" : String(scanner.fps)} />
          <Stat label="ms/frame" value={scanner === null ? "–" : String(scanner.frameMs)} />
          <Stat label="Memory" value={scanner === null ? "–" : scanner.fastMemory ? "Fast" : "PSRAM"} />
        </dl>
        <div className="mt-2 px-1">
          {error !== null && <p role="alert" className="text-xs text-level-3">Not sent. Try again.</p>}
          <Sliders settings={settings} disabled={disabled} onChange={(change) => void run(() => api.updateCamera(change))} />
        </div>
      </div>
    </Panel>
  );
}
