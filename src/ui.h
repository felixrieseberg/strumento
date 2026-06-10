#pragma once
#include <M5Unified.h>

namespace ui {
void begin();
void tick();        // call every loop; handles touch + redraw
void splash(const char* line);
void forceSetup();  // jump straight to Setup screen (first-boot)
void setDark(bool d);
void screenshot();  // dump RGB565 framebuffer as base64 to Serial
void debugScreen(int n, float arg);   // 0=home 1=brew(arg=sec) 2=ctrl 3=setup
}
