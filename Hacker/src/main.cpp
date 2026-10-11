#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "commands/AboutCommand.h"
#include "commands/CatCommand.h"
#include "commands/CdCommand.h"
#include "commands/ChatCommand.h"
#include "commands/ExitCommand.h"
#include "commands/FmCommand.h"
#include "commands/HelpCommand.h"
#include "commands/LsCommand.h"
#include "commands/MailCommand.h"
#include "commands/PingCommand.h"
#include "commands/PortscanCommand.h"
#include "commands/PwdCommand.h"
#include "commands/Registry.h"
#include "commands/ScpCommand.h"
#include "commands/SshCommand.h"
#include "commands/TelnetCommand.h"
#include "core/Session.h"
#include "core/ProviderFactory.h"
#include "core/World.h"
#include "ui/Terminal.h"

// Разбивает строку на слова по пробелам и табам.
static std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : line) {
        if (c == ' ' || c == '\t') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

// Папка данных по умолчанию — рядом с исполняемым файлом. Если её там нет,
// пробуем привычные места запуска (из build/ или из корня проекта).
static std::string defaultDataDir(const char* argv0) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path exe = fs::absolute(argv0, ec);
    fs::path base = exe.has_parent_path() ? exe.parent_path() : fs::current_path(ec);
    fs::path candidate = base / "data";
    if (fs::is_directory(candidate, ec)) return candidate.string();

    for (const std::string& path : {"data", "../data", "../../data"}) {
        if (fs::exists(path + "/world.json", ec)) return path;
    }
    return candidate.string();
}

// Путь к файлу данных внутри папки данных.
static std::string dataPath(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    return dir.back() == '/' ? dir + name : dir + "/" + name;
}

int main(int argc, char** argv) {
    // Необязательные аргументы: --start-host <ip|имя>, --unlock-contacts
    // и --data-dir <путь>.
    std::string start_host;
    std::string data_dir;
    bool unlock_contacts = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--start-host" && i + 1 < argc) {
            start_host = argv[++i];
        } else if (arg == "--data-dir" && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (arg == "--unlock-contacts") {
            unlock_contacts = true;
        }
    }
    if (data_dir.empty()) data_dir = defaultDataDir(argv[0]);

    World world;
    std::string error;
    if (!world.load(data_dir, error)) {
        std::cerr << "Failed to load world: " << error << "\n";
        return 1;
    }

    if (start_host.empty()) {
        start_host = world.playerHost();
    }

    Terminal term;

    std::unique_ptr<Session> session;
    try {
        session = std::make_unique<Session>(world, start_host);
    } catch (const std::exception& e) {
        term.print(std::string("Cannot start session: ") + e.what());
        return 1;
    }

    if (!session->mailbox().loadFromFile(dataPath(data_dir, "mail.json"), error)) {
        std::cerr << "Failed to load mail: " << error << "\n";
        return 1;
    }

    if (!session->chat().loadFromFile(dataPath(data_dir, "chat.json"), error)) {
        std::cerr << "Failed to load chat: " << error << "\n";
        return 1;
    }
    if (unlock_contacts) session->chat().unlockAll();

    // Провайдер ответов: имя берём из config.json, реализация — в одном месте.
    std::string provider_name = readChatProviderName(dataPath(data_dir, "config.json"), error);
    if (!error.empty()) {
        std::cerr << "Failed to load config: " << error << "\n";
        return 1;
    }
    std::string provider_warning;
    std::unique_ptr<ResponseProvider> provider =
        makeChatProvider(provider_name, dataPath(data_dir, "chat_replies.json"), error,
                         provider_warning);
    if (!provider) {
        std::cerr << "Failed to load chat replies: " << error << "\n";
        return 1;
    }
    session->setChatProvider(std::move(provider));

    // Регистрация утилит. Новая команда = один вызов add().
    Registry registry;
    registry.add(std::make_unique<HelpCommand>(registry));
    registry.add(std::make_unique<AboutCommand>());
    registry.add(std::make_unique<LsCommand>());
    registry.add(std::make_unique<CatCommand>());
    registry.add(std::make_unique<CdCommand>());
    registry.add(std::make_unique<PwdCommand>());
    registry.add(std::make_unique<PingCommand>());
    registry.add(std::make_unique<PortscanCommand>());
    registry.add(std::make_unique<TelnetCommand>());
    registry.add(std::make_unique<SshCommand>());
    registry.add(std::make_unique<ScpCommand>());
    registry.add(std::make_unique<FmCommand>());
    registry.add(std::make_unique<MailCommand>());
    registry.add(std::make_unique<ChatCommand>());
    registry.add(std::make_unique<ExitCommand>());

    term.print("Hacker v0.1 -- type 'help' for a list of commands.");

    // Одна строка, если выбранный провайдер ещё не реализован.
    if (!provider_warning.empty()) {
        term.print(provider_warning);
    }

    // Подсказка о почте показывается один раз при старте на домашнем хосте.
    const Host* player_host = world.findHost(world.playerHost());
    if (player_host && session->hostIp() == player_host->ip) {
        int unread = session->mailbox().unreadCount();
        if (unread > 0) {
            term.print("You have " + std::to_string(unread) +
                       " unread messages. Type 'mail'.");
        }
        int chat_unread = session->chat().unreadVisible();
        if (chat_unread > 0) {
            term.print("New chat messages: " + std::to_string(chat_unread) +
                       ". Type 'chat'.");
        }
    }

    std::string line;
    while (true) {
        std::string prompt = session->user() + "@" + session->hostName() + ":" +
                             session->currentDir() + "$ ";
        if (!term.readLine(prompt, line)) {
            term.print("");  // переводим строку после Ctrl+D
            break;
        }

        std::vector<std::string> tokens = tokenize(line);
        if (tokens.empty()) continue;  // пустой ввод игнорируем

        std::string command_name = tokens[0];
        std::vector<std::string> args(tokens.begin() + 1, tokens.end());

        // Проверка белого списка команд текущего хоста — в одном месте.
        Command* command = registry.find(command_name);
        if (!command || !session->host().allows(command_name)) {
            term.print(command_name + ": command not found. Type 'help'.");
            continue;
        }

        command->execute(*session, world, args, term);

        if (session->quitRequested()) break;
    }

    return 0;
}
