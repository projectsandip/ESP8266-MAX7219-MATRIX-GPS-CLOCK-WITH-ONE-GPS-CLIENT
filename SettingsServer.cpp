#include "SettingsServer.h"
#include "config.h"
#include <Updater.h>

SettingsServer::SettingsServer(SettingsManager &settings, DisplayManager &display, TimeSync &timeSync, uint16_t port)
  : _server(port), _settings(settings), _display(display), _timeSync(timeSync),
    _otaUploadAuthorized(false) {}

void SettingsServer::begin() {
  _server.on("/",              HTTP_GET,  [this]() { handleRoot(); });
  _server.on("/settings",      HTTP_GET,  [this]() { handleSettingsPage(); });
  _server.on("/settings/save", HTTP_POST, [this]() { handleSettingsSave(); });
  _server.on("/restart",       HTTP_POST, [this]() { handleRestart(); });
  _server.on("/update",        HTTP_GET,  [this]() { handleUpdatePage(); });
  _server.on("/update", HTTP_POST,
    [this]() { handleUpdateFinish(); },
    [this]() { handleUpdateUpload(); }
  );
  _server.onNotFound([this]() { handleNotFound(); });
  _server.begin();
  Serial.println("[Settings] Admin web server ready on port 80 (/settings, /restart, /update).");
}

void SettingsServer::handleClient() {
  _server.handleClient();
}

void SettingsServer::handleRoot() {
  _server.sendHeader("Location", "/settings");
  _server.send(303, "text/plain", "");
}

void SettingsServer::handleNotFound() {
  _server.send(404, "text/plain", "Not found");
}

// ---------------------------------------------------------------------------
// Admin auth — gates /settings, /restart, /update. Anyone connected to the
// hotspot could otherwise reflash or reconfigure this client, so this isn't
// optional. Change ADMIN_USERNAME/ADMIN_PASSWORD in config.h before deploying.
// ---------------------------------------------------------------------------
bool SettingsServer::requireAuth() {
  if (_server.authenticate(ADMIN_USERNAME, ADMIN_PASSWORD)) {
    return true;
  }
  _server.requestAuthentication(BASIC_AUTH, "GPS Master Client Admin");
  return false;
}

static String htmlAttrEscape(const String &s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '\'') out += "&#39;";
    else if (c == '"') out += "&quot;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '&') out += "&amp;";
    else out += c;
  }
  return out;
}

static String formatDuration(unsigned long totalSeconds) {
  unsigned long days    = totalSeconds / 86400UL;
  unsigned long hours   = (totalSeconds % 86400UL) / 3600UL;
  unsigned long minutes = (totalSeconds % 3600UL) / 60UL;
  unsigned long seconds = totalSeconds % 60UL;

  String out;
  if (days > 0) out += String(days) + "d ";
  if (hours > 0 || days > 0) out += String(hours) + "h ";
  if (minutes > 0 || hours > 0 || days > 0) out += String(minutes) + "m ";
  out += String(seconds) + "s";
  return out;
}

