#include <M5Unified.h>
#include "config.h"
#include "storage.h"
#include "lm_crypto.h"
#include "lm_cloud.h"
#include "ui.h"

static TaskHandle_t g_cloudTask;
static void cloudTask(void*) {
  lmcloud::begin();
  for (;;) { lmcloud::loop(); vTaskDelay(pdMS_TO_TICKS(10)); }
}

void setup() {
  auto c = M5.config();
  c.internal_spk    = true;
  c.serial_baudrate = 115200;
  M5.begin(c);

  settings.load();
  ui::begin();
  if (settings.wifiSsid.isEmpty() || settings.lmUser.isEmpty()) {
    ui::forceSetup();                       // first boot — straight to credentials
  } else {
    ui::splash("waking up …");
  }

  if (!lmcrypto::init()) { ui::splash("crypto init failed"); delay(3000); }
  // Network/cloud on core 0; UI/touch stays on core 1 (Arduino loop).
  xTaskCreatePinnedToCore(cloudTask, "lmcloud", 12*1024, nullptr, 2, &g_cloudTask, 0);
}

void loop() {
  // serial debug shell: '0'..'3' force screen ('1'→brewing@24.7s), 's' screenshot
  while (Serial.available()) {
    int c=Serial.read();
    if (c>='0'&&c<='5') ui::debugScreen(c-'0', c=='1'?24.7f:0);
    else if (c=='d'||c=='l') ui::setDark(c=='d');
    else if (c=='s') { vTaskSuspend(g_cloudTask); ui::screenshot(); vTaskResume(g_cloudTask); }
  }
  ui::tick();
  delay(5);
}
