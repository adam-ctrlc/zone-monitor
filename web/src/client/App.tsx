import { useState } from "react";
import type { ReactNode } from "react";
import type { DashboardState } from "../shared/types.ts";
import { BeltStatus, BoardLinks, Brand, PhoneStop, StopDial } from "./components/belt.tsx";
import type { BeltProps } from "./components/belt.tsx";
import { CameraPanel } from "./components/CameraPanel.tsx";
import { CameraIcon, SlidersIcon, ZonesIcon } from "./components/icons.tsx";
import { ScanHistory } from "./components/ScanHistory.tsx";
import { ScannerPage, ScannerPanel } from "./components/ScannerPanel.tsx";
import { ZoneGrid } from "./components/ZoneGrid.tsx";
import { useDashboard } from "./useDashboard.ts";
import { useIsWide } from "./useIsWide.ts";

interface LayoutProps {
  readonly state: DashboardState;
  readonly connected: boolean;
  readonly belt: BeltProps;
}

function WideLayout({ state, connected, belt }: LayoutProps) {
  return (
    <div className="flex min-h-0 flex-1">
      <aside className={`flex w-64 shrink-0 flex-col gap-5 overflow-y-auto border-r border-edge bg-panel short:gap-3 ${state.estop ? "bg-stop/10" : ""}`}>
        <div className="space-y-2 px-4 pt-4 short:pt-3">
          <Brand />
          <BoardLinks {...belt} />
        </div>
        <div className="my-auto space-y-5 short:space-y-3">
          <BeltStatus {...belt} />
          <div className="px-4"><StopDial {...belt} /></div>
        </div>
        <div className="mt-auto border-t border-edge">
          <ScannerPanel settings={state.settings} scanner={state.scanner} connected={connected} flat />
        </div>
      </aside>
      <main className="grid min-w-0 flex-1 grid-cols-[minmax(0,1.1fr)_minmax(0,1fr)] gap-3 p-3">
        <div className="flex min-h-0 min-w-0 flex-col gap-3">
          <CameraPanel state={state} className="shrink-0" pictureClassName="aspect-400/296 max-h-[58dvh] w-full" />
          <ScanHistory scans={state.scans} now={state.serverTime} className="min-h-0 flex-1" />
        </div>
        <ZoneGrid zones={state.zones} connected={connected} className="min-h-0" gridClassName="grid-cols-2" />
      </main>
    </div>
  );
}

type Tab = "camera" | "zones" | "scanner";

function TabLayout({ state, connected, belt }: LayoutProps) {
  const [tab, setTab] = useState<Tab>("camera");

  const tabButton = (id: Tab, label: string, icon: ReactNode): ReactNode => (
    <button
      type="button"
      aria-current={tab === id ? "page" : undefined}
      className={`flex h-14 flex-col items-center justify-center gap-0.5 text-xs transition-colors ${tab === id ? "text-ink" : "text-muted"}`}
      onClick={() => setTab(id)}
    >
      {icon}
      {label}
    </button>
  );

  return (
    <>
      <div className={`shrink-0 pt-[max(0.5rem,env(safe-area-inset-top))] ${state.estop ? "bg-stop/15" : ""}`}>
        <div className="relative">
          <div className="absolute top-0 left-2.5"><Brand small /></div>
          <BeltStatus {...belt} />
        </div>
      </div>
      <main className="flex min-h-0 flex-1 flex-col p-2">
        {tab === "camera" && (
          <div className="flex min-h-0 flex-1 flex-col gap-2 landscape:flex-row">
            <CameraPanel state={state} className="shrink-0 landscape:w-[calc((100dvh-174px)*1.35)] landscape:max-w-[60%]" pictureClassName="aspect-400/296 w-full landscape:aspect-auto landscape:flex-1" />
            <ScanHistory scans={state.scans} now={state.serverTime} className="min-h-0 flex-1" />
          </div>
        )}
        {tab === "zones" && <ZoneGrid zones={state.zones} connected={connected} className="flex-1" gridClassName="grid-cols-2 landscape:grid-cols-3" />}
        {tab === "scanner" && (
          <ScannerPage
            settings={state.settings}
            scanner={state.scanner}
            connected={connected}
            controllerOnline={connected && state.controller.online}
            cameraOnline={connected && state.camera.online}
          />
        )}
      </main>
      <div className="flex shrink-0 flex-col border-t border-edge bg-panel pb-[env(safe-area-inset-bottom)] landscape:flex-row-reverse landscape:items-center">
        <div className="px-2 pt-2 landscape:w-56 landscape:shrink-0 landscape:pt-0">
          <PhoneStop {...belt} />
        </div>
        <nav aria-label="Sections" className="grid grid-cols-3 landscape:flex-1">
          {tabButton("camera", "Camera", <CameraIcon />)}
          {tabButton("zones", "Zones", <ZonesIcon />)}
          {tabButton("scanner", "Scanner", <SlidersIcon />)}
        </nav>
      </div>
    </>
  );
}

export function App() {
  const { state, connected } = useDashboard();
  const wide = useIsWide();

  const belt: BeltProps = {
    connected,
    estop: state?.estop ?? false,
    controller: state?.controller,
    camera: state?.camera,
  };

  return (
    <div className={`flex h-dvh flex-col overflow-hidden ${state?.estop === true ? "shadow-[inset_0_0_0_2px] shadow-stop" : ""}`}>
      {!connected && (
        <p role="alert" className="shrink-0 bg-level-2/15 px-4 py-1.5 text-center text-sm text-level-2">
          Lost contact with the website. This may be out of date. Reconnecting…
        </p>
      )}
      {state === null ? (
        <p className="grid flex-1 place-items-center text-muted">Connecting…</p>
      ) : wide ? (
        <WideLayout state={state} connected={connected} belt={belt} />
      ) : (
        <TabLayout state={state} connected={connected} belt={belt} />
      )}
    </div>
  );
}
