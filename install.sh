#!/usr/bin/env bash
# Strumento — local build + flash for macOS / Linux.
set -euo pipefail
cd "$(dirname "$0")"

bold() { printf '\033[1m%s\033[0m\n' "$*"; }

# ── PlatformIO ───────────────────────────────────────────────────────────────
if command -v pio >/dev/null 2>&1; then
  PIO=(pio)
elif python3 -m platformio --version >/dev/null 2>&1; then
  PIO=(python3 -m platformio)
else
  bold "PlatformIO not found — installing to your user site-packages…"
  python3 -m pip install --user platformio
  PIO=(python3 -m platformio)
fi
bold "Using $("${PIO[@]}" --version)"

# ── Optional baked-in credentials ────────────────────────────────────────────
SECRETS=src/secrets.h
echo
bold "Bake WiFi + La Marzocco credentials into the firmware?"
echo "Skip to enter them on the device's Setup screen instead."
read -r -p "Configure now? [y/N] " ans
if [[ "${ans:-}" =~ ^[Yy]$ ]]; then
  read -r  -p "  WiFi SSID            : " WIFI_SSID
  read -rs -p "  WiFi password        : " WIFI_PASS; echo
  read -r  -p "  La Marzocco email    : " LM_USER
  read -rs -p "  La Marzocco password : " LM_PASS; echo
  read -r  -p "  Machine serial (optional): " LM_SERIAL
  esc() { printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'; }
  cat > "$SECRETS" <<EOF
#pragma once
#define SECRET_WIFI_SSID   "$(esc "$WIFI_SSID")"
#define SECRET_WIFI_PASS   "$(esc "$WIFI_PASS")"
#define SECRET_LM_USER     "$(esc "$LM_USER")"
#define SECRET_LM_PASS     "$(esc "$LM_PASS")"
#define SECRET_LM_SERIAL   "$(esc "$LM_SERIAL")"
EOF
  bold "Wrote $SECRETS (git-ignored)."
elif [[ ! -f "$SECRETS" ]]; then
  echo "No $SECRETS — building with empty defaults (on-device setup)."
fi

# ── Build + flash ────────────────────────────────────────────────────────────
echo
bold "Building + flashing (plug the Core2 in over USB-C)…"
"${PIO[@]}" run -t upload

echo
bold "Done. Tap SETUP on the device if you skipped credential entry."
