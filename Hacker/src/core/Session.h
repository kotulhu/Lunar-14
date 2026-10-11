#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "core/ChatStore.h"
#include "core/Host.h"
#include "core/Mailbox.h"
#include "core/ResponseProvider.h"

class World;

// Запись о скачанном файле (пока не показывается, нужна для будущих очков и улик).
struct Download {
    std::string host;    // имя хоста-источника
    std::string remote;  // удалённый путь
    std::string local;   // локальный путь
};

// Состояние игрока: стек открытых сессий (нижняя — стартовая, верхняя — текущая).
class Session {
public:
    // Создаёт нижнюю сессию на хосте, найденном по ip или имени.
    // Бросает std::runtime_error, если хост не найден.
    Session(World& world, const std::string& host_query);

    const Host& host() const { return *stack_.back().host; }
    const std::string& hostName() const { return stack_.back().host_name; }
    const std::string& hostIp() const { return stack_.back().host_ip; }
    const std::string& user() const { return stack_.back().user; }
    const std::string& currentDir() const { return stack_.back().dir; }

    // Изменяемый доступ: currentHost — верхняя сессия, localHost — домашний компьютер.
    Host& currentHost() { return *stack_.back().host; }
    Host& localHost() { return *stack_.front().host; }

    void setCurrentDir(const std::string& dir) { stack_.back().dir = dir; }

    // Добавляет сессию на удалённом хосте (успешный вход telnet/ssh).
    void push(const std::string& host_query, const std::string& user, const std::string& dir);
    // Снимает верхнюю сессию. True, если остались ещё сессии.
    bool pop();
    std::size_t depth() const { return stack_.size(); }

    void addDownload(const Download& record) { downloads_.push_back(record); }
    const std::vector<Download>& downloads() const { return downloads_; }

    // Почтовый ящик игрока — один на запуск.
    Mailbox& mailbox() { return mailbox_; }
    const Mailbox& mailbox() const { return mailbox_; }

    // Чаты игрока и поставщик ответов — тоже одни на запуск.
    ChatStore& chat() { return chat_; }
    const ChatStore& chat() const { return chat_; }
    void setChatProvider(std::unique_ptr<ResponseProvider> provider) {
        chat_provider_ = std::move(provider);
    }
    ResponseProvider* chatProvider() { return chat_provider_.get(); }

    // Команда exit выставляет флаг, а главный цикл завершает работу.
    void requestQuit() { quit_ = true; }
    bool quitRequested() const { return quit_; }

private:
    struct Frame {
        Host* host = nullptr;
        std::string host_name;
        std::string host_ip;
        std::string user;
        std::string dir;
    };

    World* world_ = nullptr;
    std::vector<Frame> stack_;
    std::vector<Download> downloads_;
    Mailbox mailbox_;
    ChatStore chat_;
    std::unique_ptr<ResponseProvider> chat_provider_;
    bool quit_ = false;
};
