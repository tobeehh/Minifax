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

// OLED Display (SSD1306 128x64, I2C)
#define OLED_SDA_PIN 21  // Standard I2C SDA
#define OLED_SCL_PIN 22  // Standard I2C SCL
#define OLED_ADDR 0x3C   // I2C Adresse (0x3C oder 0x3D)
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

// ============================================
// WLAN Einstellungen
// ============================================
#define WEBSERVER_PORT 80

// WiFiManager: Name des Config-Hotspots
#define WIFI_AP_NAME "Minifax-Setup"
// Timeout fuer Config-Portal in Sekunden (0 = kein Timeout)
#define WIFI_CONFIG_TIMEOUT 180

// Maximale Anzahl gespeicherter SMS im Verlauf
#define SMS_HISTORY_SIZE 20

// ============================================
// SMS Einstellungen
// ============================================

// Intervall fuer SMS-Polling in Millisekunden
#define SMS_CHECK_INTERVAL 5000

// Maximale SMS-Laenge die gedruckt wird
#define MAX_SMS_LENGTH 160

// ============================================
// SPIFFS Einstellungen
// ============================================
#define SMS_LOG_PATH "/sms_log.json"
#define SPIFFS_MAX_ENTRIES 50

// ============================================
// OTA Einstellungen
// ============================================
#define OTA_HOSTNAME "minifax"

#endif // CONFIG_H
