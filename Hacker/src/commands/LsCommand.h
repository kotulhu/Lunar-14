#pragma once

#include "commands/Command.h"

// ls: содержимое каталога текущего хоста.
class LsCommand : public Command {
public:
    std::string name() const override { return "ls"; }
    std::string description() const override { return "list directory contents (-a shows hidden)"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
