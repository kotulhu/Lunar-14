#include "core/Session.h"

#include <chrono>
#include <stdexcept>

#include "core/EventEngine.h"
#include "core/VirtualFs.h"
#include "core/World.h"

Session::~Session() = default;

void Session::setEventEngine(std::unique_ptr<EventEngine> engine) {
    if (engine) engine->bind(this);
    event_engine_ = std::move(engine);
}

void Session::setQuest(std::unique_ptr<Quest> quest) {
    quest_ = std::move(quest);
}

const QuestStage* Session::currentStage() const {
    if (!quest_ || !world_) return nullptr;
    return quest_->currentStage(game_state_, world_->playerHost());
}

std::string Session::localDownloadsDir() const {
    const std::string base =
        stack_.front().host->start_dir.empty() ? "/" : stack_.front().host->start_dir;
    return VirtualFs::normalize(base, "downloads");
}

bool Session::isLocalDownloadsDir(const std::string& path) const {
    return hostName() == stack_.front().host_name && path == localDownloadsDir();
}

bool Session::isLocalDownloadsParent(const std::string& path) const {
    if (hostName() != stack_.front().host_name) return false;
    return path == VirtualFs::normalize(localDownloadsDir(), "..");
}

bool Session::isInLocalDownloads(const std::string& path) const {
    if (hostName() != stack_.front().host_name) return false;
    const std::string dir = localDownloadsDir();
    return path == dir || path.rfind(dir + "/", 0) == 0;
}

std::vector<std::string> Session::listLocalDownloads(bool show_hidden) const {
    std::vector<std::string> names;
    for (const DownloadedFile& file : local_storage_.list()) {
        if (!show_hidden && !file.name.empty() && file.name[0] == '.') continue;
        names.push_back(file.name);
    }
    return names;
}

void Session::logConnection(const std::string& time, const std::string& service,
                            const std::string& user, const std::string& target, int port,
                            bool ok) {
    ConnectionAttempt attempt;
    attempt.time = time;
    attempt.service = service;
    attempt.user = user;
    attempt.target = target;
    attempt.port = port;
    attempt.ok = ok;
    connection_log_.push_back(std::move(attempt));
}

Session::Session(World& world, const std::string& host_query)
    : world_(&world), started_at_(std::chrono::steady_clock::now()) {
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
