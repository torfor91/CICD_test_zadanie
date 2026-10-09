#include "plugin_manager.hpp"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

PluginManager::~PluginManager() {
    for (auto& e : entries_) {
        if (e.plugin) delete e.plugin;
        if (e.handle) FreeLibrary(e.handle);
    }
}

int PluginManager::load_directory(const std::string& dir) {
    int count = 0;
    if (!fs::exists(dir)) return 0;
    for (auto& p : fs::directory_iterator(dir)) {
        if (!p.is_regular_file()) continue;
        if (p.path().extension() != ".dll") continue;

        HMODULE h = LoadLibraryA(p.path().string().c_str());
        if (!h) {
            std::cerr << "[plugin] cannot load " << p.path()
                      << " (err " << GetLastError() << ")\n";
            continue;
        }
        using create_fn = PipelinePlugin*(*)();
        auto create = reinterpret_cast<create_fn>(
            reinterpret_cast<void*>(GetProcAddress(h, "create_plugin")));
        if (!create) {
            std::cerr << "[plugin] no create_plugin in " << p.path() << "\n";
            FreeLibrary(h);
            continue;
        }
        PipelinePlugin* pl = create();
        entries_.push_back({h, pl});
        names_.push_back(pl->name());
        ++count;
    }
    return count;
}

PipelinePlugin* PluginManager::find(const std::string& name) const {
    for (auto& e : entries_)
        if (e.plugin->name() == name) return e.plugin;
    return nullptr;
}