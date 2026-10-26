#include "commands/SshCommand.h"

#include <string>

#include "commands/Login.h"
#include "core/Host.h"
#include "core/Notes.h"
#include "core/World.h"
#include "ui/Terminal.h"

void SshCommand::execute(Session& session, const World& world,
                         const std::vector<std::string>& args, Terminal& term) {
    const std::string usage = "usage: ssh [-p port] user@host";

    int port = 22;
    std::string target;
    if (!args.empty() && args[0] == "-p") {
        if (args.size() != 3 || !parsePort(args[1], port)) {
            term.print(usage);
            return;
        }
        target = args[2];
    } else {
        if (args.size() != 1) {
            term.print(usage);
            return;
        }
        target = args[0];
    }

    std::size_t at = target.find('@');
    if (at == std::string::npos || at == 0 || at + 1 >= target.size()) {
        term.print(usage);
        return;
    }
    std::string user = target.substr(0, at);
    std::string query = target.substr(at + 1);

    const Host* host = sshConnect("ssh", session, world, query, port, user, term);
    if (!host) return;

    noteVisit(session, host->name, port, user);
    enterShell(session, query, user, *host, term);
}
