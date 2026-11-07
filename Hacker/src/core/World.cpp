#include "core/World.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iterator>
#include <random>
#include <set>
#include <sstream>

#include <nlohmann/json.hpp>

#include "core/LocalStorage.h"
#include "core/Session.h"
#include "core/VirtualFs.h"

using json = nlohmann::json;

namespace {

std::string joinPath(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    if (dir.back() == '/') return dir + name;
    return dir + "/" + name;
}

// "\r\n" -> "\n" и убрать конечные переводы строки.
std::string cleanText(const std::string& raw) {
    std::string text;
    text.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\r' && i + 1 < raw.size() && raw[i + 1] == '\n') continue;
        text += raw[i];
    }
    while (!text.empty() && text.back() == '\n') text.pop_back();
    return text;
}

// Путь для "$file" должен быть относительным и не содержать "..".
bool safeRelative(const std::string& rel) {
    if (rel.empty() || rel[0] == '/') return false;
    std::string segment;
    for (std::size_t i = 0; i <= rel.size(); ++i) {
        if (i == rel.size() || rel[i] == '/') {
            if (segment == "..") return false;
            segment.clear();
        } else {
            segment += rel[i];
        }
    }
    return true;
}

// Строит дерево ФС из JSON-узла. Узел может быть строкой, массивом строк,
// объектом {"$file": "путь"} или каталогом (любой другой объект).
bool parseFsNode(const json& node, const std::string& host_name, const std::string& path,
                 const std::string& texts_dir, FsNode& out, std::string& error) {
    const std::string where = "host '" + host_name + "': bad fs node at '" + path + "': ";

    if (node.is_object()) {
        // Внешний текст: {"$file": "относительный/путь.txt"}.
        if (node.contains("$file")) {
            if (node.size() != 1 || !node["$file"].is_string()) {
                error = where + "'$file' must be the only key with a string value";
                return false;
            }
            std::string rel = node["$file"].get<std::string>();
            if (!safeRelative(rel)) {
                error = where + "'$file' path must be relative and must not contain '..'";
                return false;
            }
            std::ifstream in(joinPath(texts_dir, rel), std::ios::binary);
            if (!in) {
                error = where + "cannot read text file '" + rel + "'";
                return false;
            }
            std::string raw((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
            out.is_dir = false;
            out.content = cleanText(raw);
            return true;
        }

        // Типизированный файл: {"type": "program", "program_id": "...", "content": "..."}.
        if (node.contains("content") || node.contains("type")) {
            bool extra_key = false;
            for (auto it = node.begin(); it != node.end(); ++it) {
                if (it.key() != "content" && it.key() != "type" &&
                    it.key() != "program_id") extra_key = true;
            }
            if (!node.contains("content") || extra_key ||
                (node.contains("type") &&
                 (!node["type"].is_string() ||
                  !parseFileTypeName(node["type"].get<std::string>(), out.type)))) {
                error = where + "typed file must contain 'content' and a valid optional 'type'";
                return false;
            }
            if (node.contains("program_id")) {
                if (!node["program_id"].is_string() ||
                    node["program_id"].get<std::string>().empty() ||
                    out.type != FileType::Program) {
                    error = where + "'program_id' must be a non-empty string on a program file";
                    return false;
                }
                out.program_id = node["program_id"].get<std::string>();
            }
            const json& content = node["content"];
            if (content.is_string()) {
                out.content = content.get<std::string>();
            } else if (content.is_array()) {
                for (std::size_t i = 0; i < content.size(); ++i) {
                    if (!content[i].is_string()) {
                        error = where + "typed file 'content' array must contain only strings";
                        return false;
                    }
                    if (i) out.content += "\n";
                    out.content += content[i].get<std::string>();
                }
            } else {
                error = where + "typed file 'content' must be a string or array of strings";
                return false;
            }
            out.is_dir = false;
            return true;
        }

        out.is_dir = true;
        for (auto it = node.begin(); it != node.end(); ++it) {
            const std::string& key = it.key();
            if (!key.empty() && key[0] == '$') {
                error = where + "name '" + key + "' starting with '$' is reserved";
                return false;
            }
            std::string child_path = (path == "/") ? "/" + key : path + "/" + key;
            FsNode child;
            if (!parseFsNode(it.value(), host_name, child_path, texts_dir, child, error)) {
                return false;
            }
            out.children[key] = std::move(child);
        }
        return true;
    }

    if (node.is_string()) {
        out.is_dir = false;
        out.content = node.get<std::string>();
        return true;
    }

    if (node.is_array()) {
        std::string content;
        for (std::size_t i = 0; i < node.size(); ++i) {
            if (!node[i].is_string()) {
                error = where + "array must contain only strings";
                return false;
            }
            if (i > 0) content += "\n";
            content += node[i].get<std::string>();
        }
        out.is_dir = false;
        out.content = content;
        return true;
    }

    error = where + "expected string, array of strings or object";
    return false;
}

// Разбирает один файл хоста. filename используется в сообщениях об ошибках.
bool parseHost(const json& h, const std::string& filename, const std::string& texts_dir,
               Host& host, std::string& error) {
    const std::string where = "host file '" + filename + "': ";

    if (!h.is_object()) {
        error = where + "expected a JSON object";
        return false;
    }
    if (!h.contains("name") || !h["name"].is_string()) {
        error = where + "'name' must be a string";
        return false;
    }
    host.name = h["name"].get<std::string>();
    if (host.name.empty()) {
        error = where + "'name' must not be empty";
        return false;
    }
    if (!h.contains("ip") || !h["ip"].is_string()) {
        error = where + "'ip' must be a string";
        return false;
    }
    host.ip = h["ip"].get<std::string>();

    if (h.contains("alive")) {
        if (!h["alive"].is_boolean()) {
            error = "host '" + host.name + "': 'alive' must be a boolean";
            return false;
        }
        host.alive = h["alive"].get<bool>();
    }

    if (h.contains("start_dir")) {
        if (!h["start_dir"].is_string()) {
            error = "host '" + host.name + "': 'start_dir' must be a string";
            return false;
        }
        host.start_dir = h["start_dir"].get<std::string>();
    }
    if (host.start_dir.empty()) host.start_dir = "/";

    // Охват узла: необязательное целое >= 0.
    if (h.contains("reach")) {
        if (!h["reach"].is_number_integer()) {
            error = where + "'reach' must be an integer";
            return false;
        }
        int reach = h["reach"].get<int>();
        if (reach < 0) {
            error = where + "'reach' must not be negative";
            return false;
        }
        host.reach = reach;
    }

    // Порты заданы объектом {"22": {"service": "ssh", ...}, ...}.
    if (h.contains("ports")) {
        const json& ports = h["ports"];
        if (!ports.is_object()) {
            error = "host '" + host.name + "': 'ports' must be an object";
            return false;
        }
        for (auto it = ports.begin(); it != ports.end(); ++it) {
            int port = 0;
            try {
                port = std::stoi(it.key());
            } catch (const std::exception&) {
                error = "host '" + host.name + "': bad port key '" + it.key() + "'";
                return false;
            }
            const json& value = it.value();
            if (!value.is_object() || !value.contains("service") ||
                !value["service"].is_string()) {
                error = "host '" + host.name + "': bad port '" + it.key() +
                        "': expected object with string 'service'";
                return false;
            }
            PortInfo info;
            info.service = value["service"].get<std::string>();
            if (value.contains("banner")) {
                if (!value["banner"].is_string()) {
                    error = "host '" + host.name + "': bad port '" + it.key() +
                            "': 'banner' must be a string";
                    return false;
                }
                info.banner = value["banner"].get<std::string>();
            }
            host.ports[port] = std::move(info);
        }
    }

    // Аккаунты заданы массивом пар: [["user", "pass"], ...].
    if (h.contains("accounts")) {
        const json& accounts = h["accounts"];
        if (!accounts.is_array()) {
            error = "host '" + host.name + "': 'accounts' must be an array";
            return false;
        }
        for (const auto& acc : accounts) {
            if (!acc.is_array() || acc.size() != 2 || !acc[0].is_string() ||
                !acc[1].is_string()) {
                error = "host '" + host.name +
                        "': 'accounts' entries must be [\"user\", \"pass\"]";
                return false;
            }
            host.accounts.emplace_back(acc[0].get<std::string>(), acc[1].get<std::string>());
        }
    }

    // Дерево файловой системы (см. parseFsNode).
    if (h.contains("fs")) {
        if (!h["fs"].is_object()) {
            error = "host '" + host.name + "': 'fs' must be an object";
            return false;
        }
        if (!parseFsNode(h["fs"], host.name, "/", texts_dir, host.root, error)) {
            return false;
        }
    }

    // Белый список команд: "*" — все, массив — конкретные имена,
    // отсутствие поля — базовый набор по умолчанию.
    if (h.contains("commands")) {
        const json& c = h["commands"];
        if (c.is_string()) {
            if (c.get<std::string>() != "*") {
                error = "host '" + host.name + "': 'commands' string must be \"*\"";
                return false;
            }
            host.all_commands = true;
        } else if (c.is_array()) {
            for (const auto& item : c) {
                if (!item.is_string()) {
                    error = "host '" + host.name +
                            "': 'commands' array must contain only strings";
                    return false;
                }
                host.commands.push_back(item.get<std::string>());
            }
        } else {
            error = "host '" + host.name +
                    "': 'commands' must be \"*\" or an array of strings";
            return false;
        }
    } else {
        host.commands = {"ls", "cat", "cd", "pwd"};
    }

    return true;
}

bool addGenerated(World& world, const char* host_name, const char* path,
                  std::function<std::string(Session*)> generate, std::string& error) {
    Host* host = world.findHost(host_name);
    if (!host) {
        error = std::string("generated file: host '") + host_name + "' not found";
        return false;
    }
    if (!VirtualFs::addGeneratedFile(*host, path, std::move(generate))) {
        error = std::string("host '") + host_name + "': cannot initialize generated " + path;
        return false;
    }
    return true;
}

std::string formatAuthLog(Session* session) {
    if (!session || session->connectionLog().empty()) {
        return "# /var/log/auth.log";
    }
    std::string out;
    for (const auto& attempt : session->connectionLog()) {
        if (!out.empty()) out += "\n";
        out += attempt.time + " " + attempt.service + " ";
        if (!attempt.user.empty()) out += attempt.user + "@";
        out += attempt.target;
        if (attempt.port > 0) out += ":" + std::to_string(attempt.port);
        out += attempt.ok ? " accepted" : " failed";
    }
    return out;
}

std::string formatUptime(Session* session) {
    double seconds = 0.0;
    if (session) {
        using clock = std::chrono::steady_clock;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      clock::now() - session->startedAt())
                      .count();
        seconds = static_cast<double>(ms) / 1000.0;
    }
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << seconds << " 0.00";
    return out.str();
}

const char* ufoQuote() {
    static const char* quotes[] = {
        "The truth is out there. The disk space is not.",
        "I want to believe. I also want a faster modem.",
        "Trust no one. Especially not root.",
        "This BBS has been expecting you.",
        "We are not alone. Check /posts.",
        "Houston, we have a login.",
    };
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 5);
    return quotes[dist(rng)];
}

}  // namespace

bool World::load(const std::string& data_dir_in, std::string& error) {
    std::string data_dir = data_dir_in;
    while (data_dir.size() > 1 && data_dir.back() == '/') data_dir.pop_back();

    player_host_.clear();
    data_dir_ = data_dir;
    hosts_.clear();

    // world.json — имя домашнего хоста игрока.
    std::string world_path = joinPath(data_dir, "world.json");
    std::ifstream win(world_path);
    if (!win) {
        error = "cannot open file: " + world_path;
        return false;
    }
    json wroot;
    try {
        win >> wroot;
    } catch (const std::exception& e) {
        error = "world.json: invalid JSON: " + std::string(e.what());
        return false;
    }
    if (!wroot.is_object() || !wroot.contains("player_host")) {
        error = "world.json: 'player_host' is required";
        return false;
    }
    if (!wroot["player_host"].is_string()) {
        error = "world.json: 'player_host' must be a string";
        return false;
    }
    player_host_ = wroot["player_host"].get<std::string>();

    namespace fs = std::filesystem;
    std::string hosts_dir = joinPath(data_dir, "hosts");
    std::string texts_dir = joinPath(data_dir, "texts");

    std::error_code ec;
    if (!fs::is_directory(hosts_dir, ec)) {
        error = "cannot open hosts directory: " + hosts_dir;
        return false;
    }

    // Файлы хостов читаем в алфавитном порядке.
    std::vector<std::string> files;
    for (const auto& entry : fs::directory_iterator(hosts_dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;
        files.push_back(entry.path().string());
    }
    std::sort(files.begin(), files.end());

    std::set<std::string> ips;
    std::set<std::string> names;
    for (const std::string& file : files) {
        std::ifstream in(file);
        if (!in) {
            error = "cannot open file: " + file;
            return false;
        }
        json root;
        try {
            in >> root;
        } catch (const std::exception& e) {
            error = "host file '" + file + "': invalid JSON: " + std::string(e.what());
            return false;
        }

        std::string stem = fs::path(file).stem().string();
        Host host;
        if (!parseHost(root, file, texts_dir, host, error)) return false;
        if (host.name == "downloads") {
            error = "host name 'downloads' is reserved for local storage";
            return false;
        }
        if (host.name != stem) {
            error = "host file '" + file + "': name '" + host.name +
                    "' does not match file name '" + stem + "'";
            return false;
        }
        if (!ips.insert(host.ip).second) {
            error = "duplicate host ip '" + host.ip + "' in file '" + file + "'";
            return false;
        }
        if (!names.insert(host.name).second) {
            error = "duplicate host name '" + host.name + "' in file '" + file + "'";
            return false;
        }
        hosts_.push_back(std::move(host));
    }

    if (hosts_.empty()) {
        error = "no hosts found in " + hosts_dir;
        return false;
    }

    if (findHost(player_host_) == nullptr) {
        error = "player_host '" + player_host_ + "' not found among hosts";
        return false;
    }

    if (!addGenerated(*this, "home-pc", "/var/log/auth.log",
                      [](Session* s) { return formatAuthLog(s); }, error)) {
        return false;
    }
    if (!addGenerated(*this, "home-pc", "/home/hacker/.bash_history",
                      [](Session*) {
                          return std::string(
                              "ls -la\n"
                              "cat readme.txt\n"
                              "sudo rm -rf /\n"
                              "# не смейся, я один раз нажал\n"
                              "ping 127.0.0.1\n"
                              "telnet 203.0.113.5");
                      },
                      error)) {
        return false;
    }
    if (!addGenerated(*this, "ftp.jsc-files", "/etc/motd",
                      [](Session*) { return std::string("Welcome to JSC-FTP, since 1994"); },
                      error)) {
        return false;
    }
    if (!addGenerated(*this, "relay-hub", "/proc/uptime",
                      [](Session* s) { return formatUptime(s); }, error)) {
        return false;
    }
    if (!addGenerated(*this, "ufo-bbs", "/etc/motd",
                      [](Session*) { return std::string(ufoQuote()); }, error)) {
        return false;
    }

    return true;
}

const Host* World::findHost(const std::string& query) const {
    for (const auto& host : hosts_) {
        if (host.ip == query || host.name == query) {
            return &host;
        }
    }
    return nullptr;
}

Host* World::findHost(const std::string& query) {
    for (auto& host : hosts_) {
        if (host.ip == query || host.name == query) {
            return &host;
        }
    }
    return nullptr;
}
