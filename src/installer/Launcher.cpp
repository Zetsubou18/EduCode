#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;
static std::wstring quote(const std::wstring &value) { return L"\"" + value + L"\""; }
static std::wstring ownPath() {
    std::vector<wchar_t> buffer(32768);
    GetModuleFileNameW(nullptr, buffer.data(), DWORD(buffer.size()));
    return buffer.data();
}
static bool run(const std::wstring &exe, const std::wstring &args, bool wait, DWORD *code = nullptr) {
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::wstring command = quote(exe) + L" " + args;
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) return false;
    CloseHandle(process.hThread);
    if (wait) {
        WaitForSingleObject(process.hProcess, INFINITE);
        if (code) GetExitCodeProcess(process.hProcess, code);
    }
    CloseHandle(process.hProcess);
    return true;
}
static bool extract(const fs::path &source, const fs::path &destination) {
    FILE *input = _wfopen(source.c_str(), L"rb");
    if (!input) return false;
    _fseeki64(input, 0, SEEK_END);
    const auto size = _ftelli64(input);
    if (size < 16 || _fseeki64(input, size - 16, SEEK_SET)) { fclose(input); return false; }
    uint64_t length = 0;
    char magic[8]{};
    if (fread(&length, 1, 8, input) != 8 || fread(magic, 1, 8, input) != 8 ||
        std::string(magic, 8) != "EDUSETUP" || length > uint64_t(size - 16)) { fclose(input); return false; }
    fs::create_directories(destination);
    const auto archive = destination / L"bundle.zip";
    FILE *output = _wfopen(archive.c_str(), L"wb");
    if (!output || _fseeki64(input, size - 16 - static_cast<__int64>(length), SEEK_SET)) {
        if (output) fclose(output); fclose(input); return false;
    }
    char buffer[65536];
    while (length) {
        const auto count = size_t(length < sizeof(buffer) ? length : sizeof(buffer));
        if (fread(buffer, 1, count, input) != count || fwrite(buffer, 1, count, output) != count) {
            fclose(output); fclose(input); return false;
        }
        length -= count;
    }
    fclose(output); fclose(input);
    wchar_t system[MAX_PATH]{};
    GetSystemDirectoryW(system, MAX_PATH);
    const auto tar = fs::path(system) / L"tar.exe";
    DWORD code = 1;
    return run(tar.wstring(), L"-xf " + quote(archive.wstring()) + L" -C " + quote(destination.wstring()), true, &code) && code == 0;
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    const auto self = fs::path(ownPath());
    int count = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    std::vector<std::wstring> args;
    for (int i = 1; i < count; ++i) args.emplace_back(arguments[i]);
    LocalFree(arguments);
    const bool uninstall = !args.empty() && (args[0] == L"--uninstall" || args[0] == L"--uninstall-silent");
    const bool tempUninstaller = std::find(args.begin(), args.end(), L"--temp-uninstaller") != args.end();
    wchar_t temp[MAX_PATH]{}; GetTempPathW(MAX_PATH, temp);
    const auto base = fs::path(temp) / (L"EduCodeSetup-" + std::to_wstring(GetCurrentProcessId()));
    if (uninstall && !tempUninstaller) {
        fs::create_directories(base);
        const auto detached = base / L"Uninstall.exe";
        if (!CopyFileW(self.c_str(), detached.c_str(), FALSE)) return 1;
        std::wstring params;
        for (const auto &arg : args) params += quote(arg) + L" ";
        params += L"--temp-uninstaller";
        return run(detached.wstring(), params, false) ? 0 : 1;
    }
    if (args.size() >= 2 && args[0] == L"--extract-only") {
        return extract(self, fs::path(args[1])) ? 0 : 1;
    }
    const auto unpacked = base / L"runtime";
    if (!extract(self, unpacked)) {
        MessageBoxW(nullptr, L"Не удалось распаковать установщик. Требуется Windows 10/11.", L"EduCode", MB_ICONERROR);
        return 1;
    }
    const auto gui = unpacked / L"SetupGui.exe";
    std::wstring params;
    for (const auto &arg : args) if (arg != L"--temp-uninstaller") params += quote(arg) + L" ";
    params += L"--bundle " + quote(unpacked.wstring()) + L" --launcher " + quote((unpacked / L"EduCodeUninstall.exe").wstring());
    DWORD code = 1;
    const bool okay = run(gui.wstring(), params, true, &code);
    std::error_code ignored;
    fs::remove_all(unpacked, ignored);
    if (!tempUninstaller) fs::remove(base, ignored);
    if (tempUninstaller) {
        wchar_t system[MAX_PATH]{}; GetSystemDirectoryW(system, MAX_PATH);
        const auto cmd = fs::path(system) / L"cmd.exe";
        const auto cleanup = L"/c ping -n 3 127.0.0.1 >nul & del /f /q " + quote(self.wstring()) +
                             L" & rmdir /q " + quote(base.wstring());
        run(cmd.wstring(), cleanup, false);
    }
    return okay ? int(code) : 1;
}
