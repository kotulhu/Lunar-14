#pragma once

#include "commands/Command.h"

// exit: завершает игру.
class ExitCommand : public Command {
public:
    std::string name() const override { return "exit"; }
    std::string description() const override { return "quit the game"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
