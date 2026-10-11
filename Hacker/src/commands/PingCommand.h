#pragma once

#include "commands/Command.h"

// ping: эмуляция проверки доступности хоста по имени или ip.
class PingCommand : public Command {
public:
    std::string name() const override { return "ping"; }
    std::string description() const override { return "ping a host by name or ip"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
