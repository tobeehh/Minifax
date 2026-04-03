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
#include <WiFi.h>
#include "config.h"
#include "settings.h"
#include "sms_storage.h"
#include "oled_display.h"
#include "ota_update.h"
#include "webui.h"

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

        // Code Page auf WPC1252 (Windows Latin-1) setzen
        // damit Umlaute (aeoeue) korrekt gedruckt werden
        // ESC t n -> Zeichentabelle waehlen
        printerSerial.write(0x1B);
        printerSerial.write('t');
        printerSerial.write((uint8_t)16); // 16 = WPC1252
        delay(50);
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
        printLine(Settings::deviceName.c_str());
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

    // QR-Code drucken via ESC/POS nativer QR-Funktion
    // Funktioniert mit den meisten 58mm Thermodruckern
    void printQR(const char* data) {
        uint16_t len = strlen(data);

        setAlign(1); // zentriert

        // GS ( k - QR Code Befehle

        // 1. QR Model setzen (Model 2)
        printerSerial.write(0x1D);  // GS
        printerSerial.write('(');
        printerSerial.write('k');
        printerSerial.write((uint8_t)4);  // pL
        printerSerial.write((uint8_t)0);  // pH
        printerSerial.write((uint8_t)49); // cn
        printerSerial.write((uint8_t)65); // fn (Model)
        printerSerial.write((uint8_t)50); // Model 2
        printerSerial.write((uint8_t)0);

        // 2. Modulgrösse (3 Pixel pro Modul - gut lesbar auf 58mm)
        printerSerial.write(0x1D);
        printerSerial.write('(');
        printerSerial.write('k');
        printerSerial.write((uint8_t)3);
        printerSerial.write((uint8_t)0);
        printerSerial.write((uint8_t)49);
        printerSerial.write((uint8_t)67); // fn (Size)
        printerSerial.write((uint8_t)4);  // Modulgrösse 4

        // 3. Fehlerkorrektur (Level M - 15%)
        printerSerial.write(0x1D);
        printerSerial.write('(');
        printerSerial.write('k');
        printerSerial.write((uint8_t)3);
        printerSerial.write((uint8_t)0);
        printerSerial.write((uint8_t)49);
        printerSerial.write((uint8_t)69); // fn (Error correction)
        printerSerial.write((uint8_t)49); // Level M

        // 4. Daten in den QR-Buffer schreiben
        uint16_t storeLen = len + 3;
        printerSerial.write(0x1D);
        printerSerial.write('(');
        printerSerial.write('k');
        printerSerial.write((uint8_t)(storeLen & 0xFF));        // pL
        printerSerial.write((uint8_t)((storeLen >> 8) & 0xFF)); // pH
        printerSerial.write((uint8_t)49);  // cn
        printerSerial.write((uint8_t)80);  // fn (Store data)
        printerSerial.write((uint8_t)48);  // m
        printerSerial.write(data, len);

        // 5. QR-Code drucken
        printerSerial.write(0x1D);
        printerSerial.write('(');
        printerSerial.write('k');
        printerSerial.write((uint8_t)3);
        printerSerial.write((uint8_t)0);
        printerSerial.write((uint8_t)49);
        printerSerial.write((uint8_t)81); // fn (Print)
        printerSerial.write((uint8_t)48);

        delay(500); // QR-Rendering braucht etwas

        setAlign(0); // zurueck auf links
    }

    // URL fuer Antwort-Link generieren (WebUI mit vorausgefuellter Nummer)
    String replyUrl(const char* sender) {
        if (WiFi.status() != WL_CONNECTED) return "";
        String url = "http://" + WiFi.localIP().toString()
                   + "/?reply=" + String(sender);
        return url;
    }

    void printMessage(const char* sender, const char* timestamp, const char* message) {
        printHeader(sender, timestamp);
        printLine(message);
        feed(1);

        // QR-Code mit Antwort-Link (nur wenn WLAN verbunden + aktiviert)
        String url = replyUrl(sender);
        if (url.length() > 0 && Settings::printQrCode) {
            printSeparator();
            feed(1);
            setAlign(1);
            printLine("Antworten:");
            printQR(url.c_str());
            feed(1);
        }

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

    // SMS senden
    bool sendSMS(const String& number, const String& text) {
        Serial.print("[GSM] Sende SMS an ");
        Serial.println(number);

        String cmd = "AT+CMGS=\"" + number + "\"";
        simSerial.println(cmd);
        delay(500);

        // Warten auf '>' Prompt
        unsigned long start = millis();
        bool gotPrompt = false;
        while (millis() - start < 3000) {
            if (simSerial.available()) {
                char c = simSerial.read();
                if (c == '>') {
                    gotPrompt = true;
                    break;
                }
            }
        }

        if (!gotPrompt) {
            Serial.println("[GSM] FEHLER: Kein > Prompt");
            simSerial.write(0x1B); // ESC zum Abbrechen
            return false;
        }

        simSerial.print(text);
        simSerial.write(0x1A); // Ctrl+Z zum Senden

        // Auf Bestaetigung warten
        String resp = sendAT("", 10000);
        if (resp.indexOf("+CMGS:") >= 0) {
            Serial.println("[GSM] SMS gesendet!");
            return true;
        }

        Serial.println("[GSM] FEHLER: SMS senden fehlgeschlagen");
        return false;
    }
}

