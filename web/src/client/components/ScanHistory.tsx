import type { ScanEntry } from "../../shared/types.ts";
import { secondsAgo } from "../format.ts";
import { Panel } from "./Panel.tsx";

interface ScanHistoryProps {
  readonly scans: readonly ScanEntry[];
  readonly now: number;
  readonly className: string;
}

export function ScanHistory({ scans, now, className }: ScanHistoryProps) {
  return (
    <Panel title="Recent scans" className={className}>
      {scans.length === 0 ? (
        <p className="grid min-h-0 flex-1 place-items-center px-4 pb-4 text-center text-sm text-muted">Scans appear here once the S3 reports them.</p>
      ) : (
        <ol className="min-h-0 flex-1 overflow-y-auto overscroll-contain px-4 pb-2">
          {scans.map((scan) => (
            <li key={scan.id} className="flex items-baseline justify-between gap-3 border-t border-edge py-2 text-sm">
              <span className="min-w-0">
                <span className="block truncate font-mono text-[13px]" title={scan.text}>{scan.text}</span>
                {scan.zone === null && <span className="block text-xs text-level-2">No matching zone</span>}
              </span>
              <span className="shrink-0 text-xs text-muted tabular-nums">{secondsAgo(scan.at, now)}</span>
            </li>
          ))}
        </ol>
      )}
    </Panel>
  );
}
