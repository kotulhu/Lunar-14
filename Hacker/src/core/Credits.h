#pragma once

#include <string>
#include <vector>

class GameState;
class World;

// Одна строка титров рассылки (data/credits.json).
struct CreditLine {
    enum class Type { Title, Delivered, Credit, Text, Blank, Total, Ending };

    Type type = Type::Text;
    std::string text;   // title, delivered, text
    std::string host;   // delivered: имя хоста
    std::string note;   // delivered: необязательная пометка после числа
    std::string role;   // credit
    std::string name;   // credit: необязательное имя

    struct Tier {
        int min = 0;
        std::string text;
    };
    std::vector<Tier> tiers;  // ending
};

// Титры из data/credits.json: пауза и строки, готовые к выводу.
class Credits {
public:
    // Загрузка и проверка. false и понятная причина (с номером строки) при
    // ошибке. world нужен для проверки ссылок на хосты.
    bool load(const std::string& path, const World& world, std::string& error);

    // Пауза между строками в миллисекундах.
    int delayMs() const { return delay_ms_; }

    // Число посещённых хостов с reach > 0 (домашний всегда посещён).
    int relays(const World& world, const GameState& state) const;

    // Готовые строки вывода с учётом посещённых хостов. Пустая строка = blank.
    std::vector<std::string> render(const World& world, const GameState& state) const;

private:
    int delay_ms_ = 500;
    std::vector<CreditLine> lines_;
};
