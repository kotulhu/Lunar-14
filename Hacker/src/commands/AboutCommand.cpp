#include "commands/AboutCommand.h"

#include "ui/Terminal.h"

void AboutCommand::execute(Session&, const World&,
                           const std::vector<std::string>&, Terminal& term) {
    term.print("Hacker -- учебный текстовый эмулятор.");
    term.print("Вы играете за хакера в выдуманной сети.");
    term.print("Никакой реальной сети и файлов: всё живёт в памяти игры.");
    term.print("Введите help, чтобы увидеть доступные команды.");
}
