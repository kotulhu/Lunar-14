#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/Host.h"

struct DownloadedFile {
    std::string name;
    FileType type = FileType::Text;
    std::string content;
    std::string sourceHost;
    std::string sourcePath;
    std::string programId;
};

class LocalStorage {
public:
    void add(DownloadedFile file);
    bool has(const std::string& name) const;
    const DownloadedFile* get(const std::string& name) const;
    std::vector<DownloadedFile> list() const;
    std::string toJson() const;
    bool fromJson(const std::string& value, std::string& error);

private:
    std::map<std::string, DownloadedFile> files_;
};

const char* fileTypeName(FileType type);
bool parseFileTypeName(const std::string& name, FileType& type);
