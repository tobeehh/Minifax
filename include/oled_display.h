#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

// ============================================
// OLED Display (SSD1306 128x64)
// ============================================
namespace OledDisplay {
    Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
    bool available = false;

    // Status-Daten (werden von aussen gesetzt)
    String signalStrength = "?";
    String ipAddress = "---";
    int smsCount = 0;
    String lastSender = "";
    String lastMessage = "";
    bool wifiConnected = false;
    bool gsmConnected = false;

    bool init() {
        Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

        if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
            Serial.println("[OLED] WARNUNG: Display nicht gefunden!");
            return false;
        }

        available = true;
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);

        // Startbildschirm
        display.setTextSize(2);
        display.setCursor(8, 8);
        display.println("MINIFAX");
        display.setTextSize(1);
        display.setCursor(8, 32);
        display.println("Starte...");
        display.display();

        Serial.println("[OLED] Display bereit!");
        return true;
    }

    void showStatus() {
        if (!available) return;

        display.clearDisplay();

        // Header
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.print("MINIFAX");

        // Uptime rechts oben
        unsigned long sec = millis() / 1000;
        unsigned long h = sec / 3600;
        unsigned long m = (sec % 3600) / 60;
        char uptime[16];
        snprintf(uptime, sizeof(uptime), "%luh%02lum", h, m);
        int16_t x1, y1;
        uint16_t w, ht;
        display.getTextBounds(uptime, 0, 0, &x1, &y1, &w, &ht);
        display.setCursor(OLED_WIDTH - w, 0);
        display.print(uptime);

        // Trennlinie
        display.drawLine(0, 10, OLED_WIDTH, 10, SSD1306_WHITE);

        // Status Icons/Text
        display.setCursor(0, 14);
        display.print("GSM:");
        display.print(gsmConnected ? "OK" : "--");
        display.print(" Sig:");
        display.print(signalStrength);

        display.setCursor(0, 24);
        display.print("WiFi:");
        if (wifiConnected) {
            display.print(ipAddress);
        } else {
            display.print("---");
        }

        display.setCursor(0, 34);
        display.print("SMS empfangen: ");
        display.print(smsCount);

        // Letzte SMS (wenn vorhanden)
        if (lastSender.length() > 0) {
            display.drawLine(0, 44, OLED_WIDTH, 44, SSD1306_WHITE);
            display.setCursor(0, 47);
            display.print("Von: ");
            // Sender kuerzen wenn noetig
            if (lastSender.length() > 16) {
                display.print(lastSender.substring(0, 16));
            } else {
                display.print(lastSender);
            }

            display.setCursor(0, 56);
            // Nachricht kuerzen auf eine Zeile
            String preview = lastMessage;
            preview.replace("\n", " ");
            if (preview.length() > 21) {
                preview = preview.substring(0, 19) + "..";
            }
            display.print(preview);
        }

        display.display();
    }

    void showReceiving(const char* sender) {
        if (!available) return;

        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(4, 4);
        display.println("EMPFANG");
        display.setTextSize(1);
        display.setCursor(0, 28);
        display.print("Von: ");
        display.println(sender);
        display.setCursor(0, 44);
        display.println("Drucke...");
        display.display();
    }

    void showConfigPortal(const char* apName) {
        if (!available) return;

        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println("WLAN SETUP");
        display.drawLine(0, 10, OLED_WIDTH, 10, SSD1306_WHITE);
        display.setCursor(0, 16);
        display.println("Verbinde mit WLAN:");
        display.setTextSize(2);
        display.setCursor(0, 30);
        display.println(apName);
        display.setTextSize(1);
        display.setCursor(0, 52);
        display.println("Dann: 192.168.4.1");
        display.display();
    }

    void showError(const char* msg) {
        if (!available) return;

        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println("FEHLER:");
        display.setCursor(0, 16);
        display.println(msg);
        display.display();
    }

    // Regelmaessig aufrufen um Status zu aktualisieren
    unsigned long lastUpdate = 0;
    void update() {
        if (!available) return;
        // Alle 2 Sekunden aktualisieren
        if (millis() - lastUpdate > 2000) {
            showStatus();
            lastUpdate = millis();
        }
    }
}

#endif // OLED_DISPLAY_H
