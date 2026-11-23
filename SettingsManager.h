#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <Arduino.h>

// Runtime-editable versions of what used to be compile-time-only client
// settings: which hotspot to join, how often to sync, and display
// tuning (brightness, orientation fixes, UI toggles). Persisted to
// EEPROM (flash-emulated) so changes survive a reboot. Call begin() once
// in setup() AFTER EEPROM.begin(...).
//
// Mirrors the server's SettingsManager design intentionally — same
// pattern (magic/version/checksum-guarded EEPROM struct, clamped
// setters), so the two stay easy to reason about together.
class SettingsManager {
public:
  SettingsManager();

  void begin();

  const char* serverSsid() const { return _data.serverSsid; }
  const char* serverPassword() const { return _data.serverPassword; }
  uint32_t syncIntervalMs() const { return _data.syncIntervalMs; }
  uint8_t matrixBrightness() const { return _data.matrixBrightness; }
  bool moduleChainReversed() const { return _data.moduleChainReversed; }
  bool moduleMirrorBits() const { return _data.moduleMirrorBits; }
  bool enableDatePeek() const { return _data.enableDatePeek; }
  int16_t timezoneOffsetMinutes() const { return _data.timezoneOffsetMinutes; }
  bool displayFlipped() const { return _data.displayFlipped; }
  bool enableNightBrightness() const { return _data.enableNightBrightness; }
  uint8_t nightBrightness() const { return _data.nightBrightness; }
  uint8_t nightStartHour() const { return _data.nightStartHour; }
  uint8_t nightEndHour() const { return _data.nightEndHour; }

  // SSID: 1-32 chars required. Password: empty (open network) or 8-63
  // chars (WPA2 minimum) — matches whatever the server's hotspot allows.
  // Rejected outright (false, unchanged) rather than mangled, same
  // reasoning as the server's setApSsid/setApPassword.
  bool setServerSsid(const String &ssid);
  bool setServerPassword(const String &password);

  // Clamped, always "succeed" with whatever value was actually stored.
  uint32_t setSyncIntervalMs(uint32_t ms);
  uint8_t setMatrixBrightness(uint8_t level);
  int16_t setTimezoneOffsetMinutes(int16_t minutes);
  uint8_t setNightBrightness(uint8_t level);
  uint8_t setNightStartHour(uint8_t hour);
  uint8_t setNightEndHour(uint8_t hour);

  void setModuleChainReversed(bool reversed);
  void setModuleMirrorBits(bool mirrored);
  void setEnableDatePeek(bool enabled);
  void setDisplayFlipped(bool flipped);
  void setEnableNightBrightness(bool enabled);

  void save();
  void resetToDefaults();

  // --- Total runtime tracking (survives reboots) ---
  // Call tick() every loop() iteration — internally accumulates elapsed
  // time and persists to EEPROM only every few minutes, so this doesn't
  // wear out flash by writing constantly. Call flushRuntimeNow() right
  // before a deliberate restart so the last partial interval isn't lost
  // (a power loss without a graceful restart will still lose up to one
  // flush interval's worth — acceptable for an informational stat).
  void tick();
  void flushRuntimeNow();
  uint32_t totalRuntimeSeconds() const { return _data.totalRuntimeSeconds; }
  // Live estimate including time accumulated since the last flush (avoids
  // waiting up to RUNTIME_FLUSH_INTERVAL_MS to see it reflected) — safe
  // against double-counting since it adds only the *unflushed* remainder,
  // not this boot's full uptime.
  uint32_t totalRuntimeSecondsLive() const;

private:
  struct PersistedData {
    uint32_t magic;
    uint16_t version;
    char     serverSsid[33];
    char     serverPassword[64];
    uint32_t syncIntervalMs;
    uint8_t  matrixBrightness;
    uint8_t  moduleChainReversed; // stored as uint8_t, not bool, for
    uint8_t  moduleMirrorBits;    // deterministic EEPROM layout/checksum
    uint8_t  enableDatePeek;
    int16_t  timezoneOffsetMinutes;
    uint8_t  displayFlipped;
    uint8_t  enableNightBrightness;
    uint8_t  nightBrightness;
    uint8_t  nightStartHour;
    uint8_t  nightEndHour;
    uint32_t totalRuntimeSeconds; // cumulative across all boots
    uint32_t checksum;
  };

  PersistedData _data;
  unsigned long _lastRuntimeFlushMillis; // NOT persisted — runtime-only bookkeeping

  void loadDefaults();
  uint32_t computeChecksum(const PersistedData &d) const;
};

#endif // SETTINGS_MANAGER_H
