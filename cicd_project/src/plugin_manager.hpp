#pragma once
#include "plugin_api.hpp"
#include <windows.h>
#include <vector>
#include <string>

class PluginManager {
public:
    ~PluginManager();
    int load_directory(const std::string& dir);          // загружает *.dll
    PipelinePlugin* find(const std::string& name) const;
    const std::vector<std::string>& loaded_names() const { return names_; }
private:
    struct Entry { HMODULE handle = nullptr; PipelinePlugin* plugin = nullptr; };
    std::vector<Entry> entries_;
    std::vector<std::string> names_;
};