#ifndef WEBUI_H
#define WEBUI_H

#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "sms_storage.h"
#include "settings.h"

// ============================================
// WebUI Server
// ============================================
namespace WebUI {
    WebServer server(WEBSERVER_PORT);
    bool connected = false;
    String lastSignalStrength = "?";

    // Forward declarations - werden von main.cpp gesetzt
    void (*onSendSms)(const String& number, const String& text) = nullptr;

    // HTML escapen gegen XSS
    String htmlEscape(const String& text) {
        String out = text;
        out.replace("&", "&amp;");
        out.replace("<", "&lt;");
        out.replace(">", "&gt;");
        out.replace("\"", "&quot;");
        out.replace("\n", "<br>");
        return out;
    }

    // ============================================
    // HTML Seite generieren
    // ============================================
    String buildPage(const String& replyTo = "") {
        unsigned long uptimeSec = millis() / 1000;
        unsigned long h = uptimeSec / 3600;
        unsigned long m = (uptimeSec % 3600) / 60;
        unsigned long s = uptimeSec % 60;

        String html = R"rawhtml(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MINIFAX</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#1a1a2e;color:#e0e0e0;font-family:'Courier New',monospace;max-width:600px;margin:0 auto;padding:16px}
h1{text-align:center;font-size:2.5em;letter-spacing:8px;color:#fff;text-shadow:0 0 20px rgba(0,255,136,.4);margin:20px 0}
.subtitle{text-align:center;color:#666;font-size:.8em;margin-bottom:24px}
.card{background:#16213e;border:1px solid #0f3460;border-radius:8px;padding:16px;margin-bottom:16px}
.card h2{color:#00ff88;font-size:1em;margin-bottom:12px;letter-spacing:2px}
.status-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.status-item{display:flex;justify-content:space-between}
.status-item .label{color:#888}
.status-item .value{color:#00ff88}
.sms-form textarea{width:100%;background:#0a0a1a;color:#e0e0e0;border:1px solid #0f3460;border-radius:4px;padding:8px;font-family:inherit;font-size:1em;resize:vertical;min-height:60px}
.sms-form input[type=text]{width:100%;background:#0a0a1a;color:#e0e0e0;border:1px solid #0f3460;border-radius:4px;padding:8px;font-family:inherit;font-size:1em;margin-bottom:8px}
.sms-form button{width:100%;background:#00ff88;color:#1a1a2e;border:none;border-radius:4px;padding:10px;font-family:inherit;font-size:1em;font-weight:bold;cursor:pointer;letter-spacing:2px;margin-top:8px}
.sms-form button:hover{background:#00cc6a}
.sms-entry{border-bottom:1px solid #0f3460;padding:12px 0}
.sms-entry:last-child{border-bottom:none}
.sms-meta{display:flex;justify-content:space-between;margin-bottom:4px;font-size:.8em}
.sms-sender{color:#00ff88}
.sms-time{color:#666}
.sms-text{color:#e0e0e0;white-space:pre-wrap;word-break:break-word}
.empty{color:#444;text-align:center;padding:20px;font-style:italic}
.sep{border:none;border-top:1px dashed #0f3460;margin:0}
.refresh{text-align:center;margin-top:16px}
.refresh a{color:#0f3460;text-decoration:none;font-size:.8em}
.refresh a:hover{color:#00ff88}
.badge{display:inline-block;background:#0f3460;color:#00ff88;border-radius:10px;padding:2px 8px;font-size:.7em;margin-left:8px}
.nav{text-align:center;margin-bottom:16px}
.nav a{color:#00ff88;text-decoration:none;margin:0 12px;font-size:.9em}
.nav a:hover{text-decoration:underline}
.setting-row{display:flex;justify-content:space-between;align-items:center;padding:8px 0;border-bottom:1px solid #0f3460}
.setting-row:last-child{border-bottom:none}
.setting-row .label{color:#888}
.setting-row select,.setting-row input[type=range],.setting-row input[type=text]{background:#0a0a1a;color:#e0e0e0;border:1px solid #0f3460;border-radius:4px;padding:4px 8px;font-family:inherit}
.toggle{position:relative;width:44px;height:24px;cursor:pointer}
.toggle input{opacity:0;width:0;height:0}
.toggle .slider{position:absolute;top:0;left:0;right:0;bottom:0;background:#0a0a1a;border:1px solid #0f3460;border-radius:12px;transition:.2s}
.toggle input:checked+.slider{background:#00ff88}
.toggle .slider:before{content:"";position:absolute;height:18px;width:18px;left:2px;bottom:2px;background:#e0e0e0;border-radius:50%;transition:.2s}
.toggle input:checked+.slider:before{transform:translateX(20px)}
.save-btn{width:100%;background:#00ff88;color:#1a1a2e;border:none;border-radius:4px;padding:10px;font-family:inherit;font-size:1em;font-weight:bold;cursor:pointer;letter-spacing:2px;margin-top:12px}
.save-btn:hover{background:#00cc6a}
.note{color:#666;font-size:.75em;margin-top:4px}
</style>
</head>
<body>
<h1>MINIFAX</h1>
<p class="subtitle">Mini-Faxgeraet &middot; ESP32</p>
<div class="nav"><a href="/">Startseite</a><a href="/settings">Einstellungen</a></div>
)rawhtml";

        // Status Card
        html += "<div class=\"card\"><h2>STATUS</h2><div class=\"status-grid\">";
        html += "<div class=\"status-item\"><span class=\"label\">Signal</span><span class=\"value\">" + htmlEscape(lastSignalStrength) + "</span></div>";
        html += "<div class=\"status-item\"><span class=\"label\">Uptime</span><span class=\"value\">";
        html += String(h) + "h " + String(m) + "m " + String(s) + "s</span></div>";
        html += "<div class=\"status-item\"><span class=\"label\">SMS</span><span class=\"value\">" + String(SmsHistory::totalCount) + " empfangen</span></div>";
        html += "<div class=\"status-item\"><span class=\"label\">WLAN</span><span class=\"value\">" + WiFi.localIP().toString() + "</span></div>";
        html += "<div class=\"status-item\"><span class=\"label\">Modul</span><span class=\"value\">" + String(Settings::gsmModuleName()) + "</span></div>";
        html += "</div></div>";

        // SMS Senden (mit optionaler vorausgefuellter Nummer via QR-Code)
        html += "<div class=\"card\"><h2>SMS SENDEN</h2>";
        html += "<form class=\"sms-form\" method=\"POST\" action=\"/send\">";
        html += "<input type=\"text\" name=\"number\" placeholder=\"+49171...\" value=\""
              + htmlEscape(replyTo) + "\" required>";
        html += "<textarea name=\"text\" placeholder=\"Nachricht...\" required></textarea>";
        if (replyTo.length() > 0) {
            html += "<div style=\"color:#00ff88;font-size:.8em;margin-top:4px\">Antwort an " + htmlEscape(replyTo) + "</div>";
        }
        html += "<button type=\"submit\">SENDEN</button></form></div>";

        // SMS Verlauf
        html += "<div class=\"card\"><h2>EMPFANGEN<span class=\"badge\">" + String(SmsHistory::count()) + "</span></h2>";

        int count = SmsHistory::count();
        if (count == 0) {
            html += "<p class=\"empty\">Noch keine SMS empfangen.</p>";
        } else {
            int idx = SmsHistory::newestIndex();
            for (int i = 0; i < count; i++) {
                SmsEntry& e = SmsHistory::entries[idx];
                if (e.used) {
                    html += "<div class=\"sms-entry\">";
                    html += "<div class=\"sms-meta\"><span class=\"sms-sender\">" + htmlEscape(e.sender) + "</span>";
                    html += "<span class=\"sms-time\">" + htmlEscape(e.timestamp) + "</span></div>";
                    html += "<div class=\"sms-text\">" + htmlEscape(e.message) + "</div>";
                    html += "</div>";
                }
                idx = (idx - 1 + SMS_HISTORY_SIZE) % SMS_HISTORY_SIZE;
            }
        }
        html += "</div>";

        // Verlauf loeschen Button
        if (SmsHistory::count() > 0) {
            html += R"rawhtml(
<div class="card">
<form method="POST" action="/clear"><button type="submit" style="width:100%;background:#0f3460;color:#e04040;border:1px solid #e04040;border-radius:4px;padding:8px;font-family:inherit;cursor:pointer">VERLAUF LOESCHEN</button></form>
</div>
)rawhtml";
        }

        // Refresh
        html += R"rawhtml(
<div class="refresh"><a href="/">Aktualisieren</a></div>
</body></html>
)rawhtml";

        return html;
    }

    // ============================================
    // Settings Page
    // ============================================
    String buildSettingsPage(bool saved = false) {
        String html = R"rawhtml(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MINIFAX - Einstellungen</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#1a1a2e;color:#e0e0e0;font-family:'Courier New',monospace;max-width:600px;margin:0 auto;padding:16px}
h1{text-align:center;font-size:2.5em;letter-spacing:8px;color:#fff;text-shadow:0 0 20px rgba(0,255,136,.4);margin:20px 0}
.subtitle{text-align:center;color:#666;font-size:.8em;margin-bottom:24px}
.card{background:#16213e;border:1px solid #0f3460;border-radius:8px;padding:16px;margin-bottom:16px}
.card h2{color:#00ff88;font-size:1em;margin-bottom:12px;letter-spacing:2px}
.nav{text-align:center;margin-bottom:16px}
.nav a{color:#00ff88;text-decoration:none;margin:0 12px;font-size:.9em}
.nav a:hover{text-decoration:underline}
.setting-row{display:flex;justify-content:space-between;align-items:center;padding:10px 0;border-bottom:1px solid #0f3460}
.setting-row:last-child{border-bottom:none}
.setting-row .label{color:#888;flex:1}
.setting-row select,.setting-row input[type=text]{background:#0a0a1a;color:#e0e0e0;border:1px solid #0f3460;border-radius:4px;padding:6px 8px;font-family:inherit;font-size:.9em}
.setting-row select{min-width:160px}
.toggle{position:relative;display:inline-block;width:44px;height:24px}
.toggle input{opacity:0;width:0;height:0}
.toggle .slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#0a0a1a;border:1px solid #0f3460;border-radius:12px;transition:.2s}
.toggle input:checked+.slider{background:#00ff88;border-color:#00ff88}
.toggle .slider:before{content:"";position:absolute;height:18px;width:18px;left:2px;bottom:2px;background:#e0e0e0;border-radius:50%;transition:.2s}
.toggle input:checked+.slider:before{transform:translateX(20px)}
.save-btn{width:100%;background:#00ff88;color:#1a1a2e;border:none;border-radius:4px;padding:10px;font-family:inherit;font-size:1em;font-weight:bold;cursor:pointer;letter-spacing:2px;margin-top:12px}
.save-btn:hover{background:#00cc6a}
.note{color:#666;font-size:.75em;margin-top:4px}
.success{background:#00ff88;color:#1a1a2e;padding:10px;border-radius:4px;text-align:center;margin-bottom:16px;font-weight:bold}
</style>
</head>
<body>
<h1>MINIFAX</h1>
<p class="subtitle">Einstellungen</p>
<div class="nav"><a href="/">Startseite</a><a href="/settings">Einstellungen</a></div>
)rawhtml";

        if (saved) {
            html += "<div class=\"success\">Einstellungen gespeichert!</div>";
        }

        html += "<form method=\"POST\" action=\"/settings\">";

        // GSM-Modul
        html += "<div class=\"card\"><h2>GSM-MODUL</h2>";
        html += "<div class=\"setting-row\"><span class=\"label\">Modul-Typ</span>";
        html += "<select name=\"gsm\">";
        html += String("<option value=\"0\"") + (Settings::gsmModule == Settings::GSM_SIM800L ? " selected" : "") + ">SIM800L (2G)</option>";
        html += String("<option value=\"1\"") + (Settings::gsmModule == Settings::GSM_SIM7000G ? " selected" : "") + ">SIM7000G (4G LTE-M)</option>";
        html += String("<option value=\"2\"") + (Settings::gsmModule == Settings::GSM_SIM7600 ? " selected" : "") + ">SIM7600 (4G LTE)</option>";
        html += "</select></div>";
        html += "<p class=\"note\">SIM800L: 2G, guenstig (~5&euro;). SIM7000G/SIM7600: 4G, zukunftssicher (~15-20&euro;). Gleiche Pins, gleiche AT-Befehle. Nach Wechsel: Neustart noetig.</p>";
        html += "</div>";

        // Sound
        html += "<div class=\"card\"><h2>SOUND</h2>";
        html += "<div class=\"setting-row\"><span class=\"label\">Fax-Sound</span>";
        html += "<label class=\"toggle\"><input type=\"checkbox\" name=\"faxSound\" value=\"1\"";
        if (Settings::faxSoundEnabled) html += " checked";
        html += "><span class=\"slider\"></span></label></div>";
        html += "</div>";

        // Druck
        html += "<div class=\"card\"><h2>DRUCK</h2>";
        html += "<div class=\"setting-row\"><span class=\"label\">Auto-Druck</span>";
        html += "<label class=\"toggle\"><input type=\"checkbox\" name=\"autoPrint\" value=\"1\"";
        if (Settings::autoPrint) html += " checked";
        html += "><span class=\"slider\"></span></label></div>";

        html += "<div class=\"setting-row\"><span class=\"label\">QR-Code drucken</span>";
        html += "<label class=\"toggle\"><input type=\"checkbox\" name=\"printQr\" value=\"1\"";
        if (Settings::printQrCode) html += " checked";
        html += "><span class=\"slider\"></span></label></div>";
        html += "</div>";

        // Geraet
        html += "<div class=\"card\"><h2>GERAET</h2>";
        html += "<div class=\"setting-row\"><span class=\"label\">Name</span>";
        html += "<input type=\"text\" name=\"name\" value=\"" + htmlEscape(Settings::deviceName) + "\" maxlength=\"16\"></div>";
        html += "</div>";

        html += "<button type=\"submit\" class=\"save-btn\">SPEICHERN</button>";
        html += "</form>";

        html += R"rawhtml(
<div class="card" style="margin-top:16px">
<h2>INFO</h2>
<div class="setting-row"><span class="label">Firmware</span><span style="color:#00ff88">Minifax v1.0</span></div>
<div class="setting-row"><span class="label">OTA Update</span><span style="color:#00ff88">)rawhtml";
        html += String(OTA_HOSTNAME) + ".local";
        html += R"rawhtml(</span></div>
<div class="setting-row"><span class="label">SPIFFS</span><span style="color:#00ff88">)rawhtml";

        html += String(SPIFFS.usedBytes() / 1024) + " / " + String(SPIFFS.totalBytes() / 1024) + " KB";
        html += R"rawhtml(</span></div>
<div class="setting-row"><span class="label">Free Heap</span><span style="color:#00ff88">)rawhtml";
        html += String(ESP.getFreeHeap() / 1024) + " KB";
        html += "</span></div></div>";

        html += "</body></html>";
        return html;
    }

    // ============================================
    // Route Handlers
    // ============================================
    void handleRoot() {
        String replyTo = "";
        if (server.hasArg("reply")) {
            replyTo = server.arg("reply");
        }
        server.send(200, "text/html", buildPage(replyTo));
    }

    void handleSend() {
        if (server.hasArg("number") && server.hasArg("text")) {
            String number = server.arg("number");
            String text = server.arg("text");

            if (onSendSms && number.length() > 0 && text.length() > 0) {
                onSendSms(number, text);
                // Redirect zurueck zur Hauptseite
                server.sendHeader("Location", "/");
                server.send(303);
                return;
            }
        }
        server.send(400, "text/plain", "Fehler: Nummer und Text benoetigt");
    }

    void handleClear() {
        SmsHistory::clear();
        server.sendHeader("Location", "/");
        server.send(303);
    }

    void handleSettingsGet() {
        server.send(200, "text/html", buildSettingsPage(false));
    }

    void handleSettingsPost() {
        Settings::gsmModule = (Settings::GsmModule)server.arg("gsm").toInt();
        Settings::faxSoundEnabled = server.hasArg("faxSound");
        Settings::autoPrint = server.hasArg("autoPrint");
        Settings::printQrCode = server.hasArg("printQr");
        if (server.hasArg("name") && server.arg("name").length() > 0) {
            Settings::deviceName = server.arg("name");
        }

        Settings::save();
        server.send(200, "text/html", buildSettingsPage(true));
    }

    void handleNotFound() {
        server.sendHeader("Location", "/");
        server.send(302);
    }

    // ============================================
    // Init & Loop
    // ============================================
    void init() {
        Serial.print("[WIFI] Verbinde mit ");
        Serial.print(WIFI_SSID);

        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASS);

        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 40) {
            delay(500);
            Serial.print(".");
            retries++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            Serial.println();
            Serial.print("[WIFI] Verbunden! IP: ");
            Serial.println(WiFi.localIP());

            server.on("/", handleRoot);
            server.on("/send", HTTP_POST, handleSend);
            server.on("/clear", HTTP_POST, handleClear);
            server.on("/settings", HTTP_GET, handleSettingsGet);
            server.on("/settings", HTTP_POST, handleSettingsPost);
            server.onNotFound(handleNotFound);
            server.begin();

            Serial.print("[WEBUI] Server gestartet auf http://");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println();
            Serial.println("[WIFI] WARNUNG: Verbindung fehlgeschlagen!");
            Serial.println("[WIFI] WebUI nicht verfuegbar, SMS-Empfang funktioniert trotzdem.");
        }
    }

    void update() {
        if (connected) {
            server.handleClient();
        }
    }
}

#endif // WEBUI_H
