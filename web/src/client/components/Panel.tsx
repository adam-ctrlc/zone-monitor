import type { ReactNode } from "react";

interface PanelProps {
  readonly title: string;
  readonly action?: ReactNode;
  readonly className?: string;
  readonly flat?: boolean;
  readonly children: ReactNode;
}

export function Panel({ title, action, className = "", flat = false, children }: PanelProps) {
  return (
    <section aria-label={title} className={`flex min-h-0 flex-col overflow-hidden ${flat ? "" : "rounded-2xl border border-edge bg-panel"} ${className}`}>
      <header className="flex min-h-12 shrink-0 items-center justify-between gap-3 px-4">
        <h2 className="text-sm font-medium text-muted">{title}</h2>
        {action}
      </header>
      {children}
    </section>
  );
}
