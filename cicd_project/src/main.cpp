#include "pipeline.hpp"
#include "plugin_manager.hpp"
#include "stages.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

static std::string cwd() {
    char buf[MAX_PATH];
    DWORD n = GetCurrentDirectoryA(MAX_PATH, buf);
    return n ? std::string(buf, n) : ".";
}

static void usage() {
    std::cout <<
        "ci-cd — мини CI/CD пайплайн\n"
        "Использование:\n"
        "  ci-cd build  --source <dir> --output <dir>\n"
        "  ci-cd test   --tests  <dir>\n"
        "  ci-cd deploy --source <dir> --target <dir>\n"
        "  ci-cd run    --config <pipeline.json>\n";
}

static std::string get_opt(const std::vector<std::string>& a, const std::string& k) {
    for (size_t i = 0; i + 1 < a.size(); ++i)
        if (a[i] == k) return a[i+1];
    return "";
}

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) { usage(); return 2; }

    std::string cmd = args[0];

    PluginManager pm;
    int loaded = pm.load_directory("plugins");
    if (loaded)
        std::cout << "[plugins] loaded " << loaded << " plugin(s)\n";

    if (cmd == "build") {
        auto src = get_opt(args, "--source");
        auto out = get_opt(args, "--output");
        if (src.empty() || out.empty()) { usage(); return 2; }
        auto r = stages::run_build(src, out);
        std::cout << r.message << (r.success ? "\n[OK]\n" : "\n[FAIL]\n");
        return r.success ? 0 : (r.exit_code ? r.exit_code : 1);
    }
    if (cmd == "test") {
        auto t = get_opt(args, "--tests");
        if (t.empty()) { usage(); return 2; }
        auto r = stages::run_test(t);
        std::cout << r.message << (r.success ? "\n[OK]\n" : "\n[FAIL]\n");
        return r.success ? 0 : (r.exit_code ? r.exit_code : 1);
    }
    if (cmd == "deploy") {
        auto s = get_opt(args, "--source");
        auto t = get_opt(args, "--target");
        if (s.empty() || t.empty()) { usage(); return 2; }
        auto r = stages::run_deploy(s, t);
        std::cout << r.message << (r.success ? "\n[OK]\n" : "\n[FAIL]\n");
        return r.success ? 0 : (r.exit_code ? r.exit_code : 1);
    }
    if (cmd == "run") {
        auto cfg = get_opt(args, "--config");
        if (cfg.empty()) { usage(); return 2; }
        Pipeline p(pm, cwd());
        std::string err;
        if (!p.load_config(cfg, err)) {
            std::cerr << "config error: " << err << "\n";
            return 2;
        }
        return p.run();
    }

    usage();
    return 2;
}