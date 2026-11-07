#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "env.h"
#include "registry.h"

// Ищет data/ рядом с исполняемым файлом или в стандартных местах сборки.
static std::string findDataDir(const char* argv0) {
    namespace fs = std::filesystem;
    std::error_code ec;

    // Рядом с исполняемым файлом (build/data/).
    fs::path exe = fs::absolute(argv0, ec);
    if (!ec && exe.has_parent_path()) {
        fs::path candidate = exe.parent_path() / "data";
        if (fs::exists(candidate / "world.json", ec)) return candidate.string();
    }

    // Стандартные места при запуске из корня проекта или build/.
    for (const char* p : {"data", "../data", "../../data"}) {
        if (fs::exists(std::string(p) + "/world.json", ec)) return p;
    }
    return {};
}

int main(int argc, char** argv) {
    TestEnv::data_dir = findDataDir(argc > 0 ? argv[0] : "");

    if (argc > 1 && std::string(argv[1]) == "--all") {
        const std::vector<TestResult> results = TestRegistry::instance().runAll();
        int failed = 0;
        for (const auto& result : results) {
            std::cout << (result.passed ? "PASS " : "FAIL ") << result.name << "\n";
            for (const auto& failure : result.failures) {
                std::cout << "  " << failure << "\n";
            }
            if (!result.passed) ++failed;
        }
        std::cout << results.size() - static_cast<std::size_t>(failed) << " passed, "
                  << failed << " failed\n";
        return failed == 0 ? 0 : 1;
    }

    using namespace ftxui;

    auto& reg = TestRegistry::instance();
    const int n = static_cast<int>(reg.count());

    // Пункты меню: имена тестов + разделители.
    std::vector<std::string> entries(reg.names().begin(), reg.names().end());
    entries.push_back("── Run all ──");
    entries.push_back("── Quit ──");

    int selected = 0;
    int tab_index = 0;          // 0 = меню, 1 = результаты
    std::vector<TestResult> results;

    auto screen = ScreenInteractive::Fullscreen();

    // ── Рендер результатов ───────────────────────────────────────────────────
    auto renderResults = [&]() -> Element {
        Elements elems;
        int pass_count = 0, fail_count = 0;

        for (const auto& r : results) {
            if (r.passed) ++pass_count; else ++fail_count;
        }
        Color sum_col = fail_count == 0 ? Color::Green : Color::Red;
        std::string summary = " " + std::to_string(pass_count) + " passed, " +
                              std::to_string(fail_count) + " failed";
        elems.push_back(text(summary) | color(sum_col) | bold);
        elems.push_back(separator());

        for (const auto& r : results) {
            Color tag_col = r.passed ? Color::Green : Color::Red;
            elems.push_back(hbox({
                text(r.passed ? " PASS " : " FAIL ") | color(tag_col) | bold,
                text(" " + r.name),
            }));

            for (const auto& f : r.failures) {
                // Разбиваем многострочное сообщение на строки.
                std::istringstream ss(f);
                std::string line;
                bool first = true;
                while (std::getline(ss, line)) {
                    elems.push_back(
                        text((first ? "       " : "         ") + line) |
                        color(Color::Red));
                    first = false;
                }
                elems.push_back(text(""));
            }
        }

        elems.push_back(separator());
        elems.push_back(text(" Enter / Q — вернуться в меню") | dim);

        return vbox(std::move(elems)) | vscroll_indicator | frame | flex;
    };

    // ── Меню ─────────────────────────────────────────────────────────────────
    auto menu = Menu(&entries, &selected);

    // Единый компонент: роутит события по текущему tab_index.
    auto component = Renderer(menu, [&]() -> Element {
        Element body;
        if (tab_index == 0) {
            body = vbox({
                menu->Render() | frame | flex,
                separator(),
                text(" ↑↓ навигация  ·  Enter: запуск  ·  Q: выход") | dim,
            });
        } else {
            body = renderResults();
        }
        return vbox({
            text(" Lunar-14  Test Suite ") | bold | center,
            separator(),
            body,
        }) | border;
    }) | CatchEvent([&](Event e) -> bool {
        if (tab_index == 0) {
            // Q — выход из приложения.
            if (e == Event::Character('q') || e == Event::Character('Q')) {
                screen.ExitLoopClosure()();
                return true;
            }
            // Enter — запуск теста или Run all / Quit.
            if (e == Event::Return) {
                if (selected < n) {
                    results = {reg.run(static_cast<std::size_t>(selected))};
                    tab_index = 1;
                } else if (selected == n) {   // Run all
                    results = reg.runAll();
                    tab_index = 1;
                } else {                       // Quit
                    screen.ExitLoopClosure()();
                }
                return true;
            }
            // Все остальные события — в меню (навигация ↑↓).
            return menu->OnEvent(e);
        } else {
            // Экран результатов: любой Enter или Q возвращает в меню.
            if (e == Event::Return ||
                e == Event::Character('q') || e == Event::Character('Q')) {
                tab_index = 0;
                return true;
            }
            return false;
        }
    });

    screen.Loop(component);
    return 0;
}
