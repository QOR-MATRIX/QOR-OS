// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// prism — the QOR OS Wayland compositor (Linux).
//
// Hosts the QOR.Shell desktop inside a Qt Wayland Compositor scene. Run it
// nested for development:
//
//   prism --unlocked            # inside an existing Wayland/X11 session or WSLg (a build with the simulator)
//   WAYLAND_DISPLAY=wayland-1 weston-terminal   # launch a client into Prism
//
// Prism prints the socket name it listens on (the first free wayland-N, or
// --socket).
//
// Status: experimental, see docs/decisions/qtos/QTOS-0003-compositor-strategy.md and
// docs/decisions/QOR-0005-every-display-at-its-own-refresh.md.

#include "backend.h"
#include "commandline.h"
#include "kit.h"
#include "producthost.h"
#include "refusednetwork.h"
#include "serviceendpoint.h"
#include "services.h"
#include "sessionwelcome.h"
#ifdef QOR_HAS_SIMULATOR
#include "shellbenchmark.h"
#include "shellscript.h"
#include "simservices.h"
#endif

#include <QtCore/QCommandLineParser>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlExtensionPlugin>
#include <QtQuick/QQuickWindow>
#include <QtQuickControls2/QQuickStyle>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string_view>

using namespace Qt::StringLiterals;

Q_IMPORT_QML_PLUGIN(QOR_KitPlugin)
Q_IMPORT_QML_PLUGIN(LumenPlugin)
Q_IMPORT_QML_PLUGIN(QOR_ServicesPlugin)
Q_IMPORT_QML_PLUGIN(QOR_AppsPlugin)
Q_IMPORT_QML_PLUGIN(QOR_ShellPlugin)

