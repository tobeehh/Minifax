#ifndef EMOJI_REPLACE_H
#define EMOJI_REPLACE_H

#include <Arduino.h>

// ============================================
// Emoji -> ASCII-Art Ersetzung
//
// UTF-8 Emojis werden durch druckbare
// ASCII-Zeichen ersetzt, damit sie auf dem
// Thermodrucker sichtbar sind.
// ============================================

namespace EmojiReplace {

    struct EmojiMap {
        const char* utf8;    // UTF-8 Bytes des Emoji
        const char* ascii;   // ASCII Ersetzung
    };

    // Emoji-Tabelle: haeufigste Emojis
    // UTF-8 Sequenzen sind 3-4 Bytes lang
    const EmojiMap table[] = {
        // Smileys
        {"\xF0\x9F\x98\x80", ":D"},         // 😀
        {"\xF0\x9F\x98\x81", ":D"},         // 😁
        {"\xF0\x9F\x98\x82", ":'D"},        // 😂
        {"\xF0\x9F\x98\x83", ":)"},         // 😃
        {"\xF0\x9F\x98\x84", ":)"},         // 😄
        {"\xF0\x9F\x98\x85", "':)"},        // 😅
        {"\xF0\x9F\x98\x86", "xD"},         // 😆
        {"\xF0\x9F\x98\x89", ";)"},         // 😉
        {"\xF0\x9F\x98\x8A", ":)"},         // 😊
        {"\xF0\x9F\x98\x8B", ":P~"},        // 😋
        {"\xF0\x9F\x98\x8D", "<3_<3"},      // 😍
        {"\xF0\x9F\x98\x8E", "B)"},         // 😎
        {"\xF0\x9F\x98\x8F", ":>"},         // 😏
        {"\xF0\x9F\x98\x90", ":|"},         // 😐
        {"\xF0\x9F\x98\x92", "-_-"},        // 😒
        {"\xF0\x9F\x98\x94", ":("},         // 😔
        {"\xF0\x9F\x98\x96", ">.<"},        // 😖
        {"\xF0\x9F\x98\x98", ";*"},         // 😘
        {"\xF0\x9F\x98\x9A", ":*"},         // 😚
        {"\xF0\x9F\x98\x9C", ";P"},         // 😜
        {"\xF0\x9F\x98\x9D", "xP"},         // 😝
        {"\xF0\x9F\x98\x9E", ":("},         // 😞
        {"\xF0\x9F\x98\xA0", ">:("},        // 😠
        {"\xF0\x9F\x98\xA1", ">:O"},        // 😡
        {"\xF0\x9F\x98\xA2", ":'("},        // 😢
        {"\xF0\x9F\x98\xA4", ">:/ "},       // 😤
        {"\xF0\x9F\x98\xA7", "D:"},         // 😧
        {"\xF0\x9F\x98\xA8", ":O"},         // 😨
        {"\xF0\x9F\x98\xAD", "T_T"},        // 😭
        {"\xF0\x9F\x98\xAE", ":O"},         // 😮
        {"\xF0\x9F\x98\xB1", ":O!!"},       // 😱
        {"\xF0\x9F\x98\xB4", "zzZ"},        // 😴
        {"\xF0\x9F\x98\xB7", ":-#"},        // 😷
        {"\xF0\x9F\xA4\x94", ":?"},         // 🤔
        {"\xF0\x9F\xA4\x97", ">:)< "},      // 🤗
        {"\xF0\x9F\xA4\xA3", "xDD"},        // 🤣
        {"\xF0\x9F\xA4\xAE", ":X"},         // 🤮
        {"\xF0\x9F\xA5\xB0", ":))"},        // 🥰
        {"\xF0\x9F\xA5\xB2", ":'-)"},       // 🥲
        {"\xF0\x9F\xA5\xB3", "~:D~"},       // 🥳
        {"\xF0\x9F\xA5\xBA", "QQ"},         // 🥺
        {"\xF0\x9F\x99\x83", "(:"},         // 🙃
        {"\xF0\x9F\x99\x84", "-.-"},        // 🙄

        // Herzen
        {"\xE2\x9D\xA4",     "<3"},         // ❤️
        {"\xF0\x9F\x92\x94", "</3"},         // 💔
        {"\xF0\x9F\x92\x95", "<3<3"},        // 💕
        {"\xF0\x9F\x92\x96", "<3*"},         // 💖
        {"\xF0\x9F\x92\x97", "<3"},          // 💗
        {"\xF0\x9F\x92\x99", "<3"},          // 💙
        {"\xF0\x9F\x92\x9A", "<3"},          // 💚
        {"\xF0\x9F\x92\x9B", "<3"},          // 💛
        {"\xF0\x9F\x92\x9C", "<3"},          // 💜
        {"\xF0\x9F\xA4\x8D", "<3"},          // 🤍
        {"\xF0\x9F\x96\xA4", "<3"},          // 🖤

        // Haende
        {"\xF0\x9F\x91\x8D", "(y)"},         // 👍
        {"\xF0\x9F\x91\x8E", "(n)"},         // 👎
        {"\xF0\x9F\x91\x8B", "o/"},          // 👋
        {"\xF0\x9F\x91\x8F", "*clap*"},      // 👏
        {"\xF0\x9F\x99\x8F", "*pray*"},      // 🙏
        {"\xF0\x9F\xA4\x9D", "*shake*"},     // 🤝
        {"\xE2\x9C\x8C",     "V"},           // ✌️
        {"\xF0\x9F\xA4\x98", "\\m/"},        // 🤘
        {"\xF0\x9F\x92\xAA", "(flex)"},      // 💪

        // Objekte & Symbole
        {"\xF0\x9F\x94\xA5", "*fire*"},      // 🔥
        {"\xE2\xAD\x90",     "*"},           // ⭐
        {"\xF0\x9F\x8E\x89", "*party*"},     // 🎉
        {"\xF0\x9F\x8E\x8A", "*confetti*"},  // 🎊
        {"\xF0\x9F\x8E\x82", "*cake*"},      // 🎂
        {"\xF0\x9F\x8E\x81", "*gift*"},      // 🎁
        {"\xF0\x9F\x8E\xB5", "~note~"},      // 🎵
        {"\xF0\x9F\x8E\xB6", "~notes~"},     // 🎶
        {"\xF0\x9F\x92\xA1", "*idea*"},      // 💡
        {"\xF0\x9F\x92\xAF", "100!"},        // 💯
        {"\xE2\x9C\x85",     "[OK]"},        // ✅
        {"\xE2\x9D\x8C",     "[X]"},         // ❌
        {"\xE2\x9A\xA0",     "/!\\"},        // ⚠️
        {"\xF0\x9F\x92\xAC", "\"...\""},     // 💬
        {"\xF0\x9F\x93\xB1", "[phone]"},     // 📱
        {"\xF0\x9F\x93\xA7", "[mail]"},      // 📧
        {"\xF0\x9F\x95\x90", "[clock]"},     // 🕐
        {"\xE2\x98\x80",     "*sun*"},       // ☀️
        {"\xF0\x9F\x8C\x99", "*moon*"},      // 🌙
        {"\xE2\x98\x94",     "*rain*"},      // ☔
        {"\xE2\x9D\x84",     "*snow*"},      // ❄️
        {"\xF0\x9F\x92\xA9", "*poop*"},      // 💩

        // Essen & Trinken
        {"\xF0\x9F\x8D\xBA", "*beer*"},      // 🍺
        {"\xF0\x9F\x8D\xBB", "*cheers*"},    // 🍻
        {"\xF0\x9F\x8D\xB7", "*wine*"},      // 🍷
        {"\xE2\x98\x95",     "*coffee*"},    // ☕
        {"\xF0\x9F\x8D\x95", "*pizza*"},     // 🍕

        // Tiere
        {"\xF0\x9F\x90\xB1", "=^.^="},      // 🐱
        {"\xF0\x9F\x90\xB6", ":3"},          // 🐶

        // Flaggen / Sonstiges
        {"\xF0\x9F\x87\xA9\xF0\x9F\x87\xAA", "[DE]"},  // 🇩🇪
    };

