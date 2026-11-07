#include "registry.h"

#include <stdexcept>

#include "assert.h"

TestRegistry& TestRegistry::instance() {
    static TestRegistry inst;
    return inst;
}

void TestRegistry::add(const std::string& name, std::function<void()> fn) {
    names_.push_back(name);
    tests_.push_back(std::move(fn));
}

TestResult TestRegistry::run(std::size_t index) {
    TestResult result;
    result.name = names_[index];

    testing::g_failures = &result.failures;
    try {
        tests_[index]();
    } catch (const TestAborted&) {
        // Уже записано в failures через ASSERT_*.
    } catch (const std::exception& e) {
        testing::recordFailure(std::string("Unexpected exception: ") + e.what());
    } catch (...) {
        testing::recordFailure("Unexpected unknown exception");
    }
    testing::g_failures = nullptr;

    result.passed = result.failures.empty();
    return result;
}

std::vector<TestResult> TestRegistry::runAll() {
    std::vector<TestResult> results;
    results.reserve(tests_.size());
    for (std::size_t i = 0; i < tests_.size(); ++i) {
        results.push_back(run(i));
    }
    return results;
}
