#ifndef CONFIG_H
#define CONFIG_H

// ---------------------------------------------------------------------------
// WiFi — connects to the GPS_Master_Server_v3 hotspot as a station.
// These are now the DEFAULT values only — the live values are runtime-
// editable via this device's own /settings page and persisted in EEPROM
// (see SettingsManager). Must match AP_SSID / AP_PASSWORD in the server's
// settings exactly.
// ---------------------------------------------------------------------------
#define DEFAULT_SERVER_SSID     "GPS_Master_Server"
#define DEFAULT_SERVER_PASSWORD "123456789"

// Server's fixed AP IP (matches AP_LOCAL_IP in the server's config.h)
#define SERVER_IP   "192.168.4.1"
#define SERVER_TIME_ENDPOINT     "/time"      // no longer used directly (see SERVER_TIME_UTC_ENDPOINT)
#define SERVER_TIME_UTC_ENDPOINT "/time/utc"  // client applies its own timezone offset to this
#define SERVER_STATUS_ENDPOINT   "/status"

// If WiFi drops, how often to retry reconnecting (used once we've
// connected successfully at least once this boot — a patient cadence
// since a brief real-world WiFi blip likely self-resolves).
#define WIFI_RECONNECT_CHECK_MS (5UL * 60UL * 1000UL)  // 5 minutes

// ---------------------------------------------------------------------------
// Fallback AP — if the client can't reach the configured WiFi network
// within a few tries, it starts broadcasting its own temporary hotspot
// (fixed SSID, independent of any stored settings) so /settings remains
// reachable to fix credentials, without needing USB or the original
// network at all. It keeps retrying the configured WiFi in the
// background the whole time, and drops the fallback AP automatically the
// moment that succeeds — no manual restart needed either way.
// ---------------------------------------------------------------------------
#define FALLBACK_AP_SSID     "GPS_Client_Setup"
#define FALLBACK_AP_PASSWORD "setup1234"  // 8+ chars for WPA2, or "" for an open network

// How long a single connection attempt is given before it's considered
// failed (WiFi.begin() itself is non-blocking; this just bounds how long
// we wait for WiFi.status() to report success before giving up on it).
#define WIFI_CONNECT_TIMEOUT_MS 15000

// Retry cadence before the first successful connection this boot —
// deliberately quicker than WIFI_RECONNECT_CHECK_MS above, so a freshly
// flashed or misconfigured device gives fast, satisfying feedback (and
// reaches the fallback AP threshold quickly) rather than waiting minutes.
#define WIFI_INITIAL_RETRY_INTERVAL_MS 20000  // 20 seconds

// Consecutive failed attempts before falling back to AP mode.
#define WIFI_MAX_FAILED_ATTEMPTS 3

// Retry cadence while the fallback AP is active.
#define WIFI_FALLBACK_RETRY_INTERVAL_MS (2UL * 60UL * 1000UL)  // 2 minutes

// This device's own admin web server (settings/restart/OTA)
#define HTTP_PORT 80

// ---------------------------------------------------------------------------
// MAX7219 matrix chain — 4 modules, 32x8 total pixels
// ---------------------------------------------------------------------------
#define NUM_MODULES   4
#define MATRIX_WIDTH  (NUM_MODULES * 8)  // 32
#define MATRIX_HEIGHT 8

// Hardware SPI pins (ESP8266 hardware SPI: MOSI=D7/GPIO13, SCK=D5/GPIO14
// are fixed by the SPI peripheral; only CS is freely assignable)
#define MAX7219_CS_PIN 15   // GPIO15 (D8) — a safe, commonly-free CS pin

// --- Orientation fixes ---
// These are now DEFAULT values only — runtime-editable via /settings.
// If digits render in the wrong left-right module order, flip this.
#define DEFAULT_MODULE_CHAIN_REVERSED true
// If each individual digit looks horizontally mirrored, flip this.
#define DEFAULT_MODULE_MIRROR_BITS true

// This is now the DEFAULT value only — runtime-editable via /settings.
#define DEFAULT_MATRIX_BRIGHTNESS 4  // 0 (dim) - 15 (max)

// --- Timezone (client-owned, independent of the server's own TZ) ---
// The server always serves UTC via /time/utc; this client applies its own
// offset locally, so different clients on the same server can each show a
// different local time zone if needed. Minutes, e.g. India (+5:30) = 330,
// US Eastern standard time (-5:00) = -300. This is now the DEFAULT value
// only — runtime-editable via /settings.
#define DEFAULT_TIMEZONE_OFFSET_MINUTES 330

// --- Display flip (180-degree rotation) ---
// Independent of MODULE_CHAIN_REVERSED/MODULE_MIRROR_BITS above, which fix
// wiring-order artifacts — this is for when the whole assembled display is
// physically mounted upside down (or you'd just rather flip it). Rotates
// every screen 180 degrees (both axes), applied once in DisplayManager so
// nothing else needs to know about it. This is now the DEFAULT value only
// — runtime-editable via /settings.
#define DEFAULT_DISPLAY_FLIPPED false

