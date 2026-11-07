#pragma once

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Бросается ASSERT_*-макросами, чтобы немедленно прервать тест.
struct TestAborted {};

namespace testing {

// Указывает на список ошибок текущего теста. Устанавливается TestRegistry
// перед вызовом тест-функции и сбрасывается сразу после.
inline thread_local std::vector<std::string>* g_failures = nullptr;

inline void recordFailure(const std::string& msg) {
    if (g_failures) g_failures->push_back(msg);
}

// ── Внутренние реализации ────────────────────────────────────────────────────

template <typename A, typename B>
inline bool checkEq(const A& a, const B& b,
                    const char* file, int line,
                    const char* sa, const char* sb) {
    if (!(a == b)) {
        std::ostringstream oss;
        oss << file << ":" << line << "  EXPECT_EQ(" << sa << ", " << sb << ")\n"
            << "    expected: " << b << "\n"
            << "    got:      " << a;
        recordFailure(oss.str());
        return false;
    }
    return true;
}

inline bool checkTrue(bool v, const char* file, int line, const char* expr) {
    if (!v) {
        std::ostringstream oss;
        oss << file << ":" << line << "  EXPECT_TRUE(" << expr << ")  is false";
        recordFailure(oss.str());
    }
    return v;
}

}  // namespace testing

// ── Публичные макросы ────────────────────────────────────────────────────────

// Накапливает ошибку; тест продолжается.
#define EXPECT_EQ(a, b) \
    testing::checkEq((a), (b), __FILE__, __LINE__, #a, #b)

#define EXPECT_TRUE(expr) \
    testing::checkTrue(static_cast<bool>(expr), __FILE__, __LINE__, #expr)

#define EXPECT_FALSE(expr) \
    testing::checkTrue(!static_cast<bool>(expr), __FILE__, __LINE__, "!" #expr)

// Накапливает ошибку; тест прерывается немедленно через исключение.
#define ASSERT_EQ(a, b) \
    do { \
        if (!testing::checkEq((a), (b), __FILE__, __LINE__, #a, #b)) \
            throw TestAborted{}; \
    } while (false)

// Проверяет, что stmt бросает exc_type; иначе накапливает ошибку.
#define EXPECT_THROW(stmt, exc_type) \
    do { \
        bool _threw = false; \
        try { (void)(stmt); } \
        catch (const exc_type&) { _threw = true; } \
        catch (...) {} \
        if (!_threw) { \
            testing::recordFailure( \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
                "  EXPECT_THROW(" #stmt ", " #exc_type ")  did not throw"); \
        } \
    } while (false)
