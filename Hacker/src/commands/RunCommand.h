#pragma once

#include "commands/Command.h"

// run: запуск игровой программы (локально скачанной или найденной в ФС хоста).
class RunCommand : public Command {
public:
    std::string name() const override { return "run"; }
    std::string description() const override { return "execute a program"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
