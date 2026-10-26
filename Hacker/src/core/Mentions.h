#pragma once

#include <string>
#include <vector>

class World;

// Имена узлов, упомянутых в тексте (по имени или IP), кроме exclude.
// Порядок — как в мире. Это «наводки»: документ ссылается на другие узлы.
std::vector<std::string> mentionedHosts(const World& world, const std::string& text,
                                        const std::string& exclude);
