#ifndef SETTINGS_SERVER_H
#define SETTINGS_SERVER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "SettingsManager.h"
#include "DisplayManager.h"
#include "TimeSync.h"

// This device's own admin web server — /settings, /restart, /update (OTA).
// Reachable directly by IP from any browser on the same hotspot (the
// server's dashboard already links here from its Connected Clients table:
// http://<client-ip>/settings). Mirrors the server's JsonAPI admin routes
// closely on purpose, so the two stay easy to reason about together.
class SettingsServer {
public:
  SettingsServer(SettingsManager &settings, DisplayManager &display, TimeSync &timeSync, uint16_t port);

  void begin();
  void handleClient();

private:
  ESP8266WebServer _server;
  SettingsManager &_settings;
  DisplayManager  &_display;
  TimeSync        &_timeSync;
  bool _otaUploadAuthorized;

  bool requireAuth();          // sends a 401 challenge and returns false if not authenticated
  void handleRoot();           // GET  /               -> redirect to /settings
  void handleSettingsPage();   // GET  /settings        -> form showing current values
  void handleSettingsSave();   // POST /settings/save   -> applies + persists new values
  void handleRestart();        // POST /restart         -> restarts the device
  void handleUpdatePage();     // GET  /update          -> firmware upload form
  void handleUpdateFinish();   // POST /update          -> completes the upload, then restarts
  void handleUpdateUpload();   // upload handler (streamed chunks) for /update
  void handleNotFound();
};

#endif // SETTINGS_SERVER_H
