import { createHash, timingSafeEqual } from "node:crypto";
import { existsSync } from "node:fs";
import express from "express";
import type { NextFunction, Request, Response, Router } from "express";
import {
  CAMERA_MAX_EXPOSURE,
  CAMERA_MAX_GAIN,
} from "../shared/types.ts";
import type { CameraSettings, ScannerStatus } from "../shared/types.ts";
import { isLevel, Store } from "./store.ts";

const MAX_DASHBOARDS = 32;
const HEARTBEAT_MS = 1000;
const MAX_QR_TEXT = 64;

export interface AppOptions {
  readonly deviceKey: string;
  readonly store: Store;
  readonly clientDir?: string;
}

function sha256(text: string): Buffer {
  return createHash("sha256").update(text).digest();
}

function requireDeviceKey(deviceKey: string) {
  const expected = sha256(deviceKey);
  return (req: Request, res: Response, next: NextFunction): void => {
    const given = req.get("x-device-key");
    if (given !== undefined && timingSafeEqual(sha256(given), expected)) {
      next();
    } else {
      res.status(401).json({ error: "Invalid device key" });
    }
  };
}

// A page on another site can send a plain form post to this address, but it
// cannot send a JSON one without the browser asking this server's permission.
function requireJson(req: Request, res: Response, next: NextFunction): void {
  if (req.is("application/json")) {
    next();
  } else {
    res.status(415).json({ error: "Send JSON" });
  }
}

function asRecord(value: unknown): Record<string, unknown> {
  return typeof value === "object" && value !== null ? (value as Record<string, unknown>) : {};
}

function isCount(value: unknown): value is number {
  return Number.isInteger(value) && (value as number) >= 0 && (value as number) < 1_000_000;
}

function parseScanner(body: unknown): ScannerStatus | null {
  const { ok, fps, frameMs, shapes, readError, lastText, lastAgoSeconds, fastMemory } = asRecord(body);
  if (
    typeof ok !== "boolean" ||
    !isCount(fps) ||
    !isCount(frameMs) ||
    !isCount(shapes) ||
    typeof readError !== "string" ||
    readError.length > MAX_QR_TEXT ||
    typeof lastText !== "string" ||
    lastText.length > MAX_QR_TEXT ||
    typeof lastAgoSeconds !== "number" ||
    !Number.isInteger(lastAgoSeconds) ||
    typeof fastMemory !== "boolean"
  ) {
    return null;
  }
  return { ok, fps, frameMs, shapes, readError, lastText, lastAgoSeconds, fastMemory };
}

function parseSettings(body: unknown): Partial<CameraSettings> | null {
  const { flash, exposure, gain } = asRecord(body);
  const change: { flash?: boolean; exposure?: number; gain?: number } = {};
  if (flash !== undefined) {
    if (typeof flash !== "boolean") return null;
    change.flash = flash;
  }
  if (exposure !== undefined) {
    if (!Number.isInteger(exposure) || (exposure as number) < 0 || (exposure as number) > CAMERA_MAX_EXPOSURE) return null;
    change.exposure = exposure as number;
  }
  if (gain !== undefined) {
    if (!Number.isInteger(gain) || (gain as number) < 0 || (gain as number) > CAMERA_MAX_GAIN) return null;
    change.gain = gain as number;
  }
  return Object.keys(change).length > 0 ? change : null;
}

function deviceRoutes(store: Store, dashboards: () => number, deviceKey: string): Router {
  const router = express.Router();
  const json = express.json({ limit: "4kb" });

  router.use(requireDeviceKey(deviceKey));

  router.get("/commands", (_req, res) => {
    res.json(store.takeCommands());
  });

  router.post("/level", json, (req, res) => {
    const { zone, level } = asRecord(req.body);
    if (typeof zone !== "string" || !isLevel(level) || !store.setLevel(zone, level)) {
      res.status(400).json({ error: "Unknown zone or level" });
      return;
    }
    res.json({ ok: true });
  });

  router.post("/scan", json, (req, res) => {
    const { text } = asRecord(req.body);
    if (typeof text !== "string" || text.length === 0 || text.length > MAX_QR_TEXT) {
      res.status(400).json({ error: "Invalid QR text" });
      return;
    }
    store.addScan(text);
    res.json({ ok: true });
  });

  router.post("/camera/status", json, (req, res) => {
    const status = parseScanner(req.body);
    if (status === null) {
      res.status(400).json({ error: "Invalid scanner status" });
      return;
    }
    store.setScanner(status);
    const { flash, exposure, gain } = store.getSettings();
    res.json({
      wantFrames: dashboards() > 0,
      flash,
      ...(exposure === null ? {} : { aec: exposure }),
      ...(gain === null ? {} : { gain }),
    });
  });

  router.post("/camera/frame", express.raw({ type: "image/jpeg", limit: "256kb" }), (req, res) => {
    const body: unknown = req.body;
    if (!Buffer.isBuffer(body) || body.length < 4 || body[0] !== 0xff || body[1] !== 0xd8) {
      res.status(400).json({ error: "Expected a JPEG" });
      return;
    }
    store.setFrame(body);
    res.json({ ok: true });
  });

  return router;
}

