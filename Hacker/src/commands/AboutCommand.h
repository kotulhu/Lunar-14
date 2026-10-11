#pragma once

#include "commands/Command.h"

// about: короткая справка об игре.
class AboutCommand : public Command {
public:
    std::string name() const override { return "about"; }
    std::string description() const override { return "about this game"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
