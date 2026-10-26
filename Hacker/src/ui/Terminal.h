#pragma once

#include <string>

// Ввод/вывод консоли. Весь платформенный код (ANSI, VT, UTF-8) живёт только здесь.
class Terminal {
public:
    Terminal();

    void print(const std::string& line);        // строка + перевод строки
    void printInline(const std::string& text);  // без перевода строки

    // Печатает приглашение и читает строку.
    // Возвращает false при EOF (Ctrl+D), тогда out не определён.
    bool readLine(const std::string& prompt, std::string& out);

    // Печатает приглашение и читает строку без отображения ввода (пароль).
    std::string readPassword(const std::string& prompt);

    void pause(int milliseconds);               // пауза в миллисекундах

    // Отключает паузы (ключ --no-delay).
    void setNoDelay(bool value) { no_delay_ = value; }

    // Текущее локальное время в формате "HH:MM" (формируется только здесь).
    std::string localTimeHM() const;

private:
    bool no_delay_ = false;
};
