#pragma once

#include "commands/Command.h"

// ssh: подключение к хосту по протоколу ssh.
class SshCommand : public Command {
public:
    std::string name() const override { return "ssh"; }
    std::string description() const override { return "connect to a host over ssh"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
