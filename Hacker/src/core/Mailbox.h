#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Одно письмо. trigger пока зарезервирован и нигде не используется.
struct Message {
    int id = 0;
    std::string from;
    std::string subject;
    std::string date;
    bool read = false;
    std::vector<std::string> body;
    std::string trigger;
};

// Почтовый ящик игрока: письма живут только в памяти запуска.
class Mailbox {
public:
    // Загружает письма из JSON вида {"messages": [...]}.
    // При ошибке возвращает false и пишет понятную причину с номером письма.
    bool loadFromFile(const std::string& path, std::string& error);

    // Добавляет письмо; id назначается автоматически, если не задан (<= 0).
    void add(Message message);

    const std::vector<Message>& messages() const { return messages_; }
    std::vector<Message>& messages() { return messages_; }

    // Помечает письмо прочитанным по индексу. false, если индекс вне диапазона.
    bool markRead(std::size_t index);

    // Количество непрочитанных писем.
    int unreadCount() const;

private:
    std::vector<Message> messages_;
    int next_id_ = 1;
};
