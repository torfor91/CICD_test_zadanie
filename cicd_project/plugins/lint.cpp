#include "../src/plugin_api.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

namespace fs = std::filesystem;

class LintPlugin : public PipelinePlugin {
public:
    std::string name() const override { return "lint"; }

    StageResult execute(const StageContext& ctx) override {
        StageResult r;
        auto it = ctx.params.find("source");
        std::string src = (it != ctx.params.end()) ? it->second : "./src";
        if (!fs::exists(src)) { r.message = "lint: no source dir " + src; return r; }

        int issues = 0;
        std::ostringstream rep;
        for (auto& p : fs::recursive_directory_iterator(src)) {
            if (!p.is_regular_file()) continue;
            auto ext = p.path().extension();
            if (ext != ".cpp" && ext != ".hpp" && ext != ".h") continue;

            std::ifstream f(p.path());
            std::string line;
            int ln = 0;
            while (std::getline(f, line)) {
                ++ln;
                if (line.size() > 120) {
                    rep << p.path() << ":" << ln << " line > 120 chars\n"; ++issues;
                }
                if (line.find('\t') != std::string::npos) {
                    rep << p.path() << ":" << ln << " contains TAB\n"; ++issues;
                }
                if (!line.empty() && line.find_first_not_of(" \t") != std::string::npos) {
                    size_t indent = line.find_first_not_of(' ');
                    if (indent % 4 != 0 && line[0] == ' ') {
                        rep << p.path() << ":" << ln << " indent not multiple of 4\n";
                        ++issues;
                    }
                }
            }
        }
        r.success = (issues == 0);
        r.exit_code = r.success ? 0 : 1;
        r.message = "lint issues: " + std::to_string(issues) + "\n" + rep.str();
        return r;
    }
};

extern "C" PipelinePlugin* create_plugin() { return new LintPlugin(); }