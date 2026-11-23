// ---------------------------------------------------------------------------
// GPS_Master_Client_v3
//
// ESP8266 + 4x MAX7219 (32x8) matrix clock. Connects to the
// GPS_Master_Server_v3 hotspot as a station, polls its /time JSON API with
// RTT-filtered sampling, and displays HH:MM:SS with a custom 3x7 font,
// WiFi/sync status pixels, periodic date peek, and scrolling status text
// (connecting/connected/fallback-AP recovery) driven entirely through the
// normal non-blocking render loop — see UIRenderer.
//
// Also runs its own small admin web server (SettingsServer) on port 80 —
// WiFi credentials, sync interval, timezone, brightness (day/night), and
// orientation/UI toggles are all runtime-editable at /settings (persisted
// in EEPROM), reachable directly by IP from the same hotspot the server's
// dashboard already links to. If it can't reach the configured WiFi after
// a few tries, it falls back to its own temporary hotspot so /settings
// stays reachable for reconfiguration — see TimeSync. /update supports
// OTA firmware upload; ArduinoOTA is also enabled for flashing over WiFi
// from the Arduino IDE.
// ---------------------------------------------------------------------------

#include <EEPROM.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "SettingsManager.h"
#include "DisplayManager.h"
#include "TimeSync.h"
#include "UIRenderer.h"
#include "SettingsServer.h"

// EEPROM size for SettingsManager's persisted struct — generous headroom
// for future settings without needing to touch this again.
#define EEPROM_SIZE 256

SettingsManager settingsManager;
DisplayManager  display(settingsManager);
TimeSync        timeSync(settingsManager);
UIRenderer      ui(display, settingsManager);
SettingsServer  settingsServer(settingsManager, display, timeSync, HTTP_PORT);

unsigned long lastSerialLog = 0;
unsigned long lastRenderMillis = 0;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== GPS_Master_Client_v3 booting ===");

  // Settings must load before display/timeSync begin(), since both now
  // read their initial config (brightness, WiFi credentials) from here.
  EEPROM.begin(EEPROM_SIZE);
  settingsManager.begin();

  display.begin();
  timeSync.begin();

  settingsServer.begin();

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  if (strlen(OTA_PASSWORD) > 0) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  }
  ArduinoOTA.onStart([]() { Serial.println("[OTA] Update starting..."); });
  ArduinoOTA.onEnd([]() { Serial.println("[OTA] Update complete, rebooting."); });
  ArduinoOTA.onError([](ota_error_t error) { Serial.printf("[OTA] Error [%u]\n", error); });
  ArduinoOTA.begin();
  Serial.println("[OTA] ArduinoOTA ready (flash over WiFi from Arduino IDE).");

  Serial.println("=== Setup complete, entering main loop ===");
}

void loop() {
  ArduinoOTA.handle();
  settingsServer.handleClient();
  timeSync.update();
  settingsManager.tick(); // periodic runtime persistence — see SettingsManager::tick()

  if (millis() - lastRenderMillis >= DISPLAY_REFRESH_INTERVAL_MS) {
    lastRenderMillis = millis();
    ClientTime t = timeSync.getCurrentTime();
    ui.renderFrame(t);

    if (millis() - lastSerialLog > 20000) {
      lastSerialLog = millis();
      if (t.hasEverSynced) {
        Serial.printf("[%04u-%02u-%02u %02u:%02u:%02u.%03u] wifi=%d gpsLocked=%d permErr=%d fallbackAP=%d\n",
                      t.year, t.month, t.day, t.hour, t.minute, t.second, t.millisecond,
                      t.wifiConnected, t.gpsFixValid, t.permanentError, timeSync.isInFallbackApMode());
      } else {
        Serial.printf("[Client] Waiting for first sync... fallbackAP=%d\n", timeSync.isInFallbackApMode());
      }
    }
  }

  yield(); // cooperative yield for the WiFi/TCP stack — cheap no-op when nothing's pending
}
