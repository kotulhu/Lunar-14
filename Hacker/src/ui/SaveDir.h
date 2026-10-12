#pragma once

#include <string>

// Идентификатор игры: единственное место, где он задан. Используется в путях
// сохранений по умолчанию.
inline constexpr const char* kGameId = "lunar-14";

// Папка сохранений по умолчанию для текущей платформы:
//   macOS   — ~/Library/Application Support/<id>
//   Linux   — $XDG_DATA_HOME/<id> или ~/.local/share/<id>
//   Windows — %APPDATA%/<id>
// Путь возвращается как есть; каталог создаётся вызывающим кодом.
std::string defaultSaveDir();