// ============================================
// GSM Charset Konvertierung
// ============================================
namespace CharsetConvert {
    // GSM 7-bit Default Alphabet -> ASCII/Latin-1
    // Konvertiert GSM-spezifische Zeichen (Umlaute etc.)
    // GSM charset hat Sonderzeichen an anderen Positionen als ASCII

    // GSM Extension Table (nach ESC 0x1B)
    // z.B. ESC + 0x65 = Euro-Zeichen

    String gsmToLatin(const String& gsm) {
        String result = "";
        result.reserve(gsm.length());

        for (unsigned int i = 0; i < gsm.length(); i++) {
            uint8_t c = (uint8_t)gsm[i];

            switch (c) {
                // GSM Default Alphabet Sonderzeichen
                case 0x00: result += '@'; break;
                case 0x01: result += '\xa3'; break; // Pfund
                case 0x02: result += '$'; break;
                case 0x03: result += '\xa5'; break; // Yen
                case 0x04: result += '\xe8'; break; // e grave
                case 0x05: result += '\xe9'; break; // e acute
                case 0x06: result += '\xf9'; break; // u grave
                case 0x07: result += '\xec'; break; // i grave
                case 0x08: result += '\xf2'; break; // o grave
                case 0x09: result += '\xc7'; break; // C cedilla
                case 0x0B: result += '\xd8'; break; // O stroke
                case 0x0C: result += '\xf8'; break; // o stroke
                case 0x0E: result += '\xc5'; break; // A ring
                case 0x0F: result += '\xe5'; break; // a ring
                case 0x10: result += '\x44'; break; // Delta -> D
                case 0x11: result += '_'; break;
                case 0x12: result += '\x46'; break; // Phi -> F
                case 0x13: result += '\x47'; break; // Gamma -> G
                case 0x14: result += '\x4c'; break; // Lambda -> L
                case 0x15: result += '\x4f'; break; // Omega -> O
                case 0x16: result += '\x50'; break; // Pi -> P
                case 0x17: result += '\x50'; break; // Psi -> P
                case 0x18: result += '\x53'; break; // Sigma -> S
                case 0x19: result += '\x54'; break; // Theta -> T
                case 0x1A: result += '\x58'; break; // Xi -> X

                // Deutsche Umlaute im GSM Charset!
                case 0x5B: result += '\xc4'; break; // Ae -> Ä (Latin-1)
                case 0x5C: result += '\xd6'; break; // Oe -> Ö
                case 0x5D: result += '\xd1'; break; // N tilde
                case 0x5E: result += '\xdc'; break; // Ue -> Ü
                case 0x5F: result += '\xa7'; break; // Paragraph
                case 0x60: result += '\xbf'; break; // inverted ?
                case 0x7B: result += '\xe4'; break; // ae -> ä
                case 0x7C: result += '\xf6'; break; // oe -> ö
                case 0x7D: result += '\xf1'; break; // n tilde
                case 0x7E: result += '\xfc'; break; // ue -> ü
                case 0x7F: result += '\xe0'; break; // a grave

                // ESC-Sequenzen (Extension Table)
                case 0x1B:
                    if (i + 1 < gsm.length()) {
                        i++;
                        uint8_t ext = (uint8_t)gsm[i];
                        switch (ext) {
                            case 0x65: result += '\x80'; break; // Euro
                            case 0x14: result += '^'; break;
                            case 0x28: result += '{'; break;
                            case 0x29: result += '}'; break;
                            case 0x2F: result += '\\'; break;
                            case 0x3C: result += '['; break;
                            case 0x3D: result += '~'; break;
                            case 0x3E: result += ']'; break;
                            case 0x40: result += '|'; break;
                            default:   result += '?'; break;
                        }
                    }
                    break;

                // eszett (scharfes S) - im GSM Charset an Position 0x1E
                case 0x1E: result += '\xdf'; break; // ss -> ß

                default:
                    // Standard-ASCII Zeichen bleiben gleich
                    result += (char)c;
                    break;
            }
        }
        return result;
    }

