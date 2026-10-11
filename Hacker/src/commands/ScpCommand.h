#pragma once

#include "commands/Command.h"

// scp: копирование файла с удалённого хоста на домашний компьютер.
class ScpCommand : public Command {
public:
    std::string name() const override { return "scp"; }
    std::string description() const override { return "copy a file from a remote host"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
