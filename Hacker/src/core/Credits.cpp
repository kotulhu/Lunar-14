#include "core/Credits.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <set>

#include <nlohmann/json.hpp>

#include "core/GameState.h"
#include "core/Host.h"
#include "core/World.h"

using json = nlohmann::json;

namespace {

// Ширина строки в колонках (UTF-8: считаем стартовые байты).
std::size_t utf8Width(const std::string& text) {
    std::size_t width = 0;
    for (unsigned char c : text) {
        if ((c & 0xC0) != 0x80) ++width;
    }
    return width;
}

// Левая часть, точки-заполнитель и правая часть до ширины 60 колонок.
std::string dotted(const std::string& left, const std::string& right) {
    const std::size_t width = 60;
    std::size_t wl = utf8Width(left);
    std::size_t wr = utf8Width(right);
    if (wl + wr + 1 > width) return left + " " + right;  // длинно: без точек
    return left + std::string(width - wl - wr, '.') + right;
}

// Целое с разделением тысяч пробелом: 3481200 -> "3 481 200".
std::string groupThousands(long long value) {
    std::string digits = std::to_string(value);
    std::string out;
    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        out.push_back(*it);
        if (++count % 3 == 0 && it + 1 != digits.rend()) out.push_back(' ');
    }
    std::reverse(out.begin(), out.end());
    return out;
}

// Посещённые хосты (имена): домашний плюс все записанные.
std::set<std::string> visitedHosts(const World& world, const GameState& state) {
    std::set<std::string> visited;
    // player_host может быть задан ip или именем — приводим к имени хоста.
    if (const Host* home = world.findHost(world.playerHost())) {
        visited.insert(home->name);
    }
    for (const VisitedEntry& e : state.entries()) visited.insert(e.host);
    return visited;
}

}  // namespace

bool Credits::load(const std::string& path, const World& world, std::string& error) {
    delay_ms_ = 500;
    lines_.clear();

    std::ifstream in(path);
    if (!in) {
        error = "cannot open file: " + path;
        return false;
    }
    json root;
    try {
        in >> root;
    } catch (const std::exception& e) {
        error = "credits.json: invalid JSON: " + std::string(e.what());
        return false;
    }
    if (!root.is_object()) {
        error = "credits.json: expected an object";
        return false;
    }

    if (root.contains("delay_ms")) {
        if (!root["delay_ms"].is_number_integer()) {
            error = "credits.json: 'delay_ms' must be an integer";
            return false;
        }
        delay_ms_ = root["delay_ms"].get<int>();
        if (delay_ms_ < 0) {
            error = "credits.json: 'delay_ms' must not be negative";
            return false;
        }
    }

    if (!root.contains("lines") || !root["lines"].is_array()) {
        error = "credits.json: 'lines' must be an array";
        return false;
    }

    for (std::size_t i = 0; i < root["lines"].size(); ++i) {
        const json& node = root["lines"][i];
        const std::string where = "credits.json line " + std::to_string(i + 1) + ": ";
        if (!node.is_object()) {
            error = where + "expected an object";
            return false;
        }
        if (!node.contains("type") || !node["type"].is_string()) {
            error = where + "'type' must be a string";
            return false;
        }
        const std::string type = node["type"].get<std::string>();

        auto needString = [&](const char* key, std::string& dst) -> bool {
            if (!node.contains(key) || !node[key].is_string()) {
                error = where + "'" + key + "' must be a string";
                return false;
            }
            dst = node[key].get<std::string>();
            return true;
        };

        CreditLine line;
        if (type == "title") {
            line.type = CreditLine::Type::Title;
            if (!needString("text", line.text)) return false;
        } else if (type == "text") {
            line.type = CreditLine::Type::Text;
            if (!needString("text", line.text)) return false;
        } else if (type == "blank") {
            line.type = CreditLine::Type::Blank;
        } else if (type == "total") {
            line.type = CreditLine::Type::Total;
        } else if (type == "delivered") {
            line.type = CreditLine::Type::Delivered;
            if (!needString("host", line.host)) return false;
            if (!world.findHost(line.host)) {
                error = where + "unknown host '" + line.host + "'";
                return false;
            }
            if (!needString("text", line.text)) return false;
            if (node.contains("note")) {
                if (!node["note"].is_string()) {
                    error = where + "'note' must be a string";
                    return false;
                }
                line.note = node["note"].get<std::string>();
            }
        } else if (type == "credit") {
            line.type = CreditLine::Type::Credit;
            if (!needString("role", line.role)) return false;
            if (node.contains("name")) {
                if (!node["name"].is_string()) {
                    error = where + "'name' must be a string";
                    return false;
                }
                line.name = node["name"].get<std::string>();
            }
        } else if (type == "ending") {
            line.type = CreditLine::Type::Ending;
            if (!node.contains("tiers") || !node["tiers"].is_array() ||
                node["tiers"].empty()) {
                error = where + "'tiers' must be a non-empty array";
                return false;
            }
            for (const json& tier : node["tiers"]) {
                if (!tier.is_object() || !tier.contains("min") ||
                    !tier["min"].is_number_integer() || !tier.contains("text") ||
                    !tier["text"].is_string()) {
                    error = where + "each tier needs integer 'min' and string 'text'";
                    return false;
                }
                int min = tier["min"].get<int>();
                if (min < 0) {
                    error = where + "tier 'min' must not be negative";
                    return false;
                }
                line.tiers.push_back({min, tier["text"].get<std::string>()});
            }
        } else {
            error = where + "unknown type '" + type + "'";
            return false;
        }
        lines_.push_back(std::move(line));
    }
    return true;
}

int Credits::relays(const World& world, const GameState& state) const {
    int count = 0;
    for (const std::string& name : visitedHosts(world, state)) {
        const Host* host = world.findHost(name);
        if (host && host->reach > 0) ++count;
    }
    return count;
}

std::vector<std::string> Credits::render(const World& world,
                                         const GameState& state) const {
    std::set<std::string> visited = visitedHosts(world, state);

    long long total = 0;
    for (const std::string& name : visited) {
        const Host* host = world.findHost(name);
        if (host) total += host->reach;
    }

    std::vector<std::string> out;
    for (const CreditLine& line : lines_) {
        switch (line.type) {
            case CreditLine::Type::Title:
            case CreditLine::Type::Text:
                out.push_back(line.text);
                break;
            case CreditLine::Type::Blank:
                out.push_back("");
                break;
            case CreditLine::Type::Total:
                out.push_back("  ИТОГО ИНФОРМИРОВАНО: " + groupThousands(total) +
                              " компьютеров");
                break;
            case CreditLine::Type::Delivered: {
                if (visited.count(line.host) == 0) break;  // хост не посещён
                const Host* host = world.findHost(line.host);
                int reach = host ? host->reach : 0;
                if (reach <= 0) {
                    out.push_back("  " + line.text);
                    break;
                }
                std::string right = std::to_string(reach) + " компьютеров";
                if (!line.note.empty()) right += ", " + line.note;
                out.push_back(dotted("  " + line.text, right));
                break;
            }
            case CreditLine::Type::Credit:
                if (line.name.empty()) {
                    out.push_back("  " + line.role);
                } else {
                    out.push_back(dotted("  " + line.role, line.name));
                }
                break;
            case CreditLine::Type::Ending: {
                const CreditLine::Tier* best = nullptr;
                for (const CreditLine::Tier& tier : line.tiers) {
                    if (tier.min <= total && (!best || tier.min > best->min)) {
                        best = &tier;
                    }
                }
                if (best) out.push_back(best->text);
                break;
            }
        }
    }
    return out;
}
