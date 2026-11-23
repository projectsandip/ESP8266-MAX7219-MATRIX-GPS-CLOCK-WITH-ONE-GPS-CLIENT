#include "UIRenderer.h"
#include "config.h"
#include "FontText5x7.h"

UIRenderer::UIRenderer(DisplayManager &display, SettingsManager &settings)
  : _display(display), _settings(settings),
    _lastHeartbeatToggle(0), _heartbeatState(false),
    _lastDatePeekCheckMillis(0), _datePeekStartMillis(0), _inDatePeek(false),
    _lastAppliedBrightness(255),
    _startupPhase(StartupPhase::CONNECTING),
    _scrollMessage(""), _scrollX(0), _lastScrollStepMillis(0) {}

// ---------------------------------------------------------------------------
// Generic scrolling-text engine. Text moves right-to-left (enters at the
// right edge, exits at the left) — standard ticker/marquee direction, so
// it reads naturally left-to-right as it passes.
//
// A message already mid-scroll always finishes its current pass before a
// different desiredMessage is adopted (the switch only happens at a pass
// boundary) — this is what makes each message "wait to show the full
// text" rather than being cut off the instant the underlying state
// changes. Passing the SAME desiredMessage across many calls naturally
// makes it loop, since each completed pass just restarts the same text.
// ---------------------------------------------------------------------------
bool UIRenderer::stepScroll(const String &desiredMessage) {
  bool justCompletedCurrentMessage = false;

  if (_scrollMessage.length() == 0) {
    // First message ever shown — nothing in progress to protect, start
    // immediately.
    _scrollMessage = desiredMessage;
    _scrollX = MATRIX_WIDTH;
    _lastScrollStepMillis = millis();
  }

  if (millis() - _lastScrollStepMillis >= SCROLL_STEP_INTERVAL_MS) {
    _lastScrollStepMillis = millis();
    _scrollX--;

    int16_t textWidthPx = (int16_t)_scrollMessage.length() * (FONT_TEXT_WIDTH + 1);
    if (_scrollX < -textWidthPx) {
      // This pass just finished — report completion of whatever was
      // actually showing, then adopt the (possibly new) desired message
      // for the next pass.
      justCompletedCurrentMessage = true;
      _scrollMessage = desiredMessage;
      _scrollX = MATRIX_WIDTH;
    }
  }

  int16_t x = _scrollX;
  for (size_t i = 0; i < _scrollMessage.length(); i++) {
    x = _display.drawTextGlyph(x, 0, _scrollMessage[i]);
  }

  return justCompletedCurrentMessage;
}

String UIRenderer::buildFallbackMessage() const {
  String msg = "UNABLE TO CONNECT TO WIFI...STARTED ACCESS POINT...";
  msg += FALLBACK_AP_SSID;
  msg += "....HTTP://192.168.4.1  PASSWORD:";
  msg += FALLBACK_AP_PASSWORD;
  return msg;
}

// ---------------------------------------------------------------------------
// Startup sequence: CONNECTING -> (CONNECTED_ANNOUNCE) -> DONE.
// Replaces the old wipe-bar startup animation — this runs continuously
// through the normal non-blocking render loop rather than a separate
// blocking routine, so WiFi connection, OTA, and the admin web server all
// keep working normally throughout.
// ---------------------------------------------------------------------------
void UIRenderer::renderStartupSequence(const ClientTime &t) {
  if (t.inFallbackApMode) {
    // Takes priority over the plain "connecting" message — more useful
    // information, and takes over for as long as fallback AP mode lasts.
    stepScroll(buildFallbackMessage());
    return;
  }

  if (_startupPhase == StartupPhase::CONNECTING) {
    if (!t.wifiConnected) {
      stepScroll("CONNECTING TO WIFI...");
      return;
    }

    // WiFi is connected — let whatever's currently mid-scroll (the
    // connecting message, or a just-exited fallback message) finish
    // naturally, then this adopts the "connected" announcement.
    String desired = String("CONNECTED TO '") + _settings.serverSsid() + "'";
    bool wasAlreadyShowingTarget = (_scrollMessage == desired);
    bool completed = stepScroll(desired);

    if (wasAlreadyShowingTarget && completed) {
      // The connected announcement itself — not just the transition
      // into it — just finished a full pass.
      _startupPhase = StartupPhase::DONE;
    }
  }
}

