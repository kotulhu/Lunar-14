#pragma once

#include "commands/Command.h"

// fm: двухпанельный файловый менеджер.
class FmCommand : public Command {
public:
    std::string name() const override { return "fm"; }
    std::string description() const override { return "two-pane file manager"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
