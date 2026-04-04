#ifndef WEBUI_H
#define WEBUI_H

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include "config.h"
#include "sms_storage.h"
#include "settings.h"
#include "error_log.h"

// ============================================
// WebUI Server
// ============================================
namespace WebUI {
    WebServer server(WEBSERVER_PORT);
    bool connected = false;
    String lastSignalStrength = "?";

    // Forward declarations - werden von main.cpp gesetzt
    void (*onSendSms)(const String& number, const String& text) = nullptr;
    void (*onGuestbookEntry)(const String& name, const String& message) = nullptr;
    void (*onImageUpload)(const uint8_t* data, int len) = nullptr;

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
<div class="nav"><a href="/">Start</a><a href="/settings">Settings</a><a href="/log">Log</a></div>
)rawhtml";

        // Status Card
        html += "<div class=\"card\"><h2>STATUS</h2><div class=\"status-grid\">";
        html += "<div class=\"status-item\"><span class=\"label\">Signal</span><span class=\"value\">" + htmlEscape(lastSignalStrength) + "</span></div>";
        html += "<div class=\"status-item\"><span class=\"label\">Uptime</span><span class=\"value\">";
        html += String(h) + "h " + String(m) + "m " + String(s) + "s</span></div>";
        html += "<div class=\"status-item\"><span class=\"label\">SMS</span><span class=\"value\">" + String(SmsHistory::totalCount) + " empfangen</span></div>";
        html += "<div class=\"status-item\"><span class=\"label\">WLAN</span><span class=\"value\">" + String(OTA_HOSTNAME) + ".local</span></div>";
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

        // Bild drucken
        html += R"rawhtml(
<div class="card">
<h2>BILD DRUCKEN</h2>
<form class="sms-form" method="POST" action="/upload" enctype="multipart/form-data">
<input type="file" name="image" accept="image/bmp" style="width:100%;background:#0a0a1a;color:#e0e0e0;border:1px solid #0f3460;border-radius:4px;padding:8px;font-family:inherit;margin-bottom:8px">
<button type="submit">DRUCKEN</button>
</form>
<p style="color:#666;font-size:.75em;margin-top:6px">BMP-Format (24-bit). Wird automatisch auf 384px Breite skaliert und als Schwarz-Weiss Dithering gedruckt.</p>
</div>
)rawhtml";

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
<div class="nav"><a href="/">Start</a><a href="/settings">Settings</a><a href="/log">Log</a></div>
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

        // Telegram
        html += "<div class=\"card\"><h2>TELEGRAM BOT</h2>";
        html += "<div class=\"setting-row\"><span class=\"label\">Telegram aktiv</span>";
        html += "<label class=\"toggle\"><input type=\"checkbox\" name=\"tgEnabled\" value=\"1\"";
        if (Settings::telegramEnabled) html += " checked";
        html += "><span class=\"slider\"></span></label></div>";

        html += "<div class=\"setting-row\"><span class=\"label\">Bot-Token</span>";
        html += "<input type=\"text\" name=\"tgToken\" value=\"" + htmlEscape(Settings::telegramToken)
              + "\" placeholder=\"123456:ABC-DEF...\" style=\"width:220px;font-size:.8em\"></div>";

        html += "<div class=\"setting-row\"><span class=\"label\">Chat-ID</span><span style=\"color:#00ff88\">";
        html += Settings::telegramChatId.length() > 0 ? htmlEscape(Settings::telegramChatId) : "(wird automatisch gesetzt)";
        html += "</span></div>";

        html += "<p class=\"note\">1. @BotFather auf Telegram anschreiben<br>"
                "2. /newbot &rarr; Name + Username vergeben<br>"
                "3. Token hier eintragen, speichern, Neustart<br>"
                "4. Dem Bot eine Nachricht schreiben &rarr; fertig!<br><br>"
                "Empfaengt Text + Bilder. Bilder werden als Dithering-Druck ausgegeben. "
                "Befehle: /start, /status</p>";
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

        html += "<div class=\"setting-row\"><span class=\"label\">Gaestebuch-Modus</span>";
        html += "<label class=\"toggle\"><input type=\"checkbox\" name=\"guestbook\" value=\"1\"";
        if (Settings::guestbookMode) html += " checked";
        html += "><span class=\"slider\"></span></label></div>";
        html += "<p class=\"note\">Gaestebuch: Jeder im WLAN kann unter /guestbook eine Nachricht hinterlassen die sofort gedruckt wird. Perfekt fuer Partys!</p>";
        html += "</div>";

        // Geraet
        html += "<div class=\"card\"><h2>GERAET</h2>";
        html += "<div class=\"setting-row\"><span class=\"label\">Name</span>";
        html += "<input type=\"text\" name=\"name\" value=\"" + htmlEscape(Settings::deviceName) + "\" maxlength=\"16\"></div>";
        html += "</div>";

        html += "<button type=\"submit\" class=\"save-btn\">SPEICHERN</button>";
        html += "</form>";

        // WLAN (ausserhalb des Settings-Formulars)
        html += "<div class=\"card\" style=\"margin-top:16px\"><h2>WLAN</h2>";
        html += "<div class=\"setting-row\"><span class=\"label\">Verbunden mit</span><span style=\"color:#00ff88\">" + WiFi.SSID() + "</span></div>";
        html += "<div class=\"setting-row\"><span class=\"label\">IP-Adresse</span><span style=\"color:#00ff88\">" + WiFi.localIP().toString() + "</span></div>";
        html += "<div class=\"setting-row\"><span class=\"label\">Signalstaerke</span><span style=\"color:#00ff88\">" + String(WiFi.RSSI()) + " dBm</span></div>";
        html += "</div>";

        // WLAN Reset
        html += R"rawhtml(
