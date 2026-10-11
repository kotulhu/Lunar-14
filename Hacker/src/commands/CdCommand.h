#pragma once

#include "commands/Command.h"

// cd: смена текущего каталога.
class CdCommand : public Command {
public:
    std::string name() const override { return "cd"; }
    std::string description() const override { return "change the current directory"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
