// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "installerstate.h"
#include "kit.h"
#include "setupstate.h"
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml/QQmlExtensionPlugin>
#include <cstdio>
#include <string_view>

Q_IMPORT_QML_PLUGIN(QOR_KitPlugin)
Q_IMPORT_QML_PLUGIN(LumenPlugin)
Q_IMPORT_QML_PLUGIN(QOR_SetupPlugin)
using namespace Qt::StringLiterals;
int main(int argc, char **argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--build-capabilities") {
        std::fputs("{\"schema\":1,\"vmOnly\":true,\"encryption\":false}\n", stdout);
        return 0;
    }
    QGuiApplication app(argc, argv);
    app.setOrganizationName(u"QOR OS"_s);
    app.setApplicationName(u"qor-install"_s);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.process(app);
    qor::Kit::initialize();
    app.setWindowIcon(QIcon(u":/qt/qml/QOR/Kit/branding/qor-mark.png"_s));
    QQuickStyle::setStyle(u"Lumen"_s);
    qor::InstallerState installer;
    QQmlApplicationEngine engine;
    qor::applyWelcomeAppearance(engine.singletonInstance<QObject *>("QOR.Kit", "Theme"), qgetenv("QOR_REDUCED_MOTION"),
                                qgetenv("QOR_INCREASE_CONTRAST"));
    engine.setInitialProperties({ { u"installer"_s, QVariant::fromValue(&installer) } });
    engine.loadFromModule("QOR.Setup", "Installer");
    return engine.rootObjects().isEmpty() ? 2 : app.exec();
}
