#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// Pin-Konfiguration - an dein Wiring anpassen!
// ============================================

// SIM800L UART Pins
#define SIM800_TX_PIN 17  // ESP32 TX -> SIM800L RX
#define SIM800_RX_PIN 16  // ESP32 RX <- SIM800L TX
#define SIM800_BAUD   9600

// Thermodrucker UART Pins
#define PRINTER_TX_PIN 27  // ESP32 TX -> Drucker RX
#define PRINTER_RX_PIN 26  // ESP32 RX <- Drucker TX (optional)
#define PRINTER_BAUD   9600

// SIM800L Reset Pin (optional, -1 wenn nicht verbunden)
#define SIM800_RST_PIN -1

// LED Pin fuer Status-Anzeige
#define LED_PIN 2  // Onboard LED

// Piezo-Buzzer Pin
#define BUZZER_PIN 25
#define BUZZER_LEDC_CHANNEL 0

// ============================================
// WLAN Einstellungen fuer WebUI
// ============================================
#define WIFI_SSID "DEIN_WLAN"
#define WIFI_PASS "DEIN_PASSWORT"
#define WEBSERVER_PORT 80

// Maximale Anzahl gespeicherter SMS im Verlauf
#define SMS_HISTORY_SIZE 20

// ============================================
// SMS Einstellungen
// ============================================

// Intervall fuer SMS-Polling in Millisekunden
#define SMS_CHECK_INTERVAL 5000

// Maximale SMS-Laenge die gedruckt wird
#define MAX_SMS_LENGTH 160

#endif // CONFIG_H
