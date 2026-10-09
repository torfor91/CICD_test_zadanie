#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <future>
#include <mutex>
#include <atomic>

namespace tf {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

struct AssertFail {
    std::string what;
};

struct Registrar {
    Registrar(const std::string& n, std::function<void()> f) {
        registry().push_back({n, std::move(f)});
    }
};

inline int run_all() {
    auto& tests = registry();
    std::mutex mtx;
    struct Row { std::string name; bool ok; std::string err; };
    std::vector<Row> rows;
    std::atomic<int> passed{0}, failed{0};

    std::vector<std::future<void>> futs;
    for (auto& t : tests) {
        futs.push_back(std::async(std::launch::async, [&, t]() {
            Row row{t.name, true, ""};
            try { t.fn(); }
            catch (const AssertFail& a) { row.ok = false; row.err = a.what; }
            catch (const std::exception& e) { row.ok = false; row.err = e.what(); }
            catch (...) { row.ok = false; row.err = "unknown exception"; }
            {
                std::lock_guard<std::mutex> lk(mtx);
                if (row.ok) ++passed; else ++failed;
                rows.push_back(std::move(row));
            }
        }));
    }
    for (auto& f : futs) f.get();

    std::cout << "\n--- Test summary ---\n";
    for (auto& r : rows)
        std::cout << (r.ok ? "[PASS] " : "[FAIL] ") << r.name
                  << (r.ok ? "" : ("  -> " + r.err)) << "\n";
    std::cout << "Passed: " << passed << ", Failed: " << failed
              << ", Total: " << rows.size() << "\n";
    return failed == 0 ? 0 : 1;
}

} // namespace tf

#define TEST(name)                                                        \
    static void test_##name();                                            \
    static tf::Registrar reg_##name(#name, test_##name);                  \
    static void test_##name()

#define ASSERT_TRUE(cond)                                                 \
    do { if (!(cond)) throw tf::AssertFail{                               \
        std::string(__FILE__) + ":" + std::to_string(__LINE__) +          \
        " ASSERT_TRUE(" #cond ") failed"}; } while (0)

#define ASSERT_EQ(a, b)                                                   \
    do { auto _a = (a); auto _b = (b);                                    \
         if (!(_a == _b)) throw tf::AssertFail{                           \
            std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
            " ASSERT_EQ(" #a ", " #b ") failed"}; } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))