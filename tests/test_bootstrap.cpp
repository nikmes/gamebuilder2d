// Minimal Catch2 event listener to clear GB2D_* env before each test case
#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <vector>
#include <string>
#include <cstring>

#if defined(_WIN32)
#  include <windows.h>
#  include <stdlib.h>
#else
extern char** environ;
#  include <cstdlib>
#endif

namespace {
void clear_gb2d_env() {
#if defined(_WIN32)
    LPCH env = GetEnvironmentStringsA();
    if (!env) return;
    std::vector<std::string> to_clear;
    for (LPCH p = env; *p; ) {
        std::string entry(p);
        p += entry.size() + 1;
        auto eq = entry.find('=');
        if (eq == std::string::npos) continue;
        std::string name = entry.substr(0, eq);
        if (name.rfind("GB2D_", 0) == 0) to_clear.push_back(std::move(name));
    }
    FreeEnvironmentStringsA(env);
    for (auto& n : to_clear) _putenv_s(n.c_str(), "");
#else
    char** envp = environ;
    if (!envp) return;
    std::vector<std::string> to_clear;
    for (char** e = envp; *e; ++e) {
        std::string entry(*e);
        auto eq = entry.find('=');
        if (eq == std::string::npos) continue;
        std::string name = entry.substr(0, eq);
        if (name.rfind("GB2D_", 0) == 0) to_clear.push_back(std::move(name));
    }
    for (auto& n : to_clear) unsetenv(n.c_str());
#endif
}
}

class EnvBootstrap final : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;
    void testCaseStarting(Catch::TestCaseInfo const&) override {
        clear_gb2d_env();
    }
};

CATCH_REGISTER_LISTENER(EnvBootstrap);
