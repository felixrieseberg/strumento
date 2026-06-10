#include "lm_cloud.h"
#include "lm_crypto.h"
#include "storage.h"
#include "config.h"
#include "lm_ca.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <time.h>

namespace lmcloud {

static State                 g_state;
static std::function<void()> g_cb;
static WebSocketsClient      g_ws;
static String                g_access, g_refresh;
static time_t                g_tokenExpiry = 0;
static bool                  g_registered  = false;
static bool                  g_wsSubscribed = false;
static String                g_wsHdr;             // keep alive while WS holds the c_str
static uint32_t              g_nextPoll = 0, g_nextWifiTry = 0, g_nextWsTry = 0;

// cross-task command queue (UI → cloud task)
enum class Cmd : uint8_t { None, Power, Steam, CoffeeTemp, PreBrew, PreBrewTimes,
                           Backflush, Reconnect };
struct QItem { Cmd c; float f, f2; bool b; };
static QueueHandle_t g_q;

const State& state() { return g_state; }
void onChange(std::function<void()> cb) { g_cb = std::move(cb); }
static void changed() { g_state.lastUpdateMs = millis(); if (g_cb) g_cb(); }

// ── helpers ──────────────────────────────────────────────────────────────────
static void setNet(Net n, const String& msg = "") {
  if (g_state.net != n || g_state.netMsg != msg) { g_state.net = n; g_state.netMsg = msg; changed(); }
}

static MachineStatus parseMachine(const char* s) {
  if (!s) return MachineStatus::Unknown;
  if (!strcmp(s,"Brewing"))   return MachineStatus::Brewing;
  if (!strcmp(s,"PoweredOn")) return MachineStatus::PoweredOn;
  if (!strcmp(s,"StandBy"))   return MachineStatus::StandBy;
  if (!strcmp(s,"Off"))       return MachineStatus::Off;
  return MachineStatus::Unknown;
}
static BoilerStatus parseBoiler(const char* s) {
  if (!s) return BoilerStatus::Unknown;
  if (!strcmp(s,"Ready"))     return BoilerStatus::Ready;
  if (!strcmp(s,"HeatingUp")) return BoilerStatus::HeatingUp;
  if (!strcmp(s,"StandBy"))   return BoilerStatus::StandBy;
  if (!strcmp(s,"NoWater"))   return BoilerStatus::NoWater;
  if (!strcmp(s,"Off"))       return BoilerStatus::Off;
  return BoilerStatus::Unknown;
}

static void applyWidgets(JsonArrayConst widgets) {
  for (JsonObjectConst w : widgets) {
    const char* code = w["code"] | "";
    JsonObjectConst o = w["output"];
    if (!strcmp(code, "CMMachineStatus")) {
      g_state.machine        = parseMachine(o["status"] | "");
      g_state.brewingStartMs = o["brewingStartTime"].isNull() ? 0
                               : (int64_t)(o["brewingStartTime"].as<double>());
      JsonObjectConst lc = o["lastCoffee"];
      if (!lc.isNull()) {
        g_state.lastShotSec  = lc["extractionSeconds"] | g_state.lastShotSec;
        g_state.lastShotAtMs = lc["time"].isNull() ? g_state.lastShotAtMs
                               : (int64_t)(lc["time"].as<double>());
      }
      JsonObjectConst ns = o["nextStatus"];
      g_state.nextStandbyMs = (!ns.isNull() && !ns["startTime"].isNull())
                              ? (uint64_t)(ns["startTime"].as<double>()) : 0;
    } else if (!strcmp(code, "CMCoffeeBoiler")) {
      g_state.coffeeStatus = parseBoiler(o["status"] | "");
      g_state.coffeeTarget = o["targetTemperature"] | g_state.coffeeTarget;
    } else if (!strcmp(code, "CMSteamBoilerTemperature")) {
      g_state.steamStatus  = parseBoiler(o["status"] | "");
      g_state.steamEnabled = o["enabled"] | false;
    } else if (!strcmp(code, "CMPreBrewing")) {
      const char* m = o["mode"] | "Disabled";
      g_state.preBrewOn = strcmp(m, "Disabled") != 0;
      JsonArrayConst t = o["times"]["PreBrewing"];
      if (!t.isNull() && t.size() > 0) {
        g_state.preBrewIn  = t[0]["seconds"]["In"]  | g_state.preBrewIn;
        g_state.preBrewOut = t[0]["seconds"]["Out"] | g_state.preBrewOut;
      }
    }
  }
}

static void applyDashboard(JsonDocument& doc) {
  auto prevM = g_state.machine;
  g_state.modelName = doc["modelName"] | g_state.modelName;
  g_state.serial    = doc["serialNumber"] | g_state.serial;
  if (!doc["widgets"].isNull()) applyWidgets(doc["widgets"].as<JsonArrayConst>());
  if (g_state.machine != prevM)
    Serial.printf("[lm] machine %d -> %d (brewStart=%lld)\n",
                  (int)prevM, (int)g_state.machine, (long long)g_state.brewingStartMs);
  changed();
}

// ── REST ─────────────────────────────────────────────────────────────────────
static int httpJson(const char* method, const String& path, const String& body,
                    JsonDocument* out, bool auth) {
  WiFiClientSecure tls; tls.setCACert(LM_ROOT_CA);      // LM rotates certs; pinning is brittle
  HTTPClient http; http.setReuse(false); http.setTimeout(12000);
  String url = String(cfg::LM_API) + path;
  if (!http.begin(tls, url)) return -1;
  auto h = lmcrypto::sign();
  http.addHeader("X-App-Installation-Id", h.instId);
  http.addHeader("X-Timestamp",           h.timestamp);
  http.addHeader("X-Nonce",               h.nonce);
  http.addHeader("X-Request-Signature",   h.signature);
  if (auth) http.addHeader("Authorization", "Bearer " + g_access);
  http.addHeader("Content-Type", "application/json");
  int code = body.isEmpty() ? http.sendRequest(method)
                            : http.sendRequest(method, (uint8_t*)body.c_str(), body.length());
  if (out && code > 0 && code < 400) {
    String payload = http.getString();
    auto err = deserializeJson(*out, payload);
    if (err) Serial.printf("[lm] json err %s (%u bytes)\n", err.c_str(), payload.length());
  } else if (code >= 400) {
    Serial.printf("[lm] %s %s -> %d %s\n", method, path.c_str(), code,
                  http.getString().substring(0,200).c_str());
  }
  http.end();
  return code;
}

static bool registerInstall() {
  WiFiClientSecure tls; tls.setCACert(LM_ROOT_CA);
  HTTPClient http; http.setTimeout(12000);
  if (!http.begin(tls, String(cfg::LM_API) + "/auth/init")) return false;
  http.addHeader("X-App-Installation-Id", lmcrypto::installationId());
  http.addHeader("X-Request-Proof",       lmcrypto::initProof());
  http.addHeader("Content-Type", "application/json");
  String body = String("{\"pk\":\"") + lmcrypto::publicKeyB64() + "\"}";
  int code = http.POST(body);
  Serial.printf("[lm] /auth/init -> %d\n", code);
  http.end();
  return code >= 200 && code < 300;
}

static bool signIn() {
  JsonDocument body, resp;
  body["username"] = settings.lmUser;
  body["password"] = settings.lmPass;
  String s; serializeJson(body, s);
  int code = httpJson("POST", "/auth/signin", s, &resp, false);
  Serial.printf("[lm] /auth/signin -> %d\n", code);
  if (code != 200) { setNet(Net::WifiUp, "auth " + String(code)); return false; }
  g_access  = resp["accessToken"]  | "";
  g_refresh = resp["refreshToken"] | "";
  g_tokenExpiry = time(nullptr) + 50*60;   // tokens are ~1h; refresh at 50m
  return !g_access.isEmpty();
}

static bool refreshTok() {
  JsonDocument body, resp;
  body["username"]     = settings.lmUser;
  body["refreshToken"] = g_refresh;
  String s; serializeJson(body, s);
  int code = httpJson("POST", "/auth/refreshtoken", s, &resp, false);
  if (code != 200) return signIn();
  g_access  = resp["accessToken"]  | g_access;
  g_refresh = resp["refreshToken"] | g_refresh;
  g_tokenExpiry = time(nullptr) + 50*60;
  return true;
}

static void pollDashboard() {
  JsonDocument resp;
  int code = httpJson("GET", "/things/" + settings.lmSerial + "/dashboard", "", &resp, true);
  Serial.printf("[lm] /dashboard -> %d (heap=%u)\n", code, ESP.getFreeHeap());
  if (code == 200) applyDashboard(resp);
}

static bool sendCommand(const char* cmd, JsonDocument& body) {
  String s; serializeJson(body, s);
  int code = httpJson("POST", "/things/" + settings.lmSerial + "/command/" + cmd, s, nullptr, true);
  return code >= 200 && code < 300;
}

// ── public commands (UI thread → optimistic state + enqueue; cloud confirms) ─
static void enq(Cmd c, bool b=false, float f=0, float f2=0){
  QItem i{c,f,f2,b}; if(g_q) xQueueSend(g_q,&i,0);
}
bool setPower(bool on) {
  g_state.machine = on?MachineStatus::PoweredOn:MachineStatus::StandBy;
  if(on && g_state.coffeeStatus==BoilerStatus::Off) g_state.coffeeStatus=BoilerStatus::HeatingUp;
  changed(); enq(Cmd::Power,on); return true;
}
bool setSteam(bool on) {
  g_state.steamEnabled=on;
  g_state.steamStatus=on?BoilerStatus::HeatingUp:BoilerStatus::Off;
  changed(); enq(Cmd::Steam,on); return true;
}
bool setCoffeeTemp(float c) {
  g_state.coffeeTarget=roundf(c*10)/10; changed();
  enq(Cmd::CoffeeTemp,0,c); return true;
}
bool setPreBrew(bool on) {
  g_state.preBrewOn=on; changed(); enq(Cmd::PreBrew,on); return true;
}
bool setPreBrewTimes(float in,float out){
  in =constrain(roundf(in *10)/10, 1.f,9.f);
  out=constrain(roundf(out*10)/10, 1.f,9.f);
  g_state.preBrewIn=in; g_state.preBrewOut=out; changed();
  enq(Cmd::PreBrewTimes,false,in,out); return true;
}
bool startBackflush()       { enq(Cmd::Backflush);     return true; }
void reconnect()            { enq(Cmd::Reconnect); }

static void execCmd(const QItem& i){
  JsonDocument b;
  switch(i.c){
    case Cmd::Power:      b["mode"]=i.b?"BrewingMode":"StandBy";
                          sendCommand("CoffeeMachineChangeMode",b); break;
    case Cmd::Steam:      b["boilerIndex"]=1; b["enabled"]=i.b;
                          sendCommand("CoffeeMachineSettingSteamBoilerEnabled",b); break;
    case Cmd::CoffeeTemp: b["boilerIndex"]=1; b["targetTemperature"]=roundf(i.f*10)/10;
                          sendCommand("CoffeeMachineSettingCoffeeBoilerTargetTemperature",b); break;
    case Cmd::PreBrew:    b["mode"]=i.b?"PreBrewing":"Disabled";
                          sendCommand("CoffeeMachinePreBrewingChangeMode",b); break;
    case Cmd::PreBrewTimes:{
      auto t=b["times"].to<JsonObject>(); t["In"]=i.f; t["Out"]=i.f2;
      b["groupIndex"]=1; b["doseIndex"]="ByGroup";
      sendCommand("CoffeeMachinePreBrewingSettingTimes",b); break;
    }
    case Cmd::Backflush:  b["enabled"]=true;
                          sendCommand("CoffeeMachineBackFlushStartCleaning",b); break;
    case Cmd::Reconnect:
      g_ws.disconnect(); WiFi.disconnect(true,false);
      g_access=""; g_refresh=""; g_registered=false; g_wsSubscribed=false;
      g_nextWifiTry=0; g_nextPoll=0; g_nextWsTry=0;
      setNet(Net::Down,"reconnect");
      break;
    default: break;
  }
}

// ── STOMP over WS ────────────────────────────────────────────────────────────
static void stompSend(const char* verb, std::initializer_list<std::pair<const char*,String>> hdrs) {
  String f = verb; f += '\n';
  for (auto& kv : hdrs) { f += kv.first; f += ':'; f += kv.second; f += '\n'; }
  f += '\n'; f += '\x00';
  g_ws.sendTXT((uint8_t*)f.c_str(), f.length());
}

static const char* memfind(const char* hay, size_t hn, const char* needle, size_t nn) {
  if (nn > hn) return nullptr;
  for (size_t i = 0; i + nn <= hn; ++i)
    if (memcmp(hay + i, needle, nn) == 0) return hay + i;
  return nullptr;
}

static void onWsEvent(WStype_t t, uint8_t* p, size_t n) {
  switch (t) {
    case WStype_CONNECTED: {
      Serial.printf("[ws] WS upgrade ok (%.*s)\n", (int)min(n,(size_t)80), (char*)p);
      g_wsSubscribed = false;
      stompSend("CONNECT", {
        {"host", cfg::LM_HOST},
        {"accept-version", "1.2,1.1,1.0"},
        {"heart-beat", "0,0"},
        {"Authorization", "Bearer " + g_access},
      });
      break;
    }
    case WStype_TEXT: {
      const char* frame = (const char*)p;
      const char* nl = (const char*)memchr(frame, '\n', n);
      if (!nl) return;
      size_t verbLen = nl - frame;
      if (verbLen == 9 && !memcmp(frame, "CONNECTED", 9)) {
        Serial.println("[ws] STOMP CONNECTED, subscribing");
        stompSend("SUBSCRIBE", {
          {"destination", "/ws/sn/" + settings.lmSerial + "/dashboard"},
          {"ack", "auto"},
          {"id", lmcrypto::uuid4()},
          {"content-length", "0"},
        });
        g_wsSubscribed = true;
        setNet(Net::WsLive, "");
      } else if (verbLen == 7 && !memcmp(frame, "MESSAGE", 7)) {
        const char* body = memfind(frame, n, "\n\n", 2);
        if (!body) return;
        body += 2;
        size_t bodyLen = n - (body - frame);
        while (bodyLen && body[bodyLen-1] == '\0') --bodyLen;
        Serial.printf("[ws] MESSAGE %u bytes\n", (unsigned)bodyLen);
        JsonDocument doc;
        if (deserializeJson(doc, body, bodyLen) == DeserializationError::Ok)
          applyDashboard(doc);
      } else if (verbLen == 5 && !memcmp(frame, "ERROR", 5)) {
        Serial.printf("[ws] STOMP ERROR: %.*s\n", (int)min(n,(size_t)300), frame);
      } else {
        Serial.printf("[ws] STOMP %.*s (%u bytes)\n", (int)verbLen, frame, (unsigned)n);
      }
      break;
    }
    case WStype_DISCONNECTED: {
      Serial.printf("[ws] disconnected (was %s)\n", g_wsSubscribed?"live":"pending");
      g_wsSubscribed = false;
      if (g_state.net == Net::WsLive) setNet(Net::AuthOk, "ws lost");
      // library will auto-retry; give it fresh signed headers so the upgrade
      // isn't rejected on a stale X-Timestamp.
      auto h = lmcrypto::sign();
      g_wsHdr = h.asHeaderBlock();
      g_ws.setExtraHeaders(g_wsHdr.c_str());
      break;
    }
    case WStype_ERROR:
      Serial.printf("[ws] WS error: %.*s\n", (int)min(n,(size_t)120), (char*)p);
      break;
    default: break;
  }
}

static void wsStart() {
  g_ws.disconnect();
  auto h = lmcrypto::sign();
  g_wsHdr = h.asHeaderBlock();
  g_ws.setExtraHeaders(g_wsHdr.c_str());
  g_ws.onEvent(onWsEvent);
  g_ws.setReconnectInterval(5000);
  g_ws.enableHeartbeat(15000, 5000, 2);   // matches aiohttp heartbeat=15
  g_ws.beginSslWithCA(cfg::LM_HOST, 443, "/ws/connect", LM_ROOT_CA);
  Serial.println("[ws] beginSSL");
}

// ── lifecycle ────────────────────────────────────────────────────────────────
static bool ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (millis() < g_nextWifiTry) return false;
  Serial.printf("[wifi] connecting %s …\n", settings.wifiSsid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPass.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) delay(100);
  if (WiFi.status() != WL_CONNECTED) { g_nextWifiTry = millis() + 8000; return false; }
  Serial.printf("[wifi] up %s\n", WiFi.localIP().toString().c_str());
  configTzTime(cfg::TZ_POSIX, cfg::NTP_POOL);
  // wait briefly for SNTP — request signing needs sane epoch
  for (int i=0; i<40 && time(nullptr) < 1600000000; ++i) delay(250);
  setNet(Net::WifiUp, WiFi.localIP().toString());
  return true;
}

static bool ensureAuth() {
  if (!g_registered) { if (!registerInstall()) return false; g_registered = true; }
  if (g_access.isEmpty()) { if (!signIn()) return false; }
  else if (time(nullptr) + (time_t)cfg::TOKEN_REFRESH_SLOP_S > g_tokenExpiry) refreshTok();
  return true;
}

void begin() {
  g_state.serial = settings.lmSerial;
  g_q = xQueueCreate(8, sizeof(QItem));
}

void loop() {
  QItem qi;
  while (g_q && xQueueReceive(g_q,&qi,0)==pdTRUE) execCmd(qi);

  if (!ensureWifi()) { setNet(Net::Down, "wifi"); return; }
  if (!ensureAuth()) return;
  if (g_state.net < Net::AuthOk) {
    setNet(Net::AuthOk, "");
    pollDashboard();
    g_nextPoll = millis() + cfg::POLL_DASHBOARD_MS;
    wsStart();
  }

  g_ws.loop();

  if (millis() > g_nextPoll) {
    g_nextPoll = millis() + cfg::POLL_DASHBOARD_MS;
    if (!g_wsSubscribed) pollDashboard();   // REST fallback
  }
}

}  // namespace lmcloud
