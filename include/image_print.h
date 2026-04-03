#ifndef IMAGE_PRINT_H
#define IMAGE_PRINT_H

#include <Arduino.h>

// ============================================
// Bild-Druck auf Thermodrucker
//
// Empfaengt Raw-Graustufendaten, wendet
// Floyd-Steinberg Dithering an, und druckt
// als 1-bit Bitmap via ESC/POS.
//
// 58mm Drucker = 384 Pixel breit (48 Bytes)
// ============================================

#define PRINT_WIDTH_PX 384
#define PRINT_WIDTH_BYTES 48  // 384 / 8

namespace ImagePrint {

    // Floyd-Steinberg Dithering: Graustufen -> 1-bit
    // Input:  grayscale[] mit width*height Bytes (0-255)
    // Output: bitmap[] mit ceil(width/8)*height Bytes (1 bit pro Pixel)
    // Skaliert automatisch auf PRINT_WIDTH_PX Breite
    //
    // Gibt Anzahl der Bitmap-Zeilen zurueck
    int ditherAndScale(const uint8_t* grayscale, int srcWidth, int srcHeight,
                       uint8_t* bitmap, int maxBitmapSize) {

        // Zielbreite ist immer PRINT_WIDTH_PX
        float scale = (float)PRINT_WIDTH_PX / srcWidth;
        int dstHeight = (int)(srcHeight * scale);

        // Sicherheitscheck
        int bitmapSize = PRINT_WIDTH_BYTES * dstHeight;
        if (bitmapSize > maxBitmapSize) {
            dstHeight = maxBitmapSize / PRINT_WIDTH_BYTES;
        }

        // Skalierter Graustufenbuffer (zwei Zeilen reichen fuer Dithering)
        // Zeile A = aktuelle, Zeile B = naechste
        int16_t* lineA = (int16_t*)malloc(PRINT_WIDTH_PX * sizeof(int16_t));
        int16_t* lineB = (int16_t*)malloc(PRINT_WIDTH_PX * sizeof(int16_t));

        if (!lineA || !lineB) {
            if (lineA) free(lineA);
            if (lineB) free(lineB);
            Serial.println("[IMG] Nicht genug RAM fuer Dithering");
            return 0;
        }

        memset(bitmap, 0, PRINT_WIDTH_BYTES * dstHeight);

        // Erste Zeile skalieren
        for (int x = 0; x < PRINT_WIDTH_PX; x++) {
            int srcX = (int)(x / scale);
            if (srcX >= srcWidth) srcX = srcWidth - 1;
            lineA[x] = grayscale[srcX];
        }

        for (int y = 0; y < dstHeight; y++) {
            // Naechste Zeile vorbereiten
            if (y + 1 < dstHeight) {
                int srcY = (int)((y + 1) / scale);
                if (srcY >= srcHeight) srcY = srcHeight - 1;
                for (int x = 0; x < PRINT_WIDTH_PX; x++) {
                    int srcX = (int)(x / scale);
                    if (srcX >= srcWidth) srcX = srcWidth - 1;
                    lineB[x] = grayscale[srcY * srcWidth + srcX];
                }
            } else {
                memset(lineB, 128, PRINT_WIDTH_PX * sizeof(int16_t));
            }

            // Floyd-Steinberg Dithering auf aktuelle Zeile
            for (int x = 0; x < PRINT_WIDTH_PX; x++) {
                int16_t oldPixel = lineA[x];
                if (oldPixel < 0) oldPixel = 0;
                if (oldPixel > 255) oldPixel = 255;

                // Schwellwert: unter 128 = schwarz (Druckpunkt)
                uint8_t newPixel = (oldPixel < 128) ? 0 : 255;
                int16_t error = oldPixel - newPixel;

                // Pixel setzen (schwarz = 1 = Druckpunkt)
                if (newPixel == 0) {
                    int byteIdx = y * PRINT_WIDTH_BYTES + (x / 8);
                    bitmap[byteIdx] |= (0x80 >> (x % 8));
                }

                // Fehler verteilen (Floyd-Steinberg)
                if (x + 1 < PRINT_WIDTH_PX)
                    lineA[x + 1] += error * 7 / 16;
                if (y + 1 < dstHeight) {
                    if (x > 0)
                        lineB[x - 1] += error * 3 / 16;
                    lineB[x] += error * 5 / 16;
                    if (x + 1 < PRINT_WIDTH_PX)
                        lineB[x + 1] += error * 1 / 16;
                }
            }

            // Zeilen tauschen
            int16_t* tmp = lineA;
            lineA = lineB;
            lineB = tmp;
        }

        free(lineA);
        free(lineB);

        return dstHeight;
    }

