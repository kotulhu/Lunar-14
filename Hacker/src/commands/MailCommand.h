#pragma once

#include "commands/Command.h"

// mail: почтовый клиент.
class MailCommand : public Command {
public:
    std::string name() const override { return "mail"; }
    std::string description() const override { return "почтовый клиент"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
