// SPDX-License-Identifier: Apache-2.0
// Standalone installer helper: no Qt DLLs are loaded from the install directory.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <wbemidl.h>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace
{
namespace fs = std::filesystem;

struct Handle
{
    HANDLE value = nullptr;
    explicit Handle(HANDLE value) : value(value) {}
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) { CloseHandle(value); } }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
};

template<class T> struct Release
{
    void operator()(T *object) const { if (object) { object->Release(); } }
};
template<class T> using Com = std::unique_ptr<T, Release<T>>;

struct Text
{
    BSTR value;
    explicit Text(const wchar_t *text) : value(SysAllocString(text)) {}
    ~Text() { SysFreeString(value); }
};

struct Process
{
    DWORD id;
    std::wstring name;
};

bool equal(const std::wstring &a, const std::wstring &b)
{
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

bool belongs(HANDLE process, const fs::path &directory, const std::wstring &name)
{
    wchar_t path[32768];
    DWORD size = 32768;
    if (!QueryFullProcessImageNameW(process, 0, path, &size)) { return false; }
    std::error_code error;
    const auto actual = fs::weakly_canonical(path, error);
    return !error && equal(actual.wstring(), (directory / name).wstring());
}

std::vector<Process> processes(const fs::path &directory)
{
    std::vector<Process> result;
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (snapshot.value == INVALID_HANDLE_VALUE) { throw 4; }
    PROCESSENTRY32W entry {};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot.value, &entry)) { throw 4; }
    do
    {
        const std::wstring name = entry.szExeFile;
        if (!equal(name, L"SerialWorkbench.exe") && !equal(name, L"SerialWorkbenchMCP.exe")
            && !equal(name, L"SerialWorkbenchCLI.exe") && !equal(name, L"SerialWorkbenchSSHAskpass.exe"))
        {
            continue;
        }
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID));
        if (process.value && belongs(process.value, directory, name))
        {
            result.push_back({entry.th32ProcessID, name});
        }
    } while (Process32NextW(snapshot.value, &entry));
    return result;
}

std::wstring commandLine(DWORD pid)
{
    IWbemLocator *locator = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, reinterpret_cast<void **>(&locator)))) { throw 4; }
    Com<IWbemLocator> owner(locator);
    IWbemServices *service = nullptr;
    Text space(L"ROOT\\CIMV2");
    if (FAILED(locator->ConnectServer(space.value, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &service))) { throw 4; }
    Com<IWbemServices> connection(service);
    if (FAILED(CoSetProxyBlanket(service, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE))) { throw 4; }
    Text language(L"WQL");
    Text query((L"SELECT CommandLine FROM Win32_Process WHERE ProcessId=" + std::to_wstring(pid)).c_str());
    IEnumWbemClassObject *enumerator = nullptr;
    if (FAILED(service->ExecQuery(language.value, query.value,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator))) { throw 4; }
    Com<IEnumWbemClassObject> rows(enumerator);
    IWbemClassObject *object = nullptr;
    ULONG count = 0;
    if (FAILED(enumerator->Next(5000, 1, &object, &count)) || count != 1) { throw 4; }
    Com<IWbemClassObject> row(object);
    VARIANT value;
    VariantInit(&value);
    const auto code = object->Get(L"CommandLine", 0, &value, nullptr, nullptr);
    const std::wstring result = SUCCEEDED(code) && value.vt == VT_BSTR ? value.bstrVal : L"";
    VariantClear(&value);
    if (result.empty()) { throw 4; }
    return result;
}

std::wstring quote(const std::wstring &argument)
{
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t character : argument)
    {
        if (character == L'\\') { ++slashes; continue; }
        result.append(character == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        result += character;
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}

bool stopRuntime(const fs::path &directory, DWORD pid)
{
    int count = 0;
    auto args = CommandLineToArgvW(commandLine(pid).c_str(), &count);
    if (!args) { return false; }
    std::wstring profile;
    for (int index = 1; index < count; ++index)
    {
        const std::wstring argument = args[index];
        if (argument == L"--settings" && index + 1 < count) { profile = args[++index]; }
        else if (argument.rfind(L"--settings=", 0) == 0) { profile = argument.substr(11); }
    }
    LocalFree(args);
    // Relative custom profiles cannot safely be interpreted from another cwd.
    if (!profile.empty() && !fs::path(profile).is_absolute()) { return false; }
    const auto executable = (directory / L"SerialWorkbenchCLI.exe").wstring();
    auto command = quote(executable) + L" stop";
    if (!profile.empty()) { command += L" --settings " + quote(profile); }
    STARTUPINFOW startup {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child {};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, directory.c_str(), &startup, &child)) { return false; }
    Handle process(child.hProcess);
    Handle thread(child.hThread);
    if (WaitForSingleObject(process.value, 12000) != WAIT_OBJECT_0) { return false; }
    DWORD code = 1;
    GetExitCodeProcess(process.value, &code);
    return code == 0;
}

