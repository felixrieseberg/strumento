#pragma once
#include <Arduino.h>
#include <functional>
#include <vector>

namespace lmcloud {

enum class Net : uint8_t { Down, WifiUp, AuthOk, WsLive };
enum class MachineStatus : uint8_t { Unknown, Off, StandBy, PoweredOn, Brewing };
enum class BoilerStatus  : uint8_t { Unknown, Off, StandBy, HeatingUp, Ready, NoWater };

struct WakeSched {
  String  id;
  bool    enabled = false, steam = false;
  int     onMin = 0, offMin = 0;          // minutes since midnight
  uint8_t dayMask = 0;                    // bit0=Mon … bit6=Sun
};
struct Firmware { String type, version, status; };

struct State {
  Net           net          = Net::Down;
  String        netMsg;                          // human hint for top bar
  MachineStatus machine      = MachineStatus::Unknown;
  // coffee boiler
  BoilerStatus  coffeeStatus = BoilerStatus::Unknown;
  float         coffeeTarget = 0;
  // steam boiler
  BoilerStatus  steamStatus  = BoilerStatus::Unknown;
  bool          steamEnabled = false;
  // pre-brew
  bool          preBrewOn    = false;
  float         preBrewIn    = 0, preBrewOut = 0;
  // shot timer
  int64_t       brewingStartMs = 0;              // server epoch ms; 0 = not brewing
  float         lastShotSec    = 0;
  int64_t       lastShotAtMs   = 0;
  // smart standby
  bool          sbEnabled    = false;
  int           sbMinutes    = 30;
  bool          sbAfterBrew  = true;             // true=LastBrewing, false=PowerOn
  // settings/stats — slow poll
  bool          plumbedIn    = false;
  int           wifiRssi     = 0;
  int           totalCoffee  = 0, totalFlush = 0;
  int64_t       lastCleanMs  = 0;                // CMBackFlush.lastCleaningStartTime
  std::vector<Firmware>  firmwares;
  std::vector<WakeSched> schedules;
  // misc
  uint64_t      nextStandbyMs  = 0;
  String        modelName, serial;
  uint32_t      lastUpdateMs   = 0;              // millis() of last good parse
};

// Lifecycle
void begin();                       // call once after settings.load() + WiFi up
void loop();                        // call every iteration
const State& state();
void onChange(std::function<void()> cb);

// Commands (fire-and-forget; state updates arrive via WS)
bool setPower(bool on);
bool setSteam(bool on);
bool setCoffeeTemp(float c);
bool setPreBrew(bool on);
bool setPreBrewTimes(float secIn, float secOut);
bool setSmartStandby(bool enabled, int minutes, bool afterLastBrew);
bool startBackflush();

// Connectivity nudges
void reconnect();                   // drop everything and re-auth (after creds edit)

}  // namespace lmcloud
