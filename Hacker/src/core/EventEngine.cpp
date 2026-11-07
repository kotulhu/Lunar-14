#include "core/EventEngine.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>

#include <nlohmann/json.hpp>

#include "core/ChatStore.h"
#include "core/Mailbox.h"
#include "core/Session.h"
#include "core/World.h"

using json = nlohmann::json;

namespace {

std::string joinPath(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    if (dir.back() == '/') return dir + name;
    return dir + "/" + name;
}

// Заменяет все вхождения подстроки from на to.
void replaceAll(std::string& text, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    std::size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}

// Подстановки в теме и тексте письма. Неизвестные остаются как есть.
std::string substitute(std::string text, const std::string& last_host,
                       int visited_count, const std::string& player_user) {
    replaceAll(text, "{last_host}", last_host);
    replaceAll(text, "{visited_count}", std::to_string(visited_count));
    replaceAll(text, "{player_user}", player_user);
    return text;
}

// Разбирает массив шаблонов из одного файла mail_templates/*.json.
bool parseTemplateArray(const json& root, const std::string& file,
                        std::vector<MailTemplate>& out, std::string& error) {
    if (!root.is_array()) {
        error = "mail template file '" + file + "': expected an array";
        return false;
    }
    for (std::size_t i = 0; i < root.size(); ++i) {
        const json& node = root[i];
        std::string where =
            "mail template file '" + file + "' item " + std::to_string(i + 1) + ": ";
        if (!node.is_object()) {
            error = where + "expected an object";
            return false;
        }
        if (!node.contains("id") || !node["id"].is_string()) {
            error = where + "'id' must be a string";
            return false;
        }
        MailTemplate t;
        t.id = node["id"].get<std::string>();
        if (t.id.empty()) {
            error = where + "'id' must not be empty";
            return false;
        }
        where = "mail template '" + t.id + "': ";

        auto required = [&](const char* key, std::string& dst) -> bool {
            if (!node.contains(key) || !node[key].is_string()) {
                error = where + "'" + key + "' must be a string";
                return false;
            }
            dst = node[key].get<std::string>();
            return true;
        };
        if (!required("from", t.from)) return false;
        if (!required("subject", t.subject)) return false;
        if (!required("date", t.date)) return false;

        if (!node.contains("variants") || !node["variants"].is_array() ||
            node["variants"].empty()) {
            error = where + "'variants' must be a non-empty array";
            return false;
        }
        for (const json& variant : node["variants"]) {
            if (!variant.is_array() || variant.empty()) {
                error = where + "each variant must be a non-empty array of strings";
                return false;
            }
            std::vector<std::string> lines;
            for (const json& line : variant) {
                if (!line.is_string()) {
                    error = where + "variant lines must be strings";
                    return false;
                }
                lines.push_back(line.get<std::string>());
            }
            t.variants.push_back(std::move(lines));
        }
        out.push_back(std::move(t));
    }
    return true;
}

}  // namespace

