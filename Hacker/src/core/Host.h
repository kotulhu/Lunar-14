#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

// Узел виртуальной ФС: либо каталог (children), либо файл (content).
struct FsNode {
    bool is_dir = false;
    std::string content;                     // текст файла
    std::map<std::string, FsNode> children;  // содержимое каталога
};

// Открытый порт: служба и необязательный баннер.
struct PortInfo {
    std::string service;
    std::string banner;  // может быть пустым
};

// Один выдуманный хост игрового мира.
struct Host {
    std::string ip;
    std::string name;
    bool alive = true;
    std::map<int, PortInfo> ports;                               // порт -> служба и баннер
    std::vector<std::pair<std::string, std::string>> accounts;  // пары логин/пароль
    FsNode root;                                                // корень файловой системы
    std::string start_dir = "/";                                // стартовый каталог сессии
    std::vector<std::string> commands;                          // разрешённые команды
    bool all_commands = false;                                  // true, если "commands": "*"

    // Разрешена ли команда на этом хосте. help и exit доступны всегда.
    bool allows(const std::string& command) const {
        if (command == "help" || command == "exit") return true;
        if (all_commands) return true;
        for (const std::string& c : commands) {
            if (c == command) return true;
        }
        return false;
    }
};
