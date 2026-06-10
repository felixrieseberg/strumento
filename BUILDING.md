# Building from source

```sh
git clone https://github.com/felixrieseberg/strumento.git
cd strumento
./install.sh
```

The script installs PlatformIO if you don't have it, optionally writes
`src/secrets.h` so the device connects on first boot, then builds and flashes
over USB. To do it by hand:

```sh
cp src/secrets.h.example src/secrets.h   # optional, edit to taste
pio run -t upload
python3 tools/monitor.py                 # serial log
```

## How it talks to the machine

Gateway firmware ≥ v5 removed the local port-8081 API, so the only path to
real-time brew detection is La Marzocco's cloud STOMP WebSocket
(`lion.lamarzocco.io`, the same backend the official app uses). The device:

1. Generates a P-256 keypair + installation UUID on first boot (persisted to NVS).
2. Registers the public key at `/api/customer-app/auth/init` with LM's custom
   rotation-hash proof.
3. Signs in with username/password → bearer token (refreshed at 50 min).
4. Signs every request with `X-App-Installation-Id / X-Timestamp / X-Nonce /
   X-Request-Signature` (ECDSA-P256-SHA256 over `id.nonce.ts.proof`, DER, base64).
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
  ui.*            screens (Home / Brewing / Controls / Settings / Stats)
  main.cpp        boot + FreeRTOS task split (cloud on core 0, UI on core 1)
tools/            font generator, version stamp, serial monitor, screenshot
web/              ESP Web Tools installer page (served via GitHub Pages)
install.sh        guided local build + flash
```

## Screenshot harness

```sh
.venv/bin/python tools/shot.py          # captures every screen to shots/*.png
.venv/bin/python tools/shot.py --dark   # dark-mode variants
```

Serial debug shell: `0`-`5` force a screen, `d`/`l` toggle theme, `s` dump
framebuffer.

## Regenerating fonts

```sh
python3 tools/mkfont.py     # rewrites src/fonts/*.h from macOS system Cochin/Futura
```
