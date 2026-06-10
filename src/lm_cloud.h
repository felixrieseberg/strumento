#pragma once
#include <Arduino.h>
#include <functional>

namespace lmcloud {

enum class Net : uint8_t { Down, WifiUp, AuthOk, WsLive };
enum class MachineStatus : uint8_t { Unknown, Off, StandBy, PoweredOn, Brewing };
enum class BoilerStatus  : uint8_t { Unknown, Off, StandBy, HeatingUp, Ready, NoWater };

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
bool startBackflush();

// Connectivity nudges
void reconnect();                   // drop everything and re-auth (after creds edit)

}  // namespace lmcloud
