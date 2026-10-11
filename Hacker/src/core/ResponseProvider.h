#pragma once

#include <string>
#include <vector>

#include "core/ChatStore.h"

class Session;

// Запрос на ответ собеседника. Session и история добавлены на будущее
// (правила игры, ИИ-провайдеры): сейчас canned-провайдеру они не нужны.
struct ReplyRequest {
    std::string contact_id;
    std::string player_text;
    const Session& session;
    const std::vector<ChatMessage>& history;
};

// Поставщик ответов собеседника: реализацию можно менять через config.json.
class ResponseProvider {
public:
    virtual ~ResponseProvider() = default;
    virtual std::string reply(const ReplyRequest& request) = 0;
};
