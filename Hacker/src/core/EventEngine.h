#pragma once

#include <functional>
#include <random>
#include <string>
#include <vector>

class ChatStore;
class Session;
class World;

// Игровое событие. Пока один тип: "login" (успешный вход), host — имя хоста.
struct Event {
    std::string type;
    std::string host;
};

// Шаблон письма из data/mail_templates/*.json.
struct MailTemplate {
    std::string id;
    std::string from;
    std::string subject;
    std::string date;
    std::vector<std::vector<std::string>> variants;  // случайный вариант при доставке
};

// Движок правил: по событию выполняет подходящие правила из data/events.json.
class EventEngine {
public:
    // Загружает шаблоны писем и правила. Проверяет ссылки на хосты, шаблоны и
    // контакты. false и понятная причина (с id правила) при ошибке.
    bool load(const std::string& data_dir, const World& world, const ChatStore& chat,
              std::string& error);

    // Обрабатывает событие. now_hm возвращает текущее время "HH:MM" (ui-код).
    void fire(const Event& event, Session& session,
              const std::function<std::string()>& now_hm);

private:
    // Одно действие правила.
    struct Action {
        enum class Kind { Mail, Chat, UnlockContact } kind = Kind::Mail;
        std::string id;       // id шаблона или контакта
        std::string message;  // текст сообщения (для Chat)
    };

    // Одно правило.
    struct Rule {
        std::string id;
        std::vector<std::string> visited;  // все перечисленные хосты посещены
        int visited_count = -1;            // посещено не меньше N (если >= 0)
        bool once = true;
        std::vector<Action> actions;
    };

    const MailTemplate* findTemplate(const std::string& id) const;
    bool matches(const Rule& rule, const Session& session) const;

    std::vector<MailTemplate> templates_;
    std::vector<Rule> rules_;
    std::string player_host_;
    std::mt19937 rng_{std::random_device{}()};
};
