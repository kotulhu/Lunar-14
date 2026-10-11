#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "commands/Command.h"

// Реестр команд. Новая утилита = новый файл + одна строка регистрации.
class Registry {
public:
    void add(std::unique_ptr<Command> command);

    // Возвращает команду по имени или nullptr.
    Command* find(const std::string& name) const;

    // Все команды, отсортированные по имени (для help).
    std::vector<Command*> all() const;

private:
    std::unordered_map<std::string, std::unique_ptr<Command>> commands_;
};
