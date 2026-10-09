#include "pipeline.hpp"
#include "stages.hpp"
#include <iostream>
#include <chrono>
#include <ctime>

Pipeline::Pipeline(PluginManager& pm, std::string workdir)
    : pm_(pm), workdir_(std::move(workdir)) {}

bool Pipeline::load_config(const std::string& path, std::string& err) {
    try {
        auto root = json::parse_file(path);
        const auto& arr = root["pipeline"];
        if (!arr.is_array()) { err = "pipeline must be an array"; return false; }
        for (size_t i = 0; i < arr.arr->size(); ++i) {
            const auto& item = arr[i];
            if (!item.is_object()) { err = "stage must be an object"; return false; }
            StageSpec spec;
            for (auto& [k, v] : *item.obj) {
                if (k == "stage") {
                    if (!v.is_string()) { err = "stage name must be string"; return false; }
                    spec.name = v.str;
                } else {
                    if (v.is_string())      spec.params[k] = v.str;
                    else if (v.type == json::Value::Number)
                        spec.params[k] = std::to_string((long)v.num);
                }
            }
            if (spec.name.empty()) { err = "stage without name"; return false; }
            stages_.push_back(std::move(spec));
        }
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    }
}

StageResult Pipeline::run_stage(const StageSpec& s) {
    // 1) встроенные этапы
    if (s.name == "build") {
        auto it1 = s.params.find("source");
        auto it2 = s.params.find("output");
        if (it1 == s.params.end() || it2 == s.params.end())
            return {false, 1, "build requires source & output"};
        return stages::run_build(it1->second, it2->second);
    }
    if (s.name == "test") {
        auto it = s.params.find("tests");
        if (it == s.params.end()) return {false, 1, "test requires tests"};
        return stages::run_test(it->second);
    }
    if (s.name == "deploy") {
        auto it1 = s.params.find("source");
        auto it2 = s.params.find("target");
        if (it1 == s.params.end() || it2 == s.params.end())
            return {false, 1, "deploy requires source & target"};
        return stages::run_deploy(it1->second, it2->second);
    }
    if (s.name == "script") {
        auto it = s.params.find("path");
        if (it == s.params.end()) return {false, 1, "script requires path"};
        return stages::run_script(it->second);
    }

    // 2) плагины
    if (auto* pl = pm_.find(s.name)) {
        StageContext ctx;
        ctx.name    = s.name;
        ctx.params  = s.params;
        ctx.workdir = workdir_;
        return pl->execute(ctx);
    }

    return {false, 1, "unknown stage: " + s.name};
}

int Pipeline::run() {
    int idx = 0;
    for (auto& s : stages_) {
        ++idx;
        std::cout << "\n=== Stage " << idx << "/" << stages_.size()
                  << ": " << s.name << " ===\n";
        auto t0 = std::chrono::steady_clock::now();
        StageResult r = run_stage(s);
        auto t1 = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

        std::cout << r.message;
        if (!r.message.empty() && r.message.back() != '\n') std::cout << "\n";
        std::cout << "[stage " << s.name << "] "
                  << (r.success ? "OK" : "FAIL")
                  << " (exit=" << r.exit_code << ", " << ms << " ms)\n";

        if (!r.success) {
            std::cerr << "\nPipeline aborted at stage '" << s.name
                      << "': exit code " << r.exit_code << "\n";
            return r.exit_code ? r.exit_code : 1;
        }
    }
    std::cout << "\nPipeline finished successfully.\n";
    return 0;
}