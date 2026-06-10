# Strumento

**The missing display for La Marzocco espresso machines.**
Runs standalone on an [M5Stack Core2](https://shop.m5stack.com/products/m5stack-core2-esp32-iot-development-kit).
The main screen is an analog boiler-temp dial; pull the paddle and the whole
display turns into a shot timer. Touch controls cover the rest (power, steam,
pre-brew, backflush). It talks straight to La Marzocco's cloud, so there's no
Home Assistant or Bluetooth bridge in the way.

### → [Flash it from your browser](https://OWNER.github.io/REPO/)

---

## Supported machines

| Machine | Status |
|---|---|
| Linea Mini | Fully tested |
| Linea Mini R | Expected to work (same widgets + steam-level) |
| Linea Micra | Expected to work (steam-level, tri-state pre-extraction) |
| GS3 AV | Expected to work (110°C scale, per-dose pre-brew shows DoseA) |
| GS3 MP | Likely works (untested; no pre-brew) |

The UI checks the machine's capability flags and hides anything that doesn't
apply, so whatever the official app can drive should work here too. Open an
issue with your model if it doesn't.

## Hardware

| | |
|---|---|
| **M5Stack Core2** (or Core2 v1.1) | ESP32, 16 MB flash, 320×240 capacitive touch |
| **USB-C cable** | must carry data, not charge-only |
| **La Marzocco app account** | the same email/password you use in the official app, with your machine already paired |
| **Chrome or Edge, desktop** | for the web installer (Safari & Firefox lack Web Serial) |

## Install (the easy way)

1. Plug the Core2 into your computer over USB-C.
2. Open the **[web installer](https://OWNER.github.io/REPO/)** in Chrome or Edge.
3. Click **Connect**, pick the `USB Single Serial` / `CP2104` port, then
   **Install Strumento**. ~90 seconds.

## First boot

1. The device lands on the Home gauge with no connection.
2. Tap **SETUP** (the right-hand piano key).
3. Type your **WiFi** name + password, then your **La Marzocco** email +
   password. The machine serial is optional — leave blank to auto-discover.
4. Tap **Reconnect**. The boiler needle goes live within a few seconds; pulling
   the paddle flips to the shot timer instantly.

Credentials are stored in the ESP32's NVS flash and survive re-flashing.

## Install (build from source)

```sh
git clone https://github.com/OWNER/REPO.git
cd REPO
./install.sh
```

The script installs PlatformIO if missing, optionally writes `src/secrets.h`
with your WiFi + LM credentials so the device connects on first boot, then
builds and flashes over USB. To do it by hand:

```sh
cp src/secrets.h.example src/secrets.h   # optional, edit to taste
pio run -t upload
python3 tools/monitor.py                 # serial log
```

---

## What it does

| Screen | |
|---|---|
| **Home** | Coffee-boiler temp on an analog gauge styled after the LM brew dial. Also steam status, last shot time and the current pre-brew setting. Tap to toggle power. |
| **Brewing** | Takes over automatically when you pull a shot. Big 0.1 s timer with a sweep arc, synced to the machine's `brewingStartTime` so network lag doesn't throw the count off. |
| **Controls** | Steam boiler on/off, pre-brewing on/off, brew temp ±0.5 °C, backflush. |
| **Settings** | On-device keyboard for WiFi and La Marzocco credentials. Saved to NVS. Hit **Reconnect** to apply. |

## How it talks to the machine

Gateway firmware ≥ v5 removed the local port-8081 API, so the only path to
real-time brew detection is La Marzocco's cloud STOMP WebSocket
(`lion.lamarzocco.io` — the same backend the official app uses). The device:

1. Generates a P-256 keypair + installation UUID on first boot (persisted to NVS).
2. Registers the public key at `/api/customer-app/auth/init` with LM's custom
   rotation-hash proof.
3. Signs in with username/password → bearer token (refreshed at 50 min).
4. Signs every request with `X-App-Installation-Id / X-Timestamp / X-Nonce /
   X-Request-Signature` — ECDSA-P256-SHA256 over `id.nonce.ts.proof`, DER, base64.
5. Subscribes to `wss://lion.lamarzocco.io/ws/connect` →
   `/ws/sn/<serial>/dashboard` (STOMP). State pushes instantly; a 30 s REST
   poll covers WS dropouts.

All control writes are `POST /things/<sn>/command/<CommandName>`.

## Repo layout

```
src/
  config.h        palette, layout, defaults
  storage.*       NVS-backed Settings
  lm_crypto.*     P-256 keypair, LM proof rotation, request signing
  lm_cloud.*      REST + STOMP-over-WSS client, state model, command queue
  keyboard.*      modal touchscreen QWERTY
  ui.*            screens (Home / Brewing / Controls / Settings)
  main.cpp        boot + FreeRTOS task split (cloud on core 0, UI on core 1)
tools/            desktop probe, font generator, serial monitor, screenshot
web/              ESP Web Tools installer page (served via GitHub Pages)
install.sh        guided local build + flash
```

## License

MIT — see [LICENSE](LICENSE). Not affiliated with or endorsed by La Marzocco S.r.l.
