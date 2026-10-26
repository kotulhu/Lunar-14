#include "commands/Registry.h"

#include <algorithm>

void Registry::add(std::unique_ptr<Command> command, bool explicit_only) {
    std::string key = command->name();
    if (explicit_only) explicit_only_.insert(key);
    commands_[key] = std::move(command);
}

Command* Registry::find(const std::string& name) const {
    auto it = commands_.find(name);
    return it == commands_.end() ? nullptr : it->second.get();
}

bool Registry::explicitOnly(const std::string& name) const {
    return explicit_only_.count(name) != 0;
}

std::vector<Command*> Registry::all() const {
    std::vector<Command*> result;
    result.reserve(commands_.size());
    for (const auto& kv : commands_) {
        result.push_back(kv.second.get());
    }
    std::sort(result.begin(), result.end(), [](Command* a, Command* b) {
        return a->name() < b->name();
    });
    return result;
}
