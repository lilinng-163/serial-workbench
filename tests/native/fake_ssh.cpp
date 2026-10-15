// SPDX-License-Identifier: Apache-2.0

#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char **argv)
{
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
