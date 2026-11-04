import { MAX_LEVEL } from "../../shared/types.ts";
import type { ZoneState } from "../../shared/types.ts";
import { api } from "../api.ts";
import { useAction } from "../useAction.ts";
import { ConfirmButton } from "./ConfirmButton.tsx";
import { ResetIcon } from "./icons.tsx";
import { Panel } from "./Panel.tsx";

const LEVEL_TEXT = ["text-muted", "text-level-1", "text-level-2", "text-level-3"] as const;
const LEVEL_FILL = ["bg-transparent", "bg-level-1/20", "bg-level-2/20", "bg-level-3/25"] as const;

interface ZoneTileProps {
  readonly zone: ZoneState;
  readonly index: number;
  readonly disabled: boolean;
  readonly onReset: (index: number) => void;
}

// The tile fills up like the bin it stands for.
function ZoneTile({ zone, index, disabled, onReset }: ZoneTileProps) {
  const text = LEVEL_TEXT[zone.level] ?? LEVEL_TEXT[0];
  const fill = LEVEL_FILL[zone.level] ?? LEVEL_FILL[0];
  const full = zone.level >= MAX_LEVEL;

  return (
    <li className="relative isolate flex min-h-0 flex-col overflow-hidden rounded-xl bg-raised p-3 short:px-3 short:py-2">
      <span
        aria-hidden
        style={{ height: `${(zone.level / MAX_LEVEL) * 100}%` }}
        className={`absolute inset-x-0 bottom-0 -z-10 transition-[height] duration-500 ease-out ${fill}`}
      />
      <p className="shrink-0 truncate pr-8 text-sm font-medium short:text-xs" title={zone.name}>{zone.name}</p>
      <p className="mt-auto flex shrink-0 items-baseline gap-1.5" aria-label={`${zone.level} of ${MAX_LEVEL} parcels`}>
        <span className={`font-mono text-4xl leading-none font-medium tabular-nums transition-colors short:text-2xl lg:tall:text-6xl ${text}`}>{zone.level}</span>
        <span className="text-xs text-muted">{full ? "Full" : `of ${MAX_LEVEL}`}</span>
      </p>
      {zone.level > 0 && (
        <button
          type="button"
          disabled={disabled}
          aria-label={`Reset ${zone.name}`}
          title={`Reset ${zone.name}`}
          className="absolute top-1 right-1 grid size-10 place-items-center short:top-0 short:right-0 rounded-lg text-muted transition-colors hover:bg-white/10 hover:text-ink disabled:opacity-40"
          onClick={() => onReset(index)}
        >
          <ResetIcon />
        </button>
      )}
    </li>
  );
}

interface ZoneGridProps {
  readonly zones: readonly ZoneState[];
  readonly connected: boolean;
  readonly className: string;
  readonly gridClassName: string;
}

export function ZoneGrid({ zones, connected, className, gridClassName }: ZoneGridProps) {
  const { busy, error, run } = useAction();
  const disabled = busy || !connected;
  const anyFilled = zones.some((zone) => zone.level > 0);

  return (
    <Panel
      title="Zones"
      className={className}
      action={
        <div className="flex items-center gap-3">
          {error !== null && <p role="alert" className="text-xs text-level-3">Not sent. Try again.</p>}
          <ConfirmButton
            label="Reset all"
            confirmLabel="Confirm reset all"
            disabled={disabled || !anyFilled}
            className="h-9 rounded-full px-3 text-sm text-muted transition-colors hover:bg-raised hover:text-ink disabled:opacity-40"
            confirmClassName="h-9 rounded-full px-3 text-sm font-semibold"
            onConfirm={() => void run(api.resetAll)}
          />
        </div>
      }
    >
      <ul className={`grid min-h-0 flex-1 auto-rows-fr gap-2 px-3 pb-3 ${gridClassName}`}>
        {zones.map((zone, index) => (
          <ZoneTile
            key={zone.name}
            zone={zone}
            index={index}
            disabled={disabled}
            onReset={(target) => void run(() => api.resetZone(target))}
          />
        ))}
      </ul>
    </Panel>
  );
}
