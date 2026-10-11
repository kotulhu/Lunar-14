#pragma once

#include <string>
#include <vector>

class Session;
class World;
class Terminal;

// Интерфейс одной утилиты.
class Command {
public:
    virtual ~Command() = default;

    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual void execute(Session& session, const World& world,
                         const std::vector<std::string>& args, Terminal& term) = 0;
};
