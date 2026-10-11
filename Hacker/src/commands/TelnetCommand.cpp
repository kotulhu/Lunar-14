#include "commands/TelnetCommand.h"

#include <string>

#include "commands/Login.h"
#include "core/Host.h"
#include "core/World.h"
#include "ui/Terminal.h"

void TelnetCommand::execute(Session& session, const World& world,
                            const std::vector<std::string>& args, Terminal& term) {
    if (args.empty() || args.size() > 2) {
        term.print("usage: telnet <host> [port]");
        return;
    }

    int port = 23;
    if (args.size() == 2 && !parsePort(args[1], port)) {
        term.print("usage: telnet <host> [port]");
        return;
    }

    const std::string& target = args[0];
    const Host* host = world.findHost(target);
    if (!host) {
        term.print("telnet: unknown host " + target);
        return;
    }
    if (!host->alive) {
        term.print("No route to host");
        return;
    }

    auto it = host->ports.find(port);
    if (it == host->ports.end()) {
        term.print("Connection refused");
        return;
    }

    // Порт открыт, но служба не telnet: показываем баннер и закрываемся.
    if (it->second.service != "telnet") {
        term.print(it->second.banner.empty() ? it->second.service : it->second.banner);
        term.print("Connection closed by foreign host.");
        return;
    }

    term.print("Trying " + host->ip + "...");
    term.pause(500);
    term.print("Connected to " + host->name + ".");
    term.pause(500);

    std::string login;
    if (!term.readLine("login: ", login)) {
        term.print("Connection closed by foreign host.");
        return;
    }

    if (!authenticate(*host, login, "Password: ", term)) return;
    enterShell(session, target, login, *host, term);
}
