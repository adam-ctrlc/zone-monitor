import { networkInterfaces } from "node:os";
import { resolve } from "node:path";
import { createApp } from "./app.ts";
import { Store } from "./store.ts";

const deviceKey = process.env["DEVICE_KEY"];
if (deviceKey === undefined || deviceKey === "") {
  console.error("DEVICE_KEY is not set. Copy .env.example to .env and set it.");
  process.exit(1);
}

const port = Number(process.env["PORT"] ?? 3000);
const webRoot = resolve(import.meta.dirname, "../..");

const store = new Store(process.env["STATE_FILE"] ?? resolve(webRoot, "data/state.json"));
const { app, close } = createApp({ deviceKey, store, clientDir: resolve(webRoot, "dist") });

const server = app.listen(port, "0.0.0.0", () => {
  console.log(`Zone Monitor website on port ${port}. Open one of these:`);
  console.log(`  http://localhost:${port}`);
  for (const addresses of Object.values(networkInterfaces())) {
    for (const address of addresses ?? []) {
      if (address.family === "IPv4" && !address.internal) {
        console.log(`  http://${address.address}:${port}`);
      }
    }
  }
  console.log("Set SERVER_URL on both boards to the address on their Wi-Fi network.");
});

const shutdown = (): void => {
  close();
  server.close(() => process.exit(0));
};
process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
