# Minifax - Verkabelung

## Bauteile

| Bauteil | Ca. Preis |
|---------|-----------|
| ESP32 DevKit V1 | 5-8 EUR |
| SIM800L Modul (mit Antenne) | 5-8 EUR |
| 58mm Thermodrucker (TTL Seriell) | 10-15 EUR |
| LM2596 Buck-Converter (für SIM800L) | 2 EUR |
| 5V/3A Netzteil | 5 EUR |
| Thermopapier 58mm | 3 EUR |
| **Gesamt** | **~30-40 EUR** |

## Verkabelung

```
                    +-------------------+
                    |      ESP32        |
                    |                   |
  SIM800L TX  <--  | GPIO16 (RX1)      |
  SIM800L RX  -->  | GPIO17 (TX1)      |
                    |                   |
  Drucker RX  <--  | GPIO27 (TX2)      |
  (Drucker TX) --> | GPIO26 (RX2)      |
                    |                   |
                    | GND ----+---------+--- GND (alle)
                    +---------+
```

## Stromversorgung

```
5V/3A Netzteil
    |
    +---> ESP32 (via USB oder VIN)
    |
    +---> Thermodrucker VCC (5V direkt)
    |
    +---> LM2596 Buck-Converter --> 4.0V --> SIM800L VCC
```

### WICHTIG: SIM800L Stromversorgung

- SIM800L braucht **3.7V - 4.2V** (NICHT 5V, NICHT 3.3V!)
- Stromspitzen bis **2A** beim Senden
- Buck-Converter auf **4.0V** einstellen
- **100µF Elko** direkt an SIM800L VCC/GND löten (gegen Spannungseinbrüche)
- SIM800L **NICHT** über den 3.3V Pin des ESP32 versorgen!

## SIM-Karte

- Micro-SIM Format (je nach Modul)
- Prepaid-SIM mit SMS-Empfang reicht
- PIN-Abfrage vorher am Handy **deaktivieren**!
