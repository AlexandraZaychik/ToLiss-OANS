// Windows double-click log collector. No Python, PowerShell or installed 7-Zip needed.
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cwctype>
namespace fs = std::filesystem;

struct Handle {
    HANDLE value;
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
static void snapshot(const fs::path& from, const fs::path& to) {
    fs::create_directories(to.parent_path());
    Handle input{CreateFileW(from.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr)};
    if (input.value == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot read: " + from.u8string());
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(input.value, &size)) throw std::runtime_error("Cannot get log size");
    Handle output{CreateFileW(to.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr)};
    if (output.value == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create log snapshot");
    char buffer[65536];
    // Capture the size at opening; a live logger may continue appending.
    while (size.QuadPart > 0) {
        DWORD read = 0, written = 0;
        const DWORD count = static_cast<DWORD>((std::min)(size.QuadPart, LONGLONG(sizeof buffer)));
        if (!ReadFile(input.value, buffer, count, &read, nullptr) || !read)
            throw std::runtime_error("Log changed or became unreadable: " + from.u8string());
        if (!WriteFile(output.value, buffer, read, &written, nullptr) || written != read)
            throw std::runtime_error("Cannot write log snapshot (disk full?)");
        size.QuadPart -= read;
    }
}
static void run7z(const fs::path& exe, const fs::path& cwd, const std::wstring& args) {
    std::wstring command = L"\"" + exe.wstring() + L"\" -sccUTF-8 " + args;
    STARTUPINFOW startup{}; startup.cb = sizeof startup;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, cwd.c_str(), &startup, &process))
        throw std::runtime_error("Cannot start bundled 7-Zip");
    Handle thread{process.hThread}, child{process.hProcess};
    WaitForSingleObject(child.value, INFINITE);
    DWORD code = 2;
    if (!GetExitCodeProcess(child.value, &code) || code != 0)
        throw std::runtime_error("7-Zip failed, exit code " + std::to_string(code));
}
static std::wstring lower(std::wstring text) {
    for (auto& c : text) c = static_cast<wchar_t>(std::towlower(c));
    return text;
}
int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleW(L"ToLiss OANS - Collect logs (7-Zip Ultra)");
    fs::path staging, archive;
    int result = 0;
    try {
        wchar_t module[32768];
        const DWORD length = GetModuleFileNameW(nullptr, module, 32768);
        if (!length || length >= 32768) throw std::runtime_error("Cannot locate collector");
        const fs::path root = fs::path(module).parent_path();
        const fs::path seven = root / "tools/7zip/7z.exe";
        if (!fs::is_regular_file(seven) || !fs::is_regular_file(seven.parent_path() / "7z.dll"))
            throw std::runtime_error("Bundled tools/7zip/7z.exe or 7z.dll is missing");
        fs::path xplane;
        for (auto parent = root.parent_path(); !parent.empty();) {
            if (fs::is_regular_file(parent / "X-Plane.exe") ||
                fs::is_directory(parent / "Resources/plugins")) {
                xplane = parent; break;
            }
            auto next = parent.parent_path();
            if (next == parent) break;
            parent = next;
        }
        if (xplane.empty() || !fs::is_regular_file(xplane / "Log.txt"))
            throw std::runtime_error("X-Plane Log.txt not found. Run this tool from the installed aircraft plugin folder after running X-Plane.");
        wchar_t temp[MAX_PATH], unique[MAX_PATH];
        if (!GetTempPathW(MAX_PATH, temp) || !GetTempFileNameW(temp, L"OAN", 0, unique))
            throw std::runtime_error("Cannot create temporary log folder");
        staging = unique;
        fs::remove(staging);
        fs::create_directory(staging);
        unsigned count = 0;
        for (const auto& entry : fs::directory_iterator(root)) {
            if (entry.is_symlink()) continue;
            const auto name = lower(entry.path().filename().wstring());
            if (entry.is_regular_file() &&
                (lower(entry.path().extension().wstring()) == L".log" || name.find(L".log.") != std::wstring::npos)) {
                snapshot(entry.path(), staging / "plugin" / entry.path().filename()); ++count;
            } else if (entry.is_directory() && (name == L"log" || name == L"logs")) {
                for (const auto& log : fs::recursive_directory_iterator(entry.path())) {
                    if (!log.is_symlink() && log.is_regular_file()) {
                        snapshot(log.path(), staging / "plugin" / log.path().lexically_relative(root)); ++count;
                    }
                }
            }
        }
        if (!count) throw std::runtime_error("No plugin logs found in the collector folder");
        snapshot(xplane / "Log.txt", staging / "XPlane/Log.txt");
        SYSTEMTIME now{}; GetLocalTime(&now);
        wchar_t name[128];
        swprintf(name, 128, L"ToLissOANS-logs-%04u%02u%02u-%02u%02u%02u-%03u-%lu.7z",
            now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
            now.wMilliseconds, GetCurrentProcessId());
        archive = root / name;
        if (fs::exists(archive)) throw std::runtime_error("Archive already exists; please retry");
        std::cout << "Collecting " << count << " plugin log(s) + X-Plane Log.txt\n"
                  << "7z / LZMA2 / Ultra (-mx=9) / solid compression\n" << std::flush;
        run7z(seven, staging, L"a -t7z -mx=9 -m0=LZMA2 -ms=on -mmt=on -y -- \"" + archive.wstring() + L"\" plugin XPlane");
        run7z(seven, staging, L"t -- \"" + archive.wstring() + L"\"");
        std::cout << "\nSUCCESS: " << archive.u8string() << "\nSend this .7z file for troubleshooting.\n";
    } catch (const std::exception& error) {
        std::cerr << "\nERROR: " << error.what() << '\n';
        if (!archive.empty()) { std::error_code ignored; fs::remove(archive, ignored); }
        result = 1;
    }
    if (!staging.empty()) { std::error_code ignored; fs::remove_all(staging, ignored); }
    if (!(argc == 2 && std::wstring(argv[1]) == L"--no-pause")) {
        std::cout << "\nPress Enter to close..." << std::flush;
        std::cin.get();
    }
    return result;
}