    // Fuer den Textmodus: SIM800L gibt im Textmodus oft
    // schon die richtigen Zeichen aus, aber mit GSM-Charset
    // Mapping. Diese Funktion handelt beide Faelle.
    String convertSmsText(const String& text) {
        // Pruefen ob der Text GSM-kodierte Sonderzeichen enthaelt
        bool hasGsmSpecialChars = false;
        for (unsigned int i = 0; i < text.length(); i++) {
            uint8_t c = (uint8_t)text[i];
            if (c == 0x5B || c == 0x5C || c == 0x5E ||  // AeOeUe
                c == 0x7B || c == 0x7C || c == 0x7E ||  // aeoeue
                c == 0x1E) {                             // sz
                hasGsmSpecialChars = true;
                break;
            }
        }

        if (hasGsmSpecialChars) {
            return gsmToLatin(text);
        }

        // Text ist vermutlich schon in ASCII/UTF-8, durchreichen
        return text;
    }
}

// ============================================
// SMS Parser (mehrzeilig)
// ============================================
namespace SMSParser {
    // +CMT: "+491234567890","","26/04/03,12:00:00+08"
    // Zeile 1
    // Zeile 2
    // ...

    String currentSender = "";
    String currentTimestamp = "";
    String currentMessage = "";
    bool collectingMessage = false;
    unsigned long messageStartTime = 0;

    // Timeout: wenn nach der letzten Zeile keine neue kommt,
    // ist die SMS komplett (500ms reicht, SIM800L sendet schnell)
    const unsigned long MESSAGE_TIMEOUT_MS = 500;

    void parseHeader(const String& line) {
        int firstQuote = line.indexOf('"');
        int secondQuote = line.indexOf('"', firstQuote + 1);
        if (firstQuote >= 0 && secondQuote > firstQuote) {
            currentSender = line.substring(firstQuote + 1, secondQuote);
        }

        int lastQuote = line.lastIndexOf('"');
        int prevQuote = line.lastIndexOf('"', lastQuote - 1);
        if (prevQuote >= 0 && lastQuote > prevQuote) {
            currentTimestamp = line.substring(prevQuote + 1, lastQuote);
        }

        currentMessage = "";
        collectingMessage = true;
        messageStartTime = millis();
    }

    // Neue Zeile hinzufuegen
    void addLine(const String& line) {
        if (currentMessage.length() > 0) {
            currentMessage += "\n";
        }
        currentMessage += CharsetConvert::convertSmsText(line);
        messageStartTime = millis();
    }

    // Verarbeitet eingehende Zeilen vom SIM800L
    // Gibt true zurueck wenn +CMT Header erkannt wurde
    bool processLine(const String& line) {
        if (line.startsWith("+CMT:")) {
            parseHeader(line);
            return false;
        }

        // Wenn wir eine SMS sammeln und eine neue AT-Antwort kommt,
        // ist die SMS vorbei
        if (collectingMessage && (line.startsWith("+") || line == "OK" || line == "ERROR")) {
            collectingMessage = false;
            return currentMessage.length() > 0;
        }

        if (collectingMessage && line.length() > 0) {
            addLine(line);
        }

        return false;
    }

