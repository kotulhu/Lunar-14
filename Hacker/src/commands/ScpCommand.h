#pragma once

#include "commands/Command.h"

// scp: скачивание файла с текущего или удалённого хоста в downloads/.
class ScpCommand : public Command {
public:
    std::string name() const override { return "scp"; }
    std::string description() const override { return "download a file to local downloads/"; }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
