#pragma once

#include <string>

// Приведение к нижнему регистру без локалей: ASCII (A-Z) и основная кириллица
// (А-Я -> а-я, Ё -> ё) в UTF-8. Остальные символы копируются как есть.
std::string toLowerUtf8(const std::string& text);
