import { useEffect, useState } from "react";
import type { DashboardState } from "../shared/types.ts";

export interface DashboardConnection {
  readonly state: DashboardState | null;
  readonly connected: boolean;
}

// Live updates from the server. EventSource reconnects by itself.
export function useDashboard(): DashboardConnection {
  const [state, setState] = useState<DashboardState | null>(null);
  const [connected, setConnected] = useState(false);

  useEffect(() => {
    const events = new EventSource("/api/v1/events");
    events.onopen = () => setConnected(true);
    events.onerror = () => setConnected(false);
    events.onmessage = (event: MessageEvent<string>) => {
      setState(JSON.parse(event.data) as DashboardState);
      setConnected(true);
    };
    return () => events.close();
  }, []);

  return { state, connected };
}
