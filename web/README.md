# Zone Monitor website

Runs on a computer on the same Wi-Fi as the boards. The S3 connects for zones,
E-stop and QR text. The camera connects for scanner status, and sends pictures
only while this page is open.

## Run it

    npm install
    cp .env.example .env     # then set DEVICE_KEY to match the boards
    npm run build
    npm start

Needs Node 22.18 or newer. `npm start` prints the addresses to open. Set
`SERVER_URL` in `zone_monitor/secrets.h` and `zone_cam/secrets.h` to the
address on the boards' Wi-Fi network, then reflash both boards.

## Other commands

    npm run simulate      Pretends to be both boards, to try it with no hardware
    npm test              API tests
    npm run typecheck     Strict TypeScript check
    npm run dev:server    With dev:client in a second terminal, for live editing

## API (v1)

Board routes need the `x-device-key` header.

| Route | Used by | Purpose |
|---|---|---|
| `GET /api/v1/device/commands` | S3 | Resets and E-stop state |
| `POST /api/v1/device/level` | S3 | A zone's level |
| `POST /api/v1/device/scan` | S3 | QR text that was read |
| `POST /api/v1/device/camera/status` | Camera | Scanner status; the reply carries camera settings |
| `POST /api/v1/device/camera/frame` | Camera | A JPEG picture |
| `GET /api/v1/state` | Page | Everything, once |
| `GET /api/v1/events` | Page | Live updates |
| `GET /api/v1/camera/frame.jpg` | Page | Latest picture |
| `POST /api/v1/estop` | Page | `{ "on": true }` or `{ "on": false }` |
| `POST /api/v1/zones/reset` | Page | Reset all zones |
| `POST /api/v1/zones/:index/reset` | Page | Reset one zone |
| `PATCH /api/v1/camera` | Page | Flash, exposure, gain |

The page routes have no login. Anyone on the same Wi-Fi can use them.
