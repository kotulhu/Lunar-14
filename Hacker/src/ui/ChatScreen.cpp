#include "ui/ChatScreen.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "core/ChatStore.h"
#include "core/ResponseProvider.h"
#include "core/Session.h"
#include "ui/Terminal.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

using namespace ftxui;

const char* const kMe = "me";

// Переносит строку по ширине терминала. Очень длинные слова режет посимвольно.
std::vector<std::string> wrapText(const std::string& text, int width) {
    if (width < 1) width = 1;
    std::vector<std::string> lines;
    std::string line;
    int line_w = 0;
    for (const std::string& glyph : Utf8ToGlyphs(text)) {
        int glyph_w = string_width(glyph);
        if (line_w + glyph_w > width && line_w > 0) {
            lines.push_back(line);
            line.clear();
            line_w = 0;
        }
        if (line.empty() && glyph == " ") continue;  // строка не начинается с пробела
        line += glyph;
        line_w += glyph_w;
    }
    lines.push_back(line);
    return lines;
}

// Удаляет последний кодпоинт UTF-8 (Backspace по кириллице).
void removeLastGlyph(std::string& text) {
    if (text.empty()) return;
    std::size_t i = text.size();
    do {
        --i;
    } while (i > 0 && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80);
    text.erase(i);
}

// Полноэкранный чат: список контактов слева, переписка справа, строка ввода снизу.
class Chat {
public:
    Chat(Session& session, ScreenInteractive& screen)
        : session_(session), screen_(screen) {
        refreshVisible();
        openCurrent();
    }

    ~Chat() { stopWorker(); }

    void start() { worker_ = std::thread([this] { workerLoop(); }); }

    void stopWorker() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        if (worker_.joinable()) worker_.join();
    }

    Element Render() {
        Screen::Cursor cursor;
        cursor.shape = Screen::Cursor::Hidden;
        screen_.SetCursor(cursor);

        refreshVisible();

        int dimx = screen_.dimx();
        if (dimx <= 0) dimx = 80;
        int dimy = screen_.dimy();
        if (dimy <= 0) dimy = 24;

        int contacts_w = 26;
        if (dimx < 64) contacts_w = std::max(12, dimx / 3);
        int feed_w = dimx - contacts_w - 6;
        if (feed_w < 12) feed_w = 12;

        Element hints =
            text("Tab Focus  Enter Send  PgUp/PgDn Scroll  F10 Quit") |
            hcenter | color(Color::GrayLight);
        Element status_line = text(status_) | hcenter | color(Color::Yellow);

        return vbox({
                   hbox({RenderContacts(contacts_w), RenderFeed(feed_w, dimy)}) | flex,
                   RenderInput(),
                   hints,
                   status_line,
               }) |
               borderDouble | color(Color::White) | bgcolor(Color::Blue) | flex;
    }

    bool OnEvent(Event event) {
        if (event == Event::Custom) {
            processReady();
            return true;
        }
        if (event == Event::F10 || event == Event::Escape) {
            screen_.Exit();
            return true;
        }
        if (event == Event::Tab) {
            input_focus_ = !input_focus_;
            return true;
        }
        if (event == Event::PageUp) {
            scroll_ += std::max(1, view_h_ - 1);
            return true;
        }
        if (event == Event::PageDown) {
            scroll_ -= std::max(1, view_h_ - 1);
            if (scroll_ < 0) scroll_ = 0;
            return true;
        }

        if (input_focus_) {
            if (event == Event::Return) {
                send();
            } else if (event == Event::Backspace) {
                removeLastGlyph(input_);
            } else if (event.is_character()) {
                input_ += event.character();
            }
            return true;
        }

        // Фокус на списке контактов.
        if (event == Event::ArrowUp) {
            move(-1);
        } else if (event == Event::ArrowDown) {
            move(1);
        } else if (event == Event::Return) {
            input_focus_ = true;
        }
        return true;
    }

