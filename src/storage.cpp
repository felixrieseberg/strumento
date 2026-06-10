#include "storage.h"
#include <string.h>

Settings settings;

void Settings::load() {
  p_.begin("lm", false);
  wifiSsid = p_.getString("ws", cfg::DEF_WIFI_SSID);
  wifiPass = p_.getString("wp", cfg::DEF_WIFI_PASS);
  lmUser   = p_.getString("lu", cfg::DEF_LM_USER);
  lmPass   = p_.getString("lp", cfg::DEF_LM_PASS);
  lmSerial = p_.getString("ls", cfg::DEF_LM_SERIAL);
  instId   = p_.getString("iid", "");
  size_t n = p_.getBytes("epk", ecPriv, sizeof ecPriv);
  ecPrivValid = (n == sizeof ecPriv);
  p_.end();
}

void Settings::save() {
  p_.begin("lm", false);
  p_.putString("ws", wifiSsid);
  p_.putString("wp", wifiPass);
  p_.putString("lu", lmUser);
  p_.putString("lp", lmPass);
  p_.putString("ls", lmSerial);
  p_.putString("iid", instId);
  if (ecPrivValid) p_.putBytes("epk", ecPriv, sizeof ecPriv);
  p_.end();
}

void Settings::factoryReset() {
  p_.begin("lm", false);
  p_.clear();
  p_.end();
  memset(ecPriv, 0, sizeof ecPriv);
  ecPrivValid = false;
  instId = "";
  load();
}
