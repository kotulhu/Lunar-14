#include "core/Mentions.h"

#include "core/World.h"

std::vector<std::string> mentionedHosts(const World& world, const std::string& text,
                                        const std::string& exclude) {
    std::vector<std::string> found;
    for (const Host& host : world.hosts()) {
        if (host.name == exclude) continue;
        if (text.find(host.name) != std::string::npos ||
            text.find(host.ip) != std::string::npos) {
            found.push_back(host.name);
        }
    }
    return found;
}
