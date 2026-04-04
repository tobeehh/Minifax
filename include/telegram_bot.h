#ifndef TELEGRAM_BOT_H
#define TELEGRAM_BOT_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "settings.h"

// ============================================
// Telegram Bot
//
// Empfaengt Nachrichten (und Bilder!) ueber
// die Telegram Bot API via Long Polling.
// Kein externer Server noetig, laeuft direkt
// auf dem ESP32 ueber WLAN.
//
// Setup:
// 1. @BotFather auf Telegram anschreiben
// 2. /newbot -> Name + Username vergeben
// 3. Bot-Token kopieren
// 4. In Minifax Settings eintragen
// 5. Dem Bot eine Nachricht schreiben
// ============================================

#define TG_API_BASE "https://api.telegram.org/bot"
#define TG_POLL_INTERVAL 3000   // Polling alle 3 Sekunden
#define TG_POLL_TIMEOUT 10      // Long-Poll Timeout in Sekunden

namespace TelegramBot {

    // Status
    bool active = false;
    long lastUpdateId = 0;
    unsigned long lastPoll = 0;
    String botUsername = "";

    // Callbacks
    void (*onMessage)(const String& sender, const String& text) = nullptr;
    void (*onPhoto)(const String& sender, const uint8_t* data, int len) = nullptr;

    // API Request ausfuehren
    String apiRequest(const String& method, const String& params = "") {
        if (Settings::telegramToken.length() == 0) return "";

        HTTPClient http;
        String url = TG_API_BASE + Settings::telegramToken + "/" + method;
        if (params.length() > 0) {
            url += "?" + params;
        }

        http.begin(url);
        http.setTimeout(TG_POLL_TIMEOUT * 1000 + 5000);
        int code = http.GET();

        String response = "";
        if (code == HTTP_CODE_OK) {
            response = http.getString();
        } else {
            Serial.printf("[TG] API Fehler: %d\n", code);
        }

        http.end();
        return response;
    }

    // Bot-Info abrufen (getMe)
    bool verifyToken() {
        String resp = apiRequest("getMe");
        if (resp.length() == 0) return false;

        JsonDocument doc;
        if (deserializeJson(doc, resp)) return false;

        if (doc["ok"].as<bool>()) {
            botUsername = doc["result"]["username"].as<const char*>();
            Serial.print("[TG] Bot verifiziert: @");
            Serial.println(botUsername);
            return true;
        }

        Serial.println("[TG] Token ungueltig!");
        return false;
    }

    // Bild herunterladen (getFile + Download)
    bool downloadPhoto(const String& fileId, uint8_t** outData, int* outLen) {
        // Dateipfad holen
        String resp = apiRequest("getFile", "file_id=" + fileId);
        if (resp.length() == 0) return false;

        JsonDocument doc;
        if (deserializeJson(doc, resp)) return false;
        if (!doc["ok"].as<bool>()) return false;

        String filePath = doc["result"]["file_path"].as<const char*>();
        int fileSize = doc["result"]["file_size"] | 0;

        if (fileSize > 100000) {
            Serial.println("[TG] Bild zu gross (max 100KB)");
            return false;
        }

        // Datei herunterladen
        HTTPClient http;
        String url = "https://api.telegram.org/file/bot" + Settings::telegramToken + "/" + filePath;
        http.begin(url);
        int code = http.GET();

        if (code != HTTP_CODE_OK) {
            http.end();
            return false;
        }

        int len = http.getSize();
        if (len <= 0 || len > 100000) {
            http.end();
            return false;
        }

        uint8_t* buffer = (uint8_t*)malloc(len);
        if (!buffer) {
            http.end();
            return false;
        }

        WiFiClient* stream = http.getStreamPtr();
        int bytesRead = 0;
        while (bytesRead < len && stream->connected()) {
            int avail = stream->available();
            if (avail > 0) {
                int toRead = min(avail, len - bytesRead);
                stream->readBytes(buffer + bytesRead, toRead);
                bytesRead += toRead;
            }
            delay(1);
        }

        http.end();

        if (bytesRead == len) {
            *outData = buffer;
            *outLen = len;
            Serial.printf("[TG] Bild heruntergeladen: %d Bytes\n", len);
            return true;
        }

        free(buffer);
        return false;
    }

    // Nachricht senden
    bool sendMessage(const String& chatId, const String& text) {
        HTTPClient http;
        String url = TG_API_BASE + Settings::telegramToken + "/sendMessage";

        http.begin(url);
        http.addHeader("Content-Type", "application/json");

        JsonDocument doc;
        doc["chat_id"] = chatId;
        doc["text"] = text;

        String body;
        serializeJson(doc, body);

        int code = http.POST(body);
        http.end();

        return code == HTTP_CODE_OK;
    }

