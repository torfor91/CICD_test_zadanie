#pragma once
#include <string>
#include <map>

struct StageResult {
    bool success = false;
    int  exit_code = 0;
    std::string message;
};

struct StageContext {
    std::string name;
    std::map<std::string,std::string> params;
    std::string workdir;
};

class PipelinePlugin {
public:
    virtual ~PipelinePlugin() = default;
    virtual std::string name() const = 0;
    virtual StageResult execute(const StageContext& ctx) = 0;
};

// Экспорт/импорт символов для DLL
#ifdef _WIN32
  #define PLUGIN_API extern "C" __declspec(dllexport)
#else
  #define PLUGIN_API extern "C"
#endif

// В .cpp плагинов пишем:  PLUGIN_API PipelinePlugin* create_plugin() { ... }