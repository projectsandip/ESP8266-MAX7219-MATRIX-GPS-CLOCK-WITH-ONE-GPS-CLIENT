#include "TimeSync.h"
#include "config.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

// ---------------------------------------------------------------------------
// Calendar <-> epoch-seconds helpers (Howard Hinnant's days-from-civil
// algorithm — proleptic Gregorian, valid well beyond any date we care about).
// ---------------------------------------------------------------------------
static int32_t daysFromCivil(int32_t y, int32_t m, int32_t d) {
  y -= m <= 2;
  int32_t era = (y >= 0 ? y : y - 399) / 400;
  int32_t yoe = y - era * 400;                                   // 0..399
  int32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;   // 0..365
  int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;            // 0..146096
  return era * 146097 + doe - 719468; // days since 1970-01-01
}

static void civilFromDays(int32_t z, int32_t &y, int32_t &m, int32_t &d) {
  z += 719468;
  int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  int32_t doe = z - era * 146097;                                 // 0..146096
  int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // 0..399
  y = yoe + era * 400;
  int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);          // 0..365
  int32_t mp = (5 * doy + 2) / 153;                               // 0..11
  d = doy - (153 * mp + 2) / 5 + 1;                                // 1..31
  m = mp + (mp < 10 ? 3 : -9);                                     // 1..12
  y += (m <= 2);
}

uint32_t civilToEpoch(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mi, uint8_t s) {
  int32_t days = daysFromCivil(y, m, d);
  return (uint32_t)days * 86400UL + (uint32_t)h * 3600UL + (uint32_t)mi * 60UL + s;
}

void epochToCivil(uint32_t epoch, uint16_t &y, uint8_t &m, uint8_t &d,
                   uint8_t &h, uint8_t &mi, uint8_t &s) {
  int32_t days = epoch / 86400UL;
  uint32_t secOfDay = epoch % 86400UL;
  int32_t yy, mm, dd;
  civilFromDays(days, yy, mm, dd);
  y = (uint16_t)yy;
  m = (uint8_t)mm;
  d = (uint8_t)dd;
  h  = secOfDay / 3600;
  mi = (secOfDay % 3600) / 60;
  s  = secOfDay % 60;
}

// The anchor is always kept in pure UTC internally — this is only applied
// when actually producing a displayed/reported calendar value, so a
// changed timezone setting takes effect immediately without needing a
// fresh sync.
static uint32_t applyTimezoneOffset(uint32_t utcEpoch, int16_t offsetMinutes) {
  int32_t offsetSec = (int32_t)offsetMinutes * 60;
  return (uint32_t)((int64_t)utcEpoch + offsetSec);
}

// ---------------------------------------------------------------------------
// TimeSync
// ---------------------------------------------------------------------------
TimeSync::TimeSync(SettingsManager &settings)
  : _settings(settings),
    _hasAnchor(false), _anchorEpoch(0), _anchorSubMs(0), _anchorMillis(0),
    _lastSuccessMillis(0), _lastSyncAttemptMillis(0),
    _lastGpsFixValid(false),
    _lastSyncYear(0), _lastSyncMonth(0), _lastSyncDay(0),
    _lastSyncHour(0), _lastSyncMinute(0), _lastSyncSecond(0),
    _permanentErrorLatched(false),
    _wifiState(WiFiState::RETRYING), _everConnectedThisBoot(false), _consecutiveFailedAttempts(0),
    _attemptInProgress(false), _attemptStartMillis(0), _lastAttemptMillis(0) {}

void TimeSync::begin() {
  // Kicks off the actual connection attempt immediately (same as before)
  // — important because the startup animation calls syncNow() directly,
  // not update(), so if this didn't start the attempt here, WiFi.begin()
  // wouldn't be called at all until the main loop starts. The ESP8266 SDK
  // handles the handshake asynchronously regardless of whether our own
  // state machine is actively polling it, so this doesn't need to block.
  WiFi.mode(WIFI_STA);
  Serial.print("[WiFi] Connecting to ");
  Serial.println(_settings.serverSsid());
  WiFi.begin(_settings.serverSsid(), _settings.serverPassword());
  _lastAttemptMillis = millis();
  _attemptStartMillis = millis();
  _attemptInProgress = true;
}

