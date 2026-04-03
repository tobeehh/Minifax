#ifndef WEBUI_H
#define WEBUI_H

#include <WiFi.h>
#include <WebServer.h>
#include "config.h"

// ============================================
// SMS Verlauf (Ringbuffer)
// ============================================
struct SmsEntry {
    String sender;
    String timestamp;
    String message;
    bool used = false;
};

namespace SmsHistory {
    SmsEntry entries[SMS_HISTORY_SIZE];
    int writeIndex = 0;
    int totalCount = 0;

    void add(const String& sender, const String& timestamp, const String& message) {
        entries[writeIndex].sender = sender;
        entries[writeIndex].timestamp = timestamp;
        entries[writeIndex].message = message;
        entries[writeIndex].used = true;
        writeIndex = (writeIndex + 1) % SMS_HISTORY_SIZE;
        totalCount++;
    }

    // Neueste zuerst iterieren
    int newestIndex() {
        return (writeIndex - 1 + SMS_HISTORY_SIZE) % SMS_HISTORY_SIZE;
    }

    int count() {
        return totalCount < SMS_HISTORY_SIZE ? totalCount : SMS_HISTORY_SIZE;
    }
}

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
    String buildPage() {
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
</style>
</head>
<body>
<h1>MINIFAX</h1>
<p class="subtitle">Mini-Faxgeraet &middot; ESP32 + SIM800L</p>
)rawhtml";

        // Status Card
        html += "<div class=\"card\"><h2>STATUS</h2><div class=\"status-grid\">";
        html += "<div class=\"status-item\"><span class=\"label\">Signal</span><span class=\"value\">" + htmlEscape(lastSignalStrength) + "</span></div>";
        html += "<div class=\"status-item\"><span class=\"label\">Uptime</span><span class=\"value\">";
        html += String(h) + "h " + String(m) + "m " + String(s) + "s</span></div>";
        html += "<div class=\"status-item\"><span class=\"label\">SMS</span><span class=\"value\">" + String(SmsHistory::totalCount) + " empfangen</span></div>";
        html += "<div class=\"status-item\"><span class=\"label\">WLAN</span><span class=\"value\">" + WiFi.localIP().toString() + "</span></div>";
        html += "</div></div>";

        // SMS Senden
        html += R"rawhtml(
<div class="card">
<h2>SMS SENDEN</h2>
<form class="sms-form" method="POST" action="/send">
<input type="text" name="number" placeholder="+49171..." required>
<textarea name="text" placeholder="Nachricht..." required></textarea>
<button type="submit">SENDEN</button>
</form>
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

        // Refresh
        html += R"rawhtml(
<div class="refresh"><a href="/">Aktualisieren</a></div>
</body></html>
)rawhtml";

        return html;
    }

    // ============================================
    // Route Handlers
    // ============================================
    void handleRoot() {
        server.send(200, "text/html", buildPage());
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
