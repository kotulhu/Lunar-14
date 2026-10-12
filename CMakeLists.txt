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

# FTXUI (TUI) — используется только в src/ui/.
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

add_executable(hacker
    Hacker/src/main.cpp
    Hacker/src/core/World.cpp
    Hacker/src/core/Session.cpp
    Hacker/src/core/GameState.cpp
    Hacker/src/core/VirtualFs.cpp
    Hacker/src/core/Mailbox.cpp
    Hacker/src/core/ChatStore.cpp
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
    Hacker/src/commands/ExitCommand.cpp
    Hacker/src/ui/Terminal.cpp
    Hacker/src/ui/FileManagerScreen.cpp
    Hacker/src/ui/MailScreen.cpp
    Hacker/src/ui/ChatScreen.cpp
)

# Все include-пути проекта указываются от папки src.
target_include_directories(hacker PRIVATE Hacker/src)
target_link_libraries(hacker PRIVATE nlohmann_json::nlohmann_json ftxui::component)

# Копируем папку data рядом с исполняемым файлом после сборки.
add_custom_command(TARGET hacker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/data
        $<TARGET_FILE_DIR:hacker>/data
)
