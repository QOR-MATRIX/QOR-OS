// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include <QtCore/QCommandLineParser>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>

#include <cstdlib>

#ifdef Q_OS_WIN
#include "controllerinput_windows.h"
#include <QtQuick/QQuickWindow>
#endif

#ifdef QOR_QONSOLE_VERIFICATION
#include "executable_verification.h"
#endif

using namespace Qt::StringLiterals;

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(u"qor-qonsole"_s);
    app.setApplicationDisplayName(u"QOR OS · Qonsole"_s);
    app.setOrganizationName(u"QOR-MATRIX"_s);
    app.setOrganizationDomain(u"qorsync.dev"_s);
    // Application version remains unset; the project has assigned none.
    QCommandLineParser parser;
    parser.setApplicationDescription(u"Standalone QOR OS Qonsole launcher"_s);
    parser.addHelpOption();
#ifdef QOR_QONSOLE_VERIFICATION
    const QCommandLineOption flow(u"verify-flow"_s, u"Local injected-input verification; not a controller test"_s,
                                  u"existing-output-directory"_s);
    const QCommandLineOption missing(u"verify-missing-qml"_s,
                                     u"Local negative control: load a nonexistent owned resource"_s);
    parser.addOptions({ flow, missing });
#endif
    parser.process(app);
    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
#ifdef QOR_QONSOLE_VERIFICATION
    if (parser.isSet(flow) && parser.isSet(missing))
        return EXIT_FAILURE;
    const QString entry = parser.isSet(missing) ? u"MissingForVerification"_s : u"Main"_s;
    engine.loadFromModule(u"QOR.Qonsole"_s, entry);
#else
    engine.loadFromModule(u"QOR.Qonsole"_s, u"Main"_s);
#endif
    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE;
#ifdef QOR_QONSOLE_VERIFICATION
    if (parser.isSet(flow)) {
        if (!startQonsoleVerification(app, engine, parser.value(flow)))
            return EXIT_FAILURE;
        return app.exec(); // Fixture-only mode never registers a physical input reader.
    }
#endif
#ifdef Q_OS_WIN
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QObject *launcher = window ? window->findChild<QObject *>(u"qonsoleLauncher"_s) : nullptr;
    if (!window || !launcher)
        return EXIT_FAILURE;
    new qor::qonsole::WindowsControllerInput(window, launcher);
#endif
    return app.exec();
}
