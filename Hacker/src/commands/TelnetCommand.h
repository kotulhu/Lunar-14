#pragma once

#include "commands/Command.h"

// telnet: подключение к хосту по протоколу telnet.
class TelnetCommand : public Command {
public:
    std::string name() const override { return "telnet"; }
    std::string description() const override { return "connect to a host over telnet"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
