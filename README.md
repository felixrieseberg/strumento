# Strumento

**The display I always wanted for La Marzocco espresso machines.**

A beautiful little device that adds an analog boiler-temp dial, shot timer,
and easy-to-reach controls for all your machines settings (power, steam,
pre-brew, backflush). 

To make your own, you just need a ~$50 device and a USB-C cable. You can
install this software directly [from your browser](https://felixrieseberg.github.io/strumento/)

## Making your own

First, buy some ESP32 hardware. I've used the $50 [M5Stack Core2](https://shop.m5stack.com/products/m5stack-core2-esp32-iot-development-kit) but you could probably use plenty of other
devices.

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

The device will land on the Setup screen. Tap each row and type your **WiFi**
name + password, then your **La Marzocco** email + password. Leave the serial
blank to auto-discover. Tap **Reconnect** and you're done — the boiler dial
goes live within a few seconds, and pulling the paddle flips to the shot timer.

Credentials are stored on the device.

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

## License

MIT — see [LICENSE](LICENSE). Not affiliated with or endorsed by La Marzocco S.r.l.
