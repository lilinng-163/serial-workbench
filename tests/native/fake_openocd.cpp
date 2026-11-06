// SPDX-License-Identifier: Apache-2.0
// Process fixture only; never installed or distributed as an OpenOCD executable.
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>

int main(int argc, char **argv)
{
    const auto setting = std::getenv("SW_FAKE_OPENOCD_MODE");
    const std::string mode = setting ? setting : "success";
    const auto label = std::getenv("SW_FAKE_OPENOCD_LABEL");
    if (label) { std::cout << label << '\n' << std::flush; }
    if (mode == "hang")
    {
        std::cout << "Info : waiting for probe\n" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(30));
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--version")
    {
        std::cout << "OpenOCD process fixture 0.12.0\n";
        return 0;
    }
    std::string script;
    for (int index = 1; index + 1 < argc; ++index)
    {
        if (std::string(argv[index]) == "-f")
        {
            std::ifstream file(argv[index + 1], std::ios::binary);
            script.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }
    }
    if (script.empty()) { return 2; }
    if (mode == "flood")
    {
        for (int index = 0; index < 700; ++index) { std::cout << std::string(1024, 'x') << '\n'; }
    }
    std::cout << "Info : process fixture; no device access\n";
    const auto start = script.find("echo SW_OPENOCD_COMPLETE_");
    if (start != std::string::npos && mode != "missing-marker")
    {
        const auto marker = start + 5;
        std::cout << script.substr(marker, script.find('\n', marker) - marker) << '\n';
    }
    if (mode == "failure")
    {
        std::cerr << "Error: fixture write failure\n";
        return 7;
    }
    return 0;
}
