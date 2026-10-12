#include "core/VirtualFs.h"

#include <set>

#include "core/Host.h"

namespace {

// Склеивает текущий каталог и путь, если путь относительный.
std::string join(const std::string& base, const std::string& path) {
    if (!path.empty() && path[0] == '/') return path;
    if (base.empty() || base == "/") return "/" + path;
    return base + "/" + path;
}

// Ищет узел по уже нормализованному абсолютному пути. nullptr — не найдено.
const FsNode* resolve(const Host& host, const std::string& path) {
    if (path.empty() || path[0] != '/') return nullptr;
    if (path == "/") return &host.root;

    const FsNode* current = &host.root;
    std::string segment;
    for (size_t i = 1; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            if (!segment.empty()) {
                if (!current->is_dir) return nullptr;
                auto it = current->children.find(segment);
                if (it == current->children.end()) return nullptr;
                current = &it->second;
                segment.clear();
            }
        } else {
            segment += path[i];
        }
    }
    return current;
}

// Изменяемая версия resolve для операций записи.
FsNode* resolveMutable(Host& host, const std::string& path) {
    if (path.empty() || path[0] != '/') return nullptr;
    if (path == "/") return &host.root;

    FsNode* current = &host.root;
    std::string segment;
    for (size_t i = 1; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            if (!segment.empty()) {
                if (!current->is_dir) return nullptr;
                auto it = current->children.find(segment);
                if (it == current->children.end()) return nullptr;
                current = &it->second;
                segment.clear();
            }
        } else {
            segment += path[i];
        }
    }
    return current;
}

// Слой пользовательских правок: имя хоста -> (путь -> содержимое).
std::map<std::string, std::map<std::string, std::string>>& changeLayer() {
    static std::map<std::string, std::map<std::string, std::string>> layer;
    return layer;
}

// Содержимое правки по пути или nullptr, если правки нет.
const std::string* overlayLookup(const Host& host, const std::string& path) {
    auto host_it = changeLayer().find(host.name);
    if (host_it == changeLayer().end()) return nullptr;
    auto it = host_it->second.find(path);
    return it == host_it->second.end() ? nullptr : &it->second;
}

// Родительский каталог абсолютного пути ("/" для верхнего уровня).
std::string parentDir(const std::string& path) {
    std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos || slash == 0) return "/";
    return path.substr(0, slash);
}

}  // namespace

std::string VirtualFs::normalize(const std::string& current_dir, const std::string& path) {
    std::string combined = join(current_dir, path);

    // Разбираем путь на сегменты, обрабатывая "." и "..".
    std::vector<std::string> parts;
    std::string segment;
    for (size_t i = 0; i <= combined.size(); ++i) {
        if (i == combined.size() || combined[i] == '/') {
            if (segment.empty() || segment == ".") {
                // ничего не делаем
            } else if (segment == "..") {
                if (!parts.empty()) parts.pop_back();
            } else {
                parts.push_back(segment);
            }
            segment.clear();
        } else {
            segment += combined[i];
        }
    }

    std::string result = "/";
    for (size_t i = 0; i < parts.size(); ++i) {
        result += parts[i];
        if (i + 1 < parts.size()) result += "/";
    }
    return result;
}

std::vector<std::string> VirtualFs::list(const Host& host, const std::string& dir,
                                         bool show_hidden) {
    const FsNode* node = resolve(host, dir);
    if (!node || !node->is_dir) return {};

    std::set<std::string> entries;  // set сразу даёт сортировку и уникальность
    for (const auto& kv : node->children) {
        if (!show_hidden && !kv.first.empty() && kv.first[0] == '.') continue;
        entries.insert(kv.second.is_dir ? kv.first + "/" : kv.first);
    }

    // Файлы, созданные игроком, лежат в слое правок и добавляются сюда же.
    auto host_it = changeLayer().find(host.name);
    if (host_it != changeLayer().end()) {
        for (const auto& kv : host_it->second) {
            if (parentDir(kv.first) != dir) continue;
            std::string name = kv.first.substr(kv.first.find_last_of('/') + 1);
            if (name.empty()) continue;
            if (!show_hidden && name[0] == '.') continue;
            entries.insert(name);
        }
    }

    return std::vector<std::string>(entries.begin(), entries.end());
}

bool VirtualFs::isDirectory(const Host& host, const std::string& path) {
    const FsNode* node = resolve(host, path);
    return node && node->is_dir;
}

bool VirtualFs::exists(const Host& host, const std::string& path) {
    if (overlayLookup(host, path)) return true;
    return resolve(host, path) != nullptr;
}

std::string VirtualFs::readFile(const Host& host, const std::string& path, bool& ok) {
    if (const std::string* edited = overlayLookup(host, path)) {
        ok = true;
        return *edited;  // перезаписанный игроком файл
    }
    const FsNode* node = resolve(host, path);
    if (!node || node->is_dir) {
        ok = false;
        return {};
    }
    ok = true;
    if (node->generate) return node->generate();  // системный файл
    return node->content;
}

bool VirtualFs::writeFile(const Host& host, const std::string& path, const std::string& content) {
    if (path.empty() || path == "/" || path[0] != '/') return false;

    std::size_t slash = path.find_last_of('/');
    std::string dir = (slash == 0) ? "/" : path.substr(0, slash);
    std::string name = path.substr(slash + 1);
    if (name.empty()) return false;

    const FsNode* parent = resolve(host, dir);
    if (!parent || !parent->is_dir) return false;

    // Нельзя перезаписывать каталог и системные (сгенерированные) файлы.
    auto it = parent->children.find(name);
    if (it != parent->children.end() && (it->second.is_dir || it->second.generate)) return false;

    changeLayer()[host.name][path] = content;
    return true;
}

bool VirtualFs::addGeneratedFile(Host& host, const std::string& path,
                                 std::function<std::string()> generate) {
    if (path.empty() || path == "/" || path[0] != '/') return false;

    std::size_t slash = path.find_last_of('/');
    std::string dir = (slash == 0) ? "/" : path.substr(0, slash);
    std::string name = path.substr(slash + 1);
    if (name.empty()) return false;

    FsNode* parent = resolveMutable(host, dir);
    if (!parent || !parent->is_dir) return false;

    FsNode node;
    node.is_dir = false;
    node.generate = std::move(generate);
    parent->children[name] = std::move(node);
    return true;
}

bool VirtualFs::isSystemFile(const Host& host, const std::string& path) {
    const FsNode* node = resolve(host, path);
    return node && !node->is_dir && static_cast<bool>(node->generate);
}

std::size_t VirtualFs::fileSize(const Host& host, const std::string& path) {
    if (const std::string* edited = overlayLookup(host, path)) return edited->size();
    const FsNode* node = resolve(host, path);
    if (!node || node->is_dir) return 0;
    return node->content.size();
}

const std::map<std::string, std::map<std::string, std::string>>& VirtualFs::changes() {
    return changeLayer();
}

void VirtualFs::clearChanges() {
    changeLayer().clear();
}
