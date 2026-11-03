// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once

#include <QtCore/QString>

class QCommandLineParser;

namespace qor::prism {

// Prism's command line (SECURITY.md). What only scripted tests and a developer's runs need exists only in a build with
// the simulator (QOR_HAS_SIMULATOR), which a device's image is built without (image/README.md): starting unlocked, a
// JSON UI script, a screenshot and its delay, and the benchmark and its report. In any other build those options do
// not exist, so asking for one is an error before anything is read, and they read here as unset.
struct CommandLine
{
    QString services;
    QString socket;
    QString size;
    int outputs = 1;
    bool spreadClients = false;
    QString surface;
    // A build with the simulator only.
    bool unlocked = false;
    QString screenshot;
    int delayMs = 5000;
    int benchmarkSeconds = 0;
    QString benchmarkOut;
    QString script;

    // A run of a test or a measurement: silent, its preferences kept nowhere, and its Store a folder of its own.
    bool automated() const { return !screenshot.isEmpty() || benchmarkSeconds > 0 || !script.isEmpty(); }
};

// Adds Prism's options to `parser`, as this build has them.
void addOptions(QCommandLineParser &parser);
// What a parser that has processed them says.
CommandLine read(const QCommandLineParser &parser);

} // namespace qor::prism
