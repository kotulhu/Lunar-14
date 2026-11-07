cmake_minimum_required(VERSION 3.16)
project(hacker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Подключаем nlohmann/json фиксированной версии через FetchContent.
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# FTXUI (TUI) — используется в ui/ и в тестовом приложении.
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

# ── Статическая библиотека: core + commands + ui ──────────────────────────────
# hacker-bin и hacker-tester линкуются с ней; main.cpp в неё не входит.
add_library(hacker-core STATIC
    Hacker/src/core/World.cpp
    Hacker/src/core/Session.cpp
    Hacker/src/core/GameState.cpp
    Hacker/src/core/VirtualFs.cpp
    Hacker/src/core/LocalStorage.cpp
    Hacker/src/core/Mailbox.cpp
    Hacker/src/core/ChatStore.cpp
    Hacker/src/core/EventEngine.cpp
    Hacker/src/core/Credits.cpp
    Hacker/src/core/Quest.cpp
    Hacker/src/core/Mentions.cpp
    Hacker/src/core/SaveGame.cpp
    Hacker/src/core/TextUtil.cpp
    Hacker/src/core/CannedProvider.cpp
    Hacker/src/core/ProviderFactory.cpp
    Hacker/src/commands/Registry.cpp
    Hacker/src/commands/HelpCommand.cpp
    Hacker/src/commands/AboutCommand.cpp
    Hacker/src/commands/LsCommand.cpp
    Hacker/src/commands/CatCommand.cpp
    Hacker/src/commands/CdCommand.cpp
    Hacker/src/commands/PwdCommand.cpp
    Hacker/src/commands/PingCommand.cpp
    Hacker/src/commands/PortscanCommand.cpp
    Hacker/src/commands/TelnetCommand.cpp
    Hacker/src/commands/SshCommand.cpp
    Hacker/src/commands/ScpCommand.cpp
    Hacker/src/commands/FmCommand.cpp
    Hacker/src/commands/MailCommand.cpp
    Hacker/src/commands/ChatCommand.cpp
    Hacker/src/commands/BroadcastCommand.cpp
    Hacker/src/commands/VisitedCommand.cpp
    Hacker/src/commands/DownloadsCommand.cpp
    Hacker/src/commands/RunCommand.cpp
    Hacker/src/commands/ExitCommand.cpp
    Hacker/src/ui/Terminal.cpp
    Hacker/src/ui/SaveDir.cpp
    Hacker/src/ui/Signals.cpp
    Hacker/src/ui/FileManagerScreen.cpp
    Hacker/src/ui/MailScreen.cpp
    Hacker/src/ui/ChatScreen.cpp
)
target_include_directories(hacker-core PUBLIC Hacker/src)
target_link_libraries(hacker-core PUBLIC nlohmann_json::nlohmann_json ftxui::component)

# ── Основной бинарь ───────────────────────────────────────────────────────────
add_executable(hacker Hacker/src/main.cpp)
target_link_libraries(hacker PRIVATE hacker-core)

add_custom_command(TARGET hacker POST_BUILD
    # Удобный бинарь в корне проекта не должен отставать от build/hacker:
    # иначе игрок запускает старую сборку и не видит новых изменений.
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:hacker>
        ${CMAKE_CURRENT_SOURCE_DIR}/hacker-bin${CMAKE_EXECUTABLE_SUFFIX}
)

# ── Тестовый бинарь ───────────────────────────────────────────────────────────
# Собирается всегда (не опция — он лёгкий).
add_executable(hacker-tester
    tests/main.cpp
    tests/registry.cpp
    tests/test_vfs.cpp
    tests/test_savegame.cpp
    tests/test_events.cpp
    tests/test_file_types.cpp
    tests/test_scp.cpp
    tests/test_run.cpp
    tests/test_generated.cpp
)
target_include_directories(hacker-tester PRIVATE tests)
target_link_libraries(hacker-tester PRIVATE hacker-core)

# Один общий шаг копирования data/: иначе post-build команды двух целей
# одновременно пишут в build/data и иногда мешают друг другу.
add_custom_target(copy-data
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/data
        ${CMAKE_BINARY_DIR}/data
    COMMENT "Copying data/ to build/"
)
add_dependencies(hacker copy-data)
add_dependencies(hacker-tester copy-data)