<div class="card">
<h2>WLAN ZURUECKSETZEN</h2>
<p class="note" style="margin-bottom:8px">Loescht gespeicherte WLAN-Daten. Beim naechsten Start oeffnet sich der Setup-Hotspot "Minifax-Setup" um ein neues WLAN zu konfigurieren.</p>
<form method="POST" action="/wifi-reset">
<button type="submit" style="width:100%;background:#0f3460;color:#e04040;border:1px solid #e04040;border-radius:4px;padding:8px;font-family:inherit;cursor:pointer">WLAN ZURUECKSETZEN + NEUSTART</button>
</form>
</div>
)rawhtml";

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
    // Gaestebuch Seite
    // ============================================
    String buildGuestbookPage(bool sent = false) {
        String html = R"rawhtml(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>)rawhtml";
        html += htmlEscape(Settings::deviceName);
        html += R"rawhtml( - Gaestebuch</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#1a1a2e;color:#e0e0e0;font-family:'Courier New',monospace;max-width:500px;margin:0 auto;padding:16px;min-height:100vh;display:flex;flex-direction:column;justify-content:center}
h1{text-align:center;font-size:2.2em;letter-spacing:6px;color:#fff;text-shadow:0 0 20px rgba(0,255,136,.4);margin:10px 0}
.sub{text-align:center;color:#00ff88;font-size:1em;margin-bottom:24px}
.card{background:#16213e;border:1px solid #0f3460;border-radius:12px;padding:20px;margin-bottom:16px}
.card input,.card textarea{width:100%;background:#0a0a1a;color:#e0e0e0;border:1px solid #0f3460;border-radius:6px;padding:12px;font-family:inherit;font-size:1.1em;margin-bottom:12px}
.card textarea{min-height:100px;resize:vertical}
.card button{width:100%;background:#00ff88;color:#1a1a2e;border:none;border-radius:6px;padding:14px;font-family:inherit;font-size:1.2em;font-weight:bold;cursor:pointer;letter-spacing:3px}
.card button:hover{background:#00cc6a}
.success{background:#00ff88;color:#1a1a2e;padding:16px;border-radius:12px;text-align:center;margin-bottom:16px;font-size:1.1em}
.success b{font-size:1.3em}
.hint{text-align:center;color:#444;font-size:.8em;margin-top:12px}
</style>
</head>
<body>
)rawhtml";

        html += "<h1>" + htmlEscape(Settings::deviceName) + "</h1>";
        html += "<p class=\"sub\">Schreib eine Nachricht!</p>";

        if (sent) {
            html += R"rawhtml(
<div class="success">
<b>Gedruckt!</b><br>Deine Nachricht kommt gleich aus dem Drucker.
</div>
)rawhtml";
        }

        html += R"rawhtml(
<div class="card">
<form method="POST" action="/guestbook">
<input type="text" name="name" placeholder="Dein Name" maxlength="30" required>
<textarea name="msg" placeholder="Deine Nachricht..." maxlength="300" required></textarea>
<button type="submit">DRUCKEN!</button>
</form>
</div>
<p class="hint">Deine Nachricht wird sofort auf dem Mini-Fax ausgedruckt.</p>
</body></html>
)rawhtml";

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
        Settings::telegramEnabled = server.hasArg("tgEnabled");
        if (server.hasArg("tgToken")) {
            String newToken = server.arg("tgToken");
            newToken.trim();
            if (newToken != Settings::telegramToken) {
                Settings::telegramToken = newToken;
                Settings::telegramChatId = ""; // Reset bei neuem Token
            }
        }
        Settings::faxSoundEnabled = server.hasArg("faxSound");
        Settings::autoPrint = server.hasArg("autoPrint");
        Settings::printQrCode = server.hasArg("printQr");
        Settings::guestbookMode = server.hasArg("guestbook");
        if (server.hasArg("name") && server.arg("name").length() > 0) {
            Settings::deviceName = server.arg("name");
        }

        Settings::save();
        server.send(200, "text/html", buildSettingsPage(true));
    }

    // Bild-Upload Buffer
    uint8_t* uploadBuffer = nullptr;
    int uploadBufferLen = 0;
    int uploadBufferSize = 0;

    void handleUploadData() {
        HTTPUpload& upload = server.upload();

        if (upload.status == UPLOAD_FILE_START) {
            Serial.println("[IMG] Upload Start: " + upload.filename);
            // Max 100KB
            uploadBufferSize = 100000;
            uploadBuffer = (uint8_t*)malloc(uploadBufferSize);
            uploadBufferLen = 0;
            if (!uploadBuffer) {
                Serial.println("[IMG] Nicht genug RAM fuer Upload");
                uploadBufferSize = 0;
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (uploadBuffer && uploadBufferLen + upload.currentSize <= uploadBufferSize) {
                memcpy(uploadBuffer + uploadBufferLen, upload.buf, upload.currentSize);
                uploadBufferLen += upload.currentSize;
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            Serial.printf("[IMG] Upload fertig: %d Bytes\n", uploadBufferLen);
        }
    }

    void handleUploadDone() {
        if (uploadBuffer && uploadBufferLen > 0 && onImageUpload) {
            onImageUpload(uploadBuffer, uploadBufferLen);
            server.sendHeader("Location", "/");
            server.send(303);
        } else {
            server.send(400, "text/plain", "Upload fehlgeschlagen");
        }

        if (uploadBuffer) {
            free(uploadBuffer);
            uploadBuffer = nullptr;
            uploadBufferLen = 0;
            uploadBufferSize = 0;
        }
    }

    void handleLogPage() {
        String html = R"rawhtml(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MINIFAX - Log</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#1a1a2e;color:#e0e0e0;font-family:'Courier New',monospace;max-width:700px;margin:0 auto;padding:16px}
h1{text-align:center;font-size:2.5em;letter-spacing:8px;color:#fff;text-shadow:0 0 20px rgba(0,255,136,.4);margin:20px 0}
.subtitle{text-align:center;color:#666;font-size:.8em;margin-bottom:24px}
.nav{text-align:center;margin-bottom:16px}
.nav a{color:#00ff88;text-decoration:none;margin:0 12px;font-size:.9em}
.nav a:hover{text-decoration:underline}
.card{background:#16213e;border:1px solid #0f3460;border-radius:8px;padding:16px;margin-bottom:16px}
.card h2{color:#00ff88;font-size:1em;margin-bottom:12px;letter-spacing:2px}
.log-box{background:#0a0a1a;border:1px solid #0f3460;border-radius:4px;padding:12px;font-size:.8em;white-space:pre-wrap;word-break:break-all;max-height:500px;overflow-y:auto;line-height:1.6}
.log-box .ERR{color:#e04040}
.log-box .WARN{color:#e0a040}
.log-box .INFO{color:#00ff88}
.status-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:12px}
.status-item{display:flex;justify-content:space-between}
.status-item .label{color:#888}
.status-item .value{color:#00ff88}
</style>
</head>
<body>
<h1>MINIFAX</h1>
<p class="subtitle">System-Log</p>
<div class="nav"><a href="/">Start</a><a href="/settings">Settings</a><a href="/log">Log</a></div>
)rawhtml";

        // System-Info
        html += "<div class=\"card\"><h2>SYSTEM</h2><div class=\"status-grid\">";
        html += "<div class=\"status-item\"><span class=\"label\">Free Heap</span><span class=\"value\">"
              + String(ESP.getFreeHeap() / 1024) + " KB</span></div>";
        html += "<div class=\"status-item\"><span class=\"label\">Min Heap</span><span class=\"value\">"
              + String(ESP.getMinFreeHeap() / 1024) + " KB</span></div>";

        unsigned long sec = millis() / 1000;
        html += "<div class=\"status-item\"><span class=\"label\">Uptime</span><span class=\"value\">"
              + String(sec / 3600) + "h " + String((sec % 3600) / 60) + "m</span></div>";
        html += "<div class=\"status-item\"><span class=\"label\">WiFi RSSI</span><span class=\"value\">"
              + String(WiFi.RSSI()) + " dBm</span></div>";
        html += "</div></div>";

        // Log-Eintraege
        html += "<div class=\"card\"><h2>LOG</h2><div class=\"log-box\">";
        String logContent = ErrorLog::readAll();
        // ERR/WARN/INFO farbig markieren
        logContent.replace("[ERR ]", "<span class=\"ERR\">[ERR ]</span>");
        logContent.replace("[WARN]", "<span class=\"WARN\">[WARN]</span>");
        logContent.replace("[INFO]", "<span class=\"INFO\">[INFO]</span>");
        html += logContent;
        html += "</div></div>";

        // Buttons
        html += R"rawhtml(
<div style="display:flex;gap:8px">
<form method="GET" action="/log" style="flex:1"><button type="submit" style="width:100%;background:#0f3460;color:#00ff88;border:1px solid #0f3460;border-radius:4px;padding:8px;font-family:inherit;cursor:pointer">AKTUALISIEREN</button></form>
<form method="POST" action="/log-clear" style="flex:1"><button type="submit" style="width:100%;background:#0f3460;color:#e04040;border:1px solid #e04040;border-radius:4px;padding:8px;font-family:inherit;cursor:pointer">LOG LOESCHEN</button></form>
</div>
</body></html>
)rawhtml";

        server.send(200, "text/html", html);
    }

    void handleLogClear() {
        ErrorLog::clear();
        ErrorLog::info("SYS", "Log geloescht via WebUI");
        server.sendHeader("Location", "/log");
        server.send(303);
    }

    void handleGuestbookGet() {
        if (!Settings::guestbookMode) {
            server.sendHeader("Location", "/");
            server.send(302);
            return;
        }
        server.send(200, "text/html", buildGuestbookPage(false));
    }

    void handleGuestbookPost() {
        if (!Settings::guestbookMode) {
            server.send(403, "text/plain", "Gaestebuch deaktiviert");
            return;
        }

        if (server.hasArg("name") && server.hasArg("msg")) {
            String name = server.arg("name");
            String msg = server.arg("msg");

            if (name.length() > 0 && msg.length() > 0 && onGuestbookEntry) {
                onGuestbookEntry(name, msg);
            }
        }
        server.send(200, "text/html", buildGuestbookPage(true));
    }

    void handleWifiReset() {
        server.send(200, "text/html",
            "<html><body style='background:#1a1a2e;color:#e0e0e0;font-family:monospace;text-align:center;padding:40px'>"
            "<h1>WLAN zurueckgesetzt!</h1>"
            "<p>Neustart in 3 Sekunden...</p>"
            "<p>Verbinde dich dann mit dem Hotspot <b>Minifax-Setup</b></p>"
            "</body></html>");
        delay(1000);
        WiFiManager wm;
        wm.resetSettings();
        delay(1000);
        ESP.restart();
    }

    void handleNotFound() {
        server.sendHeader("Location", "/");
        server.send(302);
    }

    // ============================================
    // Init & Loop
    // ============================================
    // Callback fuer OLED-Anzeige waehrend Config-Portal
    void (*onConfigPortalStarted)(const char* apName) = nullptr;

    void init() {
        WiFiManager wm;

        // Timeout: nach X Sekunden Config-Portal schliessen
        // und ohne WLAN weiterlaufen (SMS geht trotzdem)
        wm.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT);

        // Dark Theme fuer das Config-Portal
        wm.setClass("invert");

        // Debug auf Serial
        wm.setDebugOutput(true);

        Serial.println("[WIFI] Starte WiFiManager...");
        Serial.println("[WIFI] Falls kein WLAN gespeichert: Hotspot '" WIFI_AP_NAME "'");

        // OLED Anzeige wenn Config-Portal startet
        if (onConfigPortalStarted) {
            onConfigPortalStarted(WIFI_AP_NAME);
        }

        // autoConnect: versucht gespeichertes WLAN,
        // oeffnet sonst Config-Portal als Access Point
        bool wifiOk = wm.autoConnect(WIFI_AP_NAME);

        if (wifiOk) {
            connected = true;
            Serial.println();
            Serial.print("[WIFI] Verbunden mit: ");
            Serial.println(WiFi.SSID());
            Serial.print("[WIFI] IP: ");
            Serial.println(WiFi.localIP());

            server.on("/", handleRoot);
            server.on("/send", HTTP_POST, handleSend);
            server.on("/clear", HTTP_POST, handleClear);
            server.on("/settings", HTTP_GET, handleSettingsGet);
            server.on("/settings", HTTP_POST, handleSettingsPost);
            server.on("/wifi-reset", HTTP_POST, handleWifiReset);
            server.on("/log", HTTP_GET, handleLogPage);
            server.on("/log-clear", HTTP_POST, handleLogClear);
            server.on("/upload", HTTP_POST, handleUploadDone, handleUploadData);
            server.on("/guestbook", HTTP_GET, handleGuestbookGet);
            server.on("/guestbook", HTTP_POST, handleGuestbookPost);
            server.onNotFound(handleNotFound);
            server.begin();

            Serial.print("[WEBUI] Server gestartet auf http://");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println();
            Serial.println("[WIFI] Kein WLAN konfiguriert/erreichbar.");
            Serial.println("[WIFI] WebUI nicht verfuegbar, SMS-Empfang funktioniert trotzdem.");
            Serial.println("[WIFI] Neustart um Config-Portal erneut zu oeffnen.");
        }
    }

    void update() {
        if (connected) {
            server.handleClient();
        }
    }
}

#endif // WEBUI_H
