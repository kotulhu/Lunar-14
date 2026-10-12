#include "core/Session.h"

#include <stdexcept>

#include "core/VirtualFs.h"
#include "core/World.h"

Session::Session(World& world, const std::string& host_query) : world_(&world) {
    Host* host = world.findHost(host_query);
    if (!host) {
        throw std::runtime_error("unknown host: " + host_query);
    }

    // Список известных узлов доступен на домашнем компьютере всегда.
    if (Host* home = world.findHost(world.playerHost())) {
        std::string hdir = home->start_dir.empty() ? "/" : home->start_dir;
        std::string path = (hdir == "/") ? "/visited" : hdir + "/visited";
        VirtualFs::addGeneratedFile(*home, path,
                                    [this] { return game_state_.renderVisited(); });
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
