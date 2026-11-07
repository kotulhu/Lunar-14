#include "commands/CatCommand.h"

#include "core/Notes.h"
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

    if (session.isLocalDownloadsDir(full)) {
        term.print("cat: " + args[0] + ": Is a directory");
        return;
    }

    if (VirtualFs::isReservedPath(full)) {
        std::size_t slash = full.find_last_of('/');
        std::string name = slash == std::string::npos ? full : full.substr(slash + 1);
        const DownloadedFile* file = session.localStorage().get(name);
        if (!file) {
            term.print("cat: " + args[0] + ": no such file");
            return;
        }
        if (file->type != FileType::Text && file->type != FileType::Data) {
            term.print("cat: " + args[0] + ": binary file, use 'run'");
            return;
        }
        term.print(file->content);
        noteDocument(session, "download", file->sourceHost, file->name,
                     "download:" + file->name, file->content);
        return;
    }

    if (VirtualFs::isDirectory(host, full)) {
        term.print("cat: " + args[0] + ": Is a directory");
        return;
    }

    FileType type = VirtualFs::typeOf(host, full);
    if (type != FileType::Text && type != FileType::Data) {
        term.print("cat: " + args[0] + ": binary file, use 'run'");
        return;
    }

    bool ok = false;
    std::string content = VirtualFs::readFile(host, full, ok, &session);
    if (!ok) {
        if (session.hostName() == "dead-host" && full == "/var/spool/mail/root") {
            term.print("cat: " + args[0] +
                       ": No such file or directory (operator: 555-0147)");
            return;
        }
        term.print("cat: " + args[0] + ": No such file or directory");
        return;
    }

    term.print(content);
    noteDocument(session, "file", session.hostName(), full,
                 "file:" + session.hostName() + ":" + full, content);
}
