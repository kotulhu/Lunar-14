#include "core/ChatStore.h"

#include <fstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

const std::vector<ChatMessage>& emptyHistoryStorage() {
    static const std::vector<ChatMessage> empty;
    return empty;
}

}  // namespace

bool ChatStore::loadFromFile(const std::string& path, std::string& error) {
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

    if (!root.is_object() || !root.contains("contacts")) {
        error = "chat: missing 'contacts' array";
        return false;
    }
    const json& list = root["contacts"];
    if (!list.is_array()) {
        error = "chat: 'contacts' must be an array";
        return false;
    }

    entries_.clear();
    open_id_.clear();

    for (std::size_t i = 0; i < list.size(); ++i) {
        const json& node = list[i];
        const std::string where = "contact " + std::to_string(i + 1) + ": ";

        if (!node.is_object()) {
            error = where + "expected an object";
            return false;
        }

        // Читает строку; required-поля должны присутствовать.
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

        Entry entry;
        if (!readString("id", entry.contact.id, true)) return false;
        if (entry.contact.id.empty()) {
            error = where + "'id' must not be empty";
            return false;
        }
        if (!readString("name", entry.contact.name, true)) return false;
        if (!readString("status", entry.contact.status, true)) return false;

        if (node.contains("visible")) {
            if (!node["visible"].is_boolean()) {
                error = where + "'visible' must be a boolean";
                return false;
            }
            entry.contact.visible = node["visible"].get<bool>();
        }

        entries_.push_back(std::move(entry));
        Entry& stored = entries_.back();

        if (node.contains("history")) {
            const json& history = node["history"];
            if (!history.is_array()) {
                error = where + "'history' must be an array";
                return false;
            }
            for (std::size_t j = 0; j < history.size(); ++j) {
                const json& msg = history[j];
                const std::string mwhere =
                    where + "message " + std::to_string(j + 1) + ": ";
                if (!msg.is_object()) {
                    error = mwhere + "expected an object";
                    return false;
                }
                ChatMessage message;
                auto readMsgField = [&](const char* key, std::string& dst) -> bool {
                    if (!msg.contains(key) || !msg[key].is_string()) {
                        error = mwhere + "'" + key + "' must be a string";
                        return false;
                    }
                    dst = msg[key].get<std::string>();
                    return true;
                };
                if (!readMsgField("from", message.from)) return false;
                if (!readMsgField("text", message.text)) return false;
                if (!readMsgField("time", message.time)) return false;
                add(stored.contact.id, std::move(message));
            }
        }
    }

    return true;
}

ChatStore::Entry* ChatStore::findEntry(const std::string& id) {
    for (Entry& entry : entries_) {
        if (entry.contact.id == id) return &entry;
    }
    return nullptr;
}

const ChatStore::Entry* ChatStore::findEntry(const std::string& id) const {
    for (const Entry& entry : entries_) {
        if (entry.contact.id == id) return &entry;
    }
    return nullptr;
}

std::vector<const Contact*> ChatStore::visibleContacts() const {
    std::vector<const Contact*> result;
    for (const Entry& entry : entries_) {
        if (entry.contact.visible) result.push_back(&entry.contact);
    }
    return result;
}

const Contact* ChatStore::find(const std::string& id) const {
    const Entry* entry = findEntry(id);
    return entry ? &entry->contact : nullptr;
}

const std::vector<ChatMessage>& ChatStore::history(const std::string& id) const {
    const Entry* entry = findEntry(id);
    return entry ? entry->history : emptyHistoryStorage();
}

void ChatStore::add(const std::string& contactId, ChatMessage message) {
    Entry* entry = findEntry(contactId);
    if (!entry) return;
    if (message.from != "me" && contactId != open_id_) ++entry->unread;
    entry->history.push_back(std::move(message));
}

int ChatStore::unread(const std::string& id) const {
    const Entry* entry = findEntry(id);
    return entry ? entry->unread : 0;
}

int ChatStore::unreadVisible() const {
    int total = 0;
    for (const Entry& entry : entries_) {
        if (entry.contact.visible) total += entry.unread;
    }
    return total;
}

void ChatStore::openContact(const std::string& id) {
    Entry* entry = findEntry(id);
    if (!entry) return;
    open_id_ = id;
    entry->unread = 0;
}

void ChatStore::unlock(const std::string& id) {
    Entry* entry = findEntry(id);
    if (entry) entry->contact.visible = true;
}

void ChatStore::unlockAll() {
    for (Entry& entry : entries_) entry.contact.visible = true;
}

std::vector<Contact> ChatStore::contacts() const {
    std::vector<Contact> result;
    result.reserve(entries_.size());
    for (const Entry& entry : entries_) result.push_back(entry.contact);
    return result;
}

void ChatStore::restore(const std::string& id, bool visible,
                        std::vector<ChatMessage> history, int unread) {
    Entry* entry = findEntry(id);
    if (!entry) return;
    entry->contact.visible = visible;
    entry->history = std::move(history);
    entry->unread = unread;
}
