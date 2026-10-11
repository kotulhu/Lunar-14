#pragma once

#include <exception>
#include <string>

#include "core/Host.h"
#include "core/Session.h"
#include "core/VirtualFs.h"
#include "core/World.h"
#include "ui/Terminal.h"

// Разбирает номер порта. true при корректности (1..65535).
inline bool parsePort(const std::string& text, int& port) {
    try {
        port = std::stoi(text);
    } catch (const std::exception&) {
        return false;
    }
    return port >= 1 && port <= 65535;
}

// Сверка пароля: до 3 попыток, пустой пароль — вход без запроса.
// Сообщения об отказе выводит сама. true, если вход разрешён.
inline bool authenticate(const Host& host, const std::string& user,
                         const std::string& password_prompt, Terminal& term) {
    const std::string* password = nullptr;
    for (const auto& acc : host.accounts) {
        if (acc.first == user) {
            password = &acc.second;
            break;
        }
    }

    for (int attempt = 0; attempt < 3; ++attempt) {
        if (password && password->empty()) return true;
        std::string entered = term.readPassword(password_prompt);
        if (password && entered == *password) return true;
        term.print("Access denied.");
    }

    term.print("Connection closed by foreign host.");
    return false;
}

// Выводит /etc/motd и добавляет запись в стек сессий.
inline void enterShell(Session& session, const std::string& host_query,
                       const std::string& user, const Host& host, Terminal& term) {
    bool has_motd = false;
    std::string motd = VirtualFs::readFile(host, "/etc/motd", has_motd);
    if (has_motd) term.print(motd);
    session.push(host_query, user, host.start_dir);
}

// Общие для ssh и scp проверки хоста/порта и вход. nullptr при ошибке.
// cmd задаёт префикс сообщения "unknown host" (ssh или scp).
inline const Host* sshConnect(const std::string& cmd, const World& world,
                              const std::string& host_query, int port,
                              const std::string& user, Terminal& term) {
    const Host* host = world.findHost(host_query);
    if (!host) {
        term.print(cmd + ": unknown host " + host_query);
        return nullptr;
    }
    if (!host->alive) {
        term.print("No route to host");
        return nullptr;
    }

    auto it = host->ports.find(port);
    if (it == host->ports.end()) {
        term.print("Connection refused");
        return nullptr;
    }
    // Порт открыт, но служба не ssh.
    if (it->second.service != "ssh") {
        term.print("ssh_exchange_identification: Connection closed by remote host");
        return nullptr;
    }

    term.print("Trying " + host->ip + "...");
    term.pause(500);
    term.print("Connected to " + host->name + ".");
    term.pause(500);

    if (!authenticate(*host, user, user + "@" + host->name + "'s password: ", term)) {
        return nullptr;
    }
    return host;
}
