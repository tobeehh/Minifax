/*
 * MINIFAX - Mini-Faxgeraet mit ESP32
 *
 * Empfaengt SMS ueber SIM800L GSM-Modul und druckt sie
 * sofort auf einem Thermodrucker aus.
 *
 * Hardware:
 *   - ESP32 DevKit
 *   - SIM800L GSM-Modul (mit SIM-Karte)
 *   - 58mm Thermodrucker (TTL Seriell, ESC/POS)
 *
 * Verkabelung:
 *   ESP32 GPIO17 (TX) -> SIM800L RX
 *   ESP32 GPIO16 (RX) <- SIM800L TX
 *   ESP32 GPIO27 (TX) -> Drucker RX
 *   ESP32 GPIO25       -> Piezo-Buzzer (+), anderes Bein -> GND
 *   SIM800L VCC -> 3.7-4.2V (LiPo oder Buck-Converter!)
 *   Drucker VCC -> 5-9V (separates Netzteil empfohlen)
 *   Alle GND verbinden
 */

#include <Arduino.h>
#include <HardwareSerial.h>
#include "config.h"

// ============================================
// Hardware Serial Instanzen
// ============================================
HardwareSerial simSerial(1);      // UART1 fuer SIM800L
HardwareSerial printerSerial(2);  // UART2 fuer Thermodrucker

// ============================================
// Fax-Sound via Piezo-Buzzer (LEDC PWM)
// ============================================
namespace FaxSound {
    void init() {
        ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CHANNEL);
    }

    void tonePlay(uint16_t freq, uint16_t durationMs) {
        ledcWriteTone(BUZZER_LEDC_CHANNEL, freq);
        delay(durationMs);
    }

    void toneStop() {
        ledcWriteTone(BUZZER_LEDC_CHANNEL, 0);
    }

    // CED-Ton: Empfaenger meldet sich (2100 Hz, ~2.5 Sekunden)
    void cedTone() {
        tonePlay(2100, 2500);
        toneStop();
        delay(200);
    }

    // V.21 Handshake: das klassische Fax-Kratzen
    // Simuliert mit schnellen Frequenzwechseln
    void handshakeNoise(uint16_t durationMs) {
        unsigned long start = millis();
        while (millis() - start < durationMs) {
            // Zufaellige Frequenzen zwischen 300-3000 Hz
            // simulieren das Modem-Handshake-Kratzen
            uint16_t freq = 300 + (esp_random() % 2700);
            tonePlay(freq, 20 + (esp_random() % 40));
        }
        toneStop();
    }

    // Komplette Fax-Empfangssequenz:
    // 1. Klingeln (Ring)
    // 2. CED-Antwortton (2100 Hz)
    // 3. Handshake-Kratzen
    void receiveSequence() {
        Serial.println("[FAX] *brrring brrring*");

        // Klingeln: zwei kurze hohe Toene
        for (int ring = 0; ring < 2; ring++) {
            tonePlay(1400, 200);
            tonePlay(1800, 200);
            tonePlay(1400, 200);
            toneStop();
            delay(400);
        }
        delay(300);

        // CED-Antwortton
        Serial.println("[FAX] CED-Ton (2100 Hz)");
        cedTone();

        // Handshake-Kratzen (~2 Sekunden)
        Serial.println("[FAX] Handshake...");
        handshakeNoise(2000);

        // Kurze Stille vor dem Drucken
        delay(300);
        Serial.println("[FAX] Empfang OK, drucke...");
    }

    // Kurzer Bestaetigungston nach dem Drucken
    void confirmTone() {
        tonePlay(800, 100);
        delay(50);
        tonePlay(1200, 100);
        delay(50);
        tonePlay(1600, 150);
        toneStop();
    }

    // Fehler-Ton
    void errorTone() {
        for (int i = 0; i < 3; i++) {
            tonePlay(400, 200);
            toneStop();
            delay(100);
        }
    }
}

// ============================================
// Thermodrucker ESC/POS Befehle
// ============================================
namespace Printer {
    void init() {
        printerSerial.begin(PRINTER_BAUD, SERIAL_8N1, PRINTER_RX_PIN, PRINTER_TX_PIN);
        delay(500);
        // ESC @ - Drucker initialisieren/reset
        printerSerial.write(0x1B);
        printerSerial.write('@');
        delay(100);
    }

    void setBold(bool on) {
        printerSerial.write(0x1B);
        printerSerial.write('E');
        printerSerial.write(on ? (uint8_t)1 : (uint8_t)0);
    }

    void setDoubleHeight(bool on) {
        printerSerial.write(0x1B);
        printerSerial.write('!');
        printerSerial.write(on ? (uint8_t)0x10 : (uint8_t)0x00);
    }

    void setAlign(uint8_t align) {
        // 0=links, 1=mitte, 2=rechts
        printerSerial.write(0x1B);
        printerSerial.write('a');
        printerSerial.write(align);
    }

