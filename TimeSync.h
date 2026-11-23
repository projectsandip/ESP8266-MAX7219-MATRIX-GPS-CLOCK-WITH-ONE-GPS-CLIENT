#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <Arduino.h>
#include "SettingsManager.h"

struct ClientTime {
  uint16_t year;
  uint8_t  month, day, hour, minute, second;
  uint16_t millisecond;

  bool     gpsFixValid;

  bool     wifiConnected;
  bool     inFallbackApMode; // true while broadcasting the fallback config
                              // AP (see TimeSync::isInFallbackApMode()) —
                              // exposed here so UIRenderer only needs
                              // ClientTime, not a TimeSync reference
  bool     hasEverSynced;
  unsigned long msSinceLastSuccessfulSync; // counts from boot if never synced
  bool     permanentError; // true once FREEWHEEL_PERMANENT_ERROR_MS has
                             // elapsed since the last successful sync — once
                             // set, it latches and never clears again, even
                             // if a later sync succeeds (see TimeSync)

  // Calendar date/time (client-local, timezone-adjusted) captured at the
  // moment of the last successful sync — distinct from year/month/day/...
  // above, which is the continuously-extrapolated *current* time. Only
  // meaningful once hasEverSynced is true.
  uint16_t lastSyncYear;
  uint8_t  lastSyncMonth, lastSyncDay, lastSyncHour, lastSyncMinute, lastSyncSecond;
};

class TimeSync {
public:
  TimeSync(SettingsManager &settings);

  void begin();

  // Call every loop() iteration. Drives the non-blocking WiFi connection
  // state machine (including fallback AP mode, see below) and triggers a
  // sync round every settings.syncIntervalMs() (that round itself makes
  // a few short blocking HTTP calls, which is fine at this interval).
  void update();

  // Extrapolates from the last accepted sync anchor using millis().
  ClientTime getCurrentTime();

  // Force an immediate sync attempt regardless of the interval timer
  // (used during the startup animation to get a first fix ASAP).
  bool syncNow();

  // Re-applies the currently configured WiFi credentials — call after a
  // settings change if you want to reconnect without a full reboot (the
  // settings page currently prompts for a reboot instead, but this is
  // here if that ever changes).
  void reconnectWithCurrentCredentials();

  // True while broadcasting the fallback configuration AP (see
  // FALLBACK_AP_SSID in config.h) — the client couldn't reach the
  // configured WiFi network after WIFI_MAX_FAILED_ATTEMPTS tries, so it's
  // hosting its own temporary hotspot for /settings to remain reachable,
  // while still retrying the real network in the background.
  bool isInFallbackApMode() const { return _wifiState == WiFiState::FALLBACK_AP; }

private:
  SettingsManager &_settings;

  bool          _hasAnchor;
  uint32_t      _anchorEpoch;      // whole seconds, from server's reported time
  uint16_t      _anchorSubMs;      // 0-999, fractional part server reported —
                                    // dropping this was the cause of visible
                                    // second jumps/stutter on every sync
  unsigned long _anchorMillis;     // millis() at the moment the anchor was captured
  unsigned long _lastSuccessMillis;
  unsigned long _lastSyncAttemptMillis;

  bool     _lastGpsFixValid;

  // Calendar snapshot (client-local, timezone-adjusted) captured at the
  // moment of the last successful sync — see ClientTime::lastSync*.
  uint16_t _lastSyncYear;
  uint8_t  _lastSyncMonth, _lastSyncDay, _lastSyncHour, _lastSyncMinute, _lastSyncSecond;

  bool          _permanentErrorLatched;

  // --- WiFi connection state machine ---
  enum class WiFiState { RETRYING, CONNECTED, FALLBACK_AP };
  enum class PollResult { IDLE, WAITING, JUST_TIMED_OUT, CONNECTED };

  WiFiState     _wifiState;
  bool          _everConnectedThisBoot;
  uint8_t       _consecutiveFailedAttempts;

  bool          _attemptInProgress;
  unsigned long _attemptStartMillis;
  unsigned long _lastAttemptMillis;

  void updateWiFiState();
  // Kicks off a non-blocking WiFi.begin() no more often than
  // retryIntervalMs, and polls an in-progress attempt against
  // WIFI_CONNECT_TIMEOUT_MS — never blocks the caller either way, so this
  // is safe to call every loop() iteration even while, e.g., the fallback
  // AP is actively serving /settings to someone.
  PollResult pollNonBlockingConnect(unsigned long retryIntervalMs);

  bool performOneRequest(uint32_t &outEpoch, uint16_t &outSubMs, unsigned long &outRttMs,
                          bool &outGpsFix);
};

// Calendar <-> epoch-seconds helpers (no RTC hardware on this device, so we
// don't pull in RTClib — just enough date math to extrapolate/display time).
uint32_t civilToEpoch(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mi, uint8_t s);
void epochToCivil(uint32_t epoch, uint16_t &y, uint8_t &m, uint8_t &d,
                   uint8_t &h, uint8_t &mi, uint8_t &s);

#endif // TIME_SYNC_H