// ---------------------------------------------------------------------------
// Settings page
// ---------------------------------------------------------------------------
void SettingsServer::handleSettingsPage() {
  if (!requireAuth()) return;

  bool saved = _server.hasArg("saved");
  bool reset = _server.hasArg("reset");
  bool wifiError = _server.hasArg("wifiError");
  bool wifiChanged = _server.hasArg("wifiChanged");

  String html;
  html.reserve(5200);
  html += F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
            "<title>Settings - GPS Master Client</title>"
            "<style>"
            "body{font-family:monospace;background:#111;color:#eee;padding:20px;max-width:480px;}"
            "h1{font-size:1.1em;color:#8ecae6;}"
            "label{display:block;margin-top:14px;color:#999;}"
            "input[type=text],input[type=password],input[type=number]{width:100%;box-sizing:border-box;padding:6px;margin-top:4px;"
            "background:#222;border:1px solid #444;color:#eee;border-radius:4px;}"
            "input[type=range]{width:100%;margin-top:8px;}"
            ".hint{color:#666;font-size:0.8em;margin-top:2px;}"
            "button{margin-top:18px;padding:8px 16px;border-radius:4px;border:none;"
            "background:#8ecae6;color:#111;font-weight:bold;cursor:pointer;}"
            ".danger{background:#e74c3c;color:#fff;}"
            ".msg{background:#2ecc71;color:#111;padding:8px;border-radius:4px;margin-bottom:10px;}"
            ".msg.warn{background:#f1c40f;}"
            ".msg.err{background:#e74c3c;color:#fff;}"
            ".checkbox-row{display:flex;align-items:center;gap:8px;margin-top:10px;}"
            ".checkbox-row input{width:auto;margin:0;}"
            "a{color:#8ecae6;}"
            "</style></head><body>");

  html += F("<h1>Device Status</h1>");
  {
    ClientTime ct = _timeSync.getCurrentTime();
    html += "<div class='hint'>Single run (this boot): " + formatDuration(millis() / 1000UL) + "</div>";
    html += "<div class='hint'>Total run (all-time): " + formatDuration(_settings.totalRuntimeSecondsLive()) + "</div>";
    html += "<div class='hint'>Last sync: ";
    if (ct.hasEverSynced) {
      char buf[24];
      snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
               ct.lastSyncYear, ct.lastSyncMonth, ct.lastSyncDay,
               ct.lastSyncHour, ct.lastSyncMinute, ct.lastSyncSecond);
      html += String(buf);
    } else {
      html += "never yet";
    }
    html += "</div>";
  }

  html += F("<hr><h1>WiFi (connects to server hotspot)</h1>");
  if (wifiError) html += F("<div class='msg err'>SSID/password rejected — SSID needs 1-32 characters, "
                            "password needs to be empty (open network) or 8-63 characters (WPA2 minimum). "
                            "Nothing was changed.</div>");
  if (wifiChanged) html += F("<div class='msg warn'>WiFi settings saved — restart this device for them to take effect.</div>");

  html += F("<form method='POST' action='/settings/save'>");
  html += "<label>Server hotspot name (SSID)</label><input type='text' name='serverSsid' value='" +
          htmlAttrEscape(_settings.serverSsid()) + "' maxlength='32'>";
  html += F("<div class='hint'>Must match the server's hotspot SSID exactly.</div>");

  html += F("<label>Server hotspot password</label><input type='password' name='serverPassword' "
            "placeholder='(unchanged — leave blank to keep current)' maxlength='63'>");
  html += F("<div class='hint'>Leave blank to keep the current password. Must match the server's "
            "hotspot password (or leave the checkbox below checked if the server hotspot is open).</div>");
  html += F("<div class='checkbox-row'><input type='checkbox' name='serverOpen' id='serverOpen' value='1'>"
            "<label for='serverOpen' style='display:inline;margin:0;'>Server hotspot has no password (open network)</label></div>");

  html += F("<button type='submit'>Save WiFi Settings</button>"
            "</form>");

  html += F("<hr><h1>Sync &amp; Display Settings</h1>");
  if (saved) html += F("<div class='msg'>Settings saved.</div>");
  if (reset) html += F("<div class='msg warn'>Everything reset to defaults, including WiFi credentials — restart this device for the WiFi change to take effect.</div>");

  html += F("<form method='POST' action='/settings/save'>");
  html += F("<input type='hidden' name='formSyncDisplay' value='1'>");

  html += "<label>Sync interval (ms)</label><input type='number' name='syncInterval' value='" +
          String(_settings.syncIntervalMs()) + "' min='1000' max='3600000'>";
  html += F("<div class='hint'>How often to poll the server for time. 1s-1hr.</div>");

  html += "<label>Timezone offset (minutes from UTC)</label><input type='number' name='timezone' value='" +
          String(_settings.timezoneOffsetMinutes()) + "' min='-720' max='840'>";
  html += F("<div class='hint'>e.g. 330 for India (+5:30), -300 for US Eastern (-5:00), 0 for UTC. "
            "The server always provides UTC; this offset is applied here so different clients can "
            "show different time zones. Applies immediately, no restart needed.</div>");

  html += "<label>Matrix brightness (0-15)</label><input type='number' name='brightness' value='" +
          String(_settings.matrixBrightness()) + "' min='0' max='15'>";
  html += F("<div class='hint'>Applies immediately, no restart needed.</div>");

  html += "<div class='checkbox-row'><input type='checkbox' name='chainReversed' id='chainReversed' value='1'" +
          String(_settings.moduleChainReversed() ? " checked" : "") + ">"
          "<label for='chainReversed' style='display:inline;margin:0;'>Module chain reversed</label></div>";
  html += "<div class='checkbox-row'><input type='checkbox' name='mirrorBits' id='mirrorBits' value='1'" +
          String(_settings.moduleMirrorBits() ? " checked" : "") + ">"
          "<label for='mirrorBits' style='display:inline;margin:0;'>Mirror bits (digits look horizontally flipped)</label></div>";
  html += F("<div class='hint'>Orientation fixes — see the README if digits look wrong. Apply immediately.</div>");

  html += "<div class='checkbox-row'><input type='checkbox' name='displayFlipped' id='displayFlipped' value='1'" +
          String(_settings.displayFlipped() ? " checked" : "") + ">"
          "<label for='displayFlipped' style='display:inline;margin:0;'>Reverse display (180&deg; rotation)</label></div>";
  html += F("<div class='hint'>For when the whole assembled unit is mounted upside down — different from "
            "the wiring fixes above. Applies immediately.</div>");

  html += "<div class='checkbox-row'><input type='checkbox' name='datePeek' id='datePeek' value='1'" +
          String(_settings.enableDatePeek() ? " checked" : "") + ">"
          "<label for='datePeek' style='display:inline;margin:0;'>Enable date peek (briefly show DD-MM)</label></div>";

  html += F("<button type='submit'>Save Sync &amp; Display Settings</button>"
            "</form>");

  html += F("<hr><h1>Night Brightness</h1>");
  html += F("<form method='POST' action='/settings/save'>");
  html += "<div class='checkbox-row'><input type='checkbox' name='nightEnabled' id='nightEnabled' value='1'" +
          String(_settings.enableNightBrightness() ? " checked" : "") + ">"
          "<label for='nightEnabled' style='display:inline;margin:0;'>Enable night brightness</label></div>";
  html += F("<div class='hint'>Automatically dims (or brightens) the display during a set hour range, "
            "based on this device's local time (after the timezone offset above). No light sensor needed.</div>");

  html += "<label>Night brightness (0-15)</label><input type='number' name='nightBrightness' value='" +
          String(_settings.nightBrightness()) + "' min='0' max='15'>";

  html += "<label>Night starts at (0-23)</label><input type='number' name='nightStart' value='" +
          String(_settings.nightStartHour()) + "' min='0' max='23'>";
  html += "<label>Night ends at (0-23)</label><input type='number' name='nightEnd' value='" +
          String(_settings.nightEndHour()) + "' min='0' max='23'>";
  html += F("<div class='hint'>e.g. starts 22, ends 6 for 10pm-6am. Applies immediately.</div>");

  html += F("<button type='submit'>Save Night Brightness Settings</button>"
            "</form>");

  html += F("<form method='POST' action='/settings/save' onsubmit=\"return confirm('Reset ALL settings to defaults, including WiFi credentials? This requires a restart to fully take effect.')\">"
            "<input type='hidden' name='reset' value='1'>"
            "<button type='submit' class='danger'>Reset Everything to Defaults</button>"
            "</form>");

  html += F("<hr><h1>Device Control</h1>"
            "<form method='POST' action='/restart' onsubmit=\"return confirm('Restart this device now?')\">"
            "<button type='submit' class='danger'>Restart Device</button>"
            "</form>"
            "<p><a href='/update'>Firmware update (OTA)</a></p>"
            "</body></html>");

  _server.send(200, "text/html", html);
}

