#pragma once
#include "plugin_api.hpp"
#include <string>
#include <vector>   // <-- добавить эту строку

namespace stages {

StageResult run_build (const std::string& source, const std::string& output);
StageResult run_test  (const std::string& tests_dir);
StageResult run_deploy(const std::string& source, const std::string& target);
StageResult run_script(const std::string& script);

StageResult run_command(const std::vector<std::string>& argv,
                        const std::string& workdir = "");

}