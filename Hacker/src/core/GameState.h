#pragma once

#include <string>
#include <vector>

// Запись об успешном входе на узел. Ключ — host + port + user.
struct VisitedEntry {
    std::string host;
    int port = 0;
    std::string user;
    std::string pass;
};

// Состояние игры на один запуск: список известных узлов.
// Живёт только в памяти, на диск ничего не пишется.
class GameState {
public:
    // Добавляет запись или обновляет пароль уже известной (host + port + user).
    void record(const std::string& host, int port, const std::string& user,
                const std::string& pass);

    // Поиск по ключу host + port + user. nullptr, если записи нет.
    const VisitedEntry* find(const std::string& host, int port, const std::string& user) const;

    // Все записи в порядке добавления.
    const std::vector<VisitedEntry>& entries() const { return entries_; }

    // Очищает список (перед загрузкой сохранения).
    void clear() { entries_.clear(); }

    // Текст сгенерированного файла-списка известных узлов.
    std::string renderVisited() const;

private:
    std::vector<VisitedEntry> entries_;
};
