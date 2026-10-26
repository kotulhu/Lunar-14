#include "core/GameState.h"

#include <algorithm>

void GameState::record(const std::string& host, int port, const std::string& user,
                       const std::string& pass) {
    for (VisitedEntry& e : entries_) {
        if (e.host == host && e.port == port && e.user == user) {
            e.pass = pass;  // дубль не создаём, пароль обновляем
            return;
        }
    }
    entries_.push_back({host, port, user, pass});
}

bool GameState::hasVisited(const std::string& host) const {
    for (const VisitedEntry& e : entries_) {
        if (e.host == host) return true;
    }
    return false;
}

const VisitedEntry* GameState::find(const std::string& host, int port, const std::string& user) const {
    for (const VisitedEntry& e : entries_) {
        if (e.host == host && e.port == port && e.user == user) return &e;
    }
    return nullptr;
}

bool GameState::addJournal(JournalEntry entry) {
    for (const JournalEntry& e : journal_) {
        if (e.key == entry.key) return false;
    }
    journal_.push_back(std::move(entry));
    return true;
}

const VisitedEntry* GameState::latestFor(const std::string& host, int port) const {
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (it->host == host && it->port == port) return &*it;
    }
    return nullptr;
}


int GameState::distinctHosts(const std::string& exclude) const {
    std::vector<std::string> seen;
    for (const VisitedEntry& e : entries_) {
        if (e.host == exclude) continue;
        if (std::find(seen.begin(), seen.end(), e.host) == seen.end()) {
            seen.push_back(e.host);
        }
    }
    return static_cast<int>(seen.size());
}

void GameState::markEventFired(const std::string& id) {
    if (!eventFired(id)) fired_events_.push_back(id);
}

bool GameState::eventFired(const std::string& id) const {
    return std::find(fired_events_.begin(), fired_events_.end(), id) !=
           fired_events_.end();
}
