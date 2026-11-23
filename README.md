# GPS_Master_Client_v3

ESP8266 + 4x MAX7219 (32x8 pixels) matrix clock. Connects directly to the
**GPS_Master_Server_v3** hotspot and displays synced time with a custom
pixel font, scrolling status text, and live status indicators.

## Hardware

| Component        | Connection                                          |
|-------------------|------------------------------------------------------|
| 4x MAX7219 8x8 matrix, daisy-chained | Hardware SPI: MOSI=GPIO13 (D7), SCK=GPIO14 (D5) — fixed by the SPI peripheral. CS = GPIO15 (D8), configurable in `config.h`. |
| VCC / GND          | 5V and GND — 4 chained modules can draw meaningful current at full brightness; a separate 5V supply (not just the ESP8266's onboard regulator) is recommended. |

## How it connects to the server

This client is a WiFi **station** — it joins the server's hotspot
(`SERVER_SSID` / `SERVER_PASSWORD` in `config.h`, must match the server's
`AP_SSID` / `AP_PASSWORD`) and polls `http://192.168.4.1/time` directly.
No router, no DHCP — same pattern as the server's own hotspot design.

## Time sync strategy

Every `SYNC_INTERVAL_MS` (default 15s), the client:

1. Makes `SYNC_SAMPLES_PER_ROUND` (default 3) separate HTTP GET requests to
   `/time`, measuring round-trip time for each.
2. Keeps only the **lowest-RTT** sample — the others are discarded, since a
   slow response likely means the request sat in a queue or hit WiFi
   contention, making its timing less trustworthy.
3. Rejects the sync entirely if even the best RTT exceeds
   `SYNC_MAX_ACCEPTABLE_RTT_MS` (default 150ms).
4. Anchors the local clock to the accepted server time, **back-dated by
   half the measured RTT** — assuming roughly symmetric network latency, the
   server's timestamp corresponds to the midpoint of the round trip, not
   the moment the response was received.

**Important implementation detail:** RTT is measured as the time between
sending the GET request and receiving the HTTP response — captured
immediately after `http.GET()` returns, *before* calling `getString()`.
Measuring RTT after JSON parsing/string extraction (a bug hit in an earlier
build) inflates the apparent latency with client-side processing time that
has nothing to do with the network.

Between syncs, the displayed time is extrapolated locally via `millis()` —
no need to hit the server every frame.

## Runtime-editable settings

WiFi credentials, sync interval, timezone, brightness, orientation fixes,
night brightness, and UI toggles are no longer fixed at compile time —
edit them live at `http://<this-device-ip>/settings` (values persist in
EEPROM across reboots). The server's dashboard already links here directly
from its Connected Clients table. The `config.h` constants
(`DEFAULT_SERVER_SSID` etc.) are only the fallback used on first boot or
after "Reset to Defaults".

- **WiFi (SSID/password)**: requires a restart to take effect (prompted
  on save) — must match the server's hotspot exactly.
- **Sync interval, timezone, brightness, orientation, display flip, night
  brightness, UI toggles**: apply immediately, no restart needed.

### Timezone

The client now pulls **UTC** from the server (`/time/utc`, not `/time`)
and applies its own timezone offset (in minutes, e.g. `330` for
India +5:30, `-300` for US Eastern) locally. This means different clients
on the same server can each show a different local time zone if needed —
the server's own timezone setting no longer constrains what this client
displays.

### Display flip

A full 180-degree rotation of the whole rendered image, for when the
assembled unit ends up physically mounted upside down. This is
independent of `MODULE_CHAIN_REVERSED`/`MODULE_MIRROR_BITS`, which fix
wiring-order artifacts rather than physical mounting — you may need
either, both, or neither depending on your build.

### Night brightness

An optional second, typically dimmer brightness level automatically
applied during a configurable hour range (based on this device's own
local time, after the timezone offset above) — no ambient light sensor
needed. Disabled by default.

### Device status

The top of `/settings` shows:
- **Single run** — uptime since this device's last boot
- **Total run** — cumulative uptime across all boots (persisted to
  EEPROM every 5 minutes, plus flushed on a deliberate restart — a sudden
  power loss can still lose up to 5 minutes of the count, which is fine
  for an informational stat)
- **Last sync** — the actual calendar date/time (client-local) of the
  most recent successful sync with the server, not just "how long ago"

## WiFi recovery (fallback configuration AP)

If the client can't reach the configured WiFi network (server hotspot
down, or wrong SSID/password saved), it doesn't get stuck unreachable:

