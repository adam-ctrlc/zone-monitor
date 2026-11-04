import { useEffect, useState } from "react";
import type { ReactNode } from "react";

interface ConfirmButtonProps {
  readonly label: string;
  readonly confirmLabel: string;
  readonly icon?: ReactNode;
  readonly disabled?: boolean;
  readonly className: string;
  readonly confirmClassName: string;
  readonly confirmFillClassName?: string;
  readonly onConfirm: () => void;
}

const CONFIRM_WINDOW_MS = 4000;

// Needs a second press, for actions that should not happen by accident.
// While it waits, solid amber drains away to show the time left. If the
// second press does not come, the button goes back to normal.
export function ConfirmButton({ label, confirmLabel, icon, disabled = false, className, confirmClassName, confirmFillClassName = "", onConfirm }: ConfirmButtonProps) {
  const [armed, setArmed] = useState(false);

  useEffect(() => {
    if (!armed) return undefined;
    const timer = setTimeout(() => setArmed(false), CONFIRM_WINDOW_MS);
    return () => clearTimeout(timer);
  }, [armed]);

  return (
    <button
      type="button"
      disabled={disabled}
      className={armed ? `relative overflow-hidden bg-level-2/15 text-level-2 ring-1 ring-level-2/50 ring-inset ${confirmClassName}` : className}
      onClick={() => {
        if (armed) {
          setArmed(false);
          onConfirm();
        } else {
          setArmed(true);
        }
      }}
    >
      {icon}
      {armed ? confirmLabel : label}
      {armed && (
        <span
          aria-hidden
          style={{ animationDuration: `${CONFIRM_WINDOW_MS}ms` }}
          className={`absolute inset-0 flex animate-drain items-center justify-center bg-level-2 text-canvas ${confirmFillClassName}`}
        >
          {icon}
          {confirmLabel}
        </span>
      )}
    </button>
  );
}