    const int TABLE_SIZE = sizeof(table) / sizeof(table[0]);

    // UTF-8 Emoji-Sequenz Laenge ermitteln (3 oder 4 Bytes)
    int utf8SeqLen(uint8_t firstByte) {
        if ((firstByte & 0xF8) == 0xF0) return 4;  // 11110xxx
        if ((firstByte & 0xF0) == 0xE0) return 3;  // 1110xxxx
        if ((firstByte & 0xE0) == 0xC0) return 2;  // 110xxxxx
        return 1;
    }

    // Alle Emojis im Text durch ASCII ersetzen
    String process(const String& text) {
        String result = "";
        result.reserve(text.length());

        unsigned int i = 0;
        while (i < text.length()) {
            uint8_t c = (uint8_t)text[i];
            int seqLen = utf8SeqLen(c);

            // Nur mehrbyte-Sequenzen koennen Emojis sein
            if (seqLen >= 3 && i + seqLen <= text.length()) {
                bool found = false;

                for (int e = 0; e < TABLE_SIZE; e++) {
                    int emojiLen = strlen(table[e].utf8);

                    // Flaggen-Emojis sind 8 Bytes (2x 4-Byte Sequenz)
                    if (i + emojiLen <= text.length() &&
                        memcmp(text.c_str() + i, table[e].utf8, emojiLen) == 0) {
                        result += table[e].ascii;
                        i += emojiLen;
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    // Unbekanntes Emoji: als [?] darstellen
                    result += "[?]";
                    i += seqLen;
                    // Variation Selector (0xEF 0xB8 0x8F) ueberspringen
                    if (i + 3 <= text.length() &&
                        (uint8_t)text[i] == 0xEF &&
                        (uint8_t)text[i+1] == 0xB8 &&
                        (uint8_t)text[i+2] == 0x8F) {
                        i += 3;
                    }
                }
            } else {
                result += (char)c;
                i++;
            }
        }

        return result;
    }
}

#endif // EMOJI_REPLACE_H
