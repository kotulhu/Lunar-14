#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "core/ChatStore.h"
#include "core/Credits.h"
#include "core/GameState.h"
#include "core/Host.h"
#include "core/LocalStorage.h"
#include "core/Mailbox.h"
#include "core/Quest.h"
#include "core/ResponseProvider.h"

class World;
class EventEngine;

// Состояние игрока: стек открытых сессий (нижняя — стартовая, верхняя — текущая).
class Session {
public:
    // Создаёт нижнюю сессию на хосте, найденном по ip или имени.
    // Бросает std::runtime_error, если хост не найден.
    Session(World& world, const std::string& host_query);
    ~Session();

    const Host& host() const { return *stack_.back().host; }
    const std::string& hostName() const { return stack_.back().host_name; }
    const std::string& hostIp() const { return stack_.back().host_ip; }
    const std::string& user() const { return stack_.back().user; }
    const std::string& currentDir() const { return stack_.back().dir; }

    // Логин игрока на домашнем хосте (нижняя сессия).
    const std::string& localUser() const { return stack_.front().user; }

    // Игровой мир, из которого построена сессия.
    World& world() { return *world_; }
    const World& world() const { return *world_; }

    // Изменяемый доступ: currentHost — верхняя сессия, localHost — домашний компьютер.
    Host& currentHost() { return *stack_.back().host; }
    Host& localHost() { return *stack_.front().host; }

    // Состояние игры на запуск: список известных узлов (пароли и т.п.).
    GameState& gameState() { return game_state_; }
    const GameState& gameState() const { return game_state_; }

    void setCurrentDir(const std::string& dir) { stack_.back().dir = dir; }

    // Добавляет сессию на удалённом хосте (успешный вход telnet/ssh).
    void push(const std::string& host_query, const std::string& user, const std::string& dir);
    // Снимает верхнюю сессию. True, если остались ещё сессии.
    bool pop();
    std::size_t depth() const { return stack_.size(); }

    LocalStorage& localStorage() { return local_storage_; }
    const LocalStorage& localStorage() const { return local_storage_; }

    // Локальная папка downloads на домашнем хосте. Физически файлы лежат
    // в LocalStorage, а ФС показывает их как каталог ~/downloads.
    std::string localDownloadsDir() const;
    bool isLocalDownloadsDir(const std::string& path) const;
    bool isLocalDownloadsParent(const std::string& path) const;
    bool isInLocalDownloads(const std::string& path) const;
    std::vector<std::string> listLocalDownloads(bool show_hidden) const;

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

    // Движок правил событий (может быть не установлен).
    EventEngine* events() { return event_engine_.get(); }
    const EventEngine* events() const { return event_engine_.get(); }
    void setEventEngine(std::unique_ptr<EventEngine> engine);

    // Титры финальной рассылки.
    Credits& credits() { return credits_; }
    const Credits& credits() const { return credits_; }

    // Этапы квеста (могут быть не установлены).
    Quest* quest() { return quest_.get(); }
    const Quest* quest() const { return quest_.get(); }
    void setQuest(std::unique_ptr<Quest> quest);

    // Текущий этап квеста по состоянию игры. nullptr, если этапов нет.
    const QuestStage* currentStage() const;

    // Момент создания сессии (для /proc/uptime).
    std::chrono::steady_clock::time_point startedAt() const { return started_at_; }

    // Запись telnet/ssh-попытки (в том числе неудачной) для /var/log/auth.log.
    struct ConnectionAttempt {
        std::string time;
        std::string service;
        std::string user;
        std::string target;
        int port = 0;
        bool ok = false;
    };
    void logConnection(const std::string& time, const std::string& service,
                       const std::string& user, const std::string& target, int port,
                       bool ok);
    const std::vector<ConnectionAttempt>& connectionLog() const { return connection_log_; }

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
    LocalStorage local_storage_;
    Mailbox mailbox_;
    ChatStore chat_;
    GameState game_state_;
    Credits credits_;
    std::unique_ptr<ResponseProvider> chat_provider_;
    std::unique_ptr<EventEngine> event_engine_;
    std::unique_ptr<Quest> quest_;
    std::chrono::steady_clock::time_point started_at_;
    std::vector<ConnectionAttempt> connection_log_;
    bool quit_ = false;
};
