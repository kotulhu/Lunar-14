#pragma once

#include "commands/Command.h"

// broadcast: финальная рассылка с титрами. Доступна только на хосте,
// где её имя прямо указано в списке commands (explicitOnly).
class BroadcastCommand : public Command {
public:
    std::string name() const override { return "broadcast"; }
    std::string description() const override {
        return "send your message to every reachable computer";
    }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
