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

// Реалистичная загрузка конкретной системы: Red Hat Linux 5.2, ядро 2.0.36.
// Проходят все этапы: BIOS POST, загрузчик LILO, инициализация ядра,
// init и системные службы, затем классическое приглашение login.
static void printBootSequence(Terminal& term, const Session& session) {
    // BIOS POST
    term.print("Award Modular BIOS v4.51PG, An Energy Star Ally");
    term.print("Copyright (C) 1984-98, Award Software, Inc.");
    term.print("");
    term.print("i430TX PCIset");
    term.print("Main Processor   : Pentium(r) MMX 233MHz");
    term.print("Memory Test      : 32768K OK");
    term.print("Cache Memory     : 512K");
    term.print("");
    term.print("Award Plug and Play BIOS Extension v1.0A");
    term.print("Copyright (C) 1997, Award Software, Inc.");
    term.print("");
    term.print("Detecting HDD Primary Master   ... WDC AC2340");
    term.print("Detecting HDD Primary Slave    ... None");
    term.print("Detecting HDD Secondary Master ... MITSUMI CD-ROM FX400");
    term.print("Detecting HDD Secondary Slave  ... None");
    term.print("");
    term.print("Press DEL to enter SETUP");
    term.print("");
    term.pause(300);

    // Загрузчик
    term.print("LILO 21.0 boot");
    term.print("Loading linux ......................");
    term.pause(300);

    // Ядро 2.0.36
    term.print("Linux version 2.0.36 (root@porky.devel.redhat.com) (gcc version 2.7.2.3) #1 Wed Dec 16 12:00:00 EST 1998");
    term.print("Detected 233.860004 MHz processor.");
    term.print("Console: colour VGA+ 80x25");
    term.print("POSIX conformance testing by UNIFIX");
    term.print("Calibrating delay loop... 232.65 BogoMIPS");
    term.print("Memory: 31088k/32768k available (860k kernel code, 384k reserved, 1180k data)");
    term.print("Checking 386/387 Coupling OK");
    term.print("Checking HLT instruction... OK");
    term.print("PCI: PCI BIOS revision 2.10 entry at 0xfdb90, last bus=0");
    term.print("PCI: Probing PCI hardware");
    term.print("Linux NET4.0 for Linux 2.0");
    term.print("Serial driver version 4.26 with no serial options enabled");
    term.print("tty00 at 0x03f8 (irq = 4) is a 16550A");
    term.print("tty01 at 0x02f8 (irq = 3) is a 16550A");
    term.print("Floppy drive(s): fd0 is 1.44M");
    term.print("hda: WDC AC2340, 320MB w/64kB Cache, CHS=1046/12/63");
    term.print("hdb: MITSUMI CD-ROM FX400, ATAPI CDROM drive");
    term.print("Partition check:");
    term.print(" hda: hda1 hda2 < hda5 >");
    term.print("VFS: Mounted root (ext2 filesystem) readonly.");
    term.print("Freeing unused kernel memory: 64k freed");
    term.pause(300);

    // init и системные службы
    term.print("INIT: version 2.74 booting");
    term.print("Setting clock  (localtime): Thu Dec 31 23:59:59 EST 1998  [  OK  ]");
    term.print("Mounting proc filesystem:  [  OK  ]");
    term.print("Configuring kernel parameters:  [  OK  ]");
    term.print("Activating swap partitions:  [  OK  ]");
    term.print("Setting hostname localhost.localdomain:  [  OK  ]");
    term.print("Checking root filesystem");
    term.print("/: clean, 1046/8192 files, 9450/16384 blocks");
    term.print("Remounting root filesystem in read-write mode:  [  OK  ]");
    term.print("Finding module dependencies:  [  OK  ]");
    term.print("Checking filesystems");
    term.print("Checking all file systems  [  OK  ]");
    term.print("Mounting local filesystems:  [  OK  ]");
    term.print("Enabling swap space:  [  OK  ]");
    term.print("INIT: Entering runlevel: 3");
    term.print("Starting system logger:  [  OK  ]");
    term.print("Starting kernel logger:  [  OK  ]");
    term.print("Starting portmapper:  [  OK  ]");
    term.print("Starting keytable:  [  OK  ]");
    term.print("Initializing random number generator:  [  OK  ]");
    term.print("Bringing up interface lo:  [  OK  ]");
    term.print("Bringing up interface eth0:  [  OK  ]");
    term.print("Starting crond:  [  OK  ]");
    term.print("Starting lpd:  [  OK  ]");
    term.print("Starting inetd:  [  OK  ]");
    term.print("Starting sendmail:  [  OK  ]");
    term.print("Starting httpd:  [  OK  ]");
    term.print("Starting console mouse services:  [  OK  ]");
    term.pause(300);

    term.print("");
    term.print("Red Hat Linux release 5.2 (Apollo)");
    term.print("Kernel 2.0.36 on an i586");
    term.print("");
    term.print(session.hostName() + " login: " + session.user());
}

// Короткая заставка после успешного входа: приветствие и список утилит.
static void printBootBanner(Terminal& term, const Registry& registry,
                            const Session& session) {
    term.print("");
    term.print("Welcome to " + session.hostName() + " (tty1)");
    term.print("");
    term.print("Available commands:");

    // Только то, что разрешено на текущем хосте (как в help), именами в строку.
    std::string line;
    for (Command* c : registry.all()) {
        if (!session.host().allows(c->name())) continue;
        std::string token = " " + c->name();
        if (line.size() + token.size() > 78) {
            term.print(line);
            line.clear();
        }
        line += token;
    }
    if (!line.empty()) term.print(line);
    term.print("");
    term.print("Type 'help' for a list of commands.");
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

    printBootSequence(term, *session);
    printBootBanner(term, registry, *session);

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
