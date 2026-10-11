#pragma once

#include "commands/Command.h"

// portscan: перебор портов хоста с выводом открытых служб.
class PortscanCommand : public Command {
public:
    std::string name() const override { return "portscan"; }
    std::string description() const override { return "scan a host for open ports"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
