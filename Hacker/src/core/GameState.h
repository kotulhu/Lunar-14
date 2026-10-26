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

// Запись журнала приложения visited: посещение узла или найденный документ.
// Снимок этапа квеста и его даты берётся в момент появления записи.
struct JournalEntry {
    std::string key;                     // ключ дедупа
    std::string kind;                    // "visit" или "find"
    std::string host;                    // узел-источник (имя)
    int port = 0;                        // для visit
    std::string user;                    // для visit
    std::string source;                  // для find: "file", "mail", "chat"
    std::string title;                   // подпись пункта меню
    std::string text;                    // копия текста (find)
    std::vector<std::string> mentioned;  // упомянутые узлы (имена)
    std::string stage;                   // имя этапа квеста
    std::string date;                    // игровая дата этапа
};

// Список известных узлов (логин и пароль для автовхода). Сохраняется на диск
// в state.visited и восстанавливается при загрузке.
class GameState {
public:
    // Добавляет запись или обновляет пароль уже известной (host + port + user).
    void record(const std::string& host, int port, const std::string& user,
                const std::string& pass);

    // Посещён ли узел с таким именем (для условий этапов квеста).
    bool hasVisited(const std::string& host) const;

    // Поиск по ключу host + port + user. nullptr, если записи нет.
    const VisitedEntry* find(const std::string& host, int port, const std::string& user) const;

    // Добавляет запись журнала. false, если запись с таким key уже есть.
    bool addJournal(JournalEntry entry);

    // Журнал в порядке появления записей.
    const std::vector<JournalEntry>& journal() const { return journal_; }

    // Последняя запись для пары host + port — для автовхода. nullptr, если нет.
    const VisitedEntry* latestFor(const std::string& host, int port) const;

    // Все записи в порядке добавления.
    const std::vector<VisitedEntry>& entries() const { return entries_; }

    // Число разных посещённых хостов, не считая exclude (домашний хост).
    int distinctHosts(const std::string& exclude) const;

    // Сработавшие правила событий: отметка, проверка и весь список.
    void markEventFired(const std::string& id);
    bool eventFired(const std::string& id) const;
    const std::vector<std::string>& firedEvents() const { return fired_events_; }

    // Пройдена ли финальная рассылка (broadcast).
    bool finished() const { return finished_; }
    void setFinished(bool value) { finished_ = value; }

    // Очищает список (перед загрузкой сохранения).
    void clear() {
        entries_.clear();
        journal_.clear();
        fired_events_.clear();
        finished_ = false;
    }

private:
    std::vector<VisitedEntry> entries_;
    std::vector<JournalEntry> journal_;
    std::vector<std::string> fired_events_;
    bool finished_ = false;
};
