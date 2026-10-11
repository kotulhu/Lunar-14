#pragma once

#include <string>
#include <vector>

#include "core/Host.h"

// Игровой мир: набор хостов, загружаемый из папки данных.
class World {
public:
    // Загружает мир из папки данных: world.json, hosts/*.json и texts/.
    // При ошибке возвращает false и пишет причину в error.
    bool load(const std::string& data_dir, std::string& error);

    // Поиск хоста по ip или имени. Возвращает nullptr, если не найден.
    const Host* findHost(const std::string& query) const;
    Host* findHost(const std::string& query);

    const std::string& playerHost() const { return player_host_; }
    const std::vector<Host>& hosts() const { return hosts_; }

private:
    std::string player_host_;
    std::vector<Host> hosts_;
};
