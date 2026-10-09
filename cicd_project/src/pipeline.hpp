#pragma once
#include "plugin_api.hpp"
#include "plugin_manager.hpp"
#include "json.hpp"
#include <string>
#include <vector>

struct StageSpec {
    std::string name;                          // "build", "test", ...
    std::map<std::string,std::string> params;  // остальные поля
};

class Pipeline {
public:
    Pipeline(PluginManager& pm, std::string workdir);
    // Загружает описание из JSON
    bool load_config(const std::string& path, std::string& err);
    // Прогон всех этапов по порядку
    int run();
    // Прогон одного этапа
    StageResult run_stage(const StageSpec& s);
private:
    PluginManager& pm_;
    std::string workdir_;
    std::vector<StageSpec> stages_;
};