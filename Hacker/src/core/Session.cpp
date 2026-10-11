#include "core/Session.h"

#include <stdexcept>

#include "core/World.h"

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
