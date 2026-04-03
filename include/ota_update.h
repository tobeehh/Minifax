#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <ArduinoOTA.h>
#include "config.h"

// ============================================
// OTA (Over-The-Air) Firmware Updates
// ============================================
namespace OtaUpdate {
    bool active = false;

    void init() {
        ArduinoOTA.setHostname(OTA_HOSTNAME);

        ArduinoOTA.onStart([]() {
            String type = (ArduinoOTA.getCommand() == U_FLASH)
                ? "Firmware" : "Dateisystem";
            Serial.println("[OTA] Update startet: " + type);
        });

        ArduinoOTA.onEnd([]() {
            Serial.println("\n[OTA] Update abgeschlossen! Neustart...");
        });

        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("[OTA] Fortschritt: %u%%\r", (progress / (total / 100)));
        });

        ArduinoOTA.onError([](ota_error_t error) {
            Serial.printf("[OTA] Fehler [%u]: ", error);
            switch (error) {
                case OTA_AUTH_ERROR:    Serial.println("Auth fehlgeschlagen"); break;
                case OTA_BEGIN_ERROR:   Serial.println("Begin fehlgeschlagen"); break;
                case OTA_CONNECT_ERROR: Serial.println("Connect fehlgeschlagen"); break;
                case OTA_RECEIVE_ERROR: Serial.println("Empfang fehlgeschlagen"); break;
                case OTA_END_ERROR:     Serial.println("End fehlgeschlagen"); break;
            }
        });

        ArduinoOTA.begin();
        active = true;
        Serial.println("[OTA] Bereit (Hostname: " OTA_HOSTNAME ")");
    }

    void update() {
        if (active) {
            ArduinoOTA.handle();
        }
    }
}

#endif // OTA_UPDATE_H