function dashboardRoutes(store: Store, clients: Set<Response>): Router {
  const router = express.Router();
  const json = express.json({ limit: "4kb" });

  router.get("/state", (_req, res) => {
    res.json(store.snapshot());
  });

  router.get("/events", (req, res) => {
    if (clients.size >= MAX_DASHBOARDS) {
      res.status(503).json({ error: "Too many open dashboards" });
      return;
    }
    res.writeHead(200, {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-store",
      Connection: "keep-alive",
    });
    res.write(`data: ${JSON.stringify(store.snapshot())}\n\n`);
    clients.add(res);
    req.on("close", () => clients.delete(res));
  });

  router.get("/camera/frame.jpg", (_req, res) => {
    const frame = store.getFrame();
    if (frame === null) {
      res.status(404).json({ error: "No picture yet" });
      return;
    }
    res.set("Cache-Control", "no-store").type("image/jpeg").send(frame);
  });

  router.post("/estop", requireJson, json, (req, res) => {
    const { on } = asRecord(req.body);
    if (typeof on !== "boolean") {
      res.status(400).json({ error: "on must be true or false" });
      return;
    }
    store.setEstop(on);
    res.json({ ok: true });
  });

  router.post("/zones/reset", requireJson, json, (_req, res) => {
    store.resetAll();
    res.json({ ok: true });
  });

  router.post("/zones/:index/reset", requireJson, json, (req, res) => {
    const raw = req.params["index"];
    const text = typeof raw === "string" ? raw : "";
    if (!/^\d+$/.test(text) || !store.resetZone(Number(text))) {
      res.status(404).json({ error: "Unknown zone" });
      return;
    }
    res.json({ ok: true });
  });

  router.patch("/camera", requireJson, json, (req, res) => {
    const change = parseSettings(req.body);
    if (change === null) {
      res.status(400).json({ error: "Invalid camera settings" });
      return;
    }
    store.updateSettings(change);
    res.json({ ok: true });
  });

  return router;
}

export function createApp({ deviceKey, store, clientDir }: AppOptions) {
  const app = express();
  app.disable("x-powered-by");

  const clients = new Set<Response>();

  // Pushes changes at most every 50 ms, plus a regular update so board
  // online status and "seconds ago" stay current.
  let pending: NodeJS.Timeout | null = null;
  const broadcast = (): void => {
    pending = null;
    const message = `data: ${JSON.stringify(store.snapshot())}\n\n`;
    for (const client of clients) {
      client.write(message);
    }
  };
  const unsubscribe = store.subscribe(() => {
    pending ??= setTimeout(broadcast, 50);
  });
  const heartbeat = setInterval(broadcast, HEARTBEAT_MS);

  app.use("/api/v1/device", deviceRoutes(store, () => clients.size, deviceKey));
  app.use("/api/v1", dashboardRoutes(store, clients));
  app.use("/api", (_req, res) => {
    res.status(404).json({ error: "Not found" });
  });

  if (clientDir !== undefined && existsSync(clientDir)) {
    app.use(express.static(clientDir));
    app.get("/{*path}", (_req, res) => {
      res.sendFile("index.html", { root: clientDir });
    });
  }

  app.use((error: unknown, _req: Request, res: Response, next: NextFunction) => {
    if (res.headersSent) {
      next(error);
      return;
    }
    const status = asRecord(error)["status"];
    const code = typeof status === "number" && status >= 400 && status < 500 ? status : 500;
    if (code === 500) {
      console.error(error);
    }
    res.status(code).json({ error: code === 500 ? "Server error" : "Bad request" });
  });

  const close = (): void => {
    clearInterval(heartbeat);
    if (pending !== null) clearTimeout(pending);
    unsubscribe();
    for (const client of clients) {
      client.end();
    }
    clients.clear();
  };

  return { app, close };
}
