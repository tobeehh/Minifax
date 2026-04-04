#ifndef SETTINGS_H
#define SETTINGS_H

#include <SPIFFS.h>
#include <ArduinoJson.h>

// ============================================
// Persistente Einstellungen (SPIFFS)
// ============================================
#define SETTINGS_PATH "/settings.json"

namespace Settings {
    // GSM-Modul Typen
    enum GsmModule : uint8_t {
        GSM_SIM800L = 0,   // 2G (guenstig, ~5 EUR)
        GSM_SIM7000G = 1,  // 4G LTE Cat-M / NB-IoT (~15 EUR)
        GSM_SIM7600 = 2    // 4G LTE vollstaendig (~20 EUR)
    };

    // Aktuelle Einstellungen
    GsmModule gsmModule = GSM_SIM800L;
    bool faxSoundEnabled = true;
    bool printQrCode = true;
    bool autoPrint = true;
    bool guestbookMode = false;  // Gaestebuch-Modus
    uint8_t buzzerVolume = 100;  // 0-100 (PWM duty cycle %)
    String deviceName = "MINIFAX";

    // Telegram Bot
    bool telegramEnabled = false;
    String telegramToken = "";
    String telegramChatId = "";  // Wird automatisch bei erster Nachricht gesetzt

    const char* gsmModuleName() {
        switch (gsmModule) {
            case GSM_SIM800L:  return "SIM800L (2G)";
            case GSM_SIM7000G: return "SIM7000G (4G)";
            case GSM_SIM7600:  return "SIM7600 (4G)";
            default:           return "Unbekannt";
        }
    }

    // Aus SPIFFS laden
    void load() {
        if (!SPIFFS.exists(SETTINGS_PATH)) {
            Serial.println("[SETTINGS] Keine gespeicherten Einstellungen, nutze Defaults.");
            return;
        }

        File file = SPIFFS.open(SETTINGS_PATH, "r");
        if (!file) return;

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, file);
        file.close();

        if (err) {
            Serial.print("[SETTINGS] JSON Fehler: ");
            Serial.println(err.c_str());
            return;
        }

        gsmModule = (GsmModule)(doc["gsmModule"] | (uint8_t)GSM_SIM800L);
        faxSoundEnabled = doc["faxSound"] | true;
        printQrCode = doc["printQr"] | true;
        autoPrint = doc["autoPrint"] | true;
        guestbookMode = doc["guestbook"] | false;
        buzzerVolume = doc["buzzerVol"] | 100;
        deviceName = doc["name"] | "MINIFAX";
        telegramEnabled = doc["tgEnabled"] | false;
        telegramToken = doc["tgToken"] | "";
        telegramChatId = doc["tgChatId"] | "";

        Serial.print("[SETTINGS] Geladen. GSM-Modul: ");
        Serial.println(gsmModuleName());
    }

    // Auf SPIFFS speichern
    void save() {
        JsonDocument doc;
        doc["gsmModule"] = (uint8_t)gsmModule;
        doc["faxSound"] = faxSoundEnabled;
        doc["printQr"] = printQrCode;
        doc["autoPrint"] = autoPrint;
        doc["guestbook"] = guestbookMode;
        doc["buzzerVol"] = buzzerVolume;
        doc["name"] = deviceName;
        doc["tgEnabled"] = telegramEnabled;
        doc["tgToken"] = telegramToken;
        doc["tgChatId"] = telegramChatId;

        File file = SPIFFS.open(SETTINGS_PATH, "w");
        if (!file) {
            Serial.println("[SETTINGS] FEHLER: Datei nicht schreibbar");
            return;
        }
        serializeJson(doc, file);
        file.close();
        Serial.println("[SETTINGS] Gespeichert.");
    }

    // TinyGSM Modem-Define fuer Build (nur Info, Build-Flag bleibt)
    // Zur Laufzeit nutzen wir AT-Befehle die bei allen gleich sind
    const char* modemDefine() {
        switch (gsmModule) {
            case GSM_SIM800L:  return "TINY_GSM_MODEM_SIM800";
            case GSM_SIM7000G: return "TINY_GSM_MODEM_SIM7000";
            case GSM_SIM7600:  return "TINY_GSM_MODEM_SIM7600";
            default:           return "TINY_GSM_MODEM_SIM800";
        }
    }
}

#endif // SETTINGS_H
