#include "commands/ExitCommand.h"

#include "core/Session.h"
#include "ui/Terminal.h"

void ExitCommand::execute(Session& session, const World&,
                          const std::vector<std::string>&, Terminal& term) {
    // На удалённом хосте закрываем только верхнюю сессию.
    if (session.depth() > 1) {
        std::string name = session.hostName();
        session.pop();
        term.print("Connection to " + name + " closed.");
        return;
    }
    term.print("bye");
    session.requestQuit();
}