// ---------------------------------------------------------------------------
// Frame dispatch
// ---------------------------------------------------------------------------
void UIRenderer::renderFrame(const ClientTime &t) {
  applyBrightnessIfNeeded(t);
  _display.clear();

  // 0. Startup sequence takes over the whole display until it completes
  // once; never revisited afterward even if WiFi drops again later.
  if (_startupPhase != StartupPhase::DONE) {
    renderStartupSequence(t);
    _display.flip();
    return;
  }

  // 1. Permanent error: 24h freewheel budget exhausted. This is a one-way
  // latch (see TimeSync) — once shown, it stays shown even if a sync
  // succeeds again later.
  if (t.permanentError) {
    drawPermanentErrorScreen(t);
    _display.flip();
    return;
  }

  // 2. Fallback AP mode reached post-startup (a persistent WiFi loss, not
  // just a brief blip) — same scrolling recovery message as during
  // startup, genuinely more useful here than the brief flash below.
  if (t.inFallbackApMode) {
    stepScroll(buildFallbackMessage());
    _display.flip();
    return;
  }

  // 3. Periodic WiFi-down flash: while WiFi is disconnected (but still
  // within the 24h budget, and not yet in fallback AP mode), interrupt
  // the clock every WIFI_ERROR_FLASH_INTERVAL_MS for
  // WIFI_ERROR_FLASH_DURATION_MS, then resume the clock. The display is
  // never blank/stopped in between.
  if (!t.wifiConnected) {
    unsigned long phase = millis() % WIFI_ERROR_FLASH_INTERVAL_MS;
    if (phase < WIFI_ERROR_FLASH_DURATION_MS) {
      drawWifiDownFlashScreen(t);
      _display.flip();
      return;
    }
  }

  // 4. Never synced yet (e.g. still waiting on first contact with the
  // server) — nothing meaningful to show as a clock, but keep the display
  // alive rather than showing a bogus 1970 timestamp.
  if (!t.hasEverSynced) {
    drawStatusPixels(t);
    _display.flip();
    return;
  }

  // 5. Normal clock, freewheeling on the last good sync as needed.
  bool datePeekEnabled = _settings.enableDatePeek();
  if (datePeekEnabled) {
    if (!_inDatePeek && millis() - _lastDatePeekCheckMillis > DATE_PEEK_INTERVAL_MS) {
      _inDatePeek = true;
      _datePeekStartMillis = millis();
      _lastDatePeekCheckMillis = millis();
    }
    if (_inDatePeek && millis() - _datePeekStartMillis > DATE_PEEK_DURATION_MS) {
      _inDatePeek = false;
      _lastDatePeekCheckMillis = millis();
    }
  } else {
    _inDatePeek = false; // in case it was toggled off mid-peek
  }

  if (_inDatePeek) {
    drawDatePeekScreen(t);
  } else {
    drawClockScreen(t);
  }

  drawStatusPixels(t);
  _display.flip();
}

// ---------------------------------------------------------------------------
// Clock screen: HH:MM uses 4x7 digits, SS uses 3x7 digits, row 7 holds the
// single status indicator.
// ---------------------------------------------------------------------------
void UIRenderer::drawClockScreen(const ClientTime &t) {
  const uint8_t y = 0;
  // HH:MM uses 4x7 numerals; seconds retain the compact 3x7 font.
  _display.drawLargeDigit(0, y, t.hour / 10);
  _display.drawLargeDigit(5, y, t.hour % 10);
  _display.drawColon(10, y);
  _display.drawLargeDigit(12, y, t.minute / 10);
  _display.drawLargeDigit(17, y, t.minute % 10);
  _display.drawColon(22, y);
  _display.drawDigit(24, y, t.second / 10);
  _display.drawDigit(28, y, t.second % 10);
}

// ---------------------------------------------------------------------------
// Date peek: DD-MM, centered
// ---------------------------------------------------------------------------
void UIRenderer::drawDatePeekScreen(const ClientTime &t) {
  uint8_t x = 7; // (32 - 18)/2
  const uint8_t y = 0;

  x = _display.drawDigit(x, y, t.day / 10);    x += 1;
  x = _display.drawDigit(x, y, t.day % 10);    x += 1;
  x = _display.drawDash(x, y);                 x += 1;
  x = _display.drawDigit(x, y, t.month / 10);  x += 1;
  x = _display.drawDigit(x, y, t.month % 10);
}

