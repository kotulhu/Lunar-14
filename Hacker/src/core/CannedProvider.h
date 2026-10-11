#pragma once

#include <map>
#include <random>
#include <string>
#include <vector>

#include "core/ResponseProvider.h"

// Ответы по правилам из data/chat_replies.json. Правило срабатывает, если
// сообщение игрока содержит любое из его ключевых слов (без учёта регистра).
class CannedProvider : public ResponseProvider {
public:
    // Загружает правила из JSON вида {"contacts": {"<id>": {"rules": [...], "fallback": [...]}}}.
    bool loadFromFile(const std::string& path, std::string& error);

    std::string reply(const ReplyRequest& request) override;

private:
    struct Rule {
        std::vector<std::string> keywords;
        std::vector<std::string> replies;
    };
    struct ContactRules {
        std::vector<Rule> rules;
        std::vector<std::string> fallback;
    };

    std::string pick(const std::vector<std::string>& variants);

    std::map<std::string, ContactRules> contacts_;
    std::mt19937 rng_{std::random_device{}()};
};
