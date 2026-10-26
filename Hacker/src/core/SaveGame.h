#pragma once

#include <string>

class Session;
class World;

// Сохранение игры: состояние сессии в читаемом JSON и его загрузка.
// Формат: { "version": 1, "state": { visited, journal, events, finished, mailbox, chat, files } }.
// Неизвестные разделы игнорируются, отсутствующие берутся по умолчанию.
namespace SaveGame {

// Текущая версия формата файла сохранения.
constexpr int kVersion = 1;

// Сериализует состояние сессии в JSON-текст с отступами.
std::string serialize(const Session& session);

// Загружает состояние из JSON-текста в сессию. Возвращает false и понятную
// причину при несовпадении версии или повреждении. Неизвестные хосты и контакты
// пропускаются: их число и пример попадают в warning (пустая строка, если
// пропусков не было).
bool deserialize(const std::string& text, Session& session, const World& world,
                 std::string& error, std::string& warning);

// Атомарная запись: временный файл рядом, затем переименование поверх. При
// ошибке false и понятная причина.
bool writeAtomic(const std::string& path, const std::string& content, std::string& error);

// Копия файла в <path>.bak и удаление оригинала (старая игра в архив).
// false и error, если исходный файл нельзя прочитать или скопировать.
bool backup(const std::string& path, std::string& error);

}  // namespace SaveGame
