# Lunar-14

A retro console quest: you’re a 90s hacker uncovering a government conspiracy. Fictional servers, old-school TUI apps (file manager, mail, chat), humor and easter eggs. Everything is simulated, no real network access.

Учебный консольный эмулятор «Хакер» (C++17). Настоящей сети и файловой системы нет:
всё живёт в памяти и загружается из папки `data/`.

## Утилиты

`help`, `about`, `ls`, `cat`, `cd`, `pwd`, `ping`, `portscan`, `telnet`, `ssh`, `scp`,
`fm`, `mail`, `chat`, `exit`.
Сетевые утилиты (`ping`, `portscan`, `telnet`, `ssh`, `scp`), `fm`, `mail` и `chat`
работают там, где хосту разрешён `"commands": "*"`. `portscan` перебирает порты,
`telnet`/`ssh` входят на удалённый хост и добавляют запись в стек сессий (`exit`
возвращает на предыдущий хост). `scp` копирует файл с удалённого хоста в локальный
каталог (запись попадает в сохранение). `fm` — полноэкранный двухпанельный файловый
менеджер на FTXUI. `mail` — полноэкранный почтовый клиент: список писем и предпросмотр,
Enter открывает письмо и помечает его прочитанным. Стартовые письма загружаются из
`data/mail.json`. `chat` — полноэкранный чат: список контактов слева, переписка справа,
строка ввода снизу; собеседники отвечают с задержкой 1-2 секунды. Контакты и переписки
загружаются из `data/chat.json`, правила ответов — из `data/chat_replies.json`,
выбор поставщика ответов — из `data/config.json` (ключ `chat_provider`, пока только
`canned`). Ключ `--unlock-contacts` открывает все контакты. Имена файлов и каталогов,
начинающиеся с `.`, считаются скрытыми: `ls` их не показывает, `ls -a` показывает;
`cat`/`cd` работают с ними по точному имени, а в `fm` их включает клавиша `h`.

Запись в виртуальную ФС, экраны `fm`/`mail`/`chat`, прочитанность писем и переписки
живут в текущем запуске. Прогресс сохраняется в файл `save.json` (см. ниже).

## Сохранения

Игра сохраняет состояние в читаемый `save.json` после каждой команды и при выходе
(включая Ctrl+D и Ctrl+C). Совместимость обеспечивает поле `version`: неизвестные
разделы
состояния пропускаются, отсутствующие берутся по умолчанию.

Где лежит `save.json` по умолчанию:

    macOS    ~/Library/Application Support/lunar-14/save.json
    Linux    $XDG_DATA_HOME/lunar-14/save.json или ~/.local/share/lunar-14/save.json
    Windows  %APPDATA%\lunar-14\save.json

При запуске, если сохранение найдено, игра спросит `Saved game found. Continue? [Y/n]`
(Enter или `Y` — продолжить, `N` — новая игра, старое уходит в `save.json.bak`).

Ключи:

    --new             начать новую игру без вопроса (старое сохранение -> save.json.bak)
    --no-save         не читать и не писать сохранение
    --save-dir <путь> своя папка сохранений

При разработке контента удобно запускать с `--no-save` или `--new`.

## Данные

Всё содержимое игры лежит в папке `data/`:

    data/
      world.json            имя домашнего хоста игрока ("player_host")
      hosts/<имя>.json       по одному файлу на хост; имя внутри = имя файла
      texts/                 длинные тексты, на которые ссылаются узлы ФС
      mail.json, chat.json, chat_replies.json, config.json

Узел ФС может быть строкой, массивом строк (склеиваются через `\n`) или объектом:
каталог — любой объект, а `{"$file": "путь.txt"}` подставляет содержимое
`data/texts/путь.txt`. По умолчанию папка `data/` ищется рядом с исполняемым файлом;
другую папку можно задать ключом `--data-dir <путь>` (относительный путь считается
от каталога запуска).

## Сборка

    cmake -B build
    cmake --build build

## Запуск

    ./build/hacker
    ./build/hacker -v
    ./build/hacker --start-host jsc-archive
    ./build/hacker --unlock-contacts
    ./build/hacker --data-dir ../data
    ./build/hacker --new
    ./build/hacker --no-save
    ./build/hacker --save-dir /tmp/hacker-save

## Xcode

Открыть `Hacker.xcodeproj`. Проект синхронизирует папку `Hacker` (в ней `src`),
поэтому новые файлы подхватываются автоматически. Заголовки nlohmann/json и FTXUI
и статические библиотеки FTXUI лежат в `build/_deps`, поэтому один раз выполните
`cmake -B build && cmake --build build` из корня проекта (например, перед первой
сборкой в Xcode).

Введите `help`, чтобы увидеть список утилит.
