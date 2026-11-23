#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include <Arduino.h>
#include "DisplayManager.h"
#include "TimeSync.h"
#include "SettingsManager.h"

class UIRenderer {
public:
  UIRenderer(DisplayManager &display, SettingsManager &settings);

  // Call every loop() iteration with the latest time snapshot. Internally
  // decides whether to show the startup scroll sequence, the clock, the
  // date peek, or a warning/recovery screen.
  void renderFrame(const ClientTime &t);

private:
  DisplayManager &_display;
  SettingsManager &_settings;

  unsigned long _lastHeartbeatToggle;
  bool _heartbeatState;

  unsigned long _lastDatePeekCheckMillis;
  unsigned long _datePeekStartMillis;
  bool _inDatePeek;

  // Sentinel 255 = "never applied yet", forces the first real comparison
  // in applyBrightnessIfNeeded() to always apply on the very first frame.
  uint8_t _lastAppliedBrightness;

  // --- Startup scroll sequence ---
  // CONNECTING: loop "CONNECTING TO WIFI..." (or the fallback-AP message,
  // if that's active) until WiFi connects.
  // CONNECTED_ANNOUNCE: WiFi just connected — let the in-progress message
  // finish, then show "CONNECTED TO '<ssid>'" for exactly one full pass.
  // DONE: normal operation from here on; this phase never revisits
  // CONNECTING/CONNECTED_ANNOUNCE even if WiFi drops again later (that's
  // handled separately, post-startup, by the existing flash/fallback
  // screens below).
  enum class StartupPhase { CONNECTING, CONNECTED_ANNOUNCE, DONE };
  StartupPhase _startupPhase;

  // --- Generic scrolling-text engine ---
  // A message already mid-scroll always finishes its current pass before
  // a different desired message is adopted — this is what makes each
  // message "wait to show the full text" rather than being cut off the
  // moment the underlying state changes.
  String _scrollMessage;
  int16_t _scrollX;
  unsigned long _lastScrollStepMillis;

  void drawClockScreen(const ClientTime &t);
  void drawDatePeekScreen(const ClientTime &t);
  void drawWifiDownFlashScreen(const ClientTime &t);  // periodic, blinking — WiFi down but still within the 24h freewheel budget
  void drawPermanentErrorScreen(const ClientTime &t); // latched, solid — 24h freewheel budget exhausted
  void drawStatusPixels(const ClientTime &t);
  void drawExclamation(uint8_t xCenter, uint8_t y, bool on);

  // Advances/draws the scrolling-text engine by one frame. Returns true
  // exactly once, on the frame where the message *currently being
  // displayed* completes a full pass — not on the frame where it merely
  // switches over to displaying a new desiredMessage.
  bool stepScroll(const String &desiredMessage);

  // Drives the startup sequence (CONNECTING / CONNECTED_ANNOUNCE) until
  // it reaches DONE. Not called again afterward.
  void renderStartupSequence(const ClientTime &t);

  // Builds the fallback-AP recovery message, e.g.:
  // "UNABLE TO CONNECT TO WIFI...STARTED ACCESS POINT...GPS_CLIENT_SETUP....HTTP://192.168.4.1  PASSWORD:SETUP1234"
  String buildFallbackMessage() const;

  // Recomputes the desired brightness fresh from current settings + time
  // every frame, only issuing an actual SPI write when it changes. This
  // (rather than a one-shot "apply on save" hook) is what makes night
  // brightness correctly self-correct regardless of when settings change
  // relative to the current hour, with no risk of a stale override.
  void applyBrightnessIfNeeded(const ClientTime &t);
};

#endif // UI_RENDERER_H