void SettingsServer::handleSettingsSave() {
  if (!requireAuth()) return;

  if (_server.hasArg("reset")) {
    _settings.resetToDefaults();
    _display.refreshBrightnessFromSettings();
    _server.sendHeader("Location", "/settings?reset=1");
    _server.send(303, "text/plain", "");
    return;
  }

  bool wifiChanged = false;
  bool wifiError = false;

  if (_server.hasArg("serverSsid")) {
    String ssid = _server.arg("serverSsid");
    if (!_settings.setServerSsid(ssid)) {
      wifiError = true;
    } else {
      wifiChanged = true;
    }
  }

  bool wantsOpenNetwork = _server.hasArg("serverOpen");
  String newPassword = _server.hasArg("serverPassword") ? _server.arg("serverPassword") : "";
  if (wantsOpenNetwork) {
    _settings.setServerPassword("");
    wifiChanged = true;
  } else if (newPassword.length() > 0) {
    if (!_settings.setServerPassword(newPassword)) {
      wifiError = true;
    } else {
      wifiChanged = true;
    }
  }

  if (_server.hasArg("syncInterval")) {
    _settings.setSyncIntervalMs(_server.arg("syncInterval").toInt());
  }
  if (_server.hasArg("timezone")) {
    _settings.setTimezoneOffsetMinutes((int16_t)_server.arg("timezone").toInt());
  }
  if (_server.hasArg("brightness")) {
    _settings.setMatrixBrightness((uint8_t)_server.arg("brightness").toInt());
  }
  // Checkboxes only get processed when this specific form was the one
  // submitted (marked by formSyncDisplay) — an unchecked checkbox sends
  // no value at all, so without this marker there'd be no way to tell
  // "unchecked on this form" apart from "a different form was submitted",
  // and these would get silently reset to false by, e.g., saving the
  // Night Brightness form below.
  if (_server.hasArg("formSyncDisplay")) {
    _settings.setModuleChainReversed(_server.hasArg("chainReversed"));
    _settings.setModuleMirrorBits(_server.hasArg("mirrorBits"));
    _settings.setDisplayFlipped(_server.hasArg("displayFlipped"));
    _settings.setEnableDatePeek(_server.hasArg("datePeek"));
  }

  // Night brightness fields only appear in that form's own submission —
  // same reasoning as above.
  if (_server.hasArg("nightBrightness") || _server.hasArg("nightStart") || _server.hasArg("nightEnd") || _server.hasArg("nightEnabled")) {
    _settings.setEnableNightBrightness(_server.hasArg("nightEnabled"));
    if (_server.hasArg("nightBrightness")) {
      _settings.setNightBrightness((uint8_t)_server.arg("nightBrightness").toInt());
    }
    if (_server.hasArg("nightStart")) {
      _settings.setNightStartHour((uint8_t)_server.arg("nightStart").toInt());
    }
    if (_server.hasArg("nightEnd")) {
      _settings.setNightEndHour((uint8_t)_server.arg("nightEnd").toInt());
    }
  }

  _settings.save();
  _display.refreshBrightnessFromSettings(); // applies immediately, no restart needed

  String location = "/settings?saved=1";
  if (wifiChanged) location += "&wifiChanged=1";
  if (wifiError) location += "&wifiError=1";
  _server.sendHeader("Location", location);
  _server.send(303, "text/plain", "");
}

