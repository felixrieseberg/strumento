# crema — La Marzocco Linea Mini companion (M5Stack Core2)

A bench-top companion display for the Linea Mini. Talks directly to La
Marzocco's `lion.lamarzocco.io` cloud (the same backend the official app uses)
over WiFi — no Home Assistant, no BLE pairing, no proxy.

## What it does

| Screen | |
|---|---|
| **Home** | Analog-style coffee-boiler gauge (mimics the LM brew dial), steam status, last-shot time, pre-brew config. Power toggle. |
| **Brewing** | Auto-appears the moment the paddle is pulled. Full-screen 0.1 s shot timer with sweep arc. Synced to the machine's `brewingStartTime` so latency doesn't skew the reading. |
| **Controls** | Steam boiler on/off · Pre-brewing on/off · Brew temp ±0.5 °C · Backflush. |
| **Settings** | On-device QWERTY for WiFi + La Marzocco credentials. Stored in NVS. **Reconnect** applies changes. |

## How it talks to the machine

Gateway firmware ≥ v5 removed the local port-8081 API, so the only path to
real-time brew detection is the cloud STOMP WebSocket. The device:

1. Generates a P-256 keypair + installation UUID on first boot (persisted to NVS).
2. Registers the public key at `/api/customer-app/auth/init` with LM's custom
   rotation-hash proof.
3. Signs in with username/password → bearer token (refreshed at 50 min).
4. Every request carries `X-App-Installation-Id / X-Timestamp / X-Nonce /
   X-Request-Signature` — the signature is ECDSA-P256-SHA256 over
   `id.nonce.ts.proof`, DER-encoded, base64.
5. Subscribes to `wss://lion.lamarzocco.io/ws/connect` →
   `/ws/sn/<serial>/dashboard` (STOMP). State changes push instantly; a 30 s
   REST poll covers WS dropouts.

All control writes are `POST /things/<sn>/command/<CommandName>`.

## Build / flash

```sh
pio run -t upload          # builds + flashes /dev/cu.usbserial-*
python3 tools/monitor.py   # serial log (resets on attach)
```

Defaults are seeded from `src/config.h`; edit there or use the on-device
Settings screen.

## Layout

```
src/
  config.h        palette, layout, defaults
  storage.*       NVS-backed Settings
  lm_crypto.*     P-256 keypair, LM proof rotation, request signing
  lm_cloud.*      REST + STOMP-over-WSS client, state model, command queue
  keyboard.*      modal touchscreen QWERTY
  ui.*            screens (Home / Brewing / Controls / Settings)
  main.cpp        boot + FreeRTOS task split (cloud on core 0, UI on core 1)
tools/
  probe_cloud.py  desktop sanity check against the live API
  monitor.py      no-TTY serial monitor
```
