#include "commands/MailCommand.h"

#include "ui/MailScreen.h"

void MailCommand::execute(Session& session, const World&,
                          const std::vector<std::string>&, Terminal&) {
    runMail(session);
}