    void feed(uint8_t lines = 1) {
        for (uint8_t i = 0; i < lines; i++) {
            printerSerial.write('\n');
        }
    }

    void printLine(const char* text) {
        printerSerial.print(text);
        printerSerial.write('\n');
    }

    void printSeparator() {
        printLine("--------------------------------");
    }

    void printHeader(const char* sender, const char* timestamp) {
        feed(1);
        setAlign(1); // zentriert
        setDoubleHeight(true);
        printLine("MINIFAX");
        setDoubleHeight(false);
        setAlign(0); // links
        feed(1);

        printSeparator();

        // Zeitstempel und Absender
        printerSerial.print(timestamp);
        printerSerial.print(" >>> ");
        printerSerial.println(sender);

        printSeparator();
        feed(1);
    }

    void printMessage(const char* sender, const char* timestamp, const char* message) {
        printHeader(sender, timestamp);
        printLine(message);
        feed(1);
        printSeparator();
        feed(3); // Genug Papier zum Abreissen
    }

    void printTestPage() {
        printMessage("+4915112345678", "03.04.26 12:00", "Testdruck - Minifax funktioniert!");
    }
}

// ============================================
// SIM800L GSM Modul
// ============================================
namespace GSM {
    bool initialized = false;

    // AT-Befehl senden und auf Antwort warten
    String sendAT(const char* cmd, unsigned long timeout = 2000) {
        simSerial.println(cmd);

        unsigned long start = millis();
        String response = "";

        while (millis() - start < timeout) {
            while (simSerial.available()) {
                char c = simSerial.read();
                response += c;
            }
        }

        Serial.print("AT> ");
        Serial.print(cmd);
        Serial.print(" -> ");
        Serial.println(response);

        return response;
    }

    bool waitForResponse(const char* expected, unsigned long timeout = 2000) {
        String resp = sendAT("", timeout);
        return resp.indexOf(expected) >= 0;
    }

    bool init() {
        simSerial.begin(SIM800_BAUD, SERIAL_8N1, SIM800_RX_PIN, SIM800_TX_PIN);
        delay(3000); // SIM800L braucht Zeit zum Booten

        Serial.println("[GSM] Initialisiere SIM800L...");

        // Grundlegende AT-Befehle
        String resp = sendAT("AT", 3000);
        if (resp.indexOf("OK") < 0) {
            Serial.println("[GSM] FEHLER: SIM800L antwortet nicht!");
            return false;
        }

        // Echo ausschalten
        sendAT("ATE0");

        // SMS-Textmodus aktivieren (statt PDU)
        resp = sendAT("AT+CMGF=1");
        if (resp.indexOf("OK") < 0) {
            Serial.println("[GSM] FEHLER: SMS-Textmodus nicht verfuegbar");
            return false;
        }

        // Zeichensatz auf GSM setzen
        sendAT("AT+CSCS=\"GSM\"");

        // SMS-Benachrichtigung konfigurieren:
        // Neue SMS direkt an Terminal weiterleiten
        sendAT("AT+CNMI=2,2,0,0,0");

        // Netzregistrierung pruefen
        resp = sendAT("AT+CREG?", 5000);
        if (resp.indexOf("+CREG: 0,1") >= 0 || resp.indexOf("+CREG: 0,5") >= 0) {
            Serial.println("[GSM] Im Netz registriert!");
        } else {
            Serial.println("[GSM] WARNUNG: Noch nicht im Netz registriert");
        }

        // Eigene Nummer anzeigen (wenn verfuegbar)
        sendAT("AT+CNUM");

        // Signalstaerke
        resp = sendAT("AT+CSQ");
        Serial.print("[GSM] Signalstaerke: ");
        Serial.println(resp);

        initialized = true;
        Serial.println("[GSM] Bereit!");
        return true;
    }

    // Alle gespeicherten SMS lesen und loeschen
    void checkStoredSMS() {
        String resp = sendAT("AT+CMGL=\"ALL\"", 5000);
        // Wird im Hauptloop ueber +CMT verarbeitet
    }

    // Eine bestimmte SMS lesen
    String readSMS(int index) {
        char cmd[16];
        snprintf(cmd, sizeof(cmd), "AT+CMGR=%d", index);
        return sendAT(cmd, 3000);
    }

    // SMS loeschen
    void deleteSMS(int index) {
        char cmd[16];
        snprintf(cmd, sizeof(cmd), "AT+CMGD=%d", index);
        sendAT(cmd);
    }

    // Alle SMS loeschen
    void deleteAllSMS() {
        sendAT("AT+CMGD=1,4");
    }
}

// ============================================
// SMS Parser
// ============================================
namespace SMSParser {
    // +CMT: "+491234567890","","26/04/03,12:00:00+08"
    // Nachrichtentext hier