    // Updates abholen (getUpdates mit Long Polling)
    void pollUpdates() {
        String params = "offset=" + String(lastUpdateId + 1)
                      + "&timeout=" + String(TG_POLL_TIMEOUT)
                      + "&allowed_updates=[\"message\"]";

        String resp = apiRequest("getUpdates", params);
        if (resp.length() == 0) return;

        JsonDocument doc;
        if (deserializeJson(doc, resp)) return;
        if (!doc["ok"].as<bool>()) return;

        JsonArray results = doc["result"].as<JsonArray>();
        for (JsonObject update : results) {
            long updateId = update["update_id"];
            if (updateId > lastUpdateId) {
                lastUpdateId = updateId;
            }

            JsonObject message = update["message"];
            if (message.isNull()) continue;

            // Absender
            String senderName = "";
            JsonObject from = message["from"];
            if (!from.isNull()) {
                if (from["first_name"]) {
                    senderName = from["first_name"].as<const char*>();
                }
                if (from["last_name"]) {
                    senderName += " ";
                    senderName += from["last_name"].as<const char*>();
                }
            }

            String chatId = String(message["chat"]["id"].as<long long>());

            // Whitelist pruefen (wenn aktiviert)
            if (Settings::telegramChatId.length() > 0 &&
                chatId != Settings::telegramChatId) {
                Serial.println("[TG] Nachricht von unbekanntem Chat ignoriert: " + chatId);
                sendMessage(chatId, "Nicht autorisiert. Chat-ID: " + chatId);
                continue;
            }

            // Bei erster Nachricht Chat-ID merken
            if (Settings::telegramChatId.length() == 0) {
                Serial.println("[TG] Chat-ID gespeichert: " + chatId);
                Settings::telegramChatId = chatId;
                Settings::save();
                sendMessage(chatId, "Minifax verbunden! Diese Chat-ID ist jetzt autorisiert.");
            }

            // Foto empfangen?
            JsonArray photos = message["photo"];
            if (!photos.isNull() && photos.size() > 0 && onPhoto) {
                // Groesstes Foto nehmen (letztes im Array)
                JsonObject largest = photos[photos.size() - 1];
                String fileId = largest["file_id"].as<const char*>();

                Serial.println("[TG] Foto empfangen von " + senderName);

                uint8_t* imgData = nullptr;
                int imgLen = 0;
                if (downloadPhoto(fileId, &imgData, &imgLen)) {
                    // Caption als Sender-Info nutzen
                    String caption = message["caption"] | "";
                    String sender = "TG:" + senderName;
                    if (caption.length() > 0) {
                        sender += " - " + caption;
                    }
                    onPhoto(sender, imgData, imgLen);
                    free(imgData);

                    sendMessage(chatId, "Bild gedruckt!");
                } else {
                    sendMessage(chatId, "Fehler beim Bild-Download. Max 100KB, JPEG/PNG.");
                }
                continue;
            }

            // Text-Nachricht
            String text = message["text"] | "";
            if (text.length() > 0 && onMessage) {
                Serial.println("[TG] Nachricht von " + senderName + ": " + text);

                // /start Befehl
                if (text == "/start") {
                    sendMessage(chatId, "Minifax bereit! Schick mir eine Nachricht oder ein Bild zum Drucken.");
                    continue;
                }

                // /status Befehl
                if (text == "/status") {
                    unsigned long sec = millis() / 1000;
                    String status = "MINIFAX Status:\n"
                                  + String("Uptime: ") + String(sec / 3600) + "h "
                                  + String((sec % 3600) / 60) + "m\n"
                                  + "SMS: " + String(SmsHistory::totalCount) + " empfangen\n"
                                  + "IP: " + WiFi.localIP().toString();
                    sendMessage(chatId, status);
                    continue;
                }

                String sender = "TG:" + senderName;
                onMessage(sender, text);
                sendMessage(chatId, "Gedruckt!");
            }
        }
    }

    // Initialisieren
    bool init() {
        if (!Settings::telegramEnabled || Settings::telegramToken.length() == 0) {
            Serial.println("[TG] Telegram deaktiviert oder kein Token");
            return false;
        }

        Serial.println("[TG] Initialisiere Telegram Bot...");
        if (!verifyToken()) {
            Serial.println("[TG] Token ungueltig!");
            return false;
        }

        active = true;
        lastPoll = millis();
        Serial.println("[TG] Bereit! Bot: @" + botUsername);
        return true;
    }

    // Im Loop aufrufen
    void update() {
        if (!active) return;

        if (millis() - lastPoll > TG_POLL_INTERVAL) {
            pollUpdates();
            lastPoll = millis();
        }
    }
}

#endif // TELEGRAM_BOT_H
