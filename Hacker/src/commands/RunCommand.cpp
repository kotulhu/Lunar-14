#include "commands/RunCommand.h"

#include <cstddef>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/LocalStorage.h"
#include "core/Session.h"
#include "core/VirtualFs.h"
#include "core/World.h"
#include "ui/Terminal.h"

using json = nlohmann::json;

namespace {

// Найденный файл: либо в локальном downloads/, либо в ФС текущего хоста.
struct ResolvedFile {
    enum class Status { NotFound, Directory, File };

    Status status = Status::NotFound;
    bool local = false;
    std::string name;
    FileType type = FileType::Text;
    std::string content;
    std::string programId;
};

// Конфигурация mailsend из data/mailsend.json. Тексты меняются без пересборки.
struct RecipientGroup {
    std::string name;
    std::vector<std::string> items;
};

struct CreditEntry {
    std::string role;
    std::string name;
};

struct MailsendConfig {
    int delay_ms = 200;
    std::string recipients_title = "СПИСОК ПОЛУЧАТЕЛЕЙ";
    std::vector<RecipientGroup> groups;
    std::string credits_title = "ТИТРЫ";
    std::vector<CreditEntry> credits;
    std::vector<std::string> custom_text;
};

std::string baseName(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string joinPath(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    if (dir.back() == '/') return dir + name;
    return dir + "/" + name;
}

ResolvedFile fromLocal(const DownloadedFile& file) {
    ResolvedFile resolved;
    resolved.status = ResolvedFile::Status::File;
    resolved.local = true;
    resolved.name = file.name;
    resolved.type = file.type;
    resolved.content = file.content;
    resolved.programId = file.programId;
    return resolved;
}

// Ищет файл по аргументу команды. Сначала downloads/ и текущая ФС, затем
// (для простого имени) локальное хранилище — так `run mailsend` работает
// с любого хоста, пока программа лежит в downloads/.
ResolvedFile resolveFile(Session& session, const std::string& arg) {
    ResolvedFile resolved;
    const std::string full = VirtualFs::normalize(session.currentDir(), arg);

    if (session.isLocalDownloadsDir(full)) {
        resolved.status = ResolvedFile::Status::Directory;
        return resolved;
    }

    if (VirtualFs::isReservedPath(full) || session.isInLocalDownloads(full)) {
        const DownloadedFile* file = session.localStorage().get(baseName(full));
        if (file) return fromLocal(*file);
        return resolved;
    }

    if (VirtualFs::exists(session.host(), full)) {
        if (VirtualFs::isDirectory(session.host(), full)) {
            resolved.status = ResolvedFile::Status::Directory;
            return resolved;
        }
        bool ok = false;
        std::string content = VirtualFs::readFile(session.host(), full, ok, &session);
        if (ok) {
            resolved.status = ResolvedFile::Status::File;
            resolved.name = baseName(full);
            resolved.type = VirtualFs::typeOf(session.host(), full);
            resolved.content = std::move(content);
            resolved.programId = VirtualFs::programIdOf(session.host(), full);
            return resolved;
        }
    }

    if (arg.find('/') == std::string::npos) {
        const DownloadedFile* file = session.localStorage().get(arg);
        if (file) return fromLocal(*file);
    }
    return resolved;
}

void printUsage(Terminal& term) {
    term.print("usage: run <program> [data-file]");
}

void runDiagnostic(Session& session, Terminal& term) {
    term.print("diagnostic utility");
    term.print("host: " + session.hostName() + " (" + session.hostIp() + ")");
    term.print("user: " + session.user());
    term.print("cwd:  " + session.currentDir());
    term.print("status: ok");
}

// Ширина строки в колонках (UTF-8: считаем стартовые байты).
std::size_t utf8Width(const std::string& text) {
    std::size_t width = 0;
    for (unsigned char c : text) {
        if ((c & 0xC0) != 0x80) ++width;
    }
    return width;
}

// Левая часть, точки-заполнитель и правая часть до ширины 60 колонок.
std::string dotted(const std::string& left, const std::string& right) {
    const std::size_t width = 60;
    const std::size_t wl = utf8Width(left);
    const std::size_t wr = utf8Width(right);
    if (wl + wr + 1 > width) return left + " " + right;
    return left + std::string(width - wl - wr, '.') + right;
}

bool requireString(const json& node, const char* key, const std::string& where,
                   std::string& out, std::string& error) {
    if (!node.contains(key) || !node[key].is_string()) {
        error = where + "'" + key + "' must be a string";
        return false;
    }
    out = node[key].get<std::string>();
    return true;
}

bool loadMailsendConfig(const std::string& path, MailsendConfig& config,
                        std::string& error) {
    std::ifstream in(path);
    if (!in) {
        error = "cannot open file: " + path;
        return false;
    }

    json root;
    try {
        in >> root;
    } catch (const std::exception& e) {
        error = "mailsend.json: invalid JSON: " + std::string(e.what());
        return false;
    }
    if (!root.is_object()) {
        error = "mailsend.json: expected an object";
        return false;
    }

    if (root.contains("delay_ms")) {
        if (!root["delay_ms"].is_number_integer() ||
            root["delay_ms"].get<int>() < 0) {
            error = "mailsend.json: 'delay_ms' must be a non-negative integer";
            return false;
        }
        config.delay_ms = root["delay_ms"].get<int>();
    }

    if (root.contains("recipients_title")) {
        if (!requireString(root, "recipients_title", "mailsend.json: ",
                           config.recipients_title, error)) {
            return false;
        }
    }

    if (!root.contains("recipients") || !root["recipients"].is_array()) {
        error = "mailsend.json: 'recipients' must be an array";
        return false;
    }
    for (std::size_t i = 0; i < root["recipients"].size(); ++i) {
        const json& node = root["recipients"][i];
        const std::string where =
            "mailsend.json recipients[" + std::to_string(i) + "]: ";
        if (!node.is_object()) {
            error = where + "expected an object";
            return false;
        }

        RecipientGroup group;
        if (!requireString(node, "name", where, group.name, error)) return false;
        if (!node.contains("items") || !node["items"].is_array()) {
            error = where + "'items' must be an array";
            return false;
        }
        for (const json& item : node["items"]) {
            if (!item.is_string()) {
                error = where + "every item must be a string";
                return false;
            }
            group.items.push_back(item.get<std::string>());
        }
        config.groups.push_back(std::move(group));
    }

    if (root.contains("credits_title")) {
        if (!requireString(root, "credits_title", "mailsend.json: ",
                           config.credits_title, error)) {
            return false;
        }
    }

    if (!root.contains("credits") || !root["credits"].is_array()) {
        error = "mailsend.json: 'credits' must be an array";
        return false;
    }
    for (std::size_t i = 0; i < root["credits"].size(); ++i) {
        const json& node = root["credits"][i];
        const std::string where =
            "mailsend.json credits[" + std::to_string(i) + "]: ";
        if (!node.is_object()) {
            error = where + "expected an object";
            return false;
        }

        CreditEntry credit;
        if (!requireString(node, "role", where, credit.role, error)) return false;
        if (node.contains("name")) {
            if (!node["name"].is_string()) {
                error = where + "'name' must be a string";
                return false;
            }
            credit.name = node["name"].get<std::string>();
        }
        config.credits.push_back(std::move(credit));
    }

    if (root.contains("custom_text")) {
        if (!root["custom_text"].is_array()) {
            error = "mailsend.json: 'custom_text' must be an array";
            return false;
        }
        for (const json& line : root["custom_text"]) {
            if (!line.is_string()) {
                error = "mailsend.json: every custom_text line must be a string";
                return false;
            }
            config.custom_text.push_back(line.get<std::string>());
        }
    }

    return true;
}

void printRecipients(const MailsendConfig& config, Terminal& term) {
    term.print(config.recipients_title);

    std::size_t total = 0;
    for (const RecipientGroup& group : config.groups) {
        term.print("  " + group.name);
        for (const std::string& item : group.items) {
            term.print("    " + item);
            ++total;
        }
        term.pause(config.delay_ms);
    }

    term.print("");
    term.print("  Получателей: " + std::to_string(total));
}

void printCredits(const MailsendConfig& config, Terminal& term) {
    term.print(config.credits_title);
    for (const CreditEntry& credit : config.credits) {
        if (credit.name.empty()) {
            term.print("  " + credit.role);
        } else {
            term.print(dotted("  " + credit.role, credit.name));
        }
        term.pause(config.delay_ms);
    }

    if (!config.custom_text.empty()) {
        term.print("");
        for (const std::string& line : config.custom_text) {
            term.print("  " + line);
            term.pause(config.delay_ms);
        }
    }
}

// Локальный запуск: программа сама рассылает data-файл и показывает финал.
void runLocalMailsend(Session& session, const World& world,
                      const ResolvedFile& data, Terminal& term) {
    MailsendConfig config;
    std::string error;
    if (!loadMailsendConfig(joinPath(world.dataDir(), "mailsend.json"), config,
                            error)) {
        term.print("mailsend: cannot load configuration: " + error);
        return;
    }

    term.print("JSC Mail Dispatch Utility 1.0");
    term.print("Data file: " + data.name + " (" +
               std::to_string(data.content.size()) + " B)");
    term.print("Mode: local dispatch");
    term.pause(config.delay_ms);
    term.print("Preparing message... done");
    term.print("");

    printRecipients(config, term);

    term.print("");
    term.print("Sending... done");
    term.pause(config.delay_ms);
    term.print("");

    printCredits(config, term);

    term.print("");
    term.print("THE END");
    session.gameState().setFinished(true);
}

// Запуск на ретрансляторе: payload загружается, финал запускает broadcast.
void runRelayMailsend(Session& session, const ResolvedFile& data, Terminal& term) {
    if (session.gameState().payloadReady()) {
        term.print("mailsend: payload is already staged on relay-hub");
        return;
    }

    term.print("JSC Mail Dispatch Utility 1.0");
    term.print("Data file: " + data.name + " (" +
               std::to_string(data.content.size()) + " B)");
    term.print("Relay: " + session.hostName() + " (" + session.hostIp() + ")");
    term.pause(400);
    term.print("Uploading payload... done");
    session.gameState().setPayloadReady(true);
    term.print("Payload staged. Run 'broadcast' on this relay to send it.");
}

void runMailsend(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) {
    if (args.size() < 2) {
        term.print("mailsend: missing data file");
        printUsage(term);
        return;
    }
    if (args.size() > 2) {
        term.print("mailsend: too many arguments");
        printUsage(term);
        return;
    }

    ResolvedFile data = resolveFile(session, args[1]);
    if (data.status == ResolvedFile::Status::NotFound) {
        term.print("mailsend: " + args[1] + ": no such file");
        return;
    }
    if (data.status == ResolvedFile::Status::Directory) {
        term.print("mailsend: " + args[1] + ": Is a directory");
        return;
    }
    if (data.type != FileType::Data) {
        term.print("mailsend: " + args[1] + ": expected a data file");
        return;
    }

    if (session.hostName() == "relay-hub") {
        runRelayMailsend(session, data, term);
        return;
    }
    runLocalMailsend(session, world, data, term);
}

}  // namespace

void RunCommand::execute(Session& session, const World& world,
                         const std::vector<std::string>& args, Terminal& term) {
    if (args.empty()) {
        term.print("run: missing program");
        printUsage(term);
        return;
    }

    ResolvedFile program = resolveFile(session, args[0]);
    if (program.status == ResolvedFile::Status::NotFound) {
        term.print("run: " + args[0] + ": no such file");
        return;
    }
    if (program.status == ResolvedFile::Status::Directory) {
        term.print("run: " + args[0] + ": Is a directory");
        return;
    }
    if (program.type != FileType::Program) {
        term.print("run: " + args[0] + ": not an executable program");
        return;
    }

    const std::string id = program.programId.empty() ? program.name : program.programId;
    if (id == "mailsend") {
        runMailsend(session, world, args, term);
        return;
    }
    if (id == "diagnostic" || program.name == ".diagnostic") {
        runDiagnostic(session, term);
        return;
    }

    term.print("run: " + args[0] + ": unsupported program '" + id + "'");
}
