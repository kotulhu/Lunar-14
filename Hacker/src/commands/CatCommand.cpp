#include "commands/CatCommand.h"

#include "core/Session.h"
#include "core/VirtualFs.h"
#include "ui/Terminal.h"

void CatCommand::execute(Session& session, const World&,
                         const std::vector<std::string>& args, Terminal& term) {
    if (args.empty()) {
        term.print("cat: missing operand");
        return;
    }

    std::string full = VirtualFs::normalize(session.currentDir(), args[0]);
    const Host& host = session.host();

    if (VirtualFs::isDirectory(host, full)) {
        term.print("cat: " + args[0] + ": Is a directory");
        return;
    }

    bool ok = false;
    std::string content = VirtualFs::readFile(host, full, ok);
    if (!ok) {
        term.print("cat: " + args[0] + ": No such file or directory");
        return;
    }

    term.print(content);
}
