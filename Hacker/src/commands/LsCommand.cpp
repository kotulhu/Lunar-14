#include "commands/LsCommand.h"

#include <string>

#include "core/Session.h"
#include "core/VirtualFs.h"
#include "ui/Terminal.h"

void LsCommand::execute(Session& session, const World&,
                        const std::vector<std::string>& args, Terminal& term) {
    // Опции (-a) можно ставить в любом порядке относительно пути.
    bool show_hidden = false;
    std::string path;
    bool have_path = false;

    for (const std::string& arg : args) {
        if (arg.size() > 1 && arg[0] == '-') {
            for (std::size_t i = 1; i < arg.size(); ++i) {
                if (arg[i] == 'a') {
                    show_hidden = true;
                } else {
                    term.print(std::string("ls: invalid option -- '") + arg[i] + "'");
                    return;
                }
            }
        } else if (!have_path) {
            path = arg;
            have_path = true;
        }
    }

    if (!have_path) path = session.currentDir();
    std::string full = VirtualFs::normalize(session.currentDir(), path);
    const Host& host = session.host();

    if (!VirtualFs::exists(host, full) || !VirtualFs::isDirectory(host, full)) {
        term.print("ls: " + path + ": No such file or directory");
        return;
    }

    std::vector<std::string> entries = VirtualFs::list(host, full, show_hidden);
    if (entries.empty()) {
        return;  // пустой каталог
    }

    std::string line;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        line += entries[i];
        if (i + 1 < entries.size()) line += "  ";
    }
    term.print(line);
}
