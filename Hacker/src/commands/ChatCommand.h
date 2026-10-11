#pragma once

#include "commands/Command.h"

// chat: клиент мгновенных сообщений.
class ChatCommand : public Command {
public:
    std::string name() const override { return "chat"; }
    std::string description() const override { return "чат"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
