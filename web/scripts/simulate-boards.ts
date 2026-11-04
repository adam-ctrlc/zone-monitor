// Pretends to be the S3 and the camera, to try the website with no hardware:
//   npm start            (in one terminal)
//   npm run simulate     (in another)

import { readFileSync } from "node:fs";
import { ZONE_NAMES } from "../src/shared/types.ts";

const base = `http://localhost:${process.env["PORT"] ?? 3000}/api/v1/device`;
const headers = { "x-device-key": process.env["DEVICE_KEY"] ?? "" };
const jsonHeaders = { ...headers, "Content-Type": "application/json" };
const picture = readFileSync(new URL("./sample-frame.jpg", import.meta.url));

const levels = ZONE_NAMES.map(() => 0);
let wantFrames = false;
let lastText = "";
let lastReadAt = 0;

const postJson = (path: string, body: unknown) =>
  fetch(`${base}${path}`, { method: "POST", headers: jsonHeaders, body: JSON.stringify(body) });

async function pollCommands(): Promise<void> {
  const reply = (await (await fetch(`${base}/commands`, { headers })).json()) as {
    commands: string[];
    estop: boolean;
    needLevels: boolean;
  };
  for (const command of reply.commands) {
    if (command === "RESET_ALL") levels.fill(0);
    else levels[Number(command.split(":")[1])] = 0;
  }
  if (reply.needLevels) {
    await Promise.all(ZONE_NAMES.map((zone, index) => postJson("/level", { zone, level: levels[index] })));
  }
}

async function sendStatus(): Promise<void> {
  const reply = await postJson("/camera/status", {
    ok: true,
    fps: 9,
    frameMs: 96,
    shapes: 1,
    readError: "",
    lastText,
    lastAgoSeconds: lastReadAt === 0 ? -1 : Math.round((Date.now() - lastReadAt) / 1000),
    fastMemory: false,
  });
  wantFrames = ((await reply.json()) as { wantFrames: boolean }).wantFrames;
  if (wantFrames) {
    await fetch(`${base}/camera/frame`, {
      method: "POST",
      headers: { ...headers, "Content-Type": "image/jpeg" },
      body: picture,
    });
  }
}

async function scanParcel(): Promise<void> {
  const index = Math.floor(Math.random() * ZONE_NAMES.length);
  const zone = ZONE_NAMES[index];
  if (zone === undefined) return;
  lastText = zone;
  lastReadAt = Date.now();
  await postJson("/scan", { text: zone });
  if ((levels[index] ?? 0) < 3) {
    levels[index] = (levels[index] ?? 0) + 1;
    await postJson("/level", { zone, level: levels[index] });
  }
}

const safely = (task: () => Promise<void>) => () => {
  task().catch((error: unknown) => console.error("Website not reachable:", error));
};

setInterval(safely(pollCommands), 500);
setInterval(safely(sendStatus), 500);
setInterval(safely(scanParcel), 3000);
console.log("Simulating the S3 and camera. Press Ctrl+C to stop.");
