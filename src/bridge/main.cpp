// SPDX-License-Identifier: Apache-2.0
#include "runtime.h"
#include "workbench_version.h"
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    application.setApplicationName("SerialWorkbenchMCP");
    application.setApplicationVersion(SERIAL_WORKBENCH_VERSION);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"url", "Explicit MCP URL; disables automatic startup", "url"});
    parser.addOption({"settings", "Shared settings profile; auto-start if needed", "path", sw::defaultSettingsPath()});
    parser.process(application);
    try { return sw::runStdio(parser.value("settings"), parser.value("url")); }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << std::endl;
        return 2;
    }
}
