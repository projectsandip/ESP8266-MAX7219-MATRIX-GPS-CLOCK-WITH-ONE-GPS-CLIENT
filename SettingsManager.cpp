#include "SettingsManager.h"
#include "config.h"
#include <EEPROM.h>
#include <stddef.h>

#define SETTINGS_MAGIC 0x47504343UL // "GPCC" (client, vs server's "GPCS")
#define SETTINGS_VERSION 3 // bumped: v3 removes enableSecondsBar (feature
                            // deleted) — old v1/v2 data must be detected
                            // as invalid (version mismatch) and defaulted,
                            // not partially reused.

#define MIN_SYNC_INTERVAL_MS 1000UL
#define MAX_SYNC_INTERVAL_MS (3600UL * 1000UL) // 1 hour
#define MIN_TIMEZONE_OFFSET_MINUTES (-720)  // UTC-12:00
#define MAX_TIMEZONE_OFFSET_MINUTES 840     // UTC+14:00

// How often accumulated runtime is persisted to EEPROM — kept infrequent
// to avoid wearing out flash with constant small writes.
#define RUNTIME_FLUSH_INTERVAL_MS (5UL * 60UL * 1000UL)

static uint32_t clampU32(uint32_t v, uint32_t lo, uint32_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static int16_t clampI16(int16_t v, int16_t lo, int16_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

SettingsManager::SettingsManager()
  : _lastRuntimeFlushMillis(0) {
  memset(&_data, 0, sizeof(_data));
}

uint32_t SettingsManager::computeChecksum(const PersistedData &d) const {
  const uint8_t *bytes = reinterpret_cast<const uint8_t*>(&d);
  size_t checksumOffset = offsetof(PersistedData, checksum);
  uint32_t sum = 0;
  for (size_t i = 0; i < sizeof(PersistedData); i++) {
    if (i >= checksumOffset && i < checksumOffset + sizeof(uint32_t)) continue;
    sum += bytes[i];
    sum = (sum << 1) | (sum >> 31);
  }
  return sum;
}

void SettingsManager::loadDefaults() {
  _data.magic = SETTINGS_MAGIC;
  _data.version = SETTINGS_VERSION;

  strncpy(_data.serverSsid, DEFAULT_SERVER_SSID, sizeof(_data.serverSsid) - 1);
  _data.serverSsid[sizeof(_data.serverSsid) - 1] = '\0';
  strncpy(_data.serverPassword, DEFAULT_SERVER_PASSWORD, sizeof(_data.serverPassword) - 1);
  _data.serverPassword[sizeof(_data.serverPassword) - 1] = '\0';

  _data.syncIntervalMs = DEFAULT_SYNC_INTERVAL_MS;
  _data.matrixBrightness = DEFAULT_MATRIX_BRIGHTNESS;
  _data.moduleChainReversed = DEFAULT_MODULE_CHAIN_REVERSED ? 1 : 0;
  _data.moduleMirrorBits = DEFAULT_MODULE_MIRROR_BITS ? 1 : 0;
  _data.enableDatePeek = DEFAULT_ENABLE_DATE_PEEK ? 1 : 0;
  _data.timezoneOffsetMinutes = DEFAULT_TIMEZONE_OFFSET_MINUTES;
  _data.displayFlipped = DEFAULT_DISPLAY_FLIPPED ? 1 : 0;
  _data.enableNightBrightness = DEFAULT_ENABLE_NIGHT_BRIGHTNESS ? 1 : 0;
  _data.nightBrightness = DEFAULT_NIGHT_BRIGHTNESS;
  _data.nightStartHour = DEFAULT_NIGHT_START_HOUR;
  _data.nightEndHour = DEFAULT_NIGHT_END_HOUR;
  // totalRuntimeSeconds is deliberately NOT reset here — a settings
  // reset (or a fresh/invalid EEPROM at boot, where it's already 0 from
  // the constructor's memset) shouldn't erase lifetime uptime tracking.
}

void SettingsManager::begin() {
  PersistedData loaded;
  EEPROM.get(0, loaded);

  bool valid = (loaded.magic == SETTINGS_MAGIC) &&
               (loaded.version == SETTINGS_VERSION) &&
               (loaded.checksum == computeChecksum(loaded));

  if (valid) {
    _data = loaded;
    Serial.println("[Settings] Loaded from EEPROM.");
  } else {
    Serial.println("[Settings] No valid saved settings found — using defaults.");
    loadDefaults();
    save();
  }

  _lastRuntimeFlushMillis = millis();
}

bool SettingsManager::setServerSsid(const String &ssid) {
  if (ssid.length() < 1 || ssid.length() > 32) return false;
  strncpy(_data.serverSsid, ssid.c_str(), sizeof(_data.serverSsid) - 1);
  _data.serverSsid[sizeof(_data.serverSsid) - 1] = '\0';
  return true;
}

bool SettingsManager::setServerPassword(const String &password) {
  if (password.length() != 0 && (password.length() < 8 || password.length() > 63)) {
    return false;
  }
  strncpy(_data.serverPassword, password.c_str(), sizeof(_data.serverPassword) - 1);
  _data.serverPassword[sizeof(_data.serverPassword) - 1] = '\0';
  return true;
}

uint32_t SettingsManager::setSyncIntervalMs(uint32_t ms) {
  _data.syncIntervalMs = clampU32(ms, MIN_SYNC_INTERVAL_MS, MAX_SYNC_INTERVAL_MS);
  return _data.syncIntervalMs;
}

uint8_t SettingsManager::setMatrixBrightness(uint8_t level) {
  if (level > 15) level = 15;
  _data.matrixBrightness = level;
  return _data.matrixBrightness;
}

void SettingsManager::setModuleChainReversed(bool reversed) {
  _data.moduleChainReversed = reversed ? 1 : 0;
}

void SettingsManager::setModuleMirrorBits(bool mirrored) {
  _data.moduleMirrorBits = mirrored ? 1 : 0;
}

void SettingsManager::setEnableDatePeek(bool enabled) {
  _data.enableDatePeek = enabled ? 1 : 0;
}

int16_t SettingsManager::setTimezoneOffsetMinutes(int16_t minutes) {
  _data.timezoneOffsetMinutes = clampI16(minutes, MIN_TIMEZONE_OFFSET_MINUTES, MAX_TIMEZONE_OFFSET_MINUTES);
  return _data.timezoneOffsetMinutes;
}

uint8_t SettingsManager::setNightBrightness(uint8_t level) {
  if (level > 15) level = 15;
  _data.nightBrightness = level;
  return _data.nightBrightness;
}

uint8_t SettingsManager::setNightStartHour(uint8_t hour) {
  if (hour > 23) hour = 23;
  _data.nightStartHour = hour;
  return _data.nightStartHour;
}

uint8_t SettingsManager::setNightEndHour(uint8_t hour) {
  if (hour > 23) hour = 23;
  _data.nightEndHour = hour;
  return _data.nightEndHour;
}

void SettingsManager::setDisplayFlipped(bool flipped) {
  _data.displayFlipped = flipped ? 1 : 0;
}

void SettingsManager::setEnableNightBrightness(bool enabled) {
  _data.enableNightBrightness = enabled ? 1 : 0;
}

void SettingsManager::tick() {
  if (_lastRuntimeFlushMillis == 0) {
    _lastRuntimeFlushMillis = millis();
    return;
  }
  if (millis() - _lastRuntimeFlushMillis >= RUNTIME_FLUSH_INTERVAL_MS) {
    flushRuntimeNow();
  }
}

void SettingsManager::flushRuntimeNow() {
  unsigned long now = millis();
  if (_lastRuntimeFlushMillis == 0) {
    _lastRuntimeFlushMillis = now;
    return;
  }
  uint32_t elapsedSec = (now - _lastRuntimeFlushMillis) / 1000UL;
  if (elapsedSec > 0) {
    _data.totalRuntimeSeconds += elapsedSec;
    // Advance by exactly the whole seconds we counted (not to `now`), so
    // the sub-second remainder carries over instead of being silently
    // dropped every flush.
    _lastRuntimeFlushMillis += elapsedSec * 1000UL;
    save();
  }
}

uint32_t SettingsManager::totalRuntimeSecondsLive() const {
  if (_lastRuntimeFlushMillis == 0) return _data.totalRuntimeSeconds;
  uint32_t unflushedSec = (millis() - _lastRuntimeFlushMillis) / 1000UL;
  return _data.totalRuntimeSeconds + unflushedSec;
}

void SettingsManager::save() {
  _data.magic = SETTINGS_MAGIC;
  _data.version = SETTINGS_VERSION;
  _data.checksum = computeChecksum(_data);
  EEPROM.put(0, _data);
  EEPROM.commit();
  Serial.println("[Settings] Saved to EEPROM.");
}

void SettingsManager::resetToDefaults() {
  loadDefaults();
  save();
}
