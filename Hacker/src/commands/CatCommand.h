#pragma once

#include "commands/Command.h"

// cat: вывод содержимого файла.
class CatCommand : public Command {
public:
    std::string name() const override { return "cat"; }
    std::string description() const override { return "print a file to the screen"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