// --- Night brightness ---
// A second, typically dimmer brightness level automatically applied
// during a configurable hour range (based on the client's own local time,
// after the timezone offset above) — no ambient light sensor needed.
// These are now DEFAULT values only — runtime-editable via /settings.
#define DEFAULT_ENABLE_NIGHT_BRIGHTNESS false
#define DEFAULT_NIGHT_BRIGHTNESS  1   // 0-15
#define DEFAULT_NIGHT_START_HOUR  22  // 0-23, local time
#define DEFAULT_NIGHT_END_HOUR    6   // 0-23, local time (wraps past midnight if start > end)

// ---------------------------------------------------------------------------
// Time sync behavior
// ---------------------------------------------------------------------------
// This is now the DEFAULT value only — runtime-editable via /settings.
#define DEFAULT_SYNC_INTERVAL_MS 60000  // how often to poll the server for time
// On a dedicated ESP-to-ESP hotspot (low jitter vs a shared home network),
// one sample is normally enough — each additional sample adds a full
// blocking HTTP round-trip to syncNow(), during which the display can't
// refresh. Raise this only if you're seeing frequent sync rejections due
// to RTT threshold misses.
#define SYNC_SAMPLES_PER_ROUND  1
#define SYNC_MAX_ACCEPTABLE_RTT_MS 150 // reject samples slower than this
#define SYNC_HTTP_TIMEOUT_MS     800   // kept short so a failed request
                                        // can't freeze the display for long

// The clock keeps freewheeling (display never stops) for up to this long
// with no successful sync from the server — after this, a permanent error
// screen replaces the clock for good (does not clear even if a sync
// succeeds again later; requires a reboot to recover).
#define FREEWHEEL_PERMANENT_ERROR_MS (24UL * 60UL * 60UL * 1000UL)  // 24 hours

// While WiFi is disconnected (within the 24h freewheel window above), the
// clock keeps running normally but is briefly interrupted to show an error
// message on this cadence — every INTERVAL, for DURATION, then back to the
// clock.
#define WIFI_ERROR_FLASH_INTERVAL_MS (60UL * 1000UL)  // every 1 minute
#define WIFI_ERROR_FLASH_DURATION_MS 5000              // shown for 5 seconds

// ---------------------------------------------------------------------------
// UI behavior
// ---------------------------------------------------------------------------
// These are now DEFAULT values only — runtime-editable via /settings.
#define DEFAULT_ENABLE_DATE_PEEK    false  // periodically show DD-MM instead of the clock

#define DATE_PEEK_INTERVAL_MS  10000  // show date every N ms...
#define DATE_PEEK_DURATION_MS  2000   // ...for this long

#define ERROR_FLASH_BLINK_MS   500    // blink rate for the WiFi-down warning screen
#define WIFI_INDICATOR_NO_WIFI_BLINK_MS       100UL  // rapid blink when disconnected
#define WIFI_INDICATOR_GPS_UNLOCKED_BLINK_MS  250UL  // faster blink while GPS is unlocked
#define WIFI_INDICATOR_GPS_LOCKED_BLINK_MS    1000UL // gentle blink while GPS is locked

// How many ms between each 1-pixel step of the scrolling startup/recovery
// text messages (see UIRenderer). Lower = faster scroll. 60ms is a
// comfortably readable "medium" pace for a 5x7 font on this display.
#define SCROLL_STEP_INTERVAL_MS 40

// How often the display is recomputed/redrawn. Lower = tighter latency
// between real elapsed time and what's shown, at the cost of more SPI
// traffic and less idle time for the WiFi stack between frames. 5ms gives
// a ~5ms worst-case freshness ceiling, well under perceptible for a
// seconds display, while still leaving plenty of headroom for WiFi/HTTP
// housekeeping between frames.
#define DISPLAY_REFRESH_INTERVAL_MS 20

// ---------------------------------------------------------------------------
// Admin authentication — gates this device's /settings, /restart, and
// /update (firmware upload), same reasoning as the server: anyone on the
// hotspot could otherwise reflash or reconfigure this client. Change
// before deploying.
// ---------------------------------------------------------------------------
#define ADMIN_USERNAME "admin"
#define ADMIN_PASSWORD "changeme123"

// ---------------------------------------------------------------------------
// OTA (over-the-air firmware updates) — ArduinoOTA lets you flash over
// WiFi from the Arduino IDE instead of needing a USB cable every time.
// ---------------------------------------------------------------------------
#define OTA_HOSTNAME "gps-master-client"
#define OTA_PASSWORD "changeme123"  // set to "" to disable password (not recommended)

#endif // CONFIG_H
