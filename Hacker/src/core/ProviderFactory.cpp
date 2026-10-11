#include "core/ProviderFactory.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "core/CannedProvider.h"

std::unique_ptr<ResponseProvider> makeChatProvider(const std::string& name,
                                                    const std::string& replies_path,
                                                    std::string& error,
                                                    std::string& warning) {
    warning.clear();
    if (!name.empty() && name != "canned") {
        warning = "chat: provider '" + name + "' is not implemented, using 'canned'.";
    }

    auto provider = std::make_unique<CannedProvider>();
    if (!provider->loadFromFile(replies_path, error)) return nullptr;
    return provider;
}

std::string readChatProviderName(const std::string& config_path, std::string& error) {
    std::ifstream in(config_path);
    if (!in) return "canned";  // конфига нет — обычные ответы

    nlohmann::json root;
    try {
        in >> root;
    } catch (const std::exception& e) {
        error = std::string("config: invalid JSON: ") + e.what();
        return "";
    }

    if (!root.is_object() || !root.contains("chat_provider")) return "canned";
    if (!root["chat_provider"].is_string()) {
        error = "config: 'chat_provider' must be a string";
        return "";
    }
    return root["chat_provider"].get<std::string>();
}
