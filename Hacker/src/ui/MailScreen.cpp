#include "ui/MailScreen.h"

#include <algorithm>
#include <string>
#include <vector>

#include "core/Mailbox.h"
#include "core/Notes.h"
#include "core/Session.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

using namespace ftxui;

// Полноэкранный почтовый клиент: список писем сверху, предпросмотр снизу.
class Mail {
public:
    Mail(Session& session, ScreenInteractive& screen)
        : session_(session), screen_(screen) {}

    Element Render() {
        // Скрываем курсор на время работы экрана.
        Screen::Cursor cursor;
        cursor.x = 0;
        cursor.y = 0;
        cursor.shape = Screen::Cursor::Hidden;
        screen_.SetCursor(cursor);

        return view_ ? RenderView() : RenderList();
    }

    bool OnEvent(Event event) {
        if (view_) return OnViewEvent(event);
        return OnListEvent(event);
    }

private:
    const std::vector<Message>& messages() const { return session_.mailbox().messages(); }
    int count() const { return static_cast<int>(messages().size()); }

    Element makeRow(const Message& message, bool selected) const {
        Element status = text(message.read ? " " : "N");
        Element number = text(std::to_string(message.id));
        Element from = text(message.from);
        Element subject = text(message.subject);
        Element date = text(message.date);

        if (!message.read) {  // непрочитанные — жирным
            status = status | bold;
            from = from | bold;
            subject = subject | bold;
            date = date | bold;
        }

        Element row = hbox({
            status | size(WIDTH, EQUAL, status_w_),
            number | size(WIDTH, EQUAL, num_w_),
            from | size(WIDTH, EQUAL, from_w_),
            subject | size(WIDTH, EQUAL, subj_w_),
            date | size(WIDTH, EQUAL, date_w_),
        });
        if (selected) row = row | inverted;
        return row;
    }

    Element RenderList() {
        int dimx = screen_.dimx();
        if (dimx <= 0) dimx = 80;
        int dimy = screen_.dimy();
        if (dimy <= 0) dimy = 24;

        // Ширина столбцов.
        status_w_ = 2;
        num_w_ = 4;
        date_w_ = 20;
        int rest = (dimx - 4) - (status_w_ + num_w_ + date_w_);
        if (rest < 16) rest = 16;
        from_w_ = rest / 2;
        subj_w_ = rest - from_w_;

        std::string left = "Mail - " + session_.user() + "@" + session_.hostName();
        std::string right = "Messages: " + std::to_string(count()) +
                            "  Unread: " + std::to_string(session_.mailbox().unreadCount());
        Element header = hbox({text(left) | bold, filler(), text(right)});

        Element columns = hbox({
                              text(" ") | size(WIDTH, EQUAL, status_w_),
                              text("#") | size(WIDTH, EQUAL, num_w_),
                              text("От") | size(WIDTH, EQUAL, from_w_),
                              text("Тема") | size(WIDTH, EQUAL, subj_w_),
                              text("Дата") | size(WIDTH, EQUAL, date_w_),
                          }) |
                          color(Color::GrayLight);

        // Высота списка: остальное отдаём предпросмотру. Если размер экрана ещё
        // не известен (первый кадр), не обрезаем список.
        int list_h = dimy - 12;
        if (list_h < 1) list_h = std::max(1, count());

        int offset = selected_;
        if (offset > count() - list_h) offset = count() - list_h;
        if (offset < 0) offset = 0;

        Elements rows;
        for (int i = offset; i < count() && i < offset + list_h; ++i) {
            rows.push_back(makeRow(messages()[i], i == selected_));
        }
        while (static_cast<int>(rows.size()) < list_h) rows.push_back(text(""));
        Element list = vbox(std::move(rows));

        // Предпросмотр выбранного письма.
        Element preview = text("Ящик пуст.");
        if (count() > 0) {
            int index = selected_ < 0 ? 0 : selected_;
            if (index >= count()) index = count() - 1;
            const Message& message = messages()[index];
            std::string body;
            for (std::size_t i = 0; i < message.body.size(); ++i) {
                if (i) body += " ";
                body += message.body[i];
            }
            preview = vbox({
                          hbox({text("От: ") | bold, text(message.from)}),
                          hbox({text("Тема: ") | bold, text(message.subject)}),
                          hbox({text("Дата: ") | bold, text(message.date)}),
                          separatorEmpty(),
                          paragraph(body),
                      }) |
                      flex;
        }

        Element hints =
            text("Up/Down Select  Enter Open  F10/q Quit") | hcenter | color(Color::GrayLight);

        return vbox({header, separator(), columns, list, separator(), preview, separator(), hints}) |
               borderDouble | color(Color::White) | bgcolor(Color::Blue) | flex;
    }

    Element RenderView() {
        if (view_index_ < 0 || view_index_ >= count()) {
            view_ = false;
            return RenderList();
        }
        const Message& message = messages()[view_index_];

        int dimy = screen_.dimy();
        if (dimy <= 0) dimy = 24;
        int body_h = dimy - 10;  // 8 строк шапки/подсказки + 2 рамки
        if (body_h < 1) body_h = std::max(1, static_cast<int>(message.body.size()));

        int max_scroll = static_cast<int>(message.body.size()) - body_h;
        if (max_scroll < 0) max_scroll = 0;
        if (view_scroll_ < 0) view_scroll_ = 0;
        if (view_scroll_ > max_scroll) view_scroll_ = max_scroll;

        Elements body;
        for (int i = view_scroll_;
             i < static_cast<int>(message.body.size()) && i < view_scroll_ + body_h; ++i) {
            body.push_back(text(message.body[i]));
        }
        while (static_cast<int>(body.size()) < body_h) body.push_back(text(""));

        Element hints =
            text("Esc Back  Up/Down Scroll  F10/q Quit") | hcenter | color(Color::GrayLight);

        return vbox({
                   text("Message #" + std::to_string(message.id)) | bold | hcenter,
                   separator(),
                   hbox({text("От: ") | bold, text(message.from)}),
                   hbox({text("Тема: ") | bold, text(message.subject)}),
                   hbox({text("Дата: ") | bold, text(message.date)}),
                   separator(),
                   vbox(std::move(body)),
                   separator(),
                   hints,
               }) |
               borderDouble | color(Color::White) | bgcolor(Color::Blue) | flex;
    }

    bool OnListEvent(Event event) {
        if (event == Event::F10 || event == Event::Character('q') || event == Event::Escape) {
            screen_.Exit();
            return true;
        }
        if (count() == 0) return true;

        if (event == Event::ArrowUp) {
            if (selected_ > 0) --selected_;
        } else if (event == Event::ArrowDown) {
            if (selected_ + 1 < count()) ++selected_;
        } else if (event == Event::PageUp) {
            selected_ -= 10;
            if (selected_ < 0) selected_ = 0;
        } else if (event == Event::PageDown) {
            selected_ += 10;
            if (selected_ >= count()) selected_ = count() - 1;
        } else if (event == Event::Home) {
            selected_ = 0;
        } else if (event == Event::End) {
            selected_ = count() - 1;
        } else if (event == Event::Return) {
            openSelected();
        }
        return true;
    }

    bool OnViewEvent(Event event) {
        if (event == Event::Escape) {
            view_ = false;
            return true;
        }
        if (event == Event::F10 || event == Event::Character('q')) {
            screen_.Exit();
            return true;
        }
        if (event == Event::ArrowDown) {
            ++view_scroll_;
        } else if (event == Event::ArrowUp) {
            if (view_scroll_ > 0) --view_scroll_;
        } else if (event == Event::PageDown) {
            view_scroll_ += 10;
        } else if (event == Event::PageUp) {
            view_scroll_ -= 10;
            if (view_scroll_ < 0) view_scroll_ = 0;
        } else if (event == Event::Home) {
            view_scroll_ = 0;
        } else if (event == Event::End) {
            view_scroll_ = 1 << 30;
        }
        return true;
    }

    void openSelected() {
        if (selected_ < 0 || selected_ >= count()) return;
        session_.mailbox().markRead(selected_);  // открытие помечает письмо прочитанным
        const Message& message = messages()[selected_];
        std::string text = message.from + " " + message.subject;
        for (const std::string& line : message.body) text += " " + line;
        noteDocument(session_, "mail", session_.hostName(), message.subject,
                     "mail:" + std::to_string(message.id), text);
        view_index_ = selected_;
        view_scroll_ = 0;
        view_ = true;
    }

    Session& session_;
    ScreenInteractive& screen_;

    int status_w_ = 2;
    int num_w_ = 4;
    int from_w_ = 26;
    int subj_w_ = 28;
    int date_w_ = 20;

    int selected_ = 0;
    bool view_ = false;
    int view_index_ = 0;
    int view_scroll_ = 0;
};

}  // namespace

void runMail(Session& session) {
#if defined(_WIN32)
    // UTF-8, чтобы рамки и кириллица рисовались правильно.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    auto screen = ScreenInteractive::Fullscreen();
    Mail mail(session, screen);
    auto component = Renderer([&] { return mail.Render(); });
    component = CatchEvent(component, [&](Event e) { return mail.OnEvent(e); });
    screen.Loop(component);
}
