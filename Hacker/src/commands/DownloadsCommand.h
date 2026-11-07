#pragma once

#include "commands/Command.h"

class DownloadsCommand : public Command {
public:
    std::string name() const override { return "downloads"; }
    std::string description() const override { return "list locally downloaded files"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
