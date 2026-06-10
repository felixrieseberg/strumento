#pragma once
#include <Preferences.h>
#include "config.h"

// Thin NVS wrapper holding all user-editable + crypto-persistent state.
struct Settings {
  String wifiSsid, wifiPass;
  String lmUser, lmPass, lmSerial;
  bool   darkMode = false;
  // crypto material — generated once, never shown
  String  instId;          // lowercase uuid
  uint8_t ecPriv[32];      // P-256 private scalar (raw big-endian)
  bool    ecPrivValid = false;

  void load();
  void save();
  void factoryReset();

private:
  Preferences p_;
};

extern Settings settings;