// ---------------------------------------------------------------------------
// Restart
// ---------------------------------------------------------------------------
void SettingsServer::handleRestart() {
  if (!requireAuth()) return;

  _settings.flushRuntimeNow(); // capture the partial interval before rebooting

  _server.send(200, "text/html",
    "<html><body style='font-family:monospace;background:#111;color:#eee;padding:20px;'>"
    "Restarting&hellip; give it a few seconds to reconnect and resync."
    "</body></html>");
  _server.client().flush();
  delay(300);
  ESP.restart();
}

// ---------------------------------------------------------------------------
// OTA firmware update via web upload
// ---------------------------------------------------------------------------
void SettingsServer::handleUpdatePage() {
  if (!requireAuth()) return;

  String html =
    "<html><head><meta charset='utf-8'><title>Firmware Update</title>"
    "<style>body{font-family:monospace;background:#111;color:#eee;padding:20px;max-width:480px;}"
    "h1{font-size:1.1em;color:#8ecae6;} input[type=file]{margin:14px 0;}"
    "button{padding:8px 16px;border-radius:4px;border:none;background:#8ecae6;color:#111;font-weight:bold;}"
    "a{color:#8ecae6;}</style></head><body>"
    "<h1>Firmware Update (OTA)</h1>"
    "<p>Select a compiled .bin file (Sketch &rarr; Export Compiled Binary in Arduino IDE) and upload it below. "
    "The device will restart automatically once the upload completes.</p>"
    "<form method='POST' action='/update' enctype='multipart/form-data' "
    "onsubmit=\"return confirm('Upload and flash this firmware now?')\">"
    "<input type='file' name='firmware' accept='.bin' required>"
    "<br><button type='submit'>Upload &amp; Flash</button>"
    "</form>"
    "<p><a href='/settings'>&larr; Back to settings</a></p>"
    "</body></html>";

  _server.send(200, "text/html", html);
}

void SettingsServer::handleUpdateUpload() {
  // Auth is checked once at the start of the upload stream. If it fails,
  // _otaUploadAuthorized stays false for the rest of this request, so
  // subsequent WRITE/END chunks skip Update entirely rather than calling
  // write()/end() on a session that was never begin()'d.
  HTTPUpload &upload = _server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    _otaUploadAuthorized = _server.authenticate(ADMIN_USERNAME, ADMIN_PASSWORD);
    if (!_otaUploadAuthorized) {
      Serial.println("[OTA] Upload rejected: not authenticated.");
      return;
    }
    Serial.printf("[OTA] Upload starting: %s\n", upload.filename.c_str());
    // ESP8266's Updater.h has no UPDATE_SIZE_UNKNOWN constant (that's an
    // ESP32-ism) — compute the available sketch space explicitly instead.
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!_otaUploadAuthorized) return;
    Update.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (!_otaUploadAuthorized) return;
    bool ok = Update.end(true);
    Serial.printf("[OTA] Upload finished: %s (%u bytes)\n", ok ? "OK" : "FAILED", upload.totalSize);
  }
}

void SettingsServer::handleUpdateFinish() {
  if (!_server.authenticate(ADMIN_USERNAME, ADMIN_PASSWORD)) {
    _server.requestAuthentication(BASIC_AUTH, "GPS Master Client Admin");
    return;
  }

  bool ok = _otaUploadAuthorized && !Update.hasError();
  _otaUploadAuthorized = false; // reset for the next attempt
  _server.sendHeader("Connection", "close");
  _server.send(200, "text/html",
    ok ? "<html><body style='font-family:monospace;background:#111;color:#eee;padding:20px;'>"
         "Update OK &mdash; restarting&hellip;</body></html>"
       : "<html><body style='font-family:monospace;background:#111;color:#eee;padding:20px;'>"
         "Update FAILED. Device was not restarted &mdash; try again.</body></html>");

  if (ok) {
    delay(300);
    ESP.restart();
  }
}
