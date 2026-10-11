#include "core/CannedProvider.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "core/TextUtil.h"

using json = nlohmann::json;

namespace {

// Читает массив строк; при ошибке пишет причину в error.
bool readStringArray(const json& node, const std::string& where,
                     std::vector<std::string>& out, std::string& error) {
    if (!node.is_array()) {
        error = where + " must be an array";
        return false;
    }
    for (std::size_t i = 0; i < node.size(); ++i) {
        if (!node[i].is_string()) {
            error = where + " element " + std::to_string(i + 1) + " must be a string";
            return false;
        }
        out.push_back(node[i].get<std::string>());
    }
    return true;
}

}  // namespace

bool CannedProvider::loadFromFile(const std::string& path, std::string& error) {
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
        error = "chat_replies: missing 'contacts' object";
        return false;
    }
    const json& contacts = root["contacts"];
    if (!contacts.is_object()) {
        error = "chat_replies: 'contacts' must be an object";
        return false;
    }

    contacts_.clear();

    for (auto it = contacts.begin(); it != contacts.end(); ++it) {
        const std::string where = "chat_replies: contact '" + it.key() + "': ";
        const json& node = it.value();
        if (!node.is_object()) {
            error = where + "expected an object";
            return false;
        }

        ContactRules rules;
        if (node.contains("rules")) {
            const json& list = node["rules"];
            if (!list.is_array()) {
                error = where + "'rules' must be an array";
                return false;
            }
            for (std::size_t i = 0; i < list.size(); ++i) {
                const json& rule = list[i];
                const std::string rwhere = where + "rule " + std::to_string(i + 1) + ": ";
                if (!rule.is_object()) {
                    error = rwhere + "expected an object";
                    return false;
                }
                Rule parsed;
                if (!rule.contains("keywords")) {
                    error = rwhere + "'keywords' is required";
                    return false;
                }
                if (!readStringArray(rule["keywords"], rwhere + "'keywords'",
                                     parsed.keywords, error)) {
                    return false;
                }
                if (!rule.contains("replies")) {
                    error = rwhere + "'replies' is required";
                    return false;
                }
                if (!readStringArray(rule["replies"], rwhere + "'replies'",
                                     parsed.replies, error)) {
                    return false;
                }
                rules.rules.push_back(std::move(parsed));
            }
        }

        if (node.contains("fallback")) {
            if (!readStringArray(node["fallback"], where + "'fallback'",
                                 rules.fallback, error)) {
                return false;
            }
        }

        contacts_[it.key()] = std::move(rules);
    }

    return true;
}

std::string CannedProvider::pick(const std::vector<std::string>& variants) {
    if (variants.empty()) return "";
    std::uniform_int_distribution<std::size_t> dist(0, variants.size() - 1);
    return variants[dist(rng_)];
}

std::string CannedProvider::reply(const ReplyRequest& request) {
    auto it = contacts_.find(request.contact_id);
    if (it == contacts_.end()) return "";

    const std::string text = toLowerUtf8(request.player_text);
    for (const Rule& rule : it->second.rules) {
        for (const std::string& keyword : rule.keywords) {
            if (keyword.empty()) continue;
            if (text.find(toLowerUtf8(keyword)) != std::string::npos) {
                return pick(rule.replies);
            }
        }
    }
    return pick(it->second.fallback);
}