void TimeSync::reconnectWithCurrentCredentials() {
  if (_wifiState == WiFiState::FALLBACK_AP) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
  }
  WiFi.disconnect();
  _wifiState = WiFiState::RETRYING;
  _consecutiveFailedAttempts = 0;
  _attemptInProgress = false;
  _lastAttemptMillis = 0; // forces pollNonBlockingConnect() to retry immediately
}

// Kicks off a non-blocking WiFi.begin() no more often than retryIntervalMs,
// and polls an in-progress attempt against WIFI_CONNECT_TIMEOUT_MS. Never
// blocks — safe to call every loop() iteration regardless of WiFi state.
TimeSync::PollResult TimeSync::pollNonBlockingConnect(unsigned long retryIntervalMs) {
  if (_attemptInProgress) {
    if (WiFi.status() == WL_CONNECTED) {
      _attemptInProgress = false;
      return PollResult::CONNECTED;
    }
    if (millis() - _attemptStartMillis > WIFI_CONNECT_TIMEOUT_MS) {
      _attemptInProgress = false;
      return PollResult::JUST_TIMED_OUT;
    }
    return PollResult::WAITING;
  }

  if (millis() - _lastAttemptMillis < retryIntervalMs) {
    return PollResult::IDLE;
  }

  _lastAttemptMillis = millis();
  Serial.print("[WiFi] Attempting connection to ");
  Serial.println(_settings.serverSsid());
  WiFi.begin(_settings.serverSsid(), _settings.serverPassword());
  _attemptStartMillis = millis();
  _attemptInProgress = true;
  return PollResult::WAITING;
}

void TimeSync::updateWiFiState() {
  if (WiFi.status() == WL_CONNECTED) {
    if (_wifiState != WiFiState::CONNECTED) {
      Serial.print("[WiFi] Connected, IP = ");
      Serial.println(WiFi.localIP());
      if (_wifiState == WiFiState::FALLBACK_AP) {
        Serial.println("[WiFi] Reconnected to configured network — leaving fallback AP mode.");
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
      }
    }
    _wifiState = WiFiState::CONNECTED;
    _everConnectedThisBoot = true;
    _consecutiveFailedAttempts = 0;
    _attemptInProgress = false;
    return;
  }

  if (_wifiState == WiFiState::FALLBACK_AP) {
    // Keep retrying the real network in the background — the fallback AP
    // (and SettingsServer/OTA on it) stays fully responsive throughout,
    // since this never blocks. Only the WL_CONNECTED branch above exits
    // fallback mode; individual failed attempts here don't change state.
    pollNonBlockingConnect(WIFI_FALLBACK_RETRY_INTERVAL_MS);
    return;
  }

  _wifiState = WiFiState::RETRYING;
  unsigned long retryInterval = _everConnectedThisBoot ? WIFI_RECONNECT_CHECK_MS : WIFI_INITIAL_RETRY_INTERVAL_MS;
  PollResult result = pollNonBlockingConnect(retryInterval);

  if (result == PollResult::JUST_TIMED_OUT) {
    _consecutiveFailedAttempts++;
    Serial.printf("[WiFi] Attempt failed (%u/%u before fallback AP)\n",
                  _consecutiveFailedAttempts, WIFI_MAX_FAILED_ATTEMPTS);

    if (_consecutiveFailedAttempts >= WIFI_MAX_FAILED_ATTEMPTS) {
      Serial.println("[WiFi] Too many failed attempts — starting fallback AP for configuration.");
      Serial.print("[WiFi] Connect to \"");
      Serial.print(FALLBACK_AP_SSID);
      Serial.println("\" and visit http://192.168.4.1/settings to fix WiFi credentials.");
      Serial.println("[WiFi] Still retrying the configured network in the background.");

      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(FALLBACK_AP_SSID, FALLBACK_AP_PASSWORD);
      _wifiState = WiFiState::FALLBACK_AP;
    }
  }
}

