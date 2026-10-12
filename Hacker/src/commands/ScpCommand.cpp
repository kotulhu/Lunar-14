#include "commands/ScpCommand.h"

#include <string>

#include "commands/Login.h"
#include "core/Host.h"
#include "core/Session.h"
#include "core/VirtualFs.h"
#include "core/World.h"
#include "ui/Terminal.h"

void ScpCommand::execute(Session& session, const World& world,
                         const std::vector<std::string>& args, Terminal& term) {
    const std::string usage = "usage: scp [-P port] user@host:path [local-dir]";

    std::size_t i = 0;
    int port = 22;
    if (i < args.size() && args[i] == "-P") {
        if (i + 1 >= args.size() || !parsePort(args[i + 1], port)) {
            term.print(usage);
            return;
        }
        i += 2;
    }
    if (i >= args.size()) {
        term.print(usage);
        return;
    }
    std::string remote_spec = args[i++];
    std::string local_arg = (i < args.size()) ? args[i++] : "";
    if (i != args.size()) {
        term.print(usage);
        return;
    }

    // Разбор user@host:path.
    std::size_t at = remote_spec.find('@');
    if (at == std::string::npos || at == 0) {
        term.print(usage);
        return;
    }
    std::size_t colon = remote_spec.find(':', at + 1);
    if (colon == std::string::npos || colon == at + 1 || colon + 1 >= remote_spec.size()) {
        term.print(usage);
        return;
    }
    std::string user = remote_spec.substr(0, at);
    std::string host_query = remote_spec.substr(at + 1, colon - at - 1);
    std::string remote_path = remote_spec.substr(colon + 1);

    // Общие проверки и вход (как у ssh).
    const Host* host = sshConnect("scp", session, world, host_query, port, user, term);
    if (!host) return;

    // Удалённый путь: абсолютный или от start_dir хоста.
    std::string remote_full = VirtualFs::normalize(host->start_dir, remote_path);
    if (!VirtualFs::exists(*host, remote_full)) {
        term.print("scp: " + remote_path + ": No such file or directory");
        return;
    }
    if (VirtualFs::isDirectory(*host, remote_full)) {
        term.print("scp: " + remote_path + ": not a regular file");
        return;
    }
    bool ok = false;
    std::string content = VirtualFs::readFile(*host, remote_full, ok);
    if (!ok) {
        term.print("scp: " + remote_path + ": No such file or directory");
        return;
    }

    // Локальный каталог: заданный или текущий каталог сессии.
    Host& local = session.localHost();
    std::string dir_input = local_arg.empty() ? session.currentDir() : local_arg;
    std::string local_dir = VirtualFs::normalize(session.currentDir(), dir_input);
    if (!VirtualFs::exists(local, local_dir) || !VirtualFs::isDirectory(local, local_dir)) {
        term.print("scp: " + dir_input + ": No such directory");
        return;
    }

    std::string name = remote_full.substr(remote_full.find_last_of('/') + 1);
    std::string local_path = (local_dir == "/") ? "/" + name : local_dir + "/" + name;
    if (!VirtualFs::writeFile(local, local_path, content)) {
        term.print("scp: " + local_path + ": cannot write");
        return;
    }

    term.pause(200);
    term.print(name + "   100%   " + std::to_string(content.size()) + " bytes");

    session.addDownload({host->name, remote_full, local_path});
}
