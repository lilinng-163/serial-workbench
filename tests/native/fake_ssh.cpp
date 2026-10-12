#include <cstdio>

int main()
{
    std::fputs("ready\n", stdout);
    std::fflush(stdout);

    int character = 0;
    while ((character = std::getchar()) != EOF)
    {
        std::putchar(character);
        std::fflush(stdout);
    }

    return 0;
}
