import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import type { AddressInfo } from "node:net";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { after, before, beforeEach, test } from "node:test";
import { createApp } from "./app.ts";
import { Store } from "./store.ts";
import type { DashboardState } from "../shared/types.ts";

const KEY = "test-key";
const folder = mkdtempSync(join(tmpdir(), "zone-web-"));
const statePath = join(folder, "state.json");

let base = "";
let stop: () => void = () => {};
let store: Store;

const device = (path: string, init: RequestInit = {}) =>
  fetch(`${base}/api/v1/device${path}`, {
    ...init,
    headers: { "x-device-key": KEY, "Content-Type": "application/json", ...(init.headers ?? {}) },
  });
const post = (path: string, body: unknown, method = "POST") =>
  fetch(`${base}/api/v1${path}`, {
    method,
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
const state = async () => (await (await fetch(`${base}/api/v1/state`)).json()) as DashboardState;
const SCANNER = { ok: true, fps: 12, frameMs: 80, shapes: 0, readError: "", lastText: "Lirio st.", lastAgoSeconds: 2, fastMemory: false };

async function startServer(): Promise<void> {
  store = new Store(statePath);
  const { app, close } = createApp({ deviceKey: KEY, store });
  const server = app.listen(0, "127.0.0.1");
  await new Promise((resolve) => server.once("listening", resolve));
  base = `http://127.0.0.1:${(server.address() as AddressInfo).port}`;
  stop = () => {
    close();
    server.closeAllConnections();
    server.close();
  };
}

before(startServer);
beforeEach(async () => {
  stop();
  rmSync(statePath, { force: true });
  await startServer();
});
after(() => {
  stop();
  rmSync(folder, { recursive: true, force: true });
});

test("device routes need the key", async () => {
  for (const headers of [{}, { "x-device-key": "wrong" }]) {
    const res = await fetch(`${base}/api/v1/device/commands`, { headers });
    assert.equal(res.status, 401);
  }
  assert.equal((await device("/commands")).status, 200);
});

test("old unversioned routes are gone", async () => {
  assert.equal((await fetch(`${base}/api/device/commands`, { headers: { "x-device-key": KEY } })).status, 404);
});

test("commands reply is what the S3 searches for, with no spaces", async () => {
  const body = await (await device("/commands")).text();
  assert.match(body, /"estop":false/);
  assert.match(body, /"needLevels":true/);
  assert.ok(Buffer.byteLength(body) < 2048);
});

test("E-stop reaches the S3 and survives a website restart", async () => {
  assert.equal((await post("/estop", { on: true })).status, 200);
  assert.match(await (await device("/commands")).text(), /"estop":true/);
  stop();
  await startServer();
  assert.equal((await state()).estop, true);
  assert.match(await (await device("/commands")).text(), /"estop":true/);
  await post("/estop", { on: false });
  assert.match(await (await device("/commands")).text(), /"estop":false/);
});

test("an unreadable state file starts with E-stop on", async () => {
  stop();
  writeFileSync(statePath, "{ not json");
  await startServer();
  assert.equal((await state()).estop, true);
  stop();
  writeFileSync(statePath, JSON.stringify({ estop: false, levels: [9], settings: {} }));
  await startServer();
  assert.equal((await state()).estop, true);
});

test("control routes refuse form posts from other sites", async () => {
  const res = await fetch(`${base}/api/v1/estop`, {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: "on=false",
  });
  assert.equal(res.status, 415);
  assert.equal((await fetch(`${base}/api/v1/zones/reset`, { method: "POST" })).status, 415);
});

test("levels are validated", async () => {
  const send = (body: unknown) => device("/level", { method: "POST", body: JSON.stringify(body) });
  assert.equal((await send({ zone: "Lirio st.", level: 2 })).status, 200);
  assert.equal((await state()).zones[1]?.level, 2);
  for (const bad of [
    { zone: "Nowhere", level: 1 },
    { zone: "Lirio st.", level: 4 },
    { zone: "Lirio st.", level: -1 },
    { zone: "Lirio st.", level: 1.5 },
    { zone: "__proto__", level: 1 },
    { zone: 7, level: 1 },
    {},
  ]) {
    assert.equal((await send(bad)).status, 400, JSON.stringify(bad));
  }
  assert.equal((await device("/level", { method: "POST", body: "{ bad" })).status, 400);
});

test("needLevels clears after the first level arrives", async () => {
  await device("/level", { method: "POST", body: JSON.stringify({ zone: "Lirio st.", level: 0 }) });
  assert.match(await (await device("/commands")).text(), /"needLevels":false/);
});

test("resets are delivered once", async () => {
  await device("/level", { method: "POST", body: JSON.stringify({ zone: "Lirio st.", level: 2 }) });
  assert.equal((await post("/zones/1/reset", {})).status, 200);
  assert.equal((await state()).zones[1]?.level, 0);
  await post("/zones/4/reset", {});
  const first = await (await device("/commands")).text();
  assert.match(first, /"RESET:1"/);
  assert.match(first, /"RESET:4"/);
  assert.doesNotMatch(await (await device("/commands")).text(), /RESET/);
  await post("/zones/reset", {});
  assert.match(await (await device("/commands")).text(), /"RESET_ALL"/);
  for (const bad of ["6", "-1", "1e0", "abc"]) {
    assert.equal((await post(`/zones/${bad}/reset`, {})).status, 404, bad);
  }
});

test("scans keep odd text intact and match zones", async () => {
  const send = (text: unknown) => device("/scan", { method: "POST", body: JSON.stringify({ text }) });
  await send("LIRIO ST.");
  await send('<img src=x onerror="1">');
  const { scans } = await state();
  assert.equal(scans[0]?.text, '<img src=x onerror="1">');
  assert.equal(scans[0]?.zone, null);
  assert.equal(scans[1]?.zone, "Lirio st.");
  for (const bad of ["", 5, "x".repeat(65)]) {
    assert.equal((await send(bad)).status, 400);
  }
  for (let i = 0; i < 60; i++) await send("x");
  assert.equal((await state()).scans.length, 50);
});

test("camera status reply carries settings the board can read", async () => {
  const send = async () => (await device("/camera/status", { method: "POST", body: JSON.stringify(SCANNER) })).text();
  assert.equal(await send(), '{"wantFrames":false,"flash":true}');
  await post("/camera", { flash: false, exposure: 60, gain: 16 }, "PATCH");
  assert.equal(await send(), '{"wantFrames":false,"flash":false,"aec":60,"gain":16}');
  assert.equal((await state()).scanner?.fps, 12);
  assert.equal((await state()).camera.online, true);
  for (const bad of [{ exposure: 1201 }, { exposure: -1 }, { gain: 31 }, { gain: 1.5 }, { flash: "yes" }, {}]) {
    assert.equal((await post("/camera", bad, "PATCH")).status, 400, JSON.stringify(bad));
  }
  assert.equal((await device("/camera/status", { method: "POST", body: JSON.stringify({ ...SCANNER, fps: "x" }) })).status, 400);
});

test("pictures are requested only while a dashboard is open", async () => {
  const wants = async () => {
    const reply = await device("/camera/status", { method: "POST", body: JSON.stringify(SCANNER) });
    return ((await reply.json()) as { wantFrames: boolean }).wantFrames;
  };
  assert.equal(await wants(), false);
  const controller = new AbortController();
  const stream = await fetch(`${base}/api/v1/events`, { signal: controller.signal });
  const reader = stream.body?.getReader();
  const first = await reader?.read();
  assert.match(new TextDecoder().decode(first?.value), /^data: \{/);
  assert.equal(await wants(), true);
  controller.abort();
  await new Promise((resolve) => setTimeout(resolve, 50));
  assert.equal(await wants(), false);
});

test("frames must be JPEG, within the limit, with the key", async () => {
  const send = (body: Uint8Array<ArrayBuffer>, key = KEY) =>
    fetch(`${base}/api/v1/device/camera/frame`, {
      method: "POST",
      headers: { "x-device-key": key, "Content-Type": "image/jpeg" },
      body,
    });
  const jpeg = new Uint8Array(1000).fill(7);
  jpeg[0] = 0xff;
  jpeg[1] = 0xd8;
  assert.equal((await fetch(`${base}/api/v1/camera/frame.jpg`)).status, 404);
  assert.equal((await send(jpeg, "wrong")).status, 401);
  assert.equal((await send(new Uint8Array(10))).status, 400);
  assert.equal((await send(new Uint8Array(300 * 1024))).status, 413);
  assert.equal((await send(jpeg)).status, 200);
  const res = await fetch(`${base}/api/v1/camera/frame.jpg`);
  assert.equal(res.headers.get("content-type"), "image/jpeg");
  assert.equal((await res.arrayBuffer()).byteLength, 1000);
  assert.equal((await state()).frameSeq, 1);
});

test("state file holds no secrets", async () => {
  await post("/estop", { on: true });
  assert.doesNotMatch(readFileSync(statePath, "utf8"), new RegExp(KEY));
});
