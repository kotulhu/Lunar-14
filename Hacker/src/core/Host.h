#pragma once

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

class Session;

enum class FileType { Text, Data, Program, Keygen, Virus, Antivirus };

// Узел виртуальной ФС: либо каталог (children), либо типизированный файл.
// Сгенерированный (системный) файл не описывается в JSON, а получает
// функцию, возвращающую текст (файл только для чтения).
struct FsNode {
    bool is_dir = false;
    std::string content;                     // текст файла
    FileType type = FileType::Text;
    std::string program_id;                  // id обработчика для Program
    std::map<std::string, FsNode> children;  // содержимое каталога
    std::function<std::string(Session*)> generate;  // если задана — системный файл
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
    int reach = 0;                                              // охват узла (компьютеров)
    std::vector<std::string> commands;                          // разрешённые команды
    bool all_commands = false;                                  // true, если "commands": "*"

    // Базовые файловые команды, help/exit, scp, downloads и run доступны с
    // любого хоста: ограничения узла не должны мешать навигации и запуску
    // локально скачанных программ.
    // explicitOnly-команда не попадает под "*": её имя должно быть в списке.
    bool allows(const std::string& command, bool explicit_only = false) const {
        if (command == "help" || command == "exit" || command == "scp" ||
            command == "downloads" || command == "run" || command == "ls" ||
            command == "cat" || command == "cd" || command == "pwd") return true;
        if (all_commands && !explicit_only) return true;
        for (const std::string& c : commands) {
            if (c == command) return true;
        }
        return false;
    }
};
