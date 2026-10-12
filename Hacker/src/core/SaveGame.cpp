#include "core/SaveGame.h"

#include <filesystem>
#include <fstream>
#include <system_error>

#include <nlohmann/json.hpp>

#include "core/Session.h"
#include "core/VirtualFs.h"
#include "core/World.h"

using json = nlohmann::json;

namespace {

// Разбивает текст на строки (для тела письма, сохранённого строкой).
std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string line;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(line);
            line.clear();
        } else {
            line += c;
        }
    }
    lines.push_back(line);
    return lines;
}

// Читает письмо из JSON-объекта. false, если структура не подходит.
bool parseMessage(const json& node, Message& message) {
    if (!node.is_object()) return false;
    message.id = node.value("id", 0);
    message.from = node.value("from", std::string());
    message.subject = node.value("subject", std::string());
    message.date = node.value("date", std::string());
    message.read = node.value("read", false);
    message.trigger = node.value("trigger", std::string());

    if (node.contains("body")) {
        const json& body = node["body"];
        if (body.is_string()) {
            message.body = splitLines(body.get<std::string>());
        } else if (body.is_array()) {
            for (const json& line : body) {
                if (!line.is_string()) return false;
                message.body.push_back(line.get<std::string>());
            }
        } else {
            return false;
        }
    }
    return true;
}

}  // namespace

std::string SaveGame::serialize(const Session& session) {
    json root;
    root["version"] = kVersion;

    json state = json::object();

    // visited: список известных узлов в порядке добавления.
    json visited = json::array();
    for (const VisitedEntry& e : session.gameState().entries()) {
        visited.push_back(
            {{"host", e.host}, {"port", e.port}, {"user", e.user}, {"pass", e.pass}});
    }
    state["visited"] = std::move(visited);

    // mailbox: письма целиком в текущем порядке.
    json mailbox = json::array();
    for (const Message& m : session.mailbox().messages()) {
        json node;
        node["id"] = m.id;
        node["from"] = m.from;
        node["subject"] = m.subject;
        node["date"] = m.date;
        node["read"] = m.read;
        node["body"] = m.body;
        if (!m.trigger.empty()) node["trigger"] = m.trigger;
        mailbox.push_back(std::move(node));
    }
    state["mailbox"] = std::move(mailbox);

    // chat: контакты, видимость, история и счётчик непрочитанных.
    json contacts = json::array();
    for (const Contact& c : session.chat().contacts()) {
        json node;
        node["id"] = c.id;
        node["visible"] = c.visible;
        node["unread"] = session.chat().unread(c.id);
        json history = json::array();
        for (const ChatMessage& m : session.chat().history(c.id)) {
            history.push_back({{"from", m.from}, {"text", m.text}, {"time", m.time}});
        }
        node["history"] = std::move(history);
        contacts.push_back(std::move(node));
    }
    json chat;
    chat["contacts"] = std::move(contacts);
    state["chat"] = std::move(chat);

    // files: созданные или перезаписанные игроком файлы по хостам.
    json files = json::object();
    for (const auto& host : VirtualFs::changes()) {
        json host_files = json::object();
        for (const auto& file : host.second) host_files[file.first] = file.second;
        files[host.first] = std::move(host_files);
    }
    state["files"] = std::move(files);

    root["state"] = std::move(state);
    return root.dump(2);
}