    String currentSender = "";
    String currentTimestamp = "";
    String currentMessage = "";
    bool waitingForMessage = false;

    // Parst die +CMT Header-Zeile
    void parseHeader(const String& line) {
        // Absendernummer extrahieren
        int firstQuote = line.indexOf('"');
        int secondQuote = line.indexOf('"', firstQuote + 1);
        if (firstQuote >= 0 && secondQuote > firstQuote) {
            currentSender = line.substring(firstQuote + 1, secondQuote);
        }

        // Zeitstempel extrahieren (letztes Paar Anfuehrungszeichen)
        int lastQuote = line.lastIndexOf('"');
        int prevQuote = line.lastIndexOf('"', lastQuote - 1);
        if (prevQuote >= 0 && lastQuote > prevQuote) {
            currentTimestamp = line.substring(prevQuote + 1, lastQuote);
        }

        waitingForMessage = true;
    }

    // Verarbeitet eingehende Zeilen vom SIM800L
    bool processLine(const String& line) {
        if (line.startsWith("+CMT:")) {
            parseHeader(line);
            return false;
        }

        if (waitingForMessage && line.length() > 0) {
            currentMessage = line;
            waitingForMessage = false;
            return true; // Vollstaendige SMS empfangen
        }

        return false;
    }
}

// ============================================
// Status LED
// ============================================
namespace StatusLED {
    unsigned long lastBlink = 0;
    bool state = false;

    void init() {
        pinMode(LED_PIN, OUTPUT);
    }

    // Heartbeat-Blinken: kurz an, lange aus
    void update() {
        unsigned long now = millis();
        if (state && now - lastBlink > 100) {
            digitalWrite(LED_PIN, LOW);
            state = false;
        } else if (!state && now - lastBlink > 3000) {
            digitalWrite(LED_PIN, HIGH);
            state = true;
            lastBlink = now;
        }
    }

    void flashFast(int times) {
        for (int i = 0; i < times; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(100);
            digitalWrite(LED_PIN, LOW);
            delay(100);
        }
    }
}

// ============================================
// Setup & Loop
// ============================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println("  MINIFAX - Mini Faxgeraet");
    Serial.println("  ESP32 + SIM800L + Thermodrucker");
    Serial.println("================================");
    Serial.println();

    StatusLED::init();
    FaxSound::init();
    StatusLED::flashFast(3);

    // Drucker initialisieren
    Serial.println("[PRINTER] Initialisiere Thermodrucker...");
    Printer::init();
    Serial.println("[PRINTER] Bereit!");

    // GSM initialisieren
    if (GSM::init()) {
        StatusLED::flashFast(5);
        Serial.println();
        Serial.println("[MINIFAX] System bereit! Warte auf SMS...");
        Serial.println();

        // Testseite drucken beim Start
        Printer::printTestPage();
    } else {
        Serial.println("[MINIFAX] FEHLER: GSM-Modul nicht bereit!");
        Serial.println("[MINIFAX] Pruefe Verkabelung und SIM-Karte.");
        FaxSound::errorTone();
        // Schnelles Blinken als Fehleranzeige
        while (true) {
            StatusLED::flashFast(10);
            delay(1000);
        }
    }
}

// Buffer fuer serielle Daten vom SIM800L
String simBuffer = "";

void loop() {
    StatusLED::update();

    // Daten vom SIM800L lesen
    while (simSerial.available()) {
        char c = simSerial.read();

        if (c == '\n') {
            simBuffer.trim();

            if (simBuffer.length() > 0) {
                Serial.print("[SIM] ");
                Serial.println(simBuffer);

                // SMS parsen
                if (SMSParser::processLine(simBuffer)) {
                    Serial.println();
                    Serial.println("=== NEUE SMS EMPFANGEN ===");
                    Serial.print("Von: ");
                    Serial.println(SMSParser::currentSender);
                    Serial.print("Zeit: ");
                    Serial.println(SMSParser::currentTimestamp);
                    Serial.print("Text: ");
                    Serial.println(SMSParser::currentMessage);
                    Serial.println("===========================");
                    Serial.println();

                    // Fax-Empfangssequenz abspielen!
                    FaxSound::receiveSequence();

                    // SMS drucken!
                    Printer::printMessage(
                        SMSParser::currentSender.c_str(),
                        SMSParser::currentTimestamp.c_str(),
                        SMSParser::currentMessage.c_str()
                    );

                    // Bestaetigungston
                    FaxSound::confirmTone();
                    StatusLED::flashFast(3);
                }
            }

            simBuffer = "";
        } else if (c != '\r') {
            simBuffer += c;
        }
    }

    // Debug: Befehle vom Serial Monitor an SIM800L weiterleiten
    while (Serial.available()) {
        char c = Serial.read();
        simSerial.write(c);
    }
}
