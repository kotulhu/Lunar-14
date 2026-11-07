#include "core/LocalStorage.h"

#include <nlohmann/json.hpp>

namespace {
bool validName(const std::string& name) {
    return !name.empty() && name != "." && name != ".." &&
           name.find('/') == std::string::npos && name.find('\\') == std::string::npos;
}
}  // namespace

const char* fileTypeName(FileType type) {
    switch (type) {
        case FileType::Text: return "text";
        case FileType::Data: return "data";
        case FileType::Program: return "program";
        case FileType::Keygen: return "keygen";
        case FileType::Virus: return "virus";
        case FileType::Antivirus: return "antivirus";
    }
    return "text";
}

bool parseFileTypeName(const std::string& name, FileType& type) {
    if (name == "text") type = FileType::Text;
    else if (name == "data") type = FileType::Data;
    else if (name == "program") type = FileType::Program;
    else if (name == "keygen") type = FileType::Keygen;
    else if (name == "virus") type = FileType::Virus;
    else if (name == "antivirus") type = FileType::Antivirus;
    else return false;
    return true;
}

void LocalStorage::add(DownloadedFile file) {
    if (!validName(file.name)) return;
    files_[file.name] = std::move(file);
}

bool LocalStorage::has(const std::string& name) const {
    return files_.find(name) != files_.end();
}

const DownloadedFile* LocalStorage::get(const std::string& name) const {
    auto it = files_.find(name);
    return it == files_.end() ? nullptr : &it->second;
}

std::vector<DownloadedFile> LocalStorage::list() const {
    std::vector<DownloadedFile> result;
    result.reserve(files_.size());
    for (const auto& entry : files_) result.push_back(entry.second);
    return result;
}

std::string LocalStorage::toJson() const {
    nlohmann::json result = nlohmann::json::array();
    for (const auto& entry : files_) {
        const DownloadedFile& file = entry.second;
        result.push_back({{"name", file.name},
                          {"type", fileTypeName(file.type)},
                          {"content", file.content},
                          {"sourceHost", file.sourceHost},
                          {"sourcePath", file.sourcePath},
                          {"program_id", file.programId}});
    }
    return result.dump();
}

bool LocalStorage::fromJson(const std::string& text, std::string& error) {
    nlohmann::json value;
    try {
        value = nlohmann::json::parse(text);
    } catch (const std::exception& e) {
        error = std::string("invalid downloads JSON: ") + e.what();
        return false;
    }
    if (!value.is_array()) {
        error = "downloads must be an array";
        return false;
    }
    std::map<std::string, DownloadedFile> parsed;
    for (std::size_t i = 0; i < value.size(); ++i) {
        const auto& node = value[i];
        const std::string where = "downloads[" + std::to_string(i) + "]: ";
        if (!node.is_object() || !node.contains("name") || !node["name"].is_string() ||
            !node.contains("type") || !node["type"].is_string() ||
            !node.contains("content") || !node["content"].is_string() ||
            !node.contains("sourceHost") || !node["sourceHost"].is_string() ||
            !node.contains("sourcePath") || !node["sourcePath"].is_string()) {
            error = where + "expected name, type, content, sourceHost and sourcePath strings";
            return false;
        }
        DownloadedFile file;
        file.name = node["name"].get<std::string>();
        if (!validName(file.name) ||
            !parseFileTypeName(node["type"].get<std::string>(), file.type)) {
            error = where + "invalid name or file type";
            return false;
        }
        file.content = node["content"].get<std::string>();
        file.sourceHost = node["sourceHost"].get<std::string>();
        file.sourcePath = node["sourcePath"].get<std::string>();
        if (node.contains("program_id")) {
            if (!node["program_id"].is_string()) {
                error = where + "'program_id' must be a string";
                return false;
            }
            file.programId = node["program_id"].get<std::string>();
        }
        parsed[file.name] = std::move(file);
    }
    files_ = std::move(parsed);
    return true;
}
