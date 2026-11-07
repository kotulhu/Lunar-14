#pragma once

#include <random>
#include <set>
#include <string>
#include <vector>

class ChatStore;
class Session;
class World;

enum class Signal { Login, FileRead, Download, MailRead, Command };

struct SignalData {
    std::string host;  // Login / FileRead
    std::string path;  // FileRead
    std::string name;  // Download / MailRead / Command
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
    // Загружает правила из JSON-текста; шаблоны писем должны быть загружены ранее.
    bool loadRulesJson(const std::string& text, const World& world, const ChatStore& chat,
                       std::string& error);

    void bind(Session* session) { session_ = session; }
    void onSignal(Session&, World&, Signal, const SignalData&);
    void onLogin(const std::string& host);
    void onFileRead(const std::string& host, const std::string& path);
    void onDownload(const std::string& name);
    void onMailRead(const std::string& mailId);
    void onCommand(const std::string& cmd);

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
        std::vector<std::string> visited;
        int visited_count = -1;
        std::vector<std::string> downloaded;
        std::vector<std::string> file_read;
        std::vector<std::string> mail_read;
        std::vector<std::string> command;
        bool once = true;
        std::vector<Action> actions;
    };

    const MailTemplate* findTemplate(const std::string& id) const;
    bool matches(const Rule& rule, const Session& session) const;
    void fireMatching(Session&, const SignalData&);

    std::vector<MailTemplate> templates_;
    std::vector<Rule> rules_;
    std::string player_host_;
    Session* session_ = nullptr;
    std::set<std::string> read_files_;
    std::set<std::string> downloaded_;
    std::set<std::string> read_mail_;
    std::set<std::string> commands_;
    std::mt19937 rng_{std::random_device{}()};
};
