#pragma once

#include <string>

// Глобальные параметры тестового окружения, устанавливаемые в main.cpp
// до запуска тестов.
namespace TestEnv {

// Путь к папке data/ (для тестов, которым нужен реальный мир).
// Пустая строка — данные не найдены; такие тесты пропускают нагрузку с ошибкой.
inline std::string data_dir;

}  // namespace TestEnv
