#include "core/TextUtil.h"

std::string toLowerUtf8(const std::string& text) {
    std::string out;
    std::size_t i = 0;
    while (i < text.size()) {
        unsigned char c = static_cast<unsigned char>(text[i]);

        if (c < 0x80) {  // ASCII
            if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c + 32);
            out += static_cast<char>(c);
            ++i;
            continue;
        }

        // Длина UTF-8-последовательности и стартовые биты кодпоинта.
        int len = 0;
        unsigned int code = 0;
        if ((c & 0xE0) == 0xC0) {
            len = 2;
            code = c & 0x1F;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
            code = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
            code = c & 0x07;
        } else {
            out += static_cast<char>(c);
            ++i;
            continue;
        }

        if (i + len > text.size()) {
            out += static_cast<char>(c);
            ++i;
            continue;
        }
        bool valid = true;
        for (int k = 1; k < len; ++k) {
            unsigned char cc = static_cast<unsigned char>(text[i + k]);
            if ((cc & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            code = (code << 6) | (cc & 0x3F);
        }
        if (!valid) {
            out += static_cast<char>(c);
            ++i;
            continue;
        }

        // Основная кириллица: А-Я (U+0410..U+042F) и Ё (U+0401).
        if (code >= 0x0410 && code <= 0x042F) {
            code += 0x20;
        } else if (code == 0x0401) {
            code = 0x0451;
        }

        // Кодируем кодпоинт обратно в UTF-8.
        if (code < 0x80) {
            out += static_cast<char>(code);
        } else if (code < 0x800) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
        i += len;
    }
    return out;
}
