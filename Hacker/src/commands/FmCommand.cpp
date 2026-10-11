#include "commands/FmCommand.h"

#include "ui/FileManagerScreen.h"

void FmCommand::execute(Session& session, const World&,
                        const std::vector<std::string>&, Terminal&) {
    runFileManager(session);
}