1. It retries a few times — quickly at first (every 20s before the first
   successful connection this boot), then more patiently (every 5 min)
   once it's connected successfully at least once.
2. After `WIFI_MAX_FAILED_ATTEMPTS` (default 3) consecutive failures, it
   starts its **own temporary hotspot** — `FALLBACK_AP_SSID` /
   `FALLBACK_AP_PASSWORD` in `config.h` (defaults:
   `GPS_Client_Setup` / `setup1234`) — independent of any stored
   settings, so it's always reachable even if the saved credentials are
   simply wrong.
3. Connect a phone/laptop to that network and visit
   `http://192.168.4.1/settings` to fix the WiFi credentials — no USB
   cable needed.
4. The whole time the fallback AP is up, it keeps retrying the
   configured network in the background (every 2 minutes,
   non-blocking — the fallback AP and its `/settings` page stay fully
   responsive throughout) and **automatically drops the fallback AP and
   resumes normal operation** the moment that succeeds. No manual restart
   needed either way — this recovers on its own if the server just
   happened to be temporarily down, and lets you fix things by hand if
   the credentials were actually wrong.

Change `FALLBACK_AP_PASSWORD` from the default before relying on this —
it's a real (if temporary) open door into the device's settings.

## Restart & OTA firmware updates

- **Restart**: a button on `/settings` (or POST directly to `/restart`).
- **OTA via web upload**: `/update` accepts a compiled `.bin` (Arduino IDE
  → Sketch → Export Compiled Binary) through a browser upload form,
  flashes it, and restarts automatically.
- **OTA via Arduino IDE**: `ArduinoOTA` is also enabled — once the device
  is on and connected, it should appear as a network port under Tools →
  Port (hostname set by `OTA_HOSTNAME` in `config.h`).

**Change `ADMIN_USERNAME`/`ADMIN_PASSWORD` and `OTA_PASSWORD` in
`config.h` before deploying** — these gate `/settings`, `/restart`, and
`/update`, since anyone on the hotspot could otherwise reflash or
reconfigure this client.

## Startup sequence & scrolling status text

Instead of a silent wipe animation, boot shows a scrolling text message
(5x7 font, right-to-left ticker, `SCROLL_STEP_INTERVAL_MS` pace — 60ms/px
by default) that always finishes its current pass before switching to the
next one, so nothing gets cut off mid-message:

1. **"CONNECTING TO WIFI..."** — loops until WiFi connects (or until
   fallback AP mode kicks in, see below).
2. **"CONNECTED TO '\<ssid\>'"** — shown for exactly one full pass once
   WiFi connects, then the clock appears.
