// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "commandline.h"

#include <QtCore/QCommandLineParser>

using namespace Qt::StringLiterals;

namespace qor::prism {

void addOptions(QCommandLineParser &parser)
{
#ifdef QOR_HAS_SIMULATOR
    parser.addOption({ u"services"_s, u"embedded (simulated, default) or external."_s, u"mode"_s, u"embedded"_s });
#else
    // A device: the system's own daemons, and no simulator built in to choose (SECURITY.md).
    parser.addOption({ u"services"_s, u"external: the only choice in this build."_s, u"mode"_s, u"external"_s });
#endif
    parser.addOptions({
        { u"socket"_s, u"Wayland socket name for clients (default: first free wayland-N)."_s, u"name"_s },
        { u"size"_s, u"Output window size, e.g. 1600x1000."_s, u"WxH"_s },
        { u"spread-clients"_s, u"Open each new client on the output with the fewest windows."_s },
        { u"outputs"_s, u"Nested: drive this many outputs, side by side, each a window of --size."_s, u"n"_s, u"1"_s },
        { u"surface"_s, u"Shell surface to show: home, library, beam, control-center, overview."_s, u"name"_s },
    });
#ifdef QOR_HAS_SIMULATOR
    // Tests and a developer's runs only: none of these is in a device's image.
    parser.addOptions({
        { u"unlocked"_s, u"Start on the desktop instead of the lock screen."_s },
        { u"screenshot"_s, u"Save a screenshot of the output to this path and exit."_s, u"file"_s },
        { u"delay"_s, u"Milliseconds to wait before the screenshot."_s, u"ms"_s, u"5000"_s },
        { u"benchmark"_s,
          u"Run the shell tour for N seconds, report per-output frame pacing and input latency as JSON, exit."_s,
          u"seconds"_s },
        { u"benchmark-out"_s, u"Write the benchmark JSON to this file."_s, u"file"_s },
        { u"script"_s, u"Run a JSON UI script against the shell, then exit."_s, u"file"_s },
    });
#endif
}

CommandLine read(const QCommandLineParser &parser)
{
    CommandLine line;
    line.services = parser.value(u"services"_s);
    line.socket = parser.value(u"socket"_s);
    line.size = parser.value(u"size"_s);
    line.outputs = parser.value(u"outputs"_s).toInt();
    line.spreadClients = parser.isSet(u"spread-clients"_s);
    line.surface = parser.value(u"surface"_s);
#ifdef QOR_HAS_SIMULATOR
    line.unlocked = parser.isSet(u"unlocked"_s);
    line.screenshot = parser.value(u"screenshot"_s);
    line.delayMs = parser.value(u"delay"_s).toInt();
    line.benchmarkSeconds = parser.value(u"benchmark"_s).toInt();
    line.benchmarkOut = parser.value(u"benchmark-out"_s);
    line.script = parser.value(u"script"_s);
#endif
    return line;
}

} // namespace qor::prism
