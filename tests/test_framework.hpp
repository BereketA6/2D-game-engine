#pragma once
// A ~40-line test framework so the project has zero external dependencies.
// TEST(name) registers a function at static-initialisation time; CHECK
// records a failure (with file:line) but keeps running the test.

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace tf {

struct Case { std::string name; std::function<void()> fn; };

inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

}  // namespace tf

#define TF_CONCAT2(a, b) a##b
#define TF_CONCAT(a, b) TF_CONCAT2(a, b)

#define TEST(name)                                                         \
    static void name();                                                    \
    static tf::Registrar TF_CONCAT(reg_, name)(#name, name);               \
    static void name()

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("    FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++tf::failures();                                              \
        }                                                                  \
    } while (0)

#define CHECK_EQ(a, b)                                                     \
    do {                                                                   \
        auto tf_a_ = (a); auto tf_b_ = (b);                                \
        if (!(tf_a_ == tf_b_)) {                                           \
            std::printf("    FAILED %s:%d: %s == %s  (%lld vs %lld)\n",    \
                        __FILE__, __LINE__, #a, #b,                        \
                        static_cast<long long>(tf_a_),                     \
                        static_cast<long long>(tf_b_));                    \
            ++tf::failures();                                              \
        }                                                                  \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                              \
    do {                                                                   \
        double tf_a_ = (a), tf_b_ = (b);                                   \
        if (!(tf_a_ - tf_b_ <= (tol) && tf_b_ - tf_a_ <= (tol))) {         \
            std::printf("    FAILED %s:%d: %s ~= %s  (%g vs %g)\n",        \
                        __FILE__, __LINE__, #a, #b, tf_a_, tf_b_);         \
            ++tf::failures();                                              \
        }                                                                  \
    } while (0)