3. **If it can't connect after a few tries**, this switches to a looping
   recovery message instead: `"UNABLE TO CONNECT TO WIFI...STARTED ACCESS
   POINT...GPS_CLIENT_SETUP....HTTP://192.168.4.1  PASSWORD:SETUP1234"` —
   see [WiFi recovery](#wifi-recovery-fallback-configuration-ap) below.
   This same message also takes over later (post-startup) if a persistent
   WiFi loss triggers fallback AP mode again, not just at boot.

This all runs through the normal non-blocking render loop — OTA, the
admin web server, and WiFi connection attempts keep working throughout,
unlike the old blocking wipe animation.

## Display layout (32x8)

```
Rows 0-6:  HH:MM in 4x7, SS in 3x7
Row 7:     [single status indicator: col 31]
```

- **Status indicator** (col 31) blinks rapidly when WiFi is disconnected,
  faster while connected but GPS is unlocked, and gently while GPS is locked.
- **Date peek**: every `DATE_PEEK_INTERVAL_MS` (default 10s), the clock is
  replaced with `DD-MM` for `DATE_PEEK_DURATION_MS` (default 2s). Currently
  disabled via `ENABLE_DATE_PEEK false`.
- **Freewheel budget**: the clock keeps running (never stops or blanks)
  for up to `FREEWHEEL_PERMANENT_ERROR_MS` (24 hours) with no successful
  sync from the server, extrapolating off the last good anchor the whole
  time.
- **WiFi-down flash**: while WiFi is disconnected (but still within the 24h
  budget above), the clock is briefly interrupted every
  `WIFI_ERROR_FLASH_INTERVAL_MS` (1 minute) to show three blinking "!"
  marks for `WIFI_ERROR_FLASH_DURATION_MS` (5 seconds), then resumes the
  clock automatically. This repeats for as long as WiFi stays down.
- **WiFi auto-reconnect**: if WiFi drops, a reconnect is attempted every
  `WIFI_RECONNECT_CHECK_MS` (5 minutes) in the background, independent of
  the display.
- **Permanent error**: once 24 hours pass with no successful sync, the
  display switches to a **solid** (non-blinking) "!!!" with a solid bottom
  row — visually distinct from the periodic blinking flash above. This is
  a one-way latch: it does not clear even if a sync succeeds again later.
  A reboot is required to recover from this state.

## Orientation / mirroring fixes

MAX7219 modules can differ in wiring orientation depending on the board
batch. If the display looks wrong after first flashing, check `config.h`:

- **Digits appear in the wrong left-right order** (e.g. seconds show where
  hours should be) → flip `MODULE_CHAIN_REVERSED`.
- **Each individual digit looks like a horizontal mirror image of itself**
  → flip `MODULE_MIRROR_BITS`.

These two are independent — you may need to flip one, both, or neither.

## File layout

```
GPS_Master_Client_v3/
├── CLAUDE_GPS_Master_Client_v3.ino   Main sketch: wiring, setup(), loop(), OTA
├── config.h                          Pins, default WiFi/admin credentials, timing/layout constants
├── Font3x7.h                         Custom 3x7 pixel digit font + colon/dash glyphs
├── FontText5x7.h                     5x7 bitmap font (A-Z, 0-9, punctuation) for scrolling status text
├── SettingsManager.h/.cpp            EEPROM-backed runtime-editable settings
├── SettingsServer.h/.cpp             This device's own admin web server (settings/restart/OTA)
├── DisplayManager.h/.cpp             MAX7219 SPI driver, double-buffered framebuffer
├── TimeSync.h/.cpp                   WiFi station, RTT-filtered HTTP sync, calendar math
├── UIRenderer.h/.cpp                 Startup animation, clock/date/warning screens, status pixels
└── README.md
```

## Dependencies (Arduino Library Manager)

- `ArduinoJson` (v7+)
- ESP8266 core (`ESP8266WiFi`, `ESP8266HTTPClient`, `SPI`, `ESP8266WebServer`, `EEPROM`, `ArduinoOTA`, `Updater`) — all bundled with the ESP8266 board package, no separate install needed

No RTC or GPS library needed on this device — it has no RTC/GPS hardware of
its own; all time comes from the server. Calendar math (converting between
year/month/day/hour/minute/second and a running seconds counter) is done
with small helper functions in `TimeSync.cpp`, so there's no RTClib
dependency here either.

## Setup

1. Edit `config.h`: set `SERVER_SSID` / `SERVER_PASSWORD` to match the
   server's `AP_SSID` / `AP_PASSWORD`, and check `MAX7219_CS_PIN` /
   `NUM_MODULES` match your wiring.
2. Flash to the ESP8266.
3. Power on the **server** first (so its hotspot is up), then the client.
4. Watch Serial Monitor at 115200 baud — you should see WiFi connect, the
   "CONNECTING TO WIFI..." message scroll on the matrix, then
   "CONNECTED TO '\<ssid\>'" once it joins the server's hotspot, and
   `[Sync] OK rtt=...` once it successfully reaches the server.
5. If digits look mirrored or in the wrong order, see the orientation
   section above.

## Notes / tuning

- `SYNC_MAX_ACCEPTABLE_RTT_MS` (150ms default) is fairly generous for a
  direct ESP-to-ESP hotspot connection (should typically see single-digit
  to low-double-digit ms). You could tighten this once you've seen typical
  RTTs on your actual setup — check the serial log's `rtt=...` values.
- `MATRIX_BRIGHTNESS` (0-15) — lower this if the display is uncomfortably
  bright at close range, or if 4 modules at max brightness are drawing more
  current than your supply comfortably handles.
