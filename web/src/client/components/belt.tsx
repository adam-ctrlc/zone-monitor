import type { BoardLink } from "../../shared/types.ts";
import { api } from "../api.ts";
import { useAction } from "../useAction.ts";
import { ConfirmButton } from "./ConfirmButton.tsx";
import { PlayIcon } from "./icons.tsx";

export interface BeltProps {
  readonly connected: boolean;
  readonly estop: boolean;
  readonly controller: BoardLink | undefined;
  readonly camera: BoardLink | undefined;
}

function beltState({ connected, estop, controller }: BeltProps) {
  if (estop) return { key: "stopped", label: "Stopped", tone: "text-level-3" };
  if (!connected || controller?.online !== true) return { key: "unknown", label: "S3 offline", tone: "text-level-2" };
  return { key: "running", label: "Running", tone: "text-ink" };
}

function Link({ label, online }: { readonly label: string; readonly online: boolean }) {
  return (
    <span className="inline-flex items-center gap-1.5 text-xs whitespace-nowrap text-muted" title={`${label} ${online ? "online" : "offline"}`}>
      <span aria-hidden className={`size-1.5 rounded-full ${online ? "bg-online" : "bg-stop"}`} />
      {label}
      <span className="sr-only">{online ? "online" : "offline"}</span>
    </span>
  );
}

export function BoardLinks({ connected, controller, camera }: BeltProps) {
  return (
    <div className="flex gap-3">
      <Link label="Web" online={connected} />
      <Link label="S3" online={connected && controller?.online === true} />
      <Link label="Camera" online={connected && camera?.online === true} />
    </div>
  );
}

export function Brand({ small = false }: { readonly small?: boolean }) {
  return (
    <div className="flex min-w-0 items-center gap-2">
      <img src="/favicon.svg" alt="" className={small ? "size-5 shrink-0" : "size-9 shrink-0"} />
      <h1 className={`font-semibold tracking-tight whitespace-nowrap ${small ? "text-sm" : "text-base"}`}>Zone Monitor</h1>
    </div>
  );
}

export function BeltStatus(props: BeltProps) {
  const { key, label, tone } = beltState(props);
  return (
    <div role="status" className="min-w-0">
      <p className={`pb-1.5 text-center text-sm font-medium ${tone}`}>
        <span className="sr-only">Belt: </span>
        {label}
      </p>
      <div aria-hidden data-state={key} className="belt-strip" />
    </div>
  );
}

const STOP_LABEL = "Emergency stop";

const OCTAGON = "29.3,0 70.7,0 100,29.3 100,70.7 70.7,100 29.3,100 0,70.7 0,29.3";
const OCTAGON_INNER = "31.8,6 68.2,6 94,31.8 94,68.2 68.2,94 31.8,94 6,68.2 6,31.8";

interface StopSignProps {
  readonly latched: boolean;
  readonly className: string;
}

// A flat stop sign. It scales to any size, and the extra room in the
// view box keeps the keyboard focus outline from being cut off.
function StopSign({ latched, className }: StopSignProps) {
  return (
    <svg aria-hidden viewBox="-4 -4 108 108" className={`block transition-transform duration-100 group-active:scale-95 ${className}`}>
      <polygon
        points={OCTAGON}
        className={
          latched
            ? "fill-stop/15 stroke-stop [stroke-width:1.5]"
            : "fill-stop stroke-transparent [stroke-width:3] transition-colors group-hover:fill-[#ff5252] group-focus-visible:stroke-ink group-disabled:fill-edge-strong"
        }
      />
      {!latched && <polygon points={OCTAGON_INNER} fill="none" className="stroke-white/90 [stroke-width:1.5] group-disabled:stroke-muted" />}
      <text
        x="50"
        y="51"
        textAnchor="middle"
        dominantBaseline="central"
        fontSize={latched ? 17 : 25}
        fontWeight="800"
        letterSpacing={latched ? 0.5 : 1}
        className={latched ? "fill-level-3" : "fill-white group-disabled:fill-muted"}
      >
        {latched ? "STOPPED" : "STOP"}
      </text>
    </svg>
  );
}

// Stopping takes one press. Resuming takes two, because the belt will move.
export function StopDial({ connected, estop, controller }: BeltProps) {
  const { busy, error, run } = useAction();
  const sign = "size-40 short:size-24";

  return (
    <div className="flex flex-col items-center gap-3">
      {estop ? (
        <div role="img" aria-label="Emergency stop is active">
          <StopSign latched className={sign} />
        </div>
      ) : (
        <button
          type="button"
          aria-label={STOP_LABEL}
          disabled={busy || !connected}
          className="group rounded-2xl outline-none"
          onClick={() => void run(() => api.setEstop(true))}
        >
          <StopSign latched={false} className={sign} />
        </button>
      )}
      {estop ? (
        <ConfirmButton
          label="Resume belt"
          confirmLabel="Confirm resume"
          icon={<PlayIcon />}
          disabled={busy || !connected}
          className="flex h-11 w-full items-center justify-center gap-2 rounded-xl bg-raised text-sm font-medium transition-colors hover:bg-edge-strong disabled:opacity-50"
          confirmClassName="flex h-11 w-full items-center justify-center gap-2 rounded-xl text-sm font-semibold"
          confirmFillClassName="gap-2"
          onConfirm={() => void run(() => api.setEstop(false))}
        />
      ) : (
        <p className="text-center text-xs text-muted">
          {connected && controller?.online !== true ? "Connect the S3 for this to reach the belt" : STOP_LABEL}
        </p>
      )}
      {error !== null && <p role="alert" className="text-center text-xs text-level-3">{error}</p>}
    </div>
  );
}

function OctagonGlyph() {
  return (
    <svg aria-hidden viewBox="-4 -4 108 108" className="size-6">
      <polygon points={OCTAGON} fill="none" stroke="currentColor" strokeWidth="12" strokeLinejoin="round" />
    </svg>
  );
}

// The phone version: a full-width button in its own row.
export function PhoneStop({ connected, estop }: BeltProps) {
  const { busy, error, run } = useAction();

  return (
    <div className="w-full">
      {error !== null && <p role="alert" className="pb-1 text-center text-xs text-level-3">{error}</p>}
      {estop ? (
        <ConfirmButton
          label="Resume belt"
          confirmLabel="Confirm resume"
          icon={<PlayIcon />}
          disabled={busy || !connected}
          className="flex h-13 w-full items-center justify-center gap-2 rounded-xl border-2 border-level-2 font-semibold text-level-2 disabled:opacity-50"
          confirmClassName="flex h-13 w-full items-center justify-center gap-2 rounded-xl font-semibold"
          confirmFillClassName="gap-2"
          onConfirm={() => void run(() => api.setEstop(false))}
        />
      ) : (
        <button
          type="button"
          aria-label={STOP_LABEL}
          disabled={busy || !connected}
          className="flex h-13 w-full items-center justify-center gap-2.5 rounded-xl bg-stop text-lg font-bold tracking-wider text-white transition-transform active:scale-[0.98] disabled:bg-edge-strong disabled:text-muted"
          onClick={() => void run(() => api.setEstop(true))}
        >
          <OctagonGlyph />
          STOP
        </button>
      )}
    </div>
  );
}
