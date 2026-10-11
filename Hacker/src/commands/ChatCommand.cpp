#include "commands/ChatCommand.h"

#include "ui/ChatScreen.h"

void ChatCommand::execute(Session& session, const World&,
                          const std::vector<std::string>&, Terminal&) {
    runChat(session);
}
