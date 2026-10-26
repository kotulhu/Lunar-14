#include "commands/VisitedCommand.h"

#include <cstddef>
#include <string>
#include <vector>

#include "commands/SshCommand.h"
#include "commands/TelnetCommand.h"
#include "core/GameState.h"
#include "core/Host.h"
#include "core/Quest.h"
#include "core/Session.h"
#include "core/World.h"
#include "ui/Terminal.h"

namespace {

// Пункт меню: либо узел (подключиться), либо находка (прочитать).
struct MenuItem {
    bool is_host = false;
    std::string date;
    std::string ip;
    std::string stage;
    std::string title;  // подпись пункта
    std::string host;   // имя узла (для подключения)
    int port = 0;
    std::string user;   // логин для подключения
    std::string text;   // текст находки
    std::string leads;  // непосещённые узлы, упомянутые в находке
};

// Строка-разделитель над пунктом меню.
std::string header(const MenuItem& item) {
    return "###### " + item.date + " ##### " + item.ip + " ##### " + item.stage + " #####";
}

}  // namespace

void VisitedCommand::execute(Session& session, const World& world,
                             const std::vector<std::string>& args, Terminal& term) {
    (void)args;

    GameState& state = session.gameState();
    const QuestStage* stage = session.currentStage();

    std::vector<MenuItem> items;

    // Узлы: по одному пункту на каждый сохранённый вход.
    for (const VisitedEntry& entry : state.entries()) {
        MenuItem item;
        item.is_host = true;
        item.host = entry.host;
        item.port = entry.port;
        item.user = entry.user;
        item.title = entry.host + "  (" + entry.user + "@" + entry.host + ", порт " +
                     std::to_string(entry.port) + ")";
        if (const Host* h = world.findHost(entry.host)) item.ip = h->ip;
        // Дата и этап — из журнала, если запись сохранилась.
        for (const JournalEntry& j : state.journal()) {
            if (j.kind == "visit" && j.host == entry.host && j.port == entry.port) {
                item.date = j.date;
                item.stage = j.stage;
            }
        }
        if (item.date.empty() && stage) item.date = stage->date;
        if (item.stage.empty() && stage) item.stage = stage->name;
        items.push_back(std::move(item));
    }

    // Имя домашнего узла не считаем наводкой.
    std::string home_name;
    if (const Host* home = world.findHost(world.playerHost())) home_name = home->name;

    // Находки из журнала.
    for (const JournalEntry& j : state.journal()) {
        if (j.kind != "find") continue;
        MenuItem item;
        item.is_host = false;
        item.title = j.title;
        item.text = j.text;
        item.date = j.date;
        item.stage = j.stage;
        if (const Host* h = world.findHost(j.host)) item.ip = h->ip;
        // Наводки — упомянутые, но ещё не посещённые узлы.
        for (const std::string& name : j.mentioned) {
            if (name == home_name || state.hasVisited(name)) continue;
            if (!item.leads.empty()) item.leads += ", ";
            item.leads += name;
        }
        items.push_back(std::move(item));
    }

    if (items.empty()) {
        term.print("Список пуст: вы ещё ничего не нашли.");
        return;
    }

    while (true) {
        term.print("");
        term.print("Каталог известных узлов и находок:");
        for (std::size_t i = 0; i < items.size(); ++i) {
            term.print(header(items[i]));
            std::string line = "  " + std::to_string(i + 1) + ") " + items[i].title;
            if (!items[i].leads.empty()) line += "  [наводки: " + items[i].leads + "]";
            term.print(line);
        }
        term.print("  0) выход");
        term.print("");

        std::string answer;
        if (!term.readLine("выбор> ", answer)) return;
        if (answer.empty()) return;

        // Принимаем только число; мусор не считаем ошибкой, просто выходим.
        bool numeric = answer.size() <= 9;
        for (char c : answer) {
            if (c < '0' || c > '9') {
                numeric = false;
                break;
            }
        }
        if (!numeric) return;

        int choice = std::stoi(answer);
        if (choice == 0) return;
        if (choice < 1 || choice > static_cast<int>(items.size())) {
            term.print("Нет такого пункта.");
            continue;
        }

        const MenuItem& item = items[choice - 1];
        if (!item.is_host) {
            term.print("");
            term.print(header(item));
            term.print(item.text);
            continue;
        }

        // Подключение к узлу: telnet для telnet, ssh для ssh.
        const Host* host = world.findHost(item.host);
        if (!host || !host->alive) {
            term.print("No route to host");
            return;
        }
        auto it = host->ports.find(item.port);
        std::string service = it == host->ports.end() ? "" : it->second.service;
        if (service == "ssh") {
            SshCommand ssh;
            ssh.execute(session, world,
                        {"-p", std::to_string(item.port), item.user + "@" + item.host}, term);
        } else {
            TelnetCommand telnet;
            telnet.execute(session, world, {item.host, std::to_string(item.port)}, term);
        }
        return;
    }
}