const MailTemplate* EventEngine::findTemplate(const std::string& id) const {
    for (const MailTemplate& t : templates_) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

bool EventEngine::load(const std::string& data_dir, const World& world,
                       const ChatStore& chat, std::string& error) {
    templates_.clear();
    rules_.clear();
    read_files_.clear();
    downloaded_.clear();
    read_mail_.clear();
    commands_.clear();
    player_host_ = world.playerHost();

    namespace fs = std::filesystem;

    // Шаблоны писем: все файлы data/mail_templates/*.json в алфавитном порядке.
    std::string templates_dir = joinPath(data_dir, "mail_templates");
    std::error_code ec;
    if (!fs::is_directory(templates_dir, ec)) {
        error = "cannot open mail templates directory: " + templates_dir;
        return false;
    }
    std::vector<std::string> files;
    for (const auto& entry : fs::directory_iterator(templates_dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;
        files.push_back(entry.path().string());
    }
    std::sort(files.begin(), files.end());

    std::set<std::string> template_ids;
    for (const std::string& file : files) {
        std::ifstream in(file);
        if (!in) {
            error = "cannot open file: " + file;
            return false;
        }
        json root;
        try {
            in >> root;
        } catch (const std::exception& e) {
            error = "mail template file '" + file + "': invalid JSON: " + e.what();
            return false;
        }
        std::vector<MailTemplate> parsed;
        if (!parseTemplateArray(root, file, parsed, error)) return false;
        for (MailTemplate& t : parsed) {
            if (!template_ids.insert(t.id).second) {
                error = "duplicate mail template id '" + t.id + "'";
                return false;
            }
            templates_.push_back(std::move(t));
        }
    }

    // Правила из data/events.json в порядке файла.
    std::string events_path = joinPath(data_dir, "events.json");
    std::ifstream ein(events_path);
    if (!ein) {
        error = "cannot open file: " + events_path;
        return false;
    }
    std::string events_text((std::istreambuf_iterator<char>(ein)),
                            std::istreambuf_iterator<char>());
    return loadRulesJson(events_text, world, chat, error);
}

bool EventEngine::loadRulesJson(const std::string& text, const World& world,
                                const ChatStore& chat, std::string& error) {
    rules_.clear();
    json root;
    try {
        root = json::parse(text);
    } catch (const std::exception& e) {
        error = std::string("events.json: invalid JSON: ") + e.what();
        return false;
    }
    if (!root.is_array()) {
        error = "events.json: expected an array of rules";
        return false;
    }

    std::set<std::string> rule_ids;
    for (std::size_t i = 0; i < root.size(); ++i) {
        const json& node = root[i];
        std::string where = "event #" + std::to_string(i + 1) + ": ";
        if (!node.is_object()) {
            error = where + "expected an object";
            return false;
        }
        if (!node.contains("id") || !node["id"].is_string()) {
            error = where + "'id' must be a string";
            return false;
        }
        Rule rule;
        rule.id = node["id"].get<std::string>();
        if (rule.id.empty()) {
            error = where + "'id' must not be empty";
            return false;
        }
        where = "event '" + rule.id + "': ";
        if (!rule_ids.insert(rule.id).second) {
            error = where + "duplicate rule id";
            return false;
        }

        // Поддерживаем и новый формат ("conditions"), и старый ("when").
        const char* conditions_key = nullptr;
        if (node.contains("conditions")) conditions_key = "conditions";
        else if (node.contains("when")) conditions_key = "when";
        if (conditions_key) {
            const json& conditions = node[conditions_key];
            if (!conditions.is_object()) {
                error = where + "'" + conditions_key + "' must be an object";
                return false;
            }
            // visited: строка или массив строк (совместимость со старым форматом).
            if (conditions.contains("visited")) {
                const json& visited = conditions["visited"];
                if (visited.is_string()) {
                    rule.visited.push_back(visited.get<std::string>());
                } else if (visited.is_array()) {
                    for (const json& item : visited) {
                        if (!item.is_string()) {
                            error = where + "'" + conditions_key + ".visited' must contain only strings";
                            return false;
                        }
                        rule.visited.push_back(item.get<std::string>());
                    }
                } else {
                    error = where + "'" + conditions_key + ".visited' must be a string or an array of strings";
                    return false;
                }
                for (const std::string& host_name : rule.visited) {
                    if (!world.findHost(host_name)) {
                        error = where + "unknown host '" + host_name + "' in " +
                                conditions_key + ".visited";
                        return false;
                    }
                }
            }
            auto readStringArray = [&](const char* key, std::vector<std::string>& out) -> bool {
                if (!conditions.contains(key)) return true;
                const json& value = conditions[key];
                if (!value.is_array()) {
                    error = where + "'" + conditions_key + "." + key +
                            "' must be an array of strings";
                    return false;
                }
                for (const json& item : value) {
                    if (!item.is_string()) {
                        error = where + "'" + conditions_key + "." + key +
                                "' must contain only strings";
                        return false;
                    }
                    out.push_back(item.get<std::string>());
                }
                return true;
            };
            if (!readStringArray("downloaded", rule.downloaded) ||
                !readStringArray("file_read", rule.file_read) ||
                !readStringArray("mail_read", rule.mail_read) ||
                !readStringArray("command", rule.command)) return false;
            if (conditions.contains("visited_count")) {
                const json& count = conditions["visited_count"];
                if (!count.is_number_integer()) {
                    error = where + "'" + conditions_key + ".visited_count' must be an integer";
                    return false;
                }
                int n = count.get<int>();
                if (n < 0) {
                    error = where + "'" + conditions_key + ".visited_count' must not be negative";
                    return false;
                }
                rule.visited_count = n;
            }
        }

        if (node.contains("once")) {
            if (!node["once"].is_boolean()) {
                error = where + "'once' must be a boolean";
                return false;
            }
            rule.once = node["once"].get<bool>();
        }

        // Поддерживаем и новый формат ("actions"), и старый ("do").
        const char* actions_key = nullptr;
        if (node.contains("actions")) actions_key = "actions";
        else if (node.contains("do")) actions_key = "do";
        if (!actions_key || !node[actions_key].is_array() || node[actions_key].empty()) {
            error = where + "'actions' must be a non-empty array";
            return false;
        }
        for (const json& item : node[actions_key]) {
            if (!item.is_object() || item.empty() || item.size() != 1) {
                error = where + "each action must be an object with exactly one key";
                return false;
            }
            const std::string key = item.begin().key();
            Action action;
            if (key == "mail") {
                if (!item["mail"].is_string()) {
                    error = where + "'mail' must be a string";
                    return false;
                }
                std::string tid = item["mail"].get<std::string>();
                if (!findTemplate(tid)) {
                    error = where + "unknown mail template '" + tid + "'";
                    return false;
                }
                action.kind = Action::Kind::Mail;
                action.id = tid;
            } else if (key == "chat") {
                const json& c = item["chat"];
                if (!c.is_object() || !c.contains("contact") || !c["contact"].is_string() ||
                    !c.contains("text") || !c["text"].is_string()) {
                    error = where +
                            "'chat' must be an object with string 'contact' and 'text'";
                    return false;
                }
                std::string cid = c["contact"].get<std::string>();
                if (!chat.find(cid)) {
                    error = where + "unknown contact '" + cid + "'";
                    return false;
                }
                action.kind = Action::Kind::Chat;
                action.id = cid;
                action.message = c["text"].get<std::string>();
            } else if (key == "unlock_contact") {
                if (!item["unlock_contact"].is_string()) {
                    error = where + "'unlock_contact' must be a string";
                    return false;
                }
                std::string cid = item["unlock_contact"].get<std::string>();
                if (!chat.find(cid)) {
                    error = where + "unknown contact '" + cid + "'";
                    return false;
                }
                action.kind = Action::Kind::UnlockContact;
                action.id = cid;
            } else {
                error = where + "unknown action '" + key + "'";
                return false;
            }
            rule.actions.push_back(std::move(action));
        }

        rules_.push_back(std::move(rule));
    }
    return true;
}

bool EventEngine::matches(const Rule& rule, const Session& session) const {
    const std::vector<VisitedEntry>& entries = session.gameState().entries();
    for (const std::string& host_name : rule.visited) {
        bool found = false;
        for (const VisitedEntry& e : entries) {
            if (e.host == host_name) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    if (rule.visited_count >= 0 &&
        session.gameState().distinctHosts(player_host_) < rule.visited_count) {
        return false;
    }
    auto hasAll = [](const std::vector<std::string>& values,
                     const std::set<std::string>& observed) {
        return std::all_of(values.begin(), values.end(), [&](const std::string& value) {
            return observed.find(value) != observed.end();
        });
    };
    if (!hasAll(rule.downloaded, downloaded_) || !hasAll(rule.file_read, read_files_) ||
        !hasAll(rule.mail_read, read_mail_) || !hasAll(rule.command, commands_)) return false;
    return true;
}

void EventEngine::onSignal(Session& session, World&, Signal signal,
                           const SignalData& data) {
    switch (signal) {
        case Signal::FileRead: read_files_.insert(data.path); break;
        case Signal::Download: downloaded_.insert(data.name); break;
        case Signal::MailRead: read_mail_.insert(data.name); break;
        case Signal::Command: commands_.insert(data.name); break;
        case Signal::Login: break;
    }
    fireMatching(session, data);
}

void EventEngine::fireMatching(Session& session, const SignalData& data) {
    const std::string& last_host = data.host;
    int visited_count = session.gameState().distinctHosts(player_host_);
    const std::string player_user = session.localUser();
    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char time_buffer[6] = {};
    std::strftime(time_buffer, sizeof(time_buffer), "%H:%M", &local);

    for (const Rule& rule : rules_) {
        if (rule.once && session.gameState().eventFired(rule.id)) continue;
        if (!matches(rule, session)) continue;

        for (const Action& action : rule.actions) {
            if (action.kind == Action::Kind::Mail) {
                const MailTemplate* t = findTemplate(action.id);
                if (!t) continue;
                Message message;
                message.from = t->from;
                message.subject =
                    substitute(t->subject, last_host, visited_count, player_user);
                message.date = t->date;
                message.trigger = t->id;
                std::uniform_int_distribution<std::size_t> dist(0, t->variants.size() - 1);
                for (const std::string& line : t->variants[dist(rng_)]) {
                    message.body.push_back(
                        substitute(line, last_host, visited_count, player_user));
                }
                session.mailbox().add(std::move(message));
            } else if (action.kind == Action::Kind::Chat) {
                session.chat().add(action.id, ChatMessage{action.id, action.message,
                                                          time_buffer});
            } else {  // UnlockContact
                session.chat().unlock(action.id);
            }
        }

        if (rule.once) session.gameState().markEventFired(rule.id);
    }
}

void EventEngine::onLogin(const std::string& host) {
    if (session_) onSignal(*session_, session_->world(), Signal::Login, {host, {}, {}});
}

void EventEngine::onFileRead(const std::string& host, const std::string& path) {
    if (session_) onSignal(*session_, session_->world(), Signal::FileRead, {host, path, {}});
}

void EventEngine::onDownload(const std::string& name) {
    if (session_) onSignal(*session_, session_->world(), Signal::Download, {{}, {}, name});
}

void EventEngine::onMailRead(const std::string& mailId) {
    if (session_) onSignal(*session_, session_->world(), Signal::MailRead, {{}, {}, mailId});
}

void EventEngine::onCommand(const std::string& cmd) {
    if (session_) onSignal(*session_, session_->world(), Signal::Command, {{}, {}, cmd});
}
