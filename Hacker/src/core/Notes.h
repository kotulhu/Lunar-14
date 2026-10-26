#pragma once

#include <string>
#include <vector>

#include "core/GameState.h"
#include "core/Mentions.h"
#include "core/Session.h"
#include "core/World.h"

// Помечает запись журнала снимком текущего этапа квеста.
inline void stampStage(Session& session, JournalEntry& entry) {
    if (const QuestStage* stage = session.currentStage()) {
        entry.stage = stage->name;
        entry.date = stage->date;
    }
}

// Записывает успешный вход на узел в журнал visited. Ключ — host + port,
// чтобы каждый узел был в списке один раз.
inline void noteVisit(Session& session, const std::string& host, int port,
                      const std::string& user) {
    JournalEntry entry;
    entry.key = "visit:" + host + ":" + std::to_string(port);
    entry.kind = "visit";
    entry.host = host;
    entry.port = port;
    entry.user = user;
    entry.title = host;
    stampStage(session, entry);
    session.gameState().addJournal(std::move(entry));
}

// Записывает найденный документ (файл, письмо, сообщение чата) в журнал.
// В журнал попадают только тексты, упоминающие другие узлы («наводки»).
// key — уникальный ключ записи для дедупа.
inline void noteDocument(Session& session, const std::string& source,
                         const std::string& host, const std::string& title,
                         const std::string& key, const std::string& text) {
    std::vector<std::string> mentioned = mentionedHosts(session.world(), text, host);
    if (mentioned.empty()) return;

    JournalEntry entry;
    entry.key = key;
    entry.kind = "find";
    entry.host = host;
    entry.source = source;
    entry.title = title;
    entry.text = text;
    entry.mentioned = std::move(mentioned);
    stampStage(session, entry);
    session.gameState().addJournal(std::move(entry));
}
