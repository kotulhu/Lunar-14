#include "core/Quest.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "core/GameState.h"
#include "core/World.h"

using json = nlohmann::json;

namespace {

// path + "/" + name с защитой от двойного слэша.
std::string joinPath(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    if (dir.back() == '/') return dir + name;
    return dir + "/" + name;
}

// Выполнены ли условия этапа для текущего состояния игры.
bool stageMatches(const QuestStage& stage, const GameState& state,
                  const std::string& player_host) {
    for (const std::string& host : stage.visited) {
        if (!state.hasVisited(host)) return false;
    }
    if (stage.visited_count >= 0 && state.distinctHosts(player_host) < stage.visited_count) {
        return false;
    }
    if (stage.finished && !state.finished()) return false;
    return true;
}

}  // namespace

bool Quest::load(const std::string& data_dir, const World& world, std::string& error) {
    stages_.clear();

    std::string path = joinPath(data_dir, "quest.json");
    std::ifstream in(path);
    if (!in) {
        error = "cannot open file: " + path;
        return false;
    }

    json root;
    try {
        in >> root;
    } catch (const std::exception& e) {
        error = "quest.json: invalid JSON: " + std::string(e.what());
        return false;
    }
    if (!root.is_array()) {
        error = "quest.json: root must be an array of stages";
        return false;
    }

    for (std::size_t i = 0; i < root.size(); ++i) {
        const std::string where = "quest.json stage #" + std::to_string(i) + ": ";
        const json& node = root[i];
        if (!node.is_object()) {
            error = where + "expected an object";
            return false;
        }

        QuestStage stage;
        if (!node.contains("id") || !node["id"].is_string()) {
            error = where + "'id' must be a string";
            return false;
        }
        stage.id = node["id"].get<std::string>();
        if (stage.id.empty()) {
            error = where + "'id' must not be empty";
            return false;
        }
        if (node.contains("name")) {
            if (!node["name"].is_string()) {
                error = where + "'name' must be a string";
                return false;
            }
            stage.name = node["name"].get<std::string>();
        }
        if (stage.name.empty()) stage.name = stage.id;
        if (node.contains("date")) {
            if (!node["date"].is_string()) {
                error = where + "'date' must be a string";
                return false;
            }
            stage.date = node["date"].get<std::string>();
        }

        if (node.contains("when")) {
            const json& when = node["when"];
            if (!when.is_object()) {
                error = where + "'when' must be an object";
                return false;
            }
            if (when.contains("visited")) {
                const json& visited = when["visited"];
                if (visited.is_string()) {
                    stage.visited.push_back(visited.get<std::string>());
                } else if (visited.is_array()) {
                    for (const json& item : visited) {
                        if (!item.is_string()) {
                            error = where + "'when.visited' array must contain only strings";
                            return false;
                        }
                        stage.visited.push_back(item.get<std::string>());
                    }
                } else {
                    error = where + "'when.visited' must be a string or an array of strings";
                    return false;
                }
                for (const std::string& host_name : stage.visited) {
                    if (!world.findHost(host_name)) {
                        error = where + "unknown host '" + host_name + "' in when.visited";
                        return false;
                    }
                }
            }
            if (when.contains("visited_count")) {
                if (!when["visited_count"].is_number_integer()) {
                    error = where + "'when.visited_count' must be an integer";
                    return false;
                }
                int count = when["visited_count"].get<int>();
                if (count < 0) {
                    error = where + "'when.visited_count' must not be negative";
                    return false;
                }
                stage.visited_count = count;
            }
            if (when.contains("finished")) {
                if (!when["finished"].is_boolean()) {
                    error = where + "'when.finished' must be a boolean";
                    return false;
                }
                stage.finished = when["finished"].get<bool>();
            }
        }

        stages_.push_back(std::move(stage));
    }

    return true;
}

const QuestStage* Quest::currentStage(const GameState& state,
                                      const std::string& player_host) const {
    const QuestStage* current = nullptr;
    for (const QuestStage& stage : stages_) {
        if (stageMatches(stage, state, player_host)) current = &stage;
    }
    return current;
}
