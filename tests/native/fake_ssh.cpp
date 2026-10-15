// SPDX-License-Identifier: Apache-2.0

#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <string>
#endif

static bool askPassword()
{
#ifdef _WIN32
    wchar_t executable[32768] {};
    if (!GetEnvironmentVariableW(L"SSH_ASKPASS", executable, 32768)) { return false; }
    SECURITY_ATTRIBUTES security {sizeof(security), nullptr, TRUE};
    HANDLE read = nullptr;
    HANDLE write = nullptr;
    if (!CreatePipe(&read, &write, &security, 0)) { return false; }
    SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    startup.hStdOutput = write;
    PROCESS_INFORMATION process {};
    std::wstring command = L"\"" + std::wstring(executable) + L"\" \"test@askpass-test's password:\"";
    const bool started = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    CloseHandle(write);
    char response[128] {};
    DWORD count = 0;
    const bool received = started && ReadFile(read, response, sizeof(response) - 1, &count, nullptr);
    CloseHandle(read);
    bool accepted = false;
    if (started)
    {
        WaitForSingleObject(process.hProcess, 15000);
        DWORD code = 1;
        GetExitCodeProcess(process.hProcess, &code);
        accepted = received && code == 0 && std::strcmp(response, "test-secret-42\n") == 0;
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    SecureZeroMemory(response, sizeof(response));
    return accepted;
#else
    return false;
#endif
}

int main(int argc, char **argv)
{
    if (argc > 1 && std::strcmp(argv[1], "-G") == 0)
    {
#ifdef _WIN32
        wchar_t file[32768] {};
        GetEnvironmentVariableW(L"SW_SSH_TEST_KNOWN_HOSTS", file, 32768);
        char utf8[65536] {};
        WideCharToMultiByte(CP_UTF8, 0, file, -1, utf8, sizeof(utf8), nullptr, nullptr);
        std::printf("hostname test-host\nport 22\nuser test\nuserknownhostsfile \"%s\"\n", utf8);
#endif
        return 0;
    }
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_VIRTUAL_TERMINAL_INPUT);
    DWORD outputMode = 0;
    GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &outputMode);
    SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), outputMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
    if (argc > 1 && std::strcmp(argv[argc - 1], "pty-wait") == 0)
    {
        int trigger = 0;
        while ((trigger = std::getchar()) != EOF && trigger != '!') {}
    }
    if (argc > 1 && std::strcmp(argv[argc - 1], "askpass-test") == 0)
    {
        if (!askPassword())
        {
            return 3;
        }
    }
    std::fputs("ready\r\n", stdout);
    std::fflush(stdout);

    int character = 0;
    while ((character = std::getchar()) != EOF)
    {
#ifdef _WIN32
        if (character == 5)
        {
            CONSOLE_SCREEN_BUFFER_INFO info {};
            GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
            std::printf("\r\nPTY_SIZE %d %d\r\n", int(info.dwSize.X), int(info.dwSize.Y));
            std::fflush(stdout);
            continue;
        }
#endif
        std::putchar(character);
        std::fflush(stdout);
    }

    return 0;
}
