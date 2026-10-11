#include "ui/FileManagerScreen.h"

#include <algorithm>
#include <string>
#include <vector>

#include "core/Host.h"
#include "core/Session.h"
#include "core/VirtualFs.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

using namespace ftxui;

// Элемент списка панели.
struct Entry {
    std::string name;
    bool is_dir = false;
    bool parent = false;  // запись ".."
};

// Одна панель: каталог, содержимое и позиция выбора.
struct Panel {
    std::string dir;
    std::vector<Entry> entries;
    int selected = 0;
};

std::string joinPath(const std::string& base, const std::string& name) {
    return (base == "/") ? "/" + name : base + "/" + name;
}

// Корень ФС — единственное место, где не показываем "..".
bool isRootDir(const std::string& dir) {
    return dir.empty() || dir == "/";
}

// Перечитывает каталог: "..", затем каталоги, затем файлы, по алфавиту.
void refreshPanel(const Host& host, Panel& panel, bool show_hidden) {
    std::vector<Entry> dirs;
    std::vector<Entry> files;
    for (const std::string& raw : VirtualFs::list(host, panel.dir, show_hidden)) {
        Entry entry;
        if (!raw.empty() && raw.back() == '/') {
            entry.name = raw.substr(0, raw.size() - 1);
            entry.is_dir = true;
            dirs.push_back(entry);
        } else {
            entry.name = raw;
            files.push_back(entry);
        }
    }
    auto by_name = [](const Entry& a, const Entry& b) { return a.name < b.name; };
    std::sort(dirs.begin(), dirs.end(), by_name);
    std::sort(files.begin(), files.end(), by_name);

    panel.entries.clear();
    if (!isRootDir(panel.dir)) {
        Entry up;
        up.name = "..";
        up.is_dir = true;
        up.parent = true;
        panel.entries.push_back(up);
    }
    panel.entries.insert(panel.entries.end(), dirs.begin(), dirs.end());
    panel.entries.insert(panel.entries.end(), files.begin(), files.end());

    if (panel.selected < 0) panel.selected = 0;
    if (panel.selected >= static_cast<int>(panel.entries.size())) {
        panel.selected = panel.entries.empty() ? 0 : static_cast<int>(panel.entries.size()) - 1;
    }
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string line;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(line);
            line.clear();
        } else {
            line += c;
        }
    }
    lines.push_back(line);
    return lines;
}

class FileManager {
public:
    FileManager(Session& session, ScreenInteractive& screen)
        : session_(session), screen_(screen) {
        panels_[0].dir = session.currentDir();
        panels_[1].dir = session.currentDir();
        refresh();
    }

    Element Render() {
        // Скрываем курсор на время работы экрана (FTXUI вернёт его при выходе).
        Screen::Cursor cursor;
        cursor.x = 0;
        cursor.y = 0;
        cursor.shape = Screen::Cursor::Hidden;
        screen_.SetCursor(cursor);

        if (view_) return RenderViewer();

        // На первом кадре размер экрана ещё не известен (dimy() == 0) — тогда не
        // обрезаем список, иначе была бы видна только первая строка.
        int list_h = screen_.dimy() - 6;
        if (list_h < 1) {
            list_h = 1;
            list_h = std::max(list_h, static_cast<int>(panels_[0].entries.size()));
            list_h = std::max(list_h, static_cast<int>(panels_[1].entries.size()));
        }

        Element body = hbox({
            buildPanel(panels_[0], active_ == 0, list_h),
            buildPanel(panels_[1], active_ == 1, list_h),
        });

        Element hints =
            text("Tab Panel  Enter Open  F5 Copy  h Hidden  F10 Quit") | hcenter |
            color(Color::GrayLight);
        std::string status_text = show_hidden_ ? "Hidden files: on" : "Hidden files: off";
        if (!status_.empty()) status_text += "  |  " + status_;
        Element status = text(status_text) | color(Color::White);

        return vbox({body | flex, hints, status}) | bgcolor(Color::Black);
    }

    bool OnEvent(Event e) {
        if (view_) return OnViewerEvent(e);

        if (e == Event::Escape || e == Event::F10 || e == Event::Character('q')) {
            screen_.Exit();
            return true;
        }
        if (e == Event::Tab) {
            active_ = 1 - active_;
            return true;
        }
        if (e == Event::Character('h')) {
            show_hidden_ = !show_hidden_;
            refresh();
            return true;
        }

        Panel& panel = panels_[active_];
        if (e == Event::ArrowUp) {
            if (panel.selected > 0) --panel.selected;
            return true;
        }
        if (e == Event::ArrowDown) {
            if (panel.selected + 1 < static_cast<int>(panel.entries.size())) ++panel.selected;
            return true;
        }
        if (e == Event::PageUp) {
            panel.selected -= 10;
            if (panel.selected < 0) panel.selected = 0;
            return true;
        }
        if (e == Event::PageDown) {
            panel.selected += 10;
            if (panel.selected >= static_cast<int>(panel.entries.size())) {
                panel.selected = panel.entries.empty() ? 0
                                                       : static_cast<int>(panel.entries.size()) - 1;
            }
            return true;
        }
        if (e == Event::Home) {
            panel.selected = 0;
            return true;
        }
        if (e == Event::End) {
            if (!panel.entries.empty()) panel.selected = static_cast<int>(panel.entries.size()) - 1;
            return true;
        }
        if (e == Event::Return) {
            openSelected();
            return true;
        }
        if (e == Event::F5 || e == Event::Character('c')) {
            copySelected();
            return true;
        }
        return false;
    }

private:
    Element buildPanel(Panel& panel, bool active, int list_h) {
        int offset = panel.selected;
        if (offset < 0) offset = 0;
        if (panel.selected >= offset + list_h) offset = panel.selected - list_h + 1;
        if (offset < 0) offset = 0;

        Elements rows;
        for (int i = offset; i < static_cast<int>(panel.entries.size()) && i < offset + list_h;
             ++i) {
            const Entry& entry = panel.entries[i];
            std::string label = entry.parent ? "../"
                                : entry.is_dir ? entry.name + "/"
                                               : entry.name;
            Element row = text(label);
            if (i == panel.selected) {
                row = row | bold;
                if (active) row = row | inverted;
            }
            rows.push_back(row);
        }
        while (static_cast<int>(rows.size()) < list_h) rows.push_back(text(""));

        return vbox({
                   text(panel.dir) | bold | hcenter,
                   separator(),
                   vbox(rows),
               }) |
               borderDouble | flex | color(active ? Color::Yellow : Color::White) |
               bgcolor(Color::Blue);
    }

    Element RenderViewer() {
        int view_h = screen_.dimy() - 6;
        if (view_h < 1) view_h = 1;

        int max_scroll = static_cast<int>(view_lines_.size()) - view_h;
        if (max_scroll < 0) max_scroll = 0;
        if (view_scroll_ < 0) view_scroll_ = 0;
        if (view_scroll_ > max_scroll) view_scroll_ = max_scroll;

        Elements rows;
        for (int i = view_scroll_;
             i < static_cast<int>(view_lines_.size()) && i < view_scroll_ + view_h; ++i) {
            rows.push_back(text(view_lines_[i]));
        }
        while (static_cast<int>(rows.size()) < view_h) rows.push_back(text(""));

        return vbox({
                   text("View: " + view_name_) | bold | hcenter,
                   separator(),
                   vbox(rows),
                   separator(),
                   text("Esc Close  Up/Down Scroll") | hcenter,
               }) |
               borderDouble | flex | color(Color::White) | bgcolor(Color::Blue);
    }

    bool OnViewerEvent(Event e) {
        if (e == Event::Escape) {
            view_ = false;
            return true;
        }
        if (e == Event::ArrowDown) {
            ++view_scroll_;
            return true;
        }
        if (e == Event::ArrowUp) {
            if (view_scroll_ > 0) --view_scroll_;
            return true;
        }
        if (e == Event::PageDown) {
            view_scroll_ += 10;
            return true;
        }
        if (e == Event::PageUp) {
            view_scroll_ -= 10;
            if (view_scroll_ < 0) view_scroll_ = 0;
            return true;
        }
        if (e == Event::Home) {
            view_scroll_ = 0;
            return true;
        }
        if (e == Event::End) {
            view_scroll_ = static_cast<int>(view_lines_.size());
            return true;
        }
        return true;  // в просмотре клавиши панелей не работают
    }

    void openSelected() {
        Panel& panel = panels_[active_];
        if (panel.entries.empty()) return;
        const Entry& entry = panel.entries[panel.selected];

        if (entry.parent) {
            panel.dir = VirtualFs::normalize(panel.dir, "..");
            panel.selected = 0;
            refresh();
            return;
        }
        if (entry.is_dir) {
            panel.dir = joinPath(panel.dir, entry.name);
            panel.selected = 0;
            refresh();
            return;
        }

        bool ok = false;
        std::string content =
            VirtualFs::readFile(session_.currentHost(), joinPath(panel.dir, entry.name), ok);
        if (!ok) {
            status_ = "Cannot open file";
            return;
        }
        view_name_ = entry.name;
        view_lines_ = splitLines(content);
        view_scroll_ = 0;
        view_ = true;
    }

    void copySelected() {
        Panel& source = panels_[active_];
        if (source.entries.empty()) return;
        const Entry& entry = source.entries[source.selected];
        if (entry.is_dir) {
            status_ = "Cannot copy a directory";
            return;
        }

        Host& host = session_.currentHost();
        bool ok = false;
        std::string content =
            VirtualFs::readFile(host, joinPath(source.dir, entry.name), ok);
        if (!ok) {
            status_ = "Cannot copy file";
            return;
        }

        std::string dest_path = joinPath(panels_[1 - active_].dir, entry.name);
        bool existed = VirtualFs::exists(host, dest_path);
        if (!VirtualFs::writeFile(host, dest_path, content)) {
            status_ = "Cannot copy file";
            return;
        }
        status_ = existed ? "Overwritten" : ("Copied " + entry.name);
        refresh();
    }

    void refresh() {
        Host& host = session_.currentHost();
        refreshPanel(host, panels_[0], show_hidden_);
        refreshPanel(host, panels_[1], show_hidden_);
    }

    Session& session_;
    ScreenInteractive& screen_;
    Panel panels_[2];
    int active_ = 0;
    std::string status_;
    bool show_hidden_ = false;
    bool view_ = false;
    std::string view_name_;
    std::vector<std::string> view_lines_;
    int view_scroll_ = 0;
};

}  // namespace

void runFileManager(Session& session) {
#ifdef _WIN32
    // UTF-8, чтобы рамки рисовались правильно.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    auto screen = ScreenInteractive::Fullscreen();
    FileManager fm(session, screen);
    auto component = Renderer([&] { return fm.Render(); });
    component = CatchEvent(component, [&](Event e) { return fm.OnEvent(e); });
    screen.Loop(component);
}
