#pragma once
#include <Preferences.h>
#include "config.h"

// Thin NVS wrapper holding all user-editable + crypto-persistent state.
struct Settings {
  String wifiSsid, wifiPass;
  String lmUser, lmPass, lmSerial;
  bool   darkMode   = false;
  bool   fahrenheit = false;
  uint8_t timeZoneIndex = cfg::DEF_TIME_ZONE_INDEX;
  uint8_t shotHoldIndex = cfg::DEF_SHOT_HOLD_INDEX;
  uint8_t dimSec    = 30;    // backlight dim after N s idle (0 = never)
  uint8_t sleepMin  = 2;     // AXP power-off after N min idle (0 = never)
  // crypto material — generated once, never shown
  String  instId;          // lowercase uuid
  uint8_t ecPriv[32];      // P-256 private scalar (raw big-endian)
  bool    ecPrivValid = false;

  void load();
  void save();
  void factoryReset();

  // The UI (core 1) edits these Strings while the cloud task (core 0) reads
  // them. Hold the lock around any cross-task String access. Recursive, so
  // save()/load() may be called while it is held.
  void lock();
  void unlock();

private:
  Preferences p_;
};

extern Settings settings;

struct SettingsLock {
  SettingsLock()  { settings.lock(); }
  ~SettingsLock() { settings.unlock(); }
  SettingsLock(const SettingsLock&) = delete;
  SettingsLock& operator=(const SettingsLock&) = delete;
};
