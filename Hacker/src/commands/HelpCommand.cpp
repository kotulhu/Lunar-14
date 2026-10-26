#include "commands/HelpCommand.h"

#include <algorithm>

#include "commands/Registry.h"
#include "core/Session.h"
#include "ui/Terminal.h"

void HelpCommand::execute(Session& session, const World&,
                          const std::vector<std::string>&, Terminal& term) {
    term.print("Available commands:");

    // Показываем только то, что разрешено на текущем хосте.
    std::vector<Command*> commands;
    for (Command* c : registry_.all()) {
        if (session.host().allows(c->name(), registry_.explicitOnly(c->name()))) {
            commands.push_back(c);
        }
    }

    // Выравниваем описания по самой длинной команде.
    size_t width = 0;
    for (Command* c : commands) {
        width = std::max(width, c->name().size());
    }

    for (Command* c : commands) {
        std::string line = "  " + c->name();
        line += std::string(width - c->name().size() + 2, ' ');
        line += c->description();
        term.print(line);
    }
}
