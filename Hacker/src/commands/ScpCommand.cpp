#include "commands/ScpCommand.h"

#include <iomanip>
#include <sstream>
#include <string>

#include "commands/Login.h"
#include "core/Host.h"
#include "core/Session.h"
#include "core/VirtualFs.h"
#include "core/World.h"
#include "ui/Terminal.h"

namespace {

std::string formatSize(std::size_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    std::ostringstream out;
    out << std::fixed << std::setprecision(1);
    if (bytes < 1024 * 1024) {
        out << static_cast<double>(bytes) / 1024.0 << " KB";
    } else {
        out << static_cast<double>(bytes) / (1024.0 * 1024.0) << " MB";
    }
    return out.str();
}

}  // namespace

void ScpCommand::execute(Session& session, const World& world,
                         const std::vector<std::string>& args, Terminal& term) {
    const std::string usage = "usage: scp <file> | scp [-P port] user@host:path [local-dir]";

    // Упрощённая форма скачивает файл из cwd текущего хоста прямо в downloads/.
    if (args.size() == 1 && args[0].find('@') == std::string::npos) {
        const std::string& input = args[0];
        const std::string source_path = VirtualFs::normalize(session.currentDir(), input);
        if (!VirtualFs::exists(session.host(), source_path) ||
            VirtualFs::isDirectory(session.host(), source_path)) {
            term.print("scp: " + input + ": no such file");
            return;
        }
        bool ok = false;
        std::string content =
            VirtualFs::readFile(session.host(), source_path, ok, &session);
        if (!ok) {
            term.print("scp: " + input + ": no such file");
            return;
        }
        const std::size_t slash = source_path.find_last_of('/');
        const std::string name = source_path.substr(slash + 1);
        const std::string target = "downloads/" + name;
        if (session.localStorage().has(name)) {
            term.print("scp: overwriting " + target);
        }
        session.localStorage().add({name, VirtualFs::typeOf(session.host(), source_path),
                                    content, session.hostName(), source_path,
                                    VirtualFs::programIdOf(session.host(), source_path)});
        term.print("scp: " + name + " (" + formatSize(content.size()) + ") -> " + target);
        if (session.events()) session.events()->onDownload(name);
        return;
    }

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
    std::string content = VirtualFs::readFile(*host, remote_full, ok, &session);
    if (!ok) {
        term.print("scp: " + remote_path + ": No such file or directory");
        return;
    }

    std::string name = remote_full.substr(remote_full.find_last_of('/') + 1);
    if (!local_arg.empty()) {
        // Явно заданный старый VFS-каталог остаётся поддержан; стандартное
        // назначение — локальное хранилище downloads/.
        Host& local = session.localHost();
        std::string local_dir = VirtualFs::normalize(session.currentDir(), local_arg);
        if (!VirtualFs::exists(local, local_dir) || !VirtualFs::isDirectory(local, local_dir)) {
            term.print("scp: " + local_arg + ": No such directory");
            return;
        }
        std::string local_path = (local_dir == "/") ? "/" + name : local_dir + "/" + name;
        if (!VirtualFs::writeFile(local, local_path, content)) {
            term.print("scp: " + local_path + ": cannot write");
            return;
        }
    }

    const std::string target_path = "downloads/" + name;
    if (session.localStorage().has(name)) {
        term.print("scp: overwriting " + target_path);
    }
    session.localStorage().add(
        DownloadedFile{name, VirtualFs::typeOf(*host, remote_full), content,
                       host->name, remote_full,
                       VirtualFs::programIdOf(*host, remote_full)});
    term.print("scp: " + name + " (" + formatSize(content.size()) + ") -> " + target_path);
    if (session.events()) session.events()->onDownload(name);
}
