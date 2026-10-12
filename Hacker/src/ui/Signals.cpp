#include "ui/Signals.h"

#include <csignal>

namespace {

// Флаг, безопасный для установки из обработчика сигнала.
volatile std::sig_atomic_t g_interrupted = 0;

extern "C" void handleSignal(int) {
    g_interrupted = 1;
}

}  // namespace

namespace Signals {

void install() {
#ifdef _WIN32
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);
#else
    struct sigaction action {};
    action.sa_handler = handleSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;  // без SA_RESTART: прерываем getline/read
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
#endif
}

bool interrupted() {
    return g_interrupted != 0;
}

}  // namespace Signals