private:
    void refreshVisible() {
        visible_ = session_.chat().visibleContacts();
        if (visible_.empty()) {
            selected_ = 0;
            return;
        }
        if (selected_ < 0) selected_ = 0;
        if (selected_ >= static_cast<int>(visible_.size())) {
            selected_ = static_cast<int>(visible_.size()) - 1;
        }
    }

    const Contact* current() const {
        if (selected_ < 0 || selected_ >= static_cast<int>(visible_.size())) return nullptr;
        return visible_[selected_];
    }

    void openCurrent() {
        const Contact* contact = current();
        if (contact) session_.chat().openContact(contact->id);
    }

    void move(int delta) {
        refreshVisible();
        if (visible_.empty()) return;
        int next = selected_ + delta;
        if (next < 0) next = 0;
        if (next >= static_cast<int>(visible_.size())) next = static_cast<int>(visible_.size()) - 1;
        if (next != selected_) {
            selected_ = next;
            scroll_ = 0;
            openCurrent();
        }
    }

    std::string nameFor(const std::string& id) const {
        if (id == kMe) return "Вы";
        const Contact* contact = session_.chat().find(id);
        return contact ? contact->name : id;
    }

    Element RenderContacts(int width) {
        Elements rows;
        rows.push_back(text("Контакты") | bold | hcenter);
        rows.push_back(separator());
        if (visible_.empty()) {
            rows.push_back(text("Нет контактов") | color(Color::GrayLight));
        }
        for (int i = 0; i < static_cast<int>(visible_.size()); ++i) {
            const Contact& contact = *visible_[i];
            Color dot = Color::GrayDark;
            if (contact.status == "online") {
                dot = Color::Green;
            } else if (contact.status == "away") {
                dot = Color::Yellow;
            }
            Element row = hbox({
                text(contact.name),
                text(" "),
                text("●") | color(dot),
            });
            int unread = session_.chat().unread(contact.id);
            if (unread > 0) {
                row = hbox({row, text(" (" + std::to_string(unread) + ")")});
            }
            bool highlighted = !input_focus_ && i == selected_;
            if (highlighted) row = row | inverted;
            rows.push_back(row);
        }
        Color border = input_focus_ ? Color::GrayLight : Color::Yellow;
        return vbox(std::move(rows)) | borderStyled(border) | size(WIDTH, EQUAL, width);
    }

    Element RenderFeed(int width, int dimy) {
        const Contact* contact = current();

        std::vector<std::string> lines;
        if (contact) {
            for (const ChatMessage& message : session_.chat().history(contact->id)) {
                std::string prefix =
                    "[" + message.time + "] " + nameFor(message.from) + ": ";
                for (std::string& wrapped : wrapText(prefix + message.text, width)) {
                    lines.push_back(std::move(wrapped));
                }
            }
        }

        int view_h = dimy - 8;
        if (view_h < 1) view_h = 1;
        view_h_ = view_h;

        int total = static_cast<int>(lines.size());
        int max_scroll = total - view_h;
        if (max_scroll < 0) max_scroll = 0;
        if (scroll_ > max_scroll) scroll_ = max_scroll;
        if (scroll_ < 0) scroll_ = 0;
        int start = total - view_h - scroll_;
        if (start < 0) start = 0;

        Elements body;
        for (int i = start; i < start + view_h; ++i) {
            body.push_back(i < total ? text(lines[i]) : text(""));
        }

        Element title;
        if (contact) {
            title = hbox({text(contact->name) | bold, text("  "),
                          text(contact->status) | color(Color::GrayLight)});
        } else {
            title = text("Нет переписки") | color(Color::GrayLight);
        }

        Color border = input_focus_ ? Color::GrayLight : Color::Yellow;
        return vbox({title, separator(), vbox(std::move(body))}) |
               borderStyled(border) | flex;
    }

    Element RenderInput() {
        Element content = hbox({
            text("> "),
            text(input_),
            text(input_focus_ ? "▌" : ""),
        });
        Color border = input_focus_ ? Color::Yellow : Color::GrayLight;
        return content | borderStyled(border);
    }

    void send() {
        if (input_.empty()) return;
        const Contact* contact = current();
        if (!contact) return;

        std::string text = input_;
        input_.clear();
        scroll_ = 0;
        session_.chat().openContact(contact->id);
        session_.chat().add(contact->id,
                            ChatMessage{kMe, text, terminal_.localTimeHM()});

        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(ReplyJob{contact->id, text});
        }
        cv_.notify_one();
        refreshStatus();
    }

    void workerLoop() {
        while (true) {
            ReplyJob job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
                if (stop_) return;
                job = queue_.front();
                queue_.pop_front();
            }

            // Ответ приходит не мгновенно: пауза 1-2 секунды.
            std::uniform_int_distribution<int> dist(1000, 2000);
            std::this_thread::sleep_for(std::chrono::milliseconds(dist(rng_)));

            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (stop_) return;
                ready_.push_back(job);
            }
            screen_.PostEvent(Event::Custom);
        }
    }

    // Выполняется в UI-потоке: добавляет готовые ответы в историю.
    void processReady() {
        while (true) {
            ReplyJob job;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (ready_.empty()) break;
                job = ready_.front();
                ready_.pop_front();
            }

            ResponseProvider* provider = session_.chatProvider();
            if (!provider) continue;
            ReplyRequest request{job.contact_id, job.player_text, session_,
                                 session_.chat().history(job.contact_id)};
            std::string answer = provider->reply(request);
            if (!answer.empty()) {
                session_.chat().add(
                    job.contact_id,
                    ChatMessage{job.contact_id, answer, terminal_.localTimeHM()});
            }
        }
        refreshStatus();
    }

    void refreshStatus() {
        std::string who;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!ready_.empty()) {
                who = ready_.front().contact_id;
            } else if (!queue_.empty()) {
                who = queue_.front().contact_id;
            }
        }
        if (who.empty()) {
            status_.clear();
            return;
        }
        const Contact* contact = session_.chat().find(who);
        status_ = (contact ? contact->name : who) + " печатает...";
    }

    Session& session_;
    ScreenInteractive& screen_;
    ::Terminal terminal_;

    std::vector<const Contact*> visible_;
    int selected_ = 0;
    bool input_focus_ = false;
    std::string input_;
    int scroll_ = 0;
    int view_h_ = 1;
    std::string status_;

    struct ReplyJob {
        std::string contact_id;
        std::string player_text;
    };

    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<ReplyJob> queue_;
    std::deque<ReplyJob> ready_;
    bool stop_ = false;
    std::thread worker_;
    std::mt19937 rng_{std::random_device{}()};
};

}  // namespace

void runChat(Session& session) {
#if defined(_WIN32)
    // UTF-8, чтобы кириллица и рамки рисовались правильно.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    auto screen = ScreenInteractive::Fullscreen();
    Chat chat(session, screen);
    chat.start();

    auto component = Renderer([&] { return chat.Render(); });
    component = CatchEvent(component, [&](Event e) { return chat.OnEvent(e); });
    screen.Loop(component);

    chat.stopWorker();
}