bool cliBridge(DWORD pid)
{
    int count = 0;
    auto args = CommandLineToArgvW(commandLine(pid).c_str(), &count);
    if (!args) { return false; }
    bool bridge = false;
    for (int index = 1; index < count; ++index)
    {
        const std::wstring argument = args[index];
        if (argument == L"--settings" || argument == L"--url") { ++index; continue; }
        if (argument.rfind(L"--settings=", 0) == 0 || argument.rfind(L"--url=", 0) == 0) { continue; }
        bridge = argument == L"mcp";
        break;
    }
    LocalFree(args);
    return bridge;
}

int closeProcesses(const fs::path &directory)
{
    // Save and stop every profile first. Never forcibly terminate the GUI/runtime.
    for (const auto &item : processes(directory))
    {
        if (!equal(item.name, L"SerialWorkbench.exe")) { continue; }
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, item.id));
        if (!process.value || !belongs(process.value, directory, item.name)) { continue; }
        if (WaitForSingleObject(process.value, 0) == WAIT_OBJECT_0) { continue; }
        bool stopping = false;
        try { stopping = stopRuntime(directory, item.id); }
        catch (...) { /* Older GUI versions may not provide the CLI/IPC. */ }
        if (!stopping)
        {
            EnumWindows([](HWND window, LPARAM expected) -> BOOL
            {
                DWORD pid = 0;
                GetWindowThreadProcessId(window, &pid);
                if (pid == static_cast<DWORD>(expected) && IsWindowVisible(window) && !GetWindow(window, GW_OWNER))
                {
                    PostMessageW(window, WM_CLOSE, 0, 0);
                }
                return TRUE;
            }, static_cast<LPARAM>(item.id));
        }
        if (WaitForSingleObject(process.value, 12000) != WAIT_OBJECT_0) { return 2; }
    }
    // Bridges are stateless pipes owned by Agent clients. Their stdin can remain
    // open indefinitely after the runtime exits; end only this installation's
    // helpers after the installer has obtained explicit close permission.
    for (const auto &item : processes(directory))
    {
        if (equal(item.name, L"SerialWorkbench.exe")) { return 2; }
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE, FALSE, item.id));
        if (!process.value || !belongs(process.value, directory, item.name)) { continue; }
        if (WaitForSingleObject(process.value, 0) == WAIT_OBJECT_0) { continue; }
        if (equal(item.name, L"SerialWorkbenchCLI.exe") && !cliBridge(item.id))
        {
            // A CLI configuration command may be writing an Agent's settings.
            if (WaitForSingleObject(process.value, 5000) != WAIT_OBJECT_0) { return 2; }
            continue;
        }
        if (!TerminateProcess(process.value, 0) || WaitForSingleObject(process.value, 5000) != WAIT_OBJECT_0) { return 2; }
    }
    return 0;
}

int checkFiles(const fs::path &directory)
{
    if (!processes(directory).empty()) { return 2; }
    std::error_code error;
    for (const auto &file : fs::recursive_directory_iterator(directory, error))
    {
        const auto extension = file.path().extension().wstring();
        if (!equal(extension, L".exe") && !equal(extension, L".dll")) { continue; }
        // The uninstaller is itself mapped while uninstalling.
        if (equal(file.path().filename().wstring(), L"Uninstall.exe")) { continue; }
        Handle handle(CreateFileW(file.path().c_str(), GENERIC_READ | GENERIC_WRITE, 0,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (handle.value == INVALID_HANDLE_VALUE) { return 3; }
    }
    return error ? 4 : 0;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    int count = 0;
    auto args = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!args || count != 3) { if (args) { LocalFree(args); } return 4; }
    const std::wstring action = args[1];
    const fs::path supplied = args[2];
    LocalFree(args);
    if (action != L"--check" && action != L"--close") { return 4; }
    std::error_code error;
    if (!supplied.is_absolute() || !fs::is_directory(supplied, error) || error) { return 4; }
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    int code = 4;
    try
    {
        const auto directory = fs::canonical(supplied);
        if (action == L"--close" && FAILED(initialized)) { throw 4; }
        code = action == L"--close" ? closeProcesses(directory) : 0;
        if (code == 0) { code = checkFiles(directory); }
    }
    catch (...) { code = 4; }
    if (SUCCEEDED(initialized)) { CoUninitialize(); }
    return code;
}
