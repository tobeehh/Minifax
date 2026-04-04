#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "error_log.h"

// ============================================
// Watchdog: WiFi-Reconnect + GSM-Health-Check
//
// Ueberwacht WiFi und GSM Verbindung und
// versucht bei Ausfaellen automatisch
// wiederherzustellen.
// ============================================

#define WATCHDOG_INTERVAL     30000   // Alle 30 Sekunden pruefen
#define WIFI_RECONNECT_DELAY  5000    // 5 Sekunden zwischen Reconnect-Versuchen
#define WIFI_MAX_RETRIES      5       // Max Reconnect-Versuche bevor Pause
#define WIFI_RETRY_PAUSE      300000  // 5 Minuten Pause nach Max-Retries
#define GSM_CHECK_INTERVAL    60000   // GSM alle 60 Sekunden pruefen
#define GSM_MAX_FAILURES      3       // Nach 3 Fehlern: Reset

namespace Watchdog {

    // WiFi State
    unsigned long lastWifiCheck = 0;
    int wifiRetryCount = 0;
    unsigned long wifiPauseUntil = 0;
    bool wifiWasConnected = false;

    // GSM State
    unsigned long lastGsmCheck = 0;
    int gsmFailCount = 0;

    // Callbacks
    bool (*gsmHealthCheck)() = nullptr;     // AT senden, OK zurueck?
    void (*gsmReset)() = nullptr;           // GSM-Modul neu initialisieren
    void (*onWifiReconnected)() = nullptr;  // Nach WiFi-Reconnect aufrufen
    void (*onGsmReconnected)() = nullptr;   // Nach GSM-Reset aufrufen

    // ============================================
    // WiFi Reconnect
    // ============================================
    void checkWifi() {
        if (millis() < wifiPauseUntil) return;

        if (WiFi.status() == WL_CONNECTED) {
            if (!wifiWasConnected) {
                wifiWasConnected = true;
                wifiRetryCount = 0;
                ErrorLog::info("WIFI", "Verbunden");
                if (onWifiReconnected) {
                    onWifiReconnected();
                }
            }
            return;
        }

        // WiFi nicht verbunden
        if (wifiWasConnected) {
            wifiWasConnected = false;
            ErrorLog::warn("WIFI", "Verbindung verloren!");
        }

        // Zu viele Versuche -> Pause
        if (wifiRetryCount >= WIFI_MAX_RETRIES) {
            ErrorLog::warn("WIFI", "Max Retries erreicht, Pause 5min");
            wifiPauseUntil = millis() + WIFI_RETRY_PAUSE;
            wifiRetryCount = 0;
            return;
        }

        // Reconnect versuchen
        wifiRetryCount++;
        char buf[48];
        snprintf(buf, sizeof(buf), "Reconnect Versuch %d/%d", wifiRetryCount, WIFI_MAX_RETRIES);
        ErrorLog::info("WIFI", buf);

        WiFi.disconnect();
        delay(500);
        WiFi.reconnect();
    }

    // ============================================
    // GSM Health Check
    // ============================================
    void checkGsm() {
        if (!gsmHealthCheck) return;

        bool ok = gsmHealthCheck();

        if (ok) {
            if (gsmFailCount > 0) {
                ErrorLog::info("GSM", "Wieder erreichbar");
            }
            gsmFailCount = 0;
            return;
        }

        gsmFailCount++;
        char buf[48];
        snprintf(buf, sizeof(buf), "Keine Antwort (%d/%d)", gsmFailCount, GSM_MAX_FAILURES);
        ErrorLog::warn("GSM", buf);

        if (gsmFailCount >= GSM_MAX_FAILURES) {
            ErrorLog::error("GSM", "Max Failures - Reset!");
            gsmFailCount = 0;

            if (gsmReset) {
                gsmReset();
                if (onGsmReconnected) {
                    onGsmReconnected();
                }
            }
        }
    }

    // ============================================
    // Heap-Monitoring
    // ============================================
    unsigned long lastHeapCheck = 0;
    uint32_t minFreeHeap = UINT32_MAX;

    void checkHeap() {
        uint32_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < minFreeHeap) {
            minFreeHeap = freeHeap;
        }

        // Warnen wenn Heap unter 20KB
        if (freeHeap < 20000) {
            char buf[48];
            snprintf(buf, sizeof(buf), "Wenig RAM: %u Bytes frei", freeHeap);
            ErrorLog::warn("SYS", buf);
        }

        // Kritisch unter 10KB
        if (freeHeap < 10000) {
            ErrorLog::error("SYS", "Kritisch wenig RAM! Neustart empfohlen.");
        }
    }

    // ============================================
    // Update - im Loop aufrufen
    // ============================================
    void update() {
        unsigned long now = millis();

        // WiFi check
        if (now - lastWifiCheck > WATCHDOG_INTERVAL) {
            lastWifiCheck = now;
            checkWifi();
        }

        // GSM check
        if (now - lastGsmCheck > GSM_CHECK_INTERVAL) {
            lastGsmCheck = now;
            checkGsm();
        }

        // Heap check (alle 60 Sekunden)
        if (now - lastHeapCheck > 60000) {
            lastHeapCheck = now;
            checkHeap();
        }
    }
}

#endif // WATCHDOG_H