int main(int argc, char *argv[])
{
    // Image inspection must not initialise graphics, audio, services or user state.
    if (argc == 2 && std::string_view(argv[1]) == "--build-capabilities")
        return std::fputs(qor::prism::buildCapabilities(), stdout) < 0 ? 1 : 0;

    // The compositor's own window must not try to connect to itself.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") && !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        qputenv("QT_QPA_PLATFORM", "wayland");
    qputenv("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1");
    // Before the audio engine or interface sounds touch Qt Multimedia.
    qor::audio::configureBackend();

    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(u"QOR-MATRIX"_s);
    QGuiApplication::setApplicationName(u"Prism"_s);
    QGuiApplication::setApplicationVersion(QStringLiteral(QOR_VERSION_STRING));

    QCommandLineParser parser;
    parser.setApplicationDescription(u"Prism, the QOR OS Wayland compositor."_s);
    parser.addHelpOption();
    parser.addVersionOption();
    qor::prism::addOptions(parser);
    parser.process(app);
    const qor::prism::CommandLine line = qor::prism::read(parser);

    qor::Kit::initialize();
    QQuickStyle::setStyle(u"Lumen"_s);

    // Automated runs install from the Store into a folder of their own, gone
    // with the run.
    std::unique_ptr<QTemporaryDir> automationStore;
    if (line.automated() && qEnvironmentVariableIsEmpty("QOR_STORE_DIR")) {
        automationStore = std::make_unique<QTemporaryDir>();
        qputenv("QOR_STORE_DIR", automationStore->path().toUtf8());
    }

    QUrl servicesUrl = qor::services::defaultEndpoint();
#ifdef QOR_HAS_SIMULATOR
    std::unique_ptr<qor::sim::SimServices> simulator;
    if (line.services != u"external"_s) {
        simulator = std::make_unique<qor::sim::SimServices>();
        qor::sim::SimOptions simOptions;
        // Automated runs never make sound, keep no preferences, and get a
        // private endpoint so they never meet a session someone is using.
        const bool automated = line.automated();
        simOptions.silent = automated;
        if (automated && qEnvironmentVariableIsEmpty("QOR_GAMEPADS"))
            qputenv("QOR_GAMEPADS", "virtual"); // controllers on the desk never join in
        if (automated && qEnvironmentVariableIsEmpty("QOR_TOUCHPAD"))
            qputenv("QOR_TOUCHPAD", "off");
        if (automated) {
            simOptions.persistent = false;
            simOptions.url =
                qor::services::endpoint(u"qor-services-"_s + QString::number(QCoreApplication::applicationPid()));
        }
        if (!simulator->start(simOptions))
            return 1;
        servicesUrl = simulator->url();
    }
    const bool simulated = simulator != nullptr;
#else
    if (line.services != u"external"_s) {
        qCritical("This build of Prism has no simulator: the services are the system's (--services external).");
        return 1;
    }
    const bool simulated = false;
#endif
    qor::Services::instance()->connectTo(servicesUrl);

    QQmlApplicationEngine engine;
    // Nothing in the shell is fetched from the network, whatever a window's title or a notification says.
    qor::RefusedNetworkFactory refusedNetwork;
    engine.setNetworkAccessManagerFactory(&refusedNetwork);
    QVariantMap options;
    // Only a simulated session has an unlock with nothing behind it (Desktop.tryUnlock).
    options.insert(u"simulated"_s, simulated);
    // Only a build with the simulator has a way to start unlocked (commandline.h): Desktop.unlock() asks nothing.
    options.insert(u"unlocked"_s, line.unlocked || line.benchmarkSeconds > 0);
    options.insert(u"surface"_s, line.surface);
    int width = 1600;
    int height = 1000;
    if (const QStringList parts = line.size.split(u'x'); parts.size() == 2) {
        width = parts[0].toInt();
        height = parts[1].toInt();
    }
    options.insert(u"width"_s, width);
    options.insert(u"height"_s, height);
    // On bare metal (Qt's eglfs, linuxfb or vkkhrdisplay) every screen is an
    // output, fullscreen. Nested, --outputs windows of the host.
    const QString platform = QGuiApplication::platformName();
    const bool bareMetal = platform.startsWith(u"eglfs"_s) || platform == u"linuxfb"_s || platform == u"vkkhrdisplay"_s;
    QVariantList outputList;
    if (bareMetal) {
        const QList<QScreen *> screens = QGuiApplication::screens();
        for (qsizetype i = 0; i < screens.size(); ++i) {
            const QSize screenSize = screens.at(i)->size();
            outputList.append(QVariantMap { { u"width"_s, screenSize.width() },
                                            { u"height"_s, screenSize.height() },
                                            { u"screen"_s, int(i) },
                                            { u"fullScreen"_s, true } });
            qInfo("Output %lld: %s, %dx%d at %.1f Hz", qlonglong(i), qPrintable(screens.at(i)->name()),
                  screenSize.width(), screenSize.height(), screens.at(i)->refreshRate());
        }
    } else {
        const int count = std::clamp(line.outputs, 1, 4);
        for (int i = 0; i < count; ++i)
            outputList.append(QVariantMap { { u"width"_s, width }, { u"height"_s, height } });
    }
    options.insert(u"outputs"_s, outputList);
    options.insert(u"spreadClients"_s, line.spreadClients);
    engine.setInitialProperties({ { u"options"_s, options }, { u"socketName"_s, line.socket.toUtf8() } });
    engine.loadFromModule("QOR.Prism", "Compositor");
    if (engine.rootObjects().isEmpty())
        return 2;

    // First-run welcome is an ordinary, unprivileged client after an actual
    // device unlock. Simulation and live media never start it automatically.
    if (!simulated && !QFileInfo::exists(u"/usr/lib/qor-os/live-session"_s)) {
        QObject *desktop = engine.rootObjects().first()->findChild<QObject *>(u"desktop"_s);
        auto *appearance = engine.singletonInstance<QObject *>("QOR.Kit", "Theme");
        auto *welcome = new qor::SessionWelcome(desktop, QCoreApplication::applicationDirPath() + u"/qor-setup"_s,
                                                appearance, &app);
        QObject::connect(welcome, &qor::SessionWelcome::launchAttempted, &app, [](bool started) {
            if (!started)
                qWarning("The first-run welcome could not start; it remains pending for the next session.");
        });
    }

    const QByteArray socketName = engine.rootObjects().constFirst()->property("socketName").toByteArray();
    qInfo("Prism is running. Start clients with WAYLAND_DISPLAY=%s", socketName.constData());
    // The products Prism hosts open their windows here.
    if (auto *products = engine.singletonInstance<qor::ProductHost *>("QOR.Shell", "ProductHost"))
        products->setDisplay(QString::fromUtf8(socketName));

    // One QQuickWindow per WaylandOutput, the main display's (the Desktop's)
    // first.
    QList<QQuickWindow *> outputWindows;
    for (QWindow *window : QGuiApplication::topLevelWindows())
        if (auto *quick = qobject_cast<QQuickWindow *>(window); quick && quick->isVisible())
            outputWindows.append(quick);
    std::stable_sort(outputWindows.begin(), outputWindows.end(), [](QQuickWindow *a, QQuickWindow *b) {
        const auto main = [](QQuickWindow *w) { return w->findChild<QObject *>(u"desktop"_s) != nullptr; };
        return main(a) && !main(b);
    });

#ifdef QOR_HAS_SIMULATOR
    // Tests and a developer's runs: a device's Prism has none of these (commandline.h).
    if (line.benchmarkSeconds > 0)
        qor::runShellBenchmark(outputWindows, { u"prism"_s, line.benchmarkSeconds, line.benchmarkOut });

    if (!line.script.isEmpty())
        qor::runShellScript(outputWindows.value(0), line.script);

    if (!line.screenshot.isEmpty()) {
        QTimer::singleShot(line.delayMs, &app, [path = line.screenshot, outputWindows] {
            const bool saved = !outputWindows.isEmpty() && outputWindows.first()->grabWindow().save(path);
            if (saved)
                qInfo("Saved %s", qPrintable(path));
            else
                qWarning("Could not save a screenshot to %s", qPrintable(path));
            QCoreApplication::exit(saved ? 0 : 3);
        });
    }
#endif
    return app.exec();
}
