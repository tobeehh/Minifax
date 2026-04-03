#ifndef SMS_STORAGE_H
#define SMS_STORAGE_H

#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "config.h"

// ============================================
// SMS Verlauf (Ringbuffer + SPIFFS Persistenz)
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
    bool spiffsReady = false;

    // SPIFFS initialisieren
    bool initStorage() {
        if (!SPIFFS.begin(true)) { // true = formatieren wenn noetig
            Serial.println("[SPIFFS] FEHLER: Initialisierung fehlgeschlagen!");
            return false;
        }
        spiffsReady = true;
        Serial.println("[SPIFFS] Bereit!");
        return true;
    }

    // SMS-Verlauf aus SPIFFS laden
    void loadFromStorage() {
        if (!spiffsReady) return;

        if (!SPIFFS.exists(SMS_LOG_PATH)) {
            Serial.println("[SPIFFS] Keine gespeicherten SMS gefunden.");
            return;
        }

        File file = SPIFFS.open(SMS_LOG_PATH, "r");
        if (!file) {
            Serial.println("[SPIFFS] FEHLER: Datei nicht lesbar");
            return;
        }

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, file);
        file.close();

        if (err) {
            Serial.print("[SPIFFS] JSON Fehler: ");
            Serial.println(err.c_str());
            return;
        }

        totalCount = doc["totalCount"] | 0;
        writeIndex = doc["writeIndex"] | 0;

        JsonArray arr = doc["entries"].as<JsonArray>();
        int i = 0;
        for (JsonObject obj : arr) {
            if (i >= SMS_HISTORY_SIZE) break;
            entries[i].sender = obj["s"].as<const char*>();
            entries[i].timestamp = obj["t"].as<const char*>();
            entries[i].message = obj["m"].as<const char*>();
            entries[i].used = true;
            i++;
        }

        Serial.printf("[SPIFFS] %d SMS geladen (gesamt: %d)\n", i, totalCount);
    }

    // SMS-Verlauf auf SPIFFS speichern
    void saveToStorage() {
        if (!spiffsReady) return;

        JsonDocument doc;
        doc["totalCount"] = totalCount;
        doc["writeIndex"] = writeIndex;

        JsonArray arr = doc["entries"].to<JsonArray>();
        int count = totalCount < SMS_HISTORY_SIZE ? totalCount : SMS_HISTORY_SIZE;

        // Vom aeltesten zum neuesten speichern
        int startIdx = (count < SMS_HISTORY_SIZE) ? 0 : writeIndex;
        for (int i = 0; i < count; i++) {
            int idx = (startIdx + i) % SMS_HISTORY_SIZE;
            if (entries[idx].used) {
                JsonObject obj = arr.add<JsonObject>();
                obj["s"] = entries[idx].sender;
                obj["t"] = entries[idx].timestamp;
                obj["m"] = entries[idx].message;
            }
        }

        File file = SPIFFS.open(SMS_LOG_PATH, "w");
        if (!file) {
            Serial.println("[SPIFFS] FEHLER: Datei nicht schreibbar");
            return;
        }

        serializeJson(doc, file);
        file.close();
        Serial.println("[SPIFFS] SMS gespeichert.");
    }

    // Neue SMS hinzufuegen + sofort speichern
    void add(const String& sender, const String& timestamp, const String& message) {
        entries[writeIndex].sender = sender;
        entries[writeIndex].timestamp = timestamp;
        entries[writeIndex].message = message;
        entries[writeIndex].used = true;
        writeIndex = (writeIndex + 1) % SMS_HISTORY_SIZE;
        totalCount++;

        saveToStorage();
    }

    // Neueste zuerst iterieren
    int newestIndex() {
        return (writeIndex - 1 + SMS_HISTORY_SIZE) % SMS_HISTORY_SIZE;
    }

    int count() {
        return totalCount < SMS_HISTORY_SIZE ? totalCount : SMS_HISTORY_SIZE;
    }

    // Alles loeschen
    void clear() {
        for (int i = 0; i < SMS_HISTORY_SIZE; i++) {
            entries[i] = SmsEntry();
        }
        writeIndex = 0;
        totalCount = 0;
        if (spiffsReady) {
            SPIFFS.remove(SMS_LOG_PATH);
        }
        Serial.println("[SPIFFS] Verlauf geloescht.");
    }
}

#endif // SMS_STORAGE_H
