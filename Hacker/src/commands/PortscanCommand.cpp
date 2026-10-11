#include "commands/PortscanCommand.h"

#include <exception>
#include <string>
#include <vector>

#include "core/Host.h"
#include "core/World.h"
#include "ui/Terminal.h"

namespace {

// Разбирает диапазон "a-b" или одиночный порт "a". true при корректности.
bool parseRange(const std::string& text, int& start, int& end) {
    std::size_t dash = text.find('-');
    try {
        if (dash == std::string::npos) {
            start = end = std::stoi(text);
        } else {
            start = std::stoi(text.substr(0, dash));
            end = std::stoi(text.substr(dash + 1));
        }
    } catch (const std::exception&) {
        return false;
    }
    return start >= 1 && end <= 65535 && start <= end;
}

}  // namespace

void PortscanCommand::execute(Session&, const World& world,
                              const std::vector<std::string>& args, Terminal& term) {
    if (args.empty() || args.size() > 2) {
        term.print("usage: portscan <host> [start-end]");
        return;
    }

    const std::string& target = args[0];
    const Host* host = world.findHost(target);
    if (!host) {
        term.print("portscan: unknown host " + target);
        return;
    }

    int start = 1;
    int end = 1024;
    if (args.size() == 2 && !parseRange(args[1], start, end)) {
        term.print("usage: portscan <host> [start-end]");
        return;
    }

    if (!host->alive) {
        term.pause(500);
        term.print("Host seems down.");
        return;
    }

    term.print("Starting portscan " + host->ip + " ports " + std::to_string(start) + "-" +
               std::to_string(end));

    // Прогресс: ~две дюжины точек, суммарно от ~0.6 с до ~3 с в зависимости от диапазона.
    long long count = static_cast<long long>(end) - start + 1;
    int total_ms = 600 + static_cast<int>(2400LL * (count - 1) / 65534);
    if (total_ms > 3000) total_ms = 3000;
    const int steps = 24;
    for (int i = 0; i < steps; ++i) {
        term.printInline(".");
        term.pause(total_ms / steps);
    }
    term.print("");

    std::vector<int> open;
    for (int port = start; port <= end; ++port) {
        if (host->ports.count(port)) open.push_back(port);
    }

    if (open.empty()) {
        term.print("No open ports found");
        return;
    }

    for (int port : open) {
        const PortInfo& info = host->ports.at(port);
        std::string line = std::to_string(port) + "/tcp   open  " + info.service;
        if (!info.banner.empty()) line += "  " + info.banner;
        term.print(line);
    }
}
