#pragma once

#include <functional>
#include <string>
#include <vector>

// Результат одного теста.
struct TestResult {
    std::string name;
    bool passed = false;
    std::vector<std::string> failures;  // непустой при passed == false
};

// Реестр тестов. Заполняется через статическую инициализацию (макрос TEST).
class TestRegistry {
public:
    static TestRegistry& instance();

    void add(const std::string& name, std::function<void()> fn);

    // Запускает тест по индексу; перехватывает исключения.
    TestResult run(std::size_t index);

    // Запускает все тесты подряд.
    std::vector<TestResult> runAll();

    std::size_t count() const { return tests_.size(); }
    const std::vector<std::string>& names() const { return names_; }

private:
    std::vector<std::string> names_;
    std::vector<std::function<void()>> tests_;
};

// Регистрирует тест при инициализации объектного файла.
// Использование:
//   TEST(my_test_name) {
//       EXPECT_EQ(1 + 1, 2);
//   }
#define TEST(name) \
    static void _testfn_##name(); \
    static const bool _testreg_##name = \
        (TestRegistry::instance().add(#name, _testfn_##name), true); \
    static void _testfn_##name()
