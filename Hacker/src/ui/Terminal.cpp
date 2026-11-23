#include "ui/Terminal.h"

#include <chrono>
#include <ctime>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

Terminal::Terminal() {
#ifdef _WIN32
    // Включаем ANSI (VT) и UTF-8 в консоли Windows.
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(out, &mode)) {
        SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    is_tty_ = _isatty(_fileno(stdout)) && _isatty(_fileno(stdin));
#else
    is_tty_ = isatty(STDOUT_FILENO) == 1 && isatty(STDIN_FILENO) == 1;
#endif

    if (is_tty_) {
        // Кислотно-зелёные символы на чёрном фоне.
        std::cout << "\033[92;40m" << std::flush;
    }
}

Terminal::~Terminal() {
    if (is_tty_) {
        std::cout << "\033[0m" << std::flush;
    }
}

void Terminal::print(const std::string& line) {
    std::cout << line << "\n";
}

void Terminal::printInline(const std::string& text) {
    std::cout << text << std::flush;
}

bool Terminal::readLine(const std::string& prompt, std::string& out) {
    printInline(prompt);
    // getline оставляет мигающий курсор терминала в конце приглашения.
    if (!std::getline(std::cin, out)) {
        return false;  // Ctrl+D / EOF
    }
    return true;
}

void Terminal::pause(int milliseconds) {
    if (no_delay_) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void Terminal::clearScreen() {
    if (!is_tty_) return;
    std::cout << "\033[2J\033[H" << std::flush;
}

void Terminal::waitForKey() {
    if (!is_tty_) return;

#ifdef _WIN32
    _getch();
#else
    termios old_term{};
    if (tcgetattr(STDIN_FILENO, &old_term) == 0) {
        termios new_term = old_term;
        new_term.c_lflag &= ~(ICANON | ECHO);
        new_term.c_cc[VMIN] = 1;
        new_term.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) == 0) {
            char c = 0;
            (void)read(STDIN_FILENO, &c, 1);
            tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
        }
    }
#endif
}

std::string Terminal::localTimeHM() const {
    std::time_t now = std::time(nullptr);
    std::tm parts{};
#ifdef _WIN32
    localtime_s(&parts, &now);
#else
    localtime_r(&now, &parts);
#endif
    char buffer[6];
    if (std::strftime(buffer, sizeof(buffer), "%H:%M", &parts) == 0) {
        return "00:00";
    }
    return buffer;
}

std::string Terminal::readPassword(const std::string& prompt) {
    printInline(prompt);
    std::string out;

#ifdef _WIN32
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    bool changed = false;
    if (GetConsoleMode(in, &mode)) {
        changed = SetConsoleMode(in, mode & ~ENABLE_ECHO_INPUT);
    }
    std::getline(std::cin, out);
    if (changed) SetConsoleMode(in, mode);
#else
    termios old_term{};
    bool changed = false;
    if (tcgetattr(STDIN_FILENO, &old_term) == 0) {
        termios new_term = old_term;
        new_term.c_lflag &= ~static_cast<tcflag_t>(ECHO);
        changed = tcsetattr(STDIN_FILENO, TCSANOW, &new_term) == 0;
    }
    std::getline(std::cin, out);
    if (changed) tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
#endif

    print("");  // перевод строки после ввода пароля
    return out;
}
