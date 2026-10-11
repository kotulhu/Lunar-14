#include "commands/PwdCommand.h"

#include "core/Session.h"
#include "ui/Terminal.h"

void PwdCommand::execute(Session& session, const World&,
                         const std::vector<std::string>&, Terminal& term) {
    term.print(session.currentDir());
}