// ---------------------------------------------------------------------------
// Single status indicator at the bottom-right corner (column 31, row 7).
// WiFi loss blinks fastest; otherwise the blink rate reflects GPS lock.
// ---------------------------------------------------------------------------
void UIRenderer::drawStatusPixels(const ClientTime &t) {
  unsigned long blinkInterval;
  if (!t.wifiConnected) {
    blinkInterval = WIFI_INDICATOR_NO_WIFI_BLINK_MS;
  } else if (!t.gpsFixValid) {
    blinkInterval = WIFI_INDICATOR_GPS_UNLOCKED_BLINK_MS;
  } else {
    blinkInterval = WIFI_INDICATOR_GPS_LOCKED_BLINK_MS;
  }
  _display.setPixel(31, 7, (millis() / blinkInterval) % 2 == 0);
}

// ---------------------------------------------------------------------------
// WiFi-down flash: a blinking "!" — shown periodically (not continuously)
// while WiFi is disconnected but we're still within the 24h freewheel
// budget and not yet in fallback AP mode. The clock resumes in between
// flashes.
// ---------------------------------------------------------------------------
void UIRenderer::drawExclamation(uint8_t xCenter, uint8_t y, bool on) {
  for (uint8_t row = 0; row <= 4; row++) {
    _display.setPixel(xCenter, y + row, on);
  }
  _display.setPixel(xCenter, y + 6, on);
}

void UIRenderer::drawWifiDownFlashScreen(const ClientTime &t) {
  if (millis() - _lastHeartbeatToggle > ERROR_FLASH_BLINK_MS) {
    _lastHeartbeatToggle = millis();
    _heartbeatState = !_heartbeatState;
  }

  // Three blinking exclamation marks spaced across the display
  drawExclamation(7,  0, _heartbeatState);
  drawExclamation(15, 0, _heartbeatState);
  drawExclamation(23, 0, _heartbeatState);

  // Bottom row: WiFi status pixel still shown (will be off here, since
  // this screen only appears while WiFi is actually down) for consistency
  // with the normal clock screen's status row.
  _display.setPixel(29, 7, false);
}

// ---------------------------------------------------------------------------
// Permanent error: solid (non-blinking) "!" marks plus a solid bottom row —
// visually distinct from the periodic blinking flash above, since this one
// means the 24h freewheel budget is exhausted and won't clear on its own.
// ---------------------------------------------------------------------------
void UIRenderer::drawPermanentErrorScreen(const ClientTime &t) {
  (void)t; // no per-frame state needed — this screen is static and solid
  drawExclamation(7,  0, true);
  drawExclamation(15, 0, true);
  drawExclamation(23, 0, true);
  _display.setRow(7, 0xFFFFFFFFUL); // solid bottom row, distinguishes this from the blinking flash screen
}

// ---------------------------------------------------------------------------
// Night brightness — a second, typically dimmer brightness level applied
// during a configurable hour range (client-local time, after the timezone
// offset). No ambient light sensor needed. Recomputed fresh every frame
// from current settings rather than applied once on save, so it always
// reflects the current hour and current settings regardless of when
// either changed — see the header comment on applyBrightnessIfNeeded().
// ---------------------------------------------------------------------------
static bool isWithinNightWindow(uint8_t hour, uint8_t startHour, uint8_t endHour) {
  if (startHour == endHour) return false; // zero-width window = never
  if (startHour < endHour) {
    return hour >= startHour && hour < endHour;
  }
  // Window wraps past midnight, e.g. start=22, end=6
  return hour >= startHour || hour < endHour;
}

void UIRenderer::applyBrightnessIfNeeded(const ClientTime &t) {
  uint8_t desired = _settings.matrixBrightness();

  if (_settings.enableNightBrightness() && t.hasEverSynced) {
    if (isWithinNightWindow(t.hour, _settings.nightStartHour(), _settings.nightEndHour())) {
      desired = _settings.nightBrightness();
    }
  }

  if (desired != _lastAppliedBrightness) {
    _display.setBrightness(desired);
    _lastAppliedBrightness = desired;
  }
}
