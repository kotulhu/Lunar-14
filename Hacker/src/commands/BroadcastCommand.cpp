#include "commands/BroadcastCommand.h"

#include "core/EventEngine.h"
#include "core/Session.h"
#include "core/World.h"
#include "ui/Terminal.h"

void BroadcastCommand::execute(Session& session, const World& world,
                               const std::vector<std::string>&, Terminal& term) {
    const Credits& credits = session.credits();

    term.print("Broadcast started.");
    term.pause(700);
    term.print("Relays: " + std::to_string(credits.relays(world, session.gameState())));
    term.pause(700);
    term.print("Transmitting...");
    term.pause(700);

    // Строки титров: пустая строка считается blank и держит паузу вдвое дольше.
    const int delay = credits.delayMs();
    for (const std::string& line : credits.render(world, session.gameState())) {
        term.print(line);
        term.pause(line.empty() ? delay * 2 : delay);
    }

    session.gameState().setFinished(true);
    term.print("THE END");

    // Задел на будущее: событие о завершении рассылки.
    if (EventEngine* engine = session.events()) {
        engine->fire({"broadcast", session.hostName()}, session,
                     [&term] { return term.localTimeHM(); });
    }
}