    // Bitmap zeilenweise an den Drucker senden
    // ESC/POS Raster Bit Image (GS v 0)
    void printBitmap(HardwareSerial& printer, const uint8_t* bitmap, int height) {
        // GS v 0 - Raster Bit Image drucken
        // Format: GS v 0 m xL xH yL yH [data]
        printer.write(0x1D);  // GS
        printer.write('v');
        printer.write('0');
        printer.write((uint8_t)0);  // m = Normal

        // Breite in Bytes (xL, xH)
        printer.write((uint8_t)(PRINT_WIDTH_BYTES & 0xFF));
        printer.write((uint8_t)((PRINT_WIDTH_BYTES >> 8) & 0xFF));

        // Hoehe in Dots (yL, yH)
        printer.write((uint8_t)(height & 0xFF));
        printer.write((uint8_t)((height >> 8) & 0xFF));

        // Bitmap-Daten zeilenweise senden
        // In Bloecken senden um Buffer-Overflow zu vermeiden
        int totalBytes = PRINT_WIDTH_BYTES * height;
        int offset = 0;
        while (offset < totalBytes) {
            int chunk = min(256, totalBytes - offset);
            printer.write(bitmap + offset, chunk);
            offset += chunk;
            delay(10); // Drucker Zeit geben
        }

        delay(500); // Warten bis Druck fertig
    }

    // Einfaches BMP parsen (24-bit uncompressed)
    // Gibt Graustufenbuffer zurueck (caller muss free() aufrufen)
    // Setzt width und height
    uint8_t* parseBMP(const uint8_t* data, int dataLen, int& width, int& height) {
        // BMP Header pruefen
        if (dataLen < 54 || data[0] != 'B' || data[1] != 'M') {
            Serial.println("[IMG] Kein gueltiges BMP");
            return nullptr;
        }

        int pixelOffset = data[10] | (data[11] << 8) | (data[12] << 16) | (data[13] << 24);
        width = data[18] | (data[19] << 8) | (data[20] << 16) | (data[21] << 24);
        height = data[22] | (data[23] << 8) | (data[24] << 16) | (data[25] << 24);
        int bpp = data[28] | (data[29] << 8);

        bool bottomUp = (height > 0);
        if (height < 0) height = -height;

        Serial.printf("[IMG] BMP: %dx%d, %d bpp\n", width, height, bpp);

        if (bpp != 24) {
            Serial.println("[IMG] Nur 24-bit BMP unterstuetzt");
            return nullptr;
        }

        uint8_t* gray = (uint8_t*)malloc(width * height);
        if (!gray) {
            Serial.println("[IMG] Nicht genug RAM");
            return nullptr;
        }

        int rowSize = ((width * 3 + 3) / 4) * 4; // Rows sind auf 4 Bytes aligned

        for (int y = 0; y < height; y++) {
            int srcY = bottomUp ? (height - 1 - y) : y;
            int rowStart = pixelOffset + srcY * rowSize;

            for (int x = 0; x < width; x++) {
                int px = rowStart + x * 3;
                if (px + 2 >= dataLen) continue;
                uint8_t b = data[px];
                uint8_t g = data[px + 1];
                uint8_t r = data[px + 2];
                // Luminanz-Formel
                gray[y * width + x] = (uint8_t)(0.299f * r + 0.587f * g + 0.114f * b);
            }
        }

        return gray;
    }

    // Raw-Graustufenbild drucken (Hauptfunktion)
    bool printGrayscaleImage(HardwareSerial& printer, const uint8_t* grayscale,
                             int width, int height) {
        // Bitmap-Buffer allozieren
        float scale = (float)PRINT_WIDTH_PX / width;
        int dstHeight = (int)(height * scale);
        int bitmapSize = PRINT_WIDTH_BYTES * dstHeight;

        // Maximaler Buffer: 30KB (ESP32 hat ~200KB free heap)
        if (bitmapSize > 30000) {
            dstHeight = 30000 / PRINT_WIDTH_BYTES;
            bitmapSize = PRINT_WIDTH_BYTES * dstHeight;
            Serial.println("[IMG] Bild zu gross, schneide ab");
        }

        uint8_t* bitmap = (uint8_t*)malloc(bitmapSize);
        if (!bitmap) {
            Serial.println("[IMG] Nicht genug RAM fuer Bitmap");
            return false;
        }

        int lines = ditherAndScale(grayscale, width, height, bitmap, bitmapSize);
        if (lines > 0) {
            printBitmap(printer, bitmap, lines);
        }

        free(bitmap);
        return lines > 0;
    }

    // BMP-Daten empfangen und drucken
    bool printBMPData(HardwareSerial& printer, const uint8_t* bmpData, int dataLen) {
        int width, height;
        uint8_t* gray = parseBMP(bmpData, dataLen, width, height);
        if (!gray) return false;

        bool ok = printGrayscaleImage(printer, gray, width, height);
        free(gray);
        return ok;
    }
}

#endif // IMAGE_PRINT_H
