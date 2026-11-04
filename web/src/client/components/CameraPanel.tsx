import type { DashboardState } from "../../shared/types.ts";
import { secondsAgo } from "../format.ts";

const STALE_FRAME_MS = 3000;

function scannerMessage({ scanner, camera }: DashboardState): { text: string; dot: string } {
  if (!camera.online || scanner === null) {
    return { text: "Camera offline", dot: "bg-stop" };
  }
  if (!scanner.ok) {
    return { text: "Camera failed to start", dot: "bg-stop" };
  }
  if (scanner.shapes === 0) {
    return { text: "No QR in view", dot: "bg-level-0" };
  }
  if (scanner.readError !== "") {
    return { text: `Unreadable: ${scanner.readError}`, dot: "bg-level-2" };
  }
  return { text: "QR read", dot: "bg-online" };
}

function latestQr({ scans, scanner, serverTime }: DashboardState) {
  const scan = scans[0];
  if (scan !== undefined) {
    return { key: `scan-${scan.id}`, text: scan.text, at: scan.at };
  }
  if (scanner !== null && scanner.lastText !== "" && scanner.lastAgoSeconds >= 0) {
    return { key: `camera-${scanner.lastText}`, text: scanner.lastText, at: serverTime - scanner.lastAgoSeconds * 1000 };
  }
  return null;
}

interface CameraPanelProps {
  readonly state: DashboardState;
  readonly className: string;
  readonly pictureClassName: string;
}

export function CameraPanel({ state, className, pictureClassName }: CameraPanelProps) {
  const message = scannerMessage(state);
  const latest = latestQr(state);
  const frameAt = state.frameAt;
  const stale = frameAt !== null && state.serverTime - frameAt > STALE_FRAME_MS;

  return (
    <section aria-label="QR camera" className={`flex min-h-0 flex-col overflow-hidden rounded-2xl border border-edge bg-panel ${className}`}>
      <div className={`relative min-h-0 bg-black ${pictureClassName}`}>
        {frameAt !== null ? (
          <img
            src={`/api/v1/camera/frame.jpg?seq=${state.frameSeq}`}
            alt="Latest picture from the QR camera"
            className={`absolute inset-0 size-full object-contain transition-opacity ${stale ? "opacity-35" : ""}`}
          />
        ) : (
          <p className="absolute inset-0 grid place-items-center p-6 text-center text-sm text-muted">
            {state.camera.online ? "Waiting for the first picture…" : "The picture appears when the camera connects."}
          </p>
        )}
        <p className="absolute top-3 left-3 flex max-w-[calc(100%-1.5rem)] items-center gap-2 rounded-full bg-black/70 px-3 py-1.5 text-xs font-medium backdrop-blur-sm">
          <span aria-hidden className={`size-2 shrink-0 rounded-full ${message.dot}`} />
          <span className="truncate">{message.text}</span>
        </p>
        {stale && (
          <p className="absolute right-3 bottom-3 rounded-full bg-black/70 px-3 py-1.5 text-xs text-level-2 backdrop-blur-sm">
            Last picture, {secondsAgo(frameAt, state.serverTime)}
          </p>
        )}
      </div>
      <div
        key={latest?.key ?? "none"}
        aria-live="polite"
        className={`flex shrink-0 items-baseline justify-between gap-4 px-4 py-3 ${latest !== null ? "animate-read" : ""}`}
      >
        <div className="min-w-0">
          <p className="text-xs text-muted">Last QR read</p>
          <p className={`truncate font-mono text-xl font-medium tracking-tight sm:text-2xl ${latest === null ? "text-muted" : ""}`} title={latest?.text}>
            {latest?.text ?? "None yet"}
          </p>
        </div>
        {latest !== null && <p className="shrink-0 text-sm text-muted tabular-nums">{secondsAgo(latest.at, state.serverTime)}</p>}
      </div>
    </section>
  );
}
