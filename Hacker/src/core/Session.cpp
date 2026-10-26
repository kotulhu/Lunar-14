#include "core/Session.h"

#include <stdexcept>

#include "core/EventEngine.h"
#include "core/World.h"

Session::~Session() = default;

void Session::setEventEngine(std::unique_ptr<EventEngine> engine) {
    event_engine_ = std::move(engine);
}

void Session::setQuest(std::unique_ptr<Quest> quest) {
    quest_ = std::move(quest);
}

const QuestStage* Session::currentStage() const {
    if (!quest_ || !world_) return nullptr;
    return quest_->currentStage(game_state_, world_->playerHost());
}

Session::Session(World& world, const std::string& host_query) : world_(&world) {
    Host* host = world.findHost(host_query);
    if (!host) {
        throw std::runtime_error("unknown host: " + host_query);
    }

    // Если аккаунтов нет, играем как guest.
    std::string user = host->accounts.empty() ? "guest" : host->accounts.front().first;
    std::string dir = host->start_dir.empty() ? "/" : host->start_dir;
    push(host_query, user, dir);
}

void Session::push(const std::string& host_query, const std::string& user,
                   const std::string& dir) {
    Host* host = world_->findHost(host_query);
    if (!host) return;

    Frame frame;
    frame.host = host;
    frame.host_name = host->name;
    frame.host_ip = host->ip;
    frame.user = user;
    frame.dir = dir.empty() ? "/" : dir;
    stack_.push_back(std::move(frame));
}

bool Session::pop() {
    if (stack_.size() > 1) {
        stack_.pop_back();
    }
    return stack_.size() > 1;
}
