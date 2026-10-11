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
    return std::vector<std::string>(entries.begin(), entries.end());
}

bool VirtualFs::isDirectory(const Host& host, const std::string& path) {
    const FsNode* node = resolve(host, path);
    return node && node->is_dir;
}

bool VirtualFs::exists(const Host& host, const std::string& path) {
    return resolve(host, path) != nullptr;
}

std::string VirtualFs::readFile(const Host& host, const std::string& path, bool& ok) {
    const FsNode* node = resolve(host, path);
    if (!node || node->is_dir) {
        ok = false;
        return {};
    }
    ok = true;
    return node->content;
}

bool VirtualFs::writeFile(Host& host, const std::string& path, const std::string& content) {
    if (path.empty() || path == "/" || path[0] != '/') return false;

    std::size_t slash = path.find_last_of('/');
    std::string dir = (slash == 0) ? "/" : path.substr(0, slash);
    std::string name = path.substr(slash + 1);
    if (name.empty()) return false;

    FsNode* parent = resolveMutable(host, dir);
    if (!parent || !parent->is_dir) return false;

    auto it = parent->children.find(name);
    if (it != parent->children.end() && it->second.is_dir) return false;

    FsNode node;
    node.is_dir = false;
    node.content = content;
    parent->children[name] = std::move(node);
    return true;
}

std::size_t VirtualFs::fileSize(const Host& host, const std::string& path) {
    const FsNode* node = resolve(host, path);
    if (!node || node->is_dir) return 0;
    return node->content.size();
}
