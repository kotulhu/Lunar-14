#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "commands/Command.h"

// Реестр команд. Новая утилита = новый файл + одна строка регистрации.
class Registry {
public:
    // explicitOnly: команда доступна только там, где её имя прямо указано
    // в списке commands хоста (см. Host::allows).
    void add(std::unique_ptr<Command> command, bool explicit_only = false);

    // Возвращает команду по имени или nullptr.
    Command* find(const std::string& name) const;

    // Помечена ли команда как explicitOnly.
    bool explicitOnly(const std::string& name) const;

    // Все команды, отсортированные по имени (для help).
    std::vector<Command*> all() const;

private:
    std::unordered_map<std::string, std::unique_ptr<Command>> commands_;
    std::unordered_set<std::string> explicit_only_;
};
