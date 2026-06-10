#pragma once
#include <M5Unified.h>

// Minimal on-screen QWERTY for credential entry. Blocking modal.
// Returns true if user tapped OK, false on cancel. `value` is edited in place.
bool keyboardPrompt(M5GFX& d, const char* title, String& value, bool mask = false);