bool TimeSync::performOneRequest(uint32_t &outEpoch, uint16_t &outSubMs, unsigned long &outRttMs,
                                  bool &outGpsFix) {
  WiFiClient client;
  HTTPClient http;

  String url = String("http://") + SERVER_IP + SERVER_TIME_UTC_ENDPOINT;
  http.setTimeout(SYNC_HTTP_TIMEOUT_MS);

  if (!http.begin(client, url)) {
    return false;
  }

  unsigned long t0 = millis();
  int httpCode = http.GET();
  unsigned long t1 = millis(); // capture RTT immediately here — NOT after
                                // getString()/JSON parsing, which was the
                                // bug in the earlier build that inflated
                                // RTT with client-side processing time.

  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  String payload = http.getString(); // safe to take time here now
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    return false;
  }

  uint16_t y  = doc["year"]        | 0;
  uint8_t  mo = doc["month"]       | 0;
  uint8_t  d  = doc["day"]         | 0;
  uint8_t  h  = doc["hour"]        | 0;
  uint8_t  mi = doc["minute"]      | 0;
  uint8_t  s  = doc["second"]      | 0;
  uint16_t ms = doc["millisecond"] | 0; // was previously discarded entirely —
                                         // that truncation-to-whole-seconds
                                         // was the main source of the visible
                                         // second "jumps" on every sync

  if (y == 0) return false; // malformed response

  outEpoch = civilToEpoch(y, mo, d, h, mi, s);
  outSubMs = ms;
  outRttMs = t1 - t0;
  outGpsFix         = doc["gpsFixValid"]  | false;

  return true;
}

bool TimeSync::syncNow() {
  uint32_t bestEpoch = 0;
  uint16_t bestSubMs = 0;
  unsigned long bestRtt = (unsigned long)-1;
  bool bestGpsFix = false;
  bool gotAny = false;

  for (uint8_t i = 0; i < SYNC_SAMPLES_PER_ROUND; i++) {
    uint32_t epoch;
    uint16_t subMs;
    unsigned long rtt;
    bool gpsFix;

    if (performOneRequest(epoch, subMs, rtt, gpsFix)) {
      gotAny = true;
      if (rtt < bestRtt) {
        bestRtt = rtt;
        bestEpoch = epoch;
        bestSubMs = subMs;
        bestGpsFix = gpsFix;
      }
    }
    if (i + 1 < SYNC_SAMPLES_PER_ROUND) delay(20); // brief spacing between samples
  }

  if (!gotAny || bestRtt > SYNC_MAX_ACCEPTABLE_RTT_MS) {
    if (gotAny) {
      Serial.printf("[Sync] Rejected — best RTT %lums exceeds threshold\n", bestRtt);
    } else {
      Serial.println("[Sync] Failed — no responses from server");
    }
    return false;
  }

  // The server's clock value corresponds roughly to the moment its
  // response left the server, which we approximate as the midpoint of
  // our observed round-trip (assumes roughly symmetric latency).
  unsigned long halfRtt = bestRtt / 2;
  unsigned long newAnchorMillis = millis() - halfRtt;

  // Dead-zone: if we already have an anchor, compare what it currently
  // predicts against this new reading. Sub-30ms disagreement is just RTT
  // jitter, not real drift — snapping to it anyway was contributing to the
  // visible stutter, so we only adopt the new anchor when the correction
  // is actually meaningful.
  if (_hasAnchor) {
    uint32_t oldElapsed = newAnchorMillis - _anchorMillis; // ms predicted to have passed
    int32_t oldTotalMs = (int32_t)(_anchorSubMs + oldElapsed);
    int32_t oldPredictedEpoch = _anchorEpoch + oldTotalMs / 1000;
    int32_t oldPredictedSubMs = oldTotalMs % 1000;

    int32_t diffMs = ((int32_t)bestEpoch - oldPredictedEpoch) * 1000
                      + ((int32_t)bestSubMs - oldPredictedSubMs);

    if (abs(diffMs) < 30) {
      // Close enough — keep the existing anchor running rather than
      // introducing a tiny, visually pointless correction.
      _lastSuccessMillis = millis();
      _lastGpsFixValid = bestGpsFix;

      uint32_t localEpoch = applyTimezoneOffset(bestEpoch, _settings.timezoneOffsetMinutes());
      epochToCivil(localEpoch, _lastSyncYear, _lastSyncMonth, _lastSyncDay,
                   _lastSyncHour, _lastSyncMinute, _lastSyncSecond);

      Serial.printf("[Sync] OK rtt=%lums (anchor unchanged, diff=%ldms)\n",
                    bestRtt, (long)diffMs);
      return true;
    }
  }

  _anchorEpoch = bestEpoch;
  _anchorSubMs = bestSubMs;
  _anchorMillis = newAnchorMillis; // back-date the anchor by half the RTT
  _hasAnchor = true;
  _lastSuccessMillis = millis();

  _lastGpsFixValid = bestGpsFix;

  uint32_t localEpoch = applyTimezoneOffset(bestEpoch, _settings.timezoneOffsetMinutes());
  epochToCivil(localEpoch, _lastSyncYear, _lastSyncMonth, _lastSyncDay,
               _lastSyncHour, _lastSyncMinute, _lastSyncSecond);

  Serial.printf("[Sync] OK rtt=%lums\n", bestRtt);
  return true;
}

