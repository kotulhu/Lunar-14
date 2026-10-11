#pragma once

#include "commands/Command.h"

// pwd: печатает текущий каталог.
class PwdCommand : public Command {
public:
    std::string name() const override { return "pwd"; }
    std::string description() const override { return "print the current directory"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
