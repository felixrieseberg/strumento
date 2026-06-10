#pragma once
// ───────────────────────────────────────────────────────────────────────────────
//  La Marzocco "crema" palette — pulled from Linea Mini ID: warm ivory body,
//  near-black anthracite trim, LM heritage red badge, brushed-steel midtones.
// ───────────────────────────────────────────────────────────────────────────────
#include <stdint.h>

namespace cfg {

// ── STRUMENTO palette ── RGB565 ────────────────────────────────────────────
constexpr uint16_t IVORY     = 0xF77C;   // #F4EFE3  body enamel
constexpr uint16_t IVORY_LO  = 0xEF19;   // #E8E0CE  vignette shadow
constexpr uint16_t PAPER     = 0xFFBD;   // #FAF7EF  dial face
constexpr uint16_t INK       = 0x18C2;   // #1C1A17  ticks / text
constexpr uint16_t INK_40    = 0x8C2F;   // #8A8578  secondary text
constexpr uint16_t BRASS     = 0xB46A;   // #B08D57  bezel / hairlines
constexpr uint16_t BRASS_HI  = 0xCD2E;   // brass highlight
constexpr uint16_t LM_RED    = 0xC085;   // #C8102E  the only accent
constexpr uint16_t NIGHT     = 0x0861;   // #0E0E0C  brewing ground
constexpr uint16_t LUME      = 0xEF3A;   // #E8E4D0  luminous markers on dark

// Layout
constexpr int W = 320, H = 240;
constexpr int KEY_H  = 38;               // piano-key bar
constexpr int KEY_Y  = H - KEY_H;

// Network
constexpr const char* LM_HOST   = "lion.lamarzocco.io";
constexpr const char* LM_API    = "https://lion.lamarzocco.io/api/customer-app";
constexpr const char* NTP_POOL  = "pool.ntp.org";
constexpr const char* TZ_POSIX  = "PST8PDT,M3.2.0,M11.1.0";   // adjust in settings

// Factory-seeded defaults — overridden by src/secrets.h if present, and by
// the on-device Setup screen at runtime (persisted to NVS).
}  // close namespace to allow optional include
#if __has_include("secrets.h")
#  include "secrets.h"
#endif
#ifndef SECRET_WIFI_SSID
#  define SECRET_WIFI_SSID ""
#  define SECRET_WIFI_PASS ""
#  define SECRET_LM_USER   ""
#  define SECRET_LM_PASS   ""
#  define SECRET_LM_SERIAL ""
#endif
namespace cfg {
constexpr const char* DEF_WIFI_SSID = SECRET_WIFI_SSID;
constexpr const char* DEF_WIFI_PASS = SECRET_WIFI_PASS;
constexpr const char* DEF_LM_USER   = SECRET_LM_USER;
constexpr const char* DEF_LM_PASS   = SECRET_LM_PASS;
constexpr const char* DEF_LM_SERIAL = SECRET_LM_SERIAL;

// Timing
constexpr uint32_t POLL_DASHBOARD_MS = 30000;   // REST fallback when WS quiet
constexpr uint32_t TOKEN_REFRESH_SLOP_S = 600;  // refresh 10 min before expiry

}  // namespace cfg
