#pragma once

#include <string>
#include <vector>

class GameState;
class World;

// Этап квеста: имя и игровая дата плюс условия наступления.
struct QuestStage {
    std::string id;
    std::string name;
    std::string date;
    std::vector<std::string> visited;  // все эти узлы должны быть посещены
    int visited_count = -1;            // посещено не меньше N (если >= 0)
    bool finished = false;             // пройдена финальная рассылка
};

// Именованные этапы квеста из data/quest.json. Текущий этап — последний
// по порядку, чьи условия выполнены.
class Quest {
public:
    // Загружает этапы из папки данных. false и error при ошибке.
    bool load(const std::string& data_dir, const World& world, std::string& error);

    // Текущий этап по состоянию игры. nullptr, если этапов нет.
    const QuestStage* currentStage(const GameState& state,
                                   const std::string& player_host) const;

    const std::vector<QuestStage>& stages() const { return stages_; }

private:
    std::vector<QuestStage> stages_;
};
