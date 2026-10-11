#include "commands/PingCommand.h"

#include <random>

#include "core/World.h"
#include "ui/Terminal.h"

void PingCommand::execute(Session&, const World& world,
                          const std::vector<std::string>& args, Terminal& term) {
    if (args.empty()) {
        term.print("ping: usage: ping <host>");
        return;
    }

    const std::string& target = args[0];
    const Host* host = world.findHost(target);
    if (!host) {
        term.print("ping: unknown host " + target);
        return;
    }

    // Один генератор на всю программу: задержка 20-80 мс.
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> delay(20, 80);

    if (!host->alive) {
        for (int i = 0; i < 4; ++i) {
            term.print("Request timeout for icmp_seq " + std::to_string(i));
            term.pause(500);
        }
        term.print("4 packets transmitted, 0 packets received, 100% packet loss");
        return;
    }

    for (int i = 0; i < 4; ++i) {
        int ms = delay(rng);
        term.print("64 bytes from " + host->ip + ": icmp_seq=" + std::to_string(i) +
                   " ttl=57 time=" + std::to_string(ms) + " ms");
        term.pause(500);
    }
    term.print("4 packets transmitted, 4 packets received, 0% packet loss");
}
