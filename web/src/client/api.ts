import type { CameraSettings } from "../shared/types.ts";

async function send(path: string, method: "POST" | "PATCH", body: unknown = {}): Promise<void> {
  const response = await fetch(`/api/v1${path}`, {
    method,
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  if (!response.ok) {
    throw new Error(`The server answered ${response.status}`);
  }
}

export const api = {
  setEstop: (on: boolean) => send("/estop", "POST", { on }),
  resetZone: (index: number) => send(`/zones/${index}/reset`, "POST"),
  resetAll: () => send("/zones/reset", "POST"),
  updateCamera: (change: Partial<CameraSettings>) => send("/camera", "PATCH", change),
};
