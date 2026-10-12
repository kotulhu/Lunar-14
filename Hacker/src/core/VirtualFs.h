#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

class Host;

// Виртуальная файловая система: работает поверх дерева узлов хоста.
class VirtualFs {
public:
    // Нормализует путь относительно current_dir (поддержка ".", "..", абсолютных путей).
    static std::string normalize(const std::string& current_dir, const std::string& path);

    // Возвращает отсортированное содержимое каталога (у каталогов в конце "/").
    // Скрытые имена (начинающиеся с ".") включаются только при show_hidden.
    static std::vector<std::string> list(const Host& host, const std::string& dir,
                                         bool show_hidden = false);

    static bool exists(const Host& host, const std::string& path);
    static bool isDirectory(const Host& host, const std::string& path);

    // Читает файл. ok=false, если путь не является файлом.
    static std::string readFile(const Host& host, const std::string& path, bool& ok);

    // Создаёт или перезаписывает файл. false, если родительский каталог не найден,
    // путь занят каталогом или системным файлом. Изменение ложится в слой
    // пользовательских правок (см. changes), а не в базовое дерево.
    static bool writeFile(const Host& host, const std::string& path, const std::string& content);

    // Слой пользовательских изменений: имя хоста -> (путь -> содержимое).
    // Только созданные или перезаписанные игроком файлы; базовое дерево не трогается.
    static const std::map<std::string, std::map<std::string, std::string>>& changes();

    // Сбрасывает слой пользовательских изменений (новая игра).
    static void clearChanges();

    // Добавляет сгенерированный (системный) файл: текст возвращает функция.
    static bool addGeneratedFile(Host& host, const std::string& path,
                                 std::function<std::string()> generate);

    // true, если путь — сгенерированный (системный) файл.
    static bool isSystemFile(const Host& host, const std::string& path);

    // Размер файла в байтах (0, если путь не файл).
    static std::size_t fileSize(const Host& host, const std::string& path);
};
