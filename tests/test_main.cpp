#include "test_framework.hpp"

#include <cstring>

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int ran = 0, failed_tests = 0;
    for (auto& c : tf::registry()) {
        if (filter && !std::strstr(c.name.c_str(), filter)) continue;
        const int before = tf::failures();
        std::printf("[ RUN  ] %s\n", c.name.c_str());
        c.fn();
        const bool ok = tf::failures() == before;
        std::printf("[ %s ] %s\n", ok ? " OK " : "FAIL", c.name.c_str());
        ++ran;
        if (!ok) ++failed_tests;
    }
    std::printf("\n%d tests, %d failed\n", ran, failed_tests);
    return failed_tests == 0 ? 0 : 1;
}
