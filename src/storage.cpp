#include "storage.h"
#include <string.h>

Settings settings;
static SemaphoreHandle_t s_mtx = nullptr;
static inline void take(){ if(!s_mtx) s_mtx=xSemaphoreCreateMutex();
                           xSemaphoreTake(s_mtx,portMAX_DELAY); }
static inline void give(){ xSemaphoreGive(s_mtx); }

void Settings::load() {
  take();
  p_.begin("lm", false);
  wifiSsid = p_.getString("ws", cfg::DEF_WIFI_SSID);
  wifiPass = p_.getString("wp", cfg::DEF_WIFI_PASS);
  lmUser   = p_.getString("lu", cfg::DEF_LM_USER);
  lmPass   = p_.getString("lp", cfg::DEF_LM_PASS);
  lmSerial = p_.getString("ls", cfg::DEF_LM_SERIAL);
  darkMode   = p_.getBool("dm", false);
  fahrenheit = p_.getBool("fh", false);
  instId   = p_.getString("iid", "");
  size_t n = p_.getBytes("epk", ecPriv, sizeof ecPriv);
  ecPrivValid = (n == sizeof ecPriv);
  p_.end();
  give();
}

void Settings::save() {
  take();
  p_.begin("lm", false);
  p_.putString("ws", wifiSsid);
  p_.putString("wp", wifiPass);
  p_.putString("lu", lmUser);
  p_.putString("lp", lmPass);
  p_.putString("ls", lmSerial);
  p_.putBool  ("dm", darkMode);
  p_.putBool  ("fh", fahrenheit);
  p_.putString("iid", instId);
  if (ecPrivValid) p_.putBytes("epk", ecPriv, sizeof ecPriv);
  p_.end();
  give();
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
