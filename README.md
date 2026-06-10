# Strumento

**The display I always wanted for La Marzocco espresso machines (Linea Mini,
Micra, GS3).** A beautiful little device that adds an analog boiler-temp dial,
shot timer, and easy-to-reach controls for all your machines settings (power,
steam, pre-brew, backflush). To make your own, you just need a ~$50 device and
a USB-C cable. You can then install this firmware directly
[from your browser](https://felixrieseberg.github.io/strumento/).

## Making your own

I built this on a $50
[M5Stack Core2](https://shop.m5stack.com/products/m5stack-core2-esp32-iot-development-kit)
because it's a tidy package, but the firmware isn't married to it. It's
written against [M5Unified](https://github.com/m5stack/M5Unified), so other
boards in that family (Core2 v1.1, CoreS3, Tough) should work with at most a
`platformio.ini` board swap. If you want to port it to something else
entirely, the actual requirements are:

- ESP32 with WiFi and PSRAM
- A 320×240 touch display that LovyanGFX can drive
- ~2 MB of flash for the firmware

The pre-built web installer image is Core2-only; anything else means building
from source ([BUILDING.md](BUILDING.md)).

You also need the **La Marzocco app account** you already have (the
email/password you use in the official app, with your machine paired) and a
desktop running **Chrome or Edge** for the web installer.

### Install

1. Plug the Core2 into your computer over USB-C.
2. Open the **[web installer](https://felixrieseberg.github.io/strumento/)** in
   Chrome or Edge.
3. Click **Connect**, pick the `USB Single Serial` / `CP2104` port, then
   **Install Strumento**. Takes about 90 seconds.

(Want to build it yourself? See [BUILDING.md](BUILDING.md).)

### First boot

The device will land on the Setup screen. Tap each row and type your WiFi name
+ password, then your La Marzocco email + password. Leave the serial blank to
auto-discover. Tap Reconnect and you're done — the boiler dial goes live within
a few seconds, and pulling the paddle flips to the shot timer.

Credentials are stored on the device and only used to log into your Wifi
network and La Marzocco's cloud.

## Supported machines

| Machine | Status |
|---|---|
| Linea Mini | Fully tested |
| Linea Mini R | Expected to work |
| Linea Micra | Expected to work |
| GS3 AV | Untested |
| GS3 MP | Untested |

Open an issue with your model if something's off.

## License

MIT — see [LICENSE](LICENSE). Not affiliated with or endorsed by La Marzocco
S.r.l.
