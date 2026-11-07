// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "kit.h"
#include "setupstate.h"

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QtQml/QQmlExtensionPlugin>
#include <cstdio>
#include <string_view>

Q_IMPORT_QML_PLUGIN(QOR_KitPlugin)
Q_IMPORT_QML_PLUGIN(LumenPlugin)
Q_IMPORT_QML_PLUGIN(QOR_SetupPlugin)
using namespace Qt::StringLiterals;

int main(int argc, char **argv)
{
    // A headless image check must not initialise graphics or read user state.
    if (argc == 2 && std::string_view(argv[1]) == "--build-capabilities") {
#ifdef QOR_SETUP_DEVELOPMENT
        std::fputs("{\"schema\":1,\"development\":true,\"firstRun\":true}\n", stdout);
#else
        std::fputs("{\"schema\":1,\"development\":false,\"firstRun\":true}\n", stdout);
#endif
        return 0;
    }
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(u"QOR OS"_s);
    QCoreApplication::setApplicationName(u"qor-setup"_s);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption(
        { u"first-run"_s, u"Welcome after an authenticated session starts; skip if already completed."_s });
    parser.addOption(
        { u"introduction-only"_s, u"Describe a limited installation-test image without promising a desktop."_s });
#ifdef QOR_SETUP_DEVELOPMENT
    parser.addOption(
        { u"screenshot"_s, u"Capture this development window and exit without completing setup."_s, u"file"_s });
    parser.addOption({ u"page"_s, u"Development capture page, 0 to 2."_s, u"number"_s, u"0"_s });
#endif
    parser.process(app);
    qor::SetupState state;
    if (state.completed())
        return 0;
    qor::Kit::initialize();
    QGuiApplication::setWindowIcon(QIcon(u":/qt/qml/QOR/Kit/branding/qor-mark.png"_s));
    QQuickStyle::setStyle(u"Lumen"_s);
    QQmlApplicationEngine engine;
    qor::applyWelcomeAppearance(engine.singletonInstance<QObject *>("QOR.Kit", "Theme"), qgetenv("QOR_REDUCED_MOTION"),
                                qgetenv("QOR_INCREASE_CONTRAST"));
    engine.setInitialProperties({ { u"setupState"_s, QVariant::fromValue(&state) },
                                  { u"introductionOnly"_s, parser.isSet(u"introduction-only"_s) } });
    engine.loadFromModule("QOR.Setup", "Welcome");
    if (engine.rootObjects().isEmpty())
        return 2;
    QObject::connect(&state, &qor::SetupState::completedChanged, &app, [&] {
        if (state.completed())
            app.quit();
    });
#ifdef QOR_SETUP_DEVELOPMENT
    if (parser.isSet(u"screenshot"_s)) {
        bool valid = false;
        const int page = parser.value(u"page"_s).toInt(&valid);
        if (!valid || page < 0 || page > 2)
            return 2;
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        if (!window)
            return 2;
        window->setProperty("page", page);
        QTimer::singleShot(400, &app,
                           [&, window] { app.exit(window->grabWindow().save(parser.value(u"screenshot"_s)) ? 0 : 3); });
    }
#endif
    return app.exec();
}
