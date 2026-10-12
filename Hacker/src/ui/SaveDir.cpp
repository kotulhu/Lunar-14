#include "ui/SaveDir.h"

#include <cstdlib>
#include <filesystem>

std::string defaultSaveDir() {
    namespace fs = std::filesystem;
    fs::path base;

#if defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    base = appdata ? fs::path(appdata) : fs::current_path();
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    fs::path h = home ? fs::path(home) : fs::current_path();
    base = h / "Library" / "Application Support";
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && *xdg) {
        base = fs::path(xdg);
    } else {
        const char* home = std::getenv("HOME");
        fs::path h = home ? fs::path(home) : fs::current_path();
        base = h / ".local" / "share";
    }
#endif

    return (base / kGameId).string();
}
