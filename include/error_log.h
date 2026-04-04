#ifndef ERROR_LOG_H
#define ERROR_LOG_H

#include <SPIFFS.h>
#include "config.h"

// ============================================
// Error-Log auf SPIFFS
//
// Speichert Fehler/Warnungen/Events mit
// Zeitstempel. Abrufbar ueber WebUI.
// Ringbuffer: aelteste Eintraege werden
// ueberschrieben wenn voll.
// ============================================

#define ERROR_LOG_PATH "/error_log.txt"
#define ERROR_LOG_MAX_SIZE 8192  // 8KB max

namespace ErrorLog {

    enum Level : uint8_t {
        LOG_INFO = 0,
        LOG_WARN = 1,
        LOG_ERROR = 2
    };

    const char* levelStr(Level level) {
        switch (level) {
            case LOG_INFO:  return "INFO";
            case LOG_WARN:  return "WARN";
            case LOG_ERROR: return "ERR ";
            default:        return "??? ";
        }
    }

    // Zeitstempel als Uptime (hh:mm:ss)
    String uptimeStr() {
        unsigned long sec = millis() / 1000;
        char buf[12];
        snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu",
                 sec / 3600, (sec % 3600) / 60, sec % 60);
        return String(buf);
    }

    // Log-Eintrag hinzufuegen
    void log(Level level, const char* module, const char* message) {
        String entry = uptimeStr() + " [" + levelStr(level) + "] "
                     + module + ": " + message + "\n";

        // Auf Serial ausgeben
        Serial.print("[LOG] ");
        Serial.print(entry);

        // Auf SPIFFS speichern
        if (!SPIFFS.begin(false)) return;

        // Dateigroesse pruefen, ggf. kuerzen
        if (SPIFFS.exists(ERROR_LOG_PATH)) {
            File check = SPIFFS.open(ERROR_LOG_PATH, "r");
            if (check && check.size() > ERROR_LOG_MAX_SIZE) {
                check.close();
                // Letzte Haelfte behalten
                File readFile = SPIFFS.open(ERROR_LOG_PATH, "r");
                if (readFile) {
                    readFile.seek(readFile.size() / 2);
                    // Bis zum naechsten Zeilenumbruch lesen
                    while (readFile.available() && readFile.read() != '\n') {}
                    String remaining = readFile.readString();
                    readFile.close();

                    File writeFile = SPIFFS.open(ERROR_LOG_PATH, "w");
                    if (writeFile) {
                        writeFile.print("--- Log gekuerzt ---\n");
                        writeFile.print(remaining);
                        writeFile.close();
                    }
                }
            } else if (check) {
                check.close();
            }
        }

        // Eintrag anhaengen
        File file = SPIFFS.open(ERROR_LOG_PATH, "a");
        if (file) {
            file.print(entry);
            file.close();
        }
    }

    // Kurzformen
    void info(const char* module, const char* message) {
        log(LOG_INFO, module, message);
    }

    void warn(const char* module, const char* message) {
        log(LOG_WARN, module, message);
    }

    void error(const char* module, const char* message) {
        log(LOG_ERROR, module, message);
    }

    // Ganzes Log lesen (fuer WebUI)
    String readAll() {
        if (!SPIFFS.exists(ERROR_LOG_PATH)) {
            return "(Kein Log vorhanden)";
        }

        File file = SPIFFS.open(ERROR_LOG_PATH, "r");
        if (!file) return "(Fehler beim Lesen)";

        String content = file.readString();
        file.close();
        return content;
    }

    // Log loeschen
    void clear() {
        SPIFFS.remove(ERROR_LOG_PATH);
    }

    // Startup-Eintrag
    void logStartup() {
        info("SYS", "Minifax gestartet");
        char buf[32];
        snprintf(buf, sizeof(buf), "Free Heap: %u KB", ESP.getFreeHeap() / 1024);
        info("SYS", buf);
    }
}

#endif // ERROR_LOG_H
