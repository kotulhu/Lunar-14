#pragma once

#include "commands/Command.h"

class Registry;

// help: печатает список команд из реестра.
class HelpCommand : public Command {
public:
    explicit HelpCommand(const Registry& registry) : registry_(registry) {}

    std::string name() const override { return "help"; }
    std::string description() const override { return "show this list of commands"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;

private:
    const Registry& registry_;
};
