#include "stages.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <ctime>

namespace fs = std::filesystem;

namespace {

// Экранирование аргумента для командной строки Windows (по правилам CRT)
std::string quote_arg(const std::string& a) {
    if (a.find_first_of(" \t\"") == std::string::npos) return a;
    std::string out = "\"";
    size_t bs = 0;
    for (char c : a) {
        if (c == '\\') { ++bs; out += c; }
        else if (c == '"') {
            out.append(bs, '\\');
            out += "\\\"";
            bs = 0;
        } else { bs = 0; out += c; }
    }
    out.append(bs, '\\');
    out += "\"";
    return out;
}

std::string build_cmdline(const std::vector<std::string>& argv) {
    std::string cmd;
    for (size_t i = 0; i < argv.size(); ++i) {
        if (i) cmd += ' ';
        cmd += quote_arg(argv[i]);
    }
    return cmd;
}

} // namespace

namespace stages {

// Аналог fork+execvp с захватом stdout+stderr через анонимные пайпы
StageResult run_command(const std::vector<std::string>& argv, const std::string& workdir) {
    StageResult res;
    if (argv.empty()) { res.message = "empty command"; return res; }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) {
        res.message = "CreatePipe failed: " + std::to_string(GetLastError());
        return res;
    }
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError  = wr;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::string cmd = build_cmdline(argv);
    std::vector<char> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back('\0');

    const char* cwd_ptr = workdir.empty() ? nullptr : workdir.c_str();

    BOOL ok = CreateProcessA(
        nullptr,
        cmd_buf.data(),
        nullptr, nullptr,
        TRUE,
        0,
        nullptr,
        cwd_ptr,
        &si, &pi);

    CloseHandle(wr);

    if (!ok) {
        CloseHandle(rd);
        res.message = "CreateProcess failed: " + std::to_string(GetLastError()) +
                      " (cmd: " + cmd + ")";
        res.exit_code = -1;
        return res;
    }

    std::string out;
    char buf[4096];
    DWORD n = 0;
    while (ReadFile(rd, buf, sizeof(buf), &n, nullptr) && n > 0)
        out.append(buf, n);
    CloseHandle(rd);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    res.exit_code = (int)code;
    res.success   = (code == 0);
    res.message   = out;
    return res;
}

// ---------- build ----------
StageResult run_build(const std::string& source, const std::string& output) {
    StageResult res;
    if (!fs::exists(source)) { res.message = "source dir not found: " + source; return res; }
    std::error_code ec;
    fs::create_directories(output, ec);

    std::vector<std::string> sources;
    for (auto& p : fs::recursive_directory_iterator(source)) {
        if (p.is_regular_file() && p.path().extension() == ".cpp")
            sources.push_back(p.path().string());
    }
    if (sources.empty()) { res.message = "no .cpp files in " + source; return res; }

    std::string exe = (fs::path(output) / "app.exe").string();

    std::vector<std::string> argv = {"g++", "-std=c++17", "-O2", "-o", exe};
    for (auto& s : sources) argv.push_back(s);

    auto r = run_command(argv);
    r.message = "g++ -> " + exe + "\n" + r.message;
    return r;
}

// ---------- test ----------
StageResult run_test(const std::string& tests_dir) {
    StageResult res;
    if (!fs::exists(tests_dir)) {
        res.message = "tests dir not found: " + tests_dir;
        return res;
    }

    // Собираем .cpp файлы тестов
    std::vector<std::string> sources;
    for (auto& p : fs::recursive_directory_iterator(tests_dir)) {
        if (p.is_regular_file() && p.path().extension() == ".cpp")
            sources.push_back(p.path().string());
    }
    if (sources.empty()) {
        res.message = "no tests found";
        return res;
    }

    std::string bin = (fs::path(tests_dir) / "test_runner.exe").string();

    // ВАЖНО: подключаем src/json.cpp, иначе линковщик не найдёт json::parse
    std::vector<std::string> argv = {"g++", "-std=c++17", "-O2", "-Isrc", "-o", bin};
    argv.push_back("src/json.cpp");
    for (auto& s : sources) argv.push_back(s);

    auto build_res = run_command(argv);
    if (!build_res.success) {
        build_res.message = "test compile failed:\n" + build_res.message;
        return build_res;
    }
    return run_command({bin});
}

// ---------- deploy ----------
StageResult run_deploy(const std::string& source, const std::string& target) {
    StageResult res;
    if (!fs::exists(source)) { res.message = "source not found: " + source; return res; }
    std::error_code ec;
    fs::create_directories(target, ec);
    if (ec) { res.message = "cannot create target: " + ec.message(); return res; }

    fs::path exe = fs::path(source) / "app.exe";
    if (!fs::exists(exe)) { res.message = "app.exe not found in " + source; return res; }

    fs::path dst = fs::path(target) / "app.exe";
    fs::copy_file(exe, dst, fs::copy_options::overwrite_existing, ec);
    if (ec) { res.message = "copy failed: " + ec.message(); return res; }

    std::ofstream rep(fs::path(target) / "deploy_report.txt");
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char tbuf[64]; std::tm tm{}; localtime_s(&tm, &now);
    std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm);
    rep << "Deploy report\n"
        << "Time: " << tbuf << "\n"
        << "Source: " << source << "\n"
        << "Target: " << target << "\n"
        << "Executable: " << dst.string() << "\n"
        << "Status: OK\n";

    res.success = true;
    res.message = "deployed to " + dst.string();
    return res;
}

// ---------- custom script ----------
StageResult run_script(const std::string& script) {
    if (!fs::exists(script)) return {false, 1, "script not found: " + script};

    // Приводим путь к абсолютному и нормализуем разделители
    std::error_code ec;
    fs::path abs = fs::absolute(script, ec);
    std::string path_str = (ec ? fs::path(script) : abs).string();

    // Заменяем прямые слэши на обратные — cmd.exe понимает только '\'
    for (auto& c : path_str) if (c == '/') c = '\\';

    auto ext = fs::path(path_str).extension().string();
    std::vector<std::string> argv;

    if (ext == ".bat" || ext == ".cmd") {
        // cmd /C "полный\путь\скрипт.bat"
        argv = {"cmd.exe", "/C", path_str};
    } else if (ext == ".ps1") {
        argv = {"powershell.exe", "-ExecutionPolicy", "Bypass", "-File", path_str};
    } else {
        argv = {"cmd.exe", "/C", path_str};
    }

    return run_command(argv);
}

} // namespace stages