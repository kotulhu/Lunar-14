#include "core/World.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>

#include <nlohmann/json.hpp>

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

}  // namespace

bool World::load(const std::string& data_dir_in, std::string& error) {
    std::string data_dir = data_dir_in;
    while (data_dir.size() > 1 && data_dir.back() == '/') data_dir.pop_back();

    player_host_.clear();
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
