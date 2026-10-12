#pragma once

#include <string>
#include <vector>

// Собеседник в чате. visible=false — контакт скрыт до разблокировки.
struct Contact {
    std::string id;
    std::string name;
    std::string status;
    bool visible = false;
};

// Одно сообщение переписки. from == "me" — сообщение игрока, иначе id контакта.
struct ChatMessage {
    std::string from;
    std::string text;
    std::string time;
};

// Контакты и переписки игрока: живут только в памяти запуска.
class ChatStore {
public:
    // Загружает контакты из JSON вида {"contacts": [...]}.
    // При ошибке возвращает false и пишет понятную причину (контакт/номер сообщения).
    bool loadFromFile(const std::string& path, std::string& error);

    // Видимые контакты в порядке загрузки.
    std::vector<const Contact*> visibleContacts() const;

    // Контакт по id или nullptr.
    const Contact* find(const std::string& id) const;

    // История переписки контакта (пустая, если контакта нет).
    const std::vector<ChatMessage>& history(const std::string& id) const;

    // Добавляет сообщение в переписку. Входящее сообщение, пришедшее в неоткрытую
    // переписку, увеличивает счётчик непрочитанных.
    void add(const std::string& contactId, ChatMessage message);

    // Число непрочитанных входящих по контакту.
    int unread(const std::string& id) const;

    // Число непрочитанных по видимым контактам (для приветствия).
    int unreadVisible() const;

    // Открыть переписку: контакт становится текущим, счётчик сбрасывается.
    void openContact(const std::string& id);

    // Сделать контакт видимым.
    void unlock(const std::string& id);

    // Открыть все контакты (проверочный ключ --unlock-contacts).
    void unlockAll();

    // Копии всех контактов в порядке загрузки (для сохранения).
    std::vector<Contact> contacts() const;

    // Восстанавливает контакт из сохранения: видимость, историю и счётчик
    // непрочитанных. Ничего не делает, если контакта нет.
    void restore(const std::string& id, bool visible,
                 std::vector<ChatMessage> history, int unread);

private:
    struct Entry {
        Contact contact;
        std::vector<ChatMessage> history;
        int unread = 0;
    };

    Entry* findEntry(const std::string& id);
    const Entry* findEntry(const std::string& id) const;

    std::vector<Entry> entries_;
    std::string open_id_;  // id переписки, открытой прямо сейчас
};
