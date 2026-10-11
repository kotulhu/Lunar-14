#pragma once

#include <memory>
#include <string>

class ResponseProvider;

// Создаёт провайдер чата по имени из config.json.
// "canned" — правила из chat_replies.json. "openrouter"/"local" пока не
// реализованы: пишем одну строку в warning и возвращаем canned.
// Пустое имя тоже означает canned. При ошибке загрузки правил возвращает nullptr.
std::unique_ptr<ResponseProvider> makeChatProvider(const std::string& name,
                                                    const std::string& replies_path,
                                                    std::string& error,
                                                    std::string& warning);

// Читает "chat_provider" из config.json. Если файла нет — "canned".
// При некорректном config.json пишет error и возвращает пустую строку.
std::string readChatProviderName(const std::string& config_path, std::string& error);