bool SaveGame::deserialize(const std::string& text, Session& session, const World& world,
                           std::string& error, std::string& warning) {
    json root;
    try {
        root = json::parse(text);
    } catch (const std::exception& e) {
        error = std::string("invalid JSON: ") + e.what();
        return false;
    }
    if (!root.is_object()) {
        error = "save: root must be an object";
        return false;
    }
    if (!root.contains("version") || !root["version"].is_number_integer()) {
        error = "save: missing version";
        return false;
    }
    int version = root["version"].get<int>();
    if (version != kVersion) {
        error = "save: unsupported version " + std::to_string(version);
        return false;
    }
    if (!root.contains("state") || !root["state"].is_object()) {
        error = "save: missing state";
        return false;
    }
    const json& state = root["state"];

    int skipped = 0;
    std::string first_unknown_host;

    // visited: неизвестные хосты пропускаем.
    session.gameState().clear();
    if (state.contains("visited") && state["visited"].is_array()) {
        for (const json& node : state["visited"]) {
            try {
                if (!node.is_object()) {
                    ++skipped;
                    continue;
                }
                std::string host = node.value("host", std::string());
                if (host.empty() || !world.findHost(host)) {
                    ++skipped;
                    if (first_unknown_host.empty()) first_unknown_host = host;
                    continue;
                }
                session.gameState().record(host, node.value("port", 0),
                                           node.value("user", std::string()),
                                           node.value("pass", std::string()));
            } catch (const std::exception&) {
                ++skipped;
            }
        }
    }

    // mailbox: загруженный ящик заменяет стартовый из mail.json. Если раздела
    // нет, остаётся стартовый ящик (значение по умолчанию).
    if (state.contains("mailbox") && state["mailbox"].is_array()) {
        session.mailbox().clear();
        for (const json& node : state["mailbox"]) {
            try {
                Message message;
                if (!parseMessage(node, message)) {
                    ++skipped;
                    continue;
                }
                session.mailbox().add(std::move(message));
            } catch (const std::exception&) {
                ++skipped;
            }
        }
    }

    // chat: неизвестные контакты пропускаем, известные восстанавливаем.
    if (state.contains("chat") && state["chat"].is_object() && state["chat"].contains("contacts") &&
        state["chat"]["contacts"].is_array()) {
        for (const json& node : state["chat"]["contacts"]) {
            try {
                if (!node.is_object()) {
                    ++skipped;
                    continue;
                }
                std::string id = node.value("id", std::string());
                if (id.empty() || !session.chat().find(id)) {
                    ++skipped;
                    continue;
                }
                std::vector<ChatMessage> history;
                if (node.contains("history") && node["history"].is_array()) {
                    for (const json& msg : node["history"]) {
                        if (!msg.is_object()) continue;
                        history.push_back({msg.value("from", std::string()),
                                           msg.value("text", std::string()),
                                           msg.value("time", std::string())});
                    }
                }
                session.chat().restore(id, node.value("visible", false), std::move(history),
                                       node.value("unread", 0));
            } catch (const std::exception&) {
                ++skipped;
            }
        }
    }

    // files: пользовательские правки ФС; родителя нет — пропускаем.
    if (state.contains("files") && state["files"].is_object()) {
        for (auto it = state["files"].begin(); it != state["files"].end(); ++it) {
            const std::string host_name = it.key();
            const Host* host = world.findHost(host_name);
            if (!host || !it.value().is_object()) {
                ++skipped;
                if (first_unknown_host.empty()) first_unknown_host = host_name;
                continue;
            }
            for (auto f = it.value().begin(); f != it.value().end(); ++f) {
                if (!f.value().is_string() ||
                    !VirtualFs::writeFile(*host, f.key(), f.value().get<std::string>())) {
                    ++skipped;
                }
            }
        }
    }

    if (skipped > 0) {
        warning = "Skipped " + std::to_string(skipped) + " entries from the saved game";
        if (!first_unknown_host.empty()) {
            warning += " (unknown host: " + first_unknown_host + ")";
        }
    }
    return true;
}

bool SaveGame::writeAtomic(const std::string& path, const std::string& content,
                           std::string& error) {
    namespace fs = std::filesystem;
    std::string tmp = path + ".tmp";

    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "cannot open " + tmp;
            return false;
        }
        out << content;
        out.flush();
        if (!out) {
            error = "cannot write " + tmp;
            return false;
        }
    }

    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {
        // На Windows переименование поверх существующего файла запрещено.
        fs::remove(path, ec);
        fs::rename(tmp, path, ec);
        if (ec) {
            error = "cannot rename to " + path;
            return false;
        }
    }
    return true;
}

bool SaveGame::backup(const std::string& path, std::string& error) {
    namespace fs = std::filesystem;
    std::error_code ec;

    if (!fs::exists(path, ec)) return true;  // нечего архивировать

    fs::path bak = path + ".bak";
    fs::copy_file(path, bak, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        error = "cannot copy to " + bak.string();
        return false;
    }
    fs::remove(path, ec);
    if (ec) {
        error = "cannot remove " + path;
        return false;
    }
    return true;
}
