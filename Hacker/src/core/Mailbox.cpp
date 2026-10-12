#include "core/Mailbox.h"

#include <fstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

// Разбивает строку на строки по "\n" (убираем "\r").
std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string line;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(line);
            line.clear();
        } else if (c != '\r') {
            line += c;
        }
    }
    lines.push_back(line);
    return lines;
}

}  // namespace

bool Mailbox::loadFromFile(const std::string& path, std::string& error) {
    std::ifstream in(path);
    if (!in) {
        error = "cannot open file: " + path;
        return false;
    }

    json root;
    try {
        in >> root;
    } catch (const std::exception& e) {
        error = std::string("invalid JSON: ") + e.what();
        return false;
    }

    if (!root.is_object() || !root.contains("messages")) {
        error = "mail: missing 'messages' array";
        return false;
    }
    const json& list = root["messages"];
    if (!list.is_array()) {
        error = "mail: 'messages' must be an array";
        return false;
    }

    messages_.clear();
    next_id_ = 1;

    for (std::size_t i = 0; i < list.size(); ++i) {
        const json& node = list[i];
        const std::string where = "message " + std::to_string(i + 1) + ": ";

        if (!node.is_object()) {
            error = where + "expected an object";
            return false;
        }

        Message message;
        if (node.contains("id")) {
            if (!node["id"].is_number_integer()) {
                error = where + "'id' must be an integer";
                return false;
            }
            message.id = node["id"].get<int>();
        }

        // Читает необязательную строку; required-строки должны присутствовать.
        auto readString = [&](const char* key, std::string& dst, bool required) -> bool {
            if (!node.contains(key)) {
                if (required) {
                    error = where + "'" + key + "' is required";
                    return false;
                }
                return true;
            }
            if (!node[key].is_string()) {
                error = where + "'" + key + "' must be a string";
                return false;
            }
            dst = node[key].get<std::string>();
            return true;
        };

        if (!readString("from", message.from, true)) return false;
        if (!readString("subject", message.subject, true)) return false;
        if (!readString("date", message.date, true)) return false;
        if (!readString("trigger", message.trigger, false)) return false;

        if (node.contains("read")) {
            if (!node["read"].is_boolean()) {
                error = where + "'read' must be a boolean";
                return false;
            }
            message.read = node["read"].get<bool>();
        }

        if (node.contains("body")) {
            const json& body = node["body"];
            if (body.is_string()) {
                message.body = splitLines(body.get<std::string>());
            } else if (body.is_array()) {
                for (std::size_t j = 0; j < body.size(); ++j) {
                    if (!body[j].is_string()) {
                        error = where + "'body' element " + std::to_string(j + 1) +
                                " must be a string";
                        return false;
                    }
                    message.body.push_back(body[j].get<std::string>());
                }
            } else {
                error = where + "'body' must be a string or an array of strings";
                return false;
            }
        }

        add(std::move(message));
    }

    return true;
}

void Mailbox::add(Message message) {
    if (message.id <= 0) message.id = next_id_;
    if (message.id >= next_id_) next_id_ = message.id + 1;
    messages_.push_back(std::move(message));
}

void Mailbox::clear() {
    messages_.clear();
    next_id_ = 1;
}

bool Mailbox::markRead(std::size_t index) {
    if (index >= messages_.size()) return false;
    messages_[index].read = true;
    return true;
}

int Mailbox::unreadCount() const {
    int count = 0;
    for (const Message& message : messages_) {
        if (!message.read) ++count;
    }
    return count;
}
