#include "core/GameState.h"

#include <algorithm>

namespace {

// Ширина строки в колонках терминала (UTF-8: считаем стартовые байты).
std::size_t utf8_width(const std::string& text) {
    std::size_t width = 0;
    for (unsigned char c : text) {
        if ((c & 0xC0) != 0x80) ++width;
    }
    return width;
}

// Дополняет строку пробелами справа до нужной ширины (в колонках).
std::string pad(const std::string& text, std::size_t width) {
    std::size_t w = utf8_width(text);
    if (w >= width) return text;
    return text + std::string(width - w, ' ');
}

}  // namespace

void GameState::record(const std::string& host, int port, const std::string& user,
                       const std::string& pass) {
    for (VisitedEntry& e : entries_) {
        if (e.host == host && e.port == port && e.user == user) {
            e.pass = pass;  // дубль не создаём, пароль обновляем
            return;
        }
    }
    entries_.push_back({host, port, user, pass});
}

const VisitedEntry* GameState::find(const std::string& host, int port,
                                    const std::string& user) const {
    for (const VisitedEntry& e : entries_) {
        if (e.host == host && e.port == port && e.user == user) return &e;
    }
    return nullptr;
}

std::string GameState::renderVisited() const {
    // Ширины колонок считаем по заголовкам и данным (в колонках терминала).
    std::size_t name_w = utf8_width("хост");
    std::size_t port_w = utf8_width("порт");
    std::size_t login_w = utf8_width("логин");
    for (const VisitedEntry& e : entries_) {
        name_w = std::max(name_w, utf8_width(e.host));
        port_w = std::max(port_w, utf8_width(std::to_string(e.port)));
        login_w = std::max(login_w, utf8_width(e.user));
    }

    const std::string gap = "   ";
    std::string text = "# известные узлы (кто-то ленивый вёл этот список вручную)\n";
    // У строки-заголовка есть префикс "# ", поэтому колонка имени на 2 уже.
    text += "# " + pad("хост", name_w) + gap + pad("порт", port_w) + gap +
            pad("логин", login_w) + gap + "пароль";
    for (const VisitedEntry& e : entries_) {
        text += "\n" + pad(e.host, name_w + 2) + gap + pad(std::to_string(e.port), port_w) +
                gap + pad(e.user, login_w) + gap + e.pass;
    }
    return text;
}
