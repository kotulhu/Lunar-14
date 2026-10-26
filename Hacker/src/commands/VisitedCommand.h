#pragma once

#include "commands/Command.h"

// visited: каталог известных узлов и найденных документов.
// Только на домашнем хосте: подключение к узлам и чтение находок.
class VisitedCommand : public Command {
public:
    std::string name() const override { return "visited"; }
    std::string description() const override {
        return "list known hosts and found documents";
    }
    void execute(Session& session, const World& world,
                 const std::vector<std::string>& args, Terminal& term) override;
};