void TimeSync::update() {
  updateWiFiState();

  if (WiFi.status() != WL_CONNECTED) return;

  if (millis() - _lastSyncAttemptMillis < _settings.syncIntervalMs() && _hasAnchor) return;
  _lastSyncAttemptMillis = millis();

  syncNow();
}

ClientTime TimeSync::getCurrentTime() {
  ClientTime ct;
  memset(&ct, 0, sizeof(ct));

  ct.wifiConnected = (WiFi.status() == WL_CONNECTED);
  ct.inFallbackApMode = isInFallbackApMode();
  ct.hasEverSynced = _hasAnchor;
  ct.gpsFixValid = _lastGpsFixValid;

  ct.lastSyncYear = _lastSyncYear;
  ct.lastSyncMonth = _lastSyncMonth;
  ct.lastSyncDay = _lastSyncDay;
  ct.lastSyncHour = _lastSyncHour;
  ct.lastSyncMinute = _lastSyncMinute;
  ct.lastSyncSecond = _lastSyncSecond;

  // _lastSuccessMillis starts at 0, so this naturally reads as "time since
  // boot" before the first sync ever lands, and "time since last sync"
  // afterward — one counter covers both cases, feeding the same 24h
  // freewheel budget whether or not a sync has ever succeeded.
  ct.msSinceLastSuccessfulSync = millis() - _lastSuccessMillis;

  if (!_permanentErrorLatched && ct.msSinceLastSuccessfulSync >= FREEWHEEL_PERMANENT_ERROR_MS) {
    _permanentErrorLatched = true;
    Serial.println("[TimeSync] 24h with no successful sync — latching permanent error.");
  }
  ct.permanentError = _permanentErrorLatched;

  if (!_hasAnchor) {
    // Never synced yet — no valid anchor to extrapolate a real clock from.
    return ct;
  }

  unsigned long elapsedMs = millis() - _anchorMillis;
  uint32_t totalMs = (uint32_t)_anchorSubMs + elapsedMs;
  uint32_t extraSeconds = totalMs / 1000;
  uint16_t subMs = totalMs % 1000;

  // _anchorEpoch is always pure UTC — apply the client's own timezone
  // offset here, fresh every call, so a settings change takes effect
  // immediately without needing a new sync.
  uint32_t nowEpochUtc = _anchorEpoch + extraSeconds;
  uint32_t nowEpochLocal = applyTimezoneOffset(nowEpochUtc, _settings.timezoneOffsetMinutes());
  epochToCivil(nowEpochLocal, ct.year, ct.month, ct.day, ct.hour, ct.minute, ct.second);
  ct.millisecond = subMs;

  return ct;
}
