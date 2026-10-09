#include "../src/plugin_api.hpp"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>

class NotifyPlugin : public PipelinePlugin {
public:
    std::string name() const override { return "notify"; }

    StageResult execute(const StageContext& ctx) override {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &now);   // Windows / MinGW
#else
        localtime_r(&now, &tm);   // POSIX (на будущее)
#endif
        std::cout << "[notify " << std::put_time(&tm, "%H:%M:%S") << "] "
                  << "stage '" << ctx.name << "' started\n";
        std::cout << "[notify " << std::put_time(&tm, "%H:%M:%S") << "] "
                  << "stage '" << ctx.name << "' finished\n";
        return {true, 0, ""};
    }
};

PLUGIN_API PipelinePlugin* create_plugin() { return new NotifyPlugin(); }