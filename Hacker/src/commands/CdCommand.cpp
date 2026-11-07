#include "commands/CdCommand.h"

#include "core/Session.h"
#include "core/VirtualFs.h"
#include "ui/Terminal.h"

void CdCommand::execute(Session& session, const World&,
                        const std::vector<std::string>& args, Terminal& term) {
    // Без аргумента идём в стартовый каталог хоста.
    std::string target = args.empty() ? session.host().start_dir : args[0];
    if (target.empty()) target = "/";

    std::string full = VirtualFs::normalize(session.currentDir(), target);
    const Host& host = session.host();

    // downloads/ существует только на домашнем хосте и только виртуально.
    if (session.isLocalDownloadsDir(full)) {
        session.setCurrentDir(full);
        return;
    }
    if (session.isInLocalDownloads(full)) {
        term.print("cd: " + target + ": Not a directory");
        return;
    }

    if (!VirtualFs::exists(host, full)) {
        term.print("cd: " + target + ": No such file or directory");
        return;
    }
    if (!VirtualFs::isDirectory(host, full)) {
        term.print("cd: " + target + ": Not a directory");
        return;
    }

    session.setCurrentDir(full);
}