    // Timeout pruefen - aufrufen im Loop
    // Gibt true zurueck wenn eine SMS fertig gesammelt wurde
    bool checkTimeout() {
        if (collectingMessage && currentMessage.length() > 0 &&
            millis() - messageStartTime > MESSAGE_TIMEOUT_MS) {
            collectingMessage = false;
            return true;
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

    // OLED Display initialisieren
    OledDisplay::init();

    // SPIFFS + Einstellungen + SMS-Verlauf laden
    SmsHistory::initStorage();
    Settings::load();
    SmsHistory::loadFromStorage();

    Serial.print("[MINIFAX] GSM-Modul: ");
    Serial.println(Settings::gsmModuleName());

    // Drucker initialisieren
    Serial.println("[PRINTER] Initialisiere Thermodrucker...");
    Printer::init();
    Serial.println("[PRINTER] Bereit!");

    // GSM initialisieren
    if (GSM::init()) {
        OledDisplay::gsmConnected = true;
        StatusLED::flashFast(5);
        Serial.println();
        Serial.println("[MINIFAX] System bereit! Warte auf SMS...");
        Serial.println();

        // WebUI starten (WiFiManager)
        WebUI::onSendSms = webSendSms;
        WebUI::onGuestbookEntry = handleGuestbookEntry;
        WebUI::onConfigPortalStarted = [](const char* apName) {
            OledDisplay::showConfigPortal(apName);
        };
        WebUI::init();

        // OTA starten (nur wenn WLAN verbunden)
        if (WebUI::connected) {
            OtaUpdate::init();
            OledDisplay::wifiConnected = true;
            OledDisplay::ipAddress = WiFi.localIP().toString();
        }

        // Testseite drucken beim Start
        Printer::printTestPage();
    } else {
        Serial.println("[MINIFAX] FEHLER: GSM-Modul nicht bereit!");
        Serial.println("[MINIFAX] Pruefe Verkabelung und SIM-Karte.");
        OledDisplay::showError("GSM nicht bereit!\nVerkabelung pruefen");
        FaxSound::errorTone();
        // Schnelles Blinken als Fehleranzeige
        while (true) {
            StatusLED::flashFast(10);
            delay(1000);
        }
    }
}

// Gaestebuch-Eintrag drucken
void handleGuestbookEntry(const String& name, const String& message) {
    Serial.println("[GAESTEBUCH] Neuer Eintrag von: " + name);

    if (Settings::faxSoundEnabled) {
        FaxSound::receiveSequence();
    }

    // Gaestebuch-Ausdruck
    Printer::feed(1);
    Printer::setAlign(1);
    Printer::setDoubleHeight(true);
    Printer::printLine(Settings::deviceName.c_str());
    Printer::setDoubleHeight(false);
    Printer::printLine("GAESTEBUCH");
    Printer::setAlign(0);
    Printer::feed(1);
    Printer::printSeparator();

    // Zeitstempel
    unsigned long sec = millis() / 1000;
    unsigned long h = sec / 3600;
    unsigned long m = (sec % 3600) / 60;
    char ts[16];
    snprintf(ts, sizeof(ts), "%02lu:%02lu", h, m);
    printerSerial.print(ts);
    printerSerial.print(" - ");
    Printer::setBold(true);
    printerSerial.println(name.c_str());
    Printer::setBold(false);

    Printer::printSeparator();
    Printer::feed(1);
    Printer::printLine(message.c_str());
    Printer::feed(1);
    Printer::printSeparator();

    // QR-Code zum Gaestebuch (damit naechster Gast auch schreiben kann)
    if (Settings::printQrCode && WiFi.status() == WL_CONNECTED) {
        Printer::feed(1);
        Printer::setAlign(1);
        Printer::printLine("Auch was schreiben?");
        String gbUrl = "http://" + WiFi.localIP().toString() + "/guestbook";
        Printer::printQR(gbUrl.c_str());
    }

    Printer::feed(3);

    if (Settings::faxSoundEnabled) {
        FaxSound::confirmTone();
    }

    // Im Verlauf speichern
    SmsHistory::add(name, "Gaestebuch", message);

    OledDisplay::lastSender = name;
    OledDisplay::lastMessage = message;
    OledDisplay::smsCount = SmsHistory::totalCount;
}

// SMS senden Callback fuer WebUI
void webSendSms(const String& number, const String& text) {
    if (GSM::sendSMS(number, text)) {
        Serial.println("[WEBUI] SMS gesendet via WebUI");
    } else {
        Serial.println("[WEBUI] SMS senden fehlgeschlagen");
        FaxSound::errorTone();
    }
}

// Empfangene SMS verarbeiten: Sound + Druck
void handleReceivedSMS() {
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

    // Im Verlauf speichern (WebUI + SPIFFS)
    SmsHistory::add(SMSParser::currentSender, SMSParser::currentTimestamp, SMSParser::currentMessage);

    // OLED: Empfangsanzeige
    OledDisplay::lastSender = SMSParser::currentSender;
    OledDisplay::lastMessage = SMSParser::currentMessage;
    OledDisplay::smsCount = SmsHistory::totalCount;
    OledDisplay::showReceiving(SMSParser::currentSender.c_str());

    // Fax-Empfangssequenz abspielen (wenn aktiviert)
    if (Settings::faxSoundEnabled) {
        FaxSound::receiveSequence();
    }

    // SMS drucken (wenn Auto-Druck aktiviert)
    if (Settings::autoPrint) {
        Printer::printMessage(
            SMSParser::currentSender.c_str(),
            SMSParser::currentTimestamp.c_str(),
            SMSParser::currentMessage.c_str()
        );
    }

    // Bestaetigungston
    if (Settings::faxSoundEnabled) {
        FaxSound::confirmTone();
    }
    StatusLED::flashFast(3);
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

                if (SMSParser::processLine(simBuffer)) {
                    handleReceivedSMS();
                }
            }

            simBuffer = "";
        } else if (c != '\r') {
            simBuffer += c;
        }
    }

    // Mehrzeilige SMS: Timeout pruefen
    // Wenn keine neue Zeile mehr kommt, ist die SMS komplett
    if (SMSParser::checkTimeout()) {
        handleReceivedSMS();
    }

    // WebUI Requests verarbeiten
    WebUI::update();

    // OTA Updates pruefen
    OtaUpdate::update();

    // OLED Status aktualisieren
    OledDisplay::update();

    // Debug: Befehle vom Serial Monitor an SIM800L weiterleiten
    while (Serial.available()) {
        char c = Serial.read();
        simSerial.write(c);
    }
}
