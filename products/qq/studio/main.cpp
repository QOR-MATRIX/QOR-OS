// QQ Studio: the native editor for QQ, the QOR Engine (ADR-083).
//
//   qq-studio [model.gltf] [--project <folder>] [--capture <frame.png>] [--launcher-channel <name>]
//
// Opens a new scene, with the given glTF 2.0 model or the bundled sample, in the given project folder (the QOR Launcher
// passes the project open in Projects). With --capture, the Studio renders until its scene has settled, saves the whole
// window as an image and exits: how a person, a check or the agent (P3.3) sees what the Studio shows. Exit code 0 saved,
// 1 not saved, 2 timed out.
//
// The Studio is its own process, started by the QOR Launcher; it never holds a key, and anything to be signed is
// asked of the launcher's host (ADR-083 decision 5). The launcher names its channel with --launcher-channel and writes
// a one-time token on the Studio's stdin; through that channel the Studio publishes a project as its Cartridge
// (launcherlink.h, ADR-088), and when the launcher closes it, the Studio exits (ADR-046 decision 2).
//
// It offers its tools to language models (DIRECTION P3.3) on a local pipe only this user can open: an MCP client starts
// qq-mcp, which relays to it (agent/). One Studio at a time serves the pipe; QQ_MCP_PIPE names another.
//
// Changed for QOR OS: the watchdog for logic that never returns (QOR-0018).
//
// Changed for QOR OS: each project's workspace (projectworkspace.h). --project always wins. With nothing at all on the
// command line, the Studio goes back to the last project, by the same way and past the same trust check as any other
// change of project: only a person starting it directly does that, since the launcher names its project. A model given
// on the command line asks for a new scene, and a capture is a look at what was asked for, which keeps no workspace.
//
// Changed for QOR OS: qq-studio --project <folder> --build <scene> makes a game of the project (gamebuild.h) with no window
// and no display, prints "QQ-BUILD {json}" on standard output and exits: 0 built, 3 refused. Nobody is there to be asked, so it
// builds only a project already on the person's trusted list, and it compiles nothing, as its answer says.
//
// Changed for QOR OS: Qt's QML disk cache only as it is compiled into the Studio, never a .qmlc or .jsc on disk.
//
// Changed for QOR OS: the asset library (assetlibrary.h). --asset-packs <folder>, or QOR_ASSET_PACKS as Prism sets it where the
// Store's format 2 engine exists, names the Store's folder of installed items; with neither there is no library. With no window,
// qq-studio --project <folder> --asset-packs <folder> --use-pack <id> [--pack-file <path>]... [--replace] copies a pack, or those
// of its files, into the project, and --update-pack <id> [--beside] updates it, the new version of each changed copy beside it
// with --beside and skipped without. Each prints "QQ-ASSETS {json}" and exits 0 done, 3 refused, and takes only a project on the
// person's trusted list.

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlextensionplugin.h>

#include <cstdio>
#include <memory>

#include "assetlibrary.h"
#include "confinement.h"
#include "gamebuild.h"
#include "gpu.h"
#include "launcherlink.h"
#include "mcpserver.h"
#include "designloop.h"
#include "sceneio.h"
#include "studiotools.h"
#include "watchdog.h"

Q_IMPORT_QML_PLUGIN(QQPlugin)
Q_IMPORT_QML_PLUGIN(QQ_StudioPlugin)

namespace {

/// Save the window once its scene has settled and a few more frames are drawn; then quit.
void captureWhenSettled(QQuickWindow *window, const QString &file)
{
    auto *frames = new int(0);
    QObject::connect(window, &QQuickWindow::frameSwapped, window, [frames] { ++*frames; });
    auto *poll = new QTimer(window);
    auto *waited = new int(0);
    QObject::connect(poll, &QTimer::timeout, window, [=] {
        ++*waited;
        if (window->property("settled").toBool() && *frames >= 12) {
            poll->stop();
            QCoreApplication::exit(window->grabWindow().save(file) ? 0 : 1);
            return;
        }
        if (*waited > 400) {  // 20 seconds
            poll->stop();
            QCoreApplication::exit(2);
            return;
        }
        window->update();  // a still scene draws once and waits; ask for frames
    });
    poll->start(50);
}

/// A build asks for no window, so it needs no display: it starts no graphical application.
int buildAndExit(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("QQ Studio"));
    QCoreApplication::setOrganizationName(QStringLiteral("Demiurge"));
    QCommandLineParser parser;
    const QCommandLineOption project(QStringLiteral("project"), QStringLiteral("The project folder."), QStringLiteral("folder"));
    const QCommandLineOption build(QStringLiteral("build"), QStringLiteral("The scene the game starts in."), QStringLiteral("scene"));
    parser.addHelpOption();
    parser.addOption(project);
    parser.addOption(build);
    parser.process(app);
    const QString folder = parser.value(project);
    // What is built is one press from playing in the Library. The Studio asks about a project from elsewhere, and here there is
    // nobody to ask, so this takes only what is on the person's list, and adds nothing to it, however empty the folder. It is
    // asked by the build, of the real folder the build then copies, so a link changed meanwhile builds nothing it was not asked.
    qq::GameBuild::Limits limits;
    limits.trusted = [](const QString &real) { return qq::SceneIO::realFolderOnTrustedList(real); };
    QVariantMap result = qq::GameBuild::make(folder.isEmpty() ? folder : QDir().absoluteFilePath(folder), parser.value(build),
                                             qq::GameBuild::gamesFolder(), limits);
    if (result.value(QStringLiteral("ok")).toBool()) {
        result.insert(QStringLiteral("compiled"), false);
        result.insert(QStringLiteral("note"), QObject::tr("Nothing was compiled: this copies the project's files. Code in it that does not "
                                                          "build is found when the game plays, or by building in QQ Studio."));
    }
    // Its answer, on standard output: what asked for it reads that, with nothing else to set.
    const QByteArray line = "QQ-BUILD " + QJsonDocument(QJsonObject::fromVariantMap(result)).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(line.constData(), 1, size_t(line.size()), stdout);
    std::fflush(stdout);
    return result.value(QStringLiteral("ok")).toBool() ? 0 : 3;
}

/// The Store's folder of installed items the library reads: the command line's, else Prism's, else none.
QString assetPacksFrom(const QCommandLineParser &parser, const QCommandLineOption &option)
{
    const QString folder = parser.isSet(option) ? parser.value(option) : qEnvironmentVariable("QOR_ASSET_PACKS");
    return folder.isEmpty() ? QString() : QDir().absoluteFilePath(folder);
}

/// A pack used or updated with no window: nobody is there to be asked, so only a project on the person's list.
int assetsAndExit(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("QQ Studio"));
    QCoreApplication::setOrganizationName(QStringLiteral("Demiurge"));
    QCommandLineParser parser;
    const QCommandLineOption project(QStringLiteral("project"), QStringLiteral("The project folder."), QStringLiteral("folder"));
    const QCommandLineOption packs(QStringLiteral("asset-packs"), QStringLiteral("The Store's folder of installed items."), QStringLiteral("folder"));
    const QCommandLineOption use(QStringLiteral("use-pack"), QStringLiteral("Copy the pack into the project."), QStringLiteral("id"));
    const QCommandLineOption file(QStringLiteral("pack-file"), QStringLiteral("Only this file of it."), QStringLiteral("path"));
    const QCommandLineOption replace(QStringLiteral("replace"), QStringLiteral("Replace a file already there that differs."));
    const QCommandLineOption update(QStringLiteral("update-pack"), QStringLiteral("Update the pack in the project."), QStringLiteral("id"));
    const QCommandLineOption beside(QStringLiteral("beside"), QStringLiteral("Put the new version of each changed copy beside it."));
    parser.addHelpOption();
    for (const QCommandLineOption &option : {project, packs, use, file, replace, update, beside})
        parser.addOption(option);
    parser.process(app);
    const QString folder = parser.value(project).isEmpty() ? QString() : QDir().absoluteFilePath(parser.value(project));
    const QString installed = assetPacksFrom(parser, packs);
    QVariantMap result;
    if (installed.isEmpty()) {
        result = {{QStringLiteral("ok"), false}, {QStringLiteral("problem"), QObject::tr("No folder of installed packs was named.")}};
    } else if (folder.isEmpty() || !qq::SceneIO::realFolderOnTrustedList(qq::Confinement::realPath(folder))) {
        result = {{QStringLiteral("ok"), false},
                  {QStringLiteral("problem"), QObject::tr("%1 is not a project you have said you trust. Open it in QQ Studio and trust it there.").arg(folder)}};
    } else if (parser.isSet(use)) {
        result = qq::AssetLibrary::usePack(folder, installed, parser.value(use), parser.values(file), parser.isSet(replace));
    } else {
        const QVariantMap plan = qq::AssetLibrary::planUpdate(folder, installed, parser.value(update));
        QStringList changed;
        for (const QVariant &each : plan.value(QStringLiteral("changed")).toList())
            changed.append(each.toMap().value(QStringLiteral("path")).toString());
        result = plan.value(QStringLiteral("ok")).toBool()
                     ? qq::AssetLibrary::updatePack(folder, installed, parser.value(update), parser.isSet(beside) ? changed : QStringList())
                     : plan;
    }
    const QByteArray line = "QQ-ASSETS " + QJsonDocument(QJsonObject::fromVariantMap(result)).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(line.constData(), 1, size_t(line.size()), stdout);
    std::fflush(stdout);
    return result.value(QStringLiteral("ok")).toBool() ? 0 : 3;
}

}  // namespace

int main(int argc, char *argv[])
{
    // Either way of writing it: nothing that asks for a build, or for a pack with no window, opens a window.
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--") == 0)
            break;
        if (qstrcmp(argv[i], "--build") == 0 || qstrncmp(argv[i], "--build=", 8) == 0)
            return buildAndExit(argc, argv);
        for (const char *option : {"--use-pack", "--update-pack"}) {
            const uint length = qstrlen(option);
            if (qstrcmp(argv[i], option) == 0 || (qstrncmp(argv[i], option, length) == 0 && argv[i][length] == '='))
                return assetsAndExit(argc, argv);
        }
    }

    // Before any engine exists, which reads these once. What Qt compiled into the Studio itself, and no compiled unit kept on
    // disk: one beside a project's file, or one Qt kept of it before, would be mapped in as code in place of the source that is
    // shown and checked (SceneIO::brokenCode compiles a project's QML before a build looks for units). The Player keeps the
    // cache off altogether.
    qputenv("QML_DISK_CACHE", "aot");
    qunsetenv("QML_FORCE_DISK_CACHE");

    // Before the application exists: the GPU is chosen when the first window is shown.
    const QString gpu = qq::preferHighPerformanceGpu();
    QGuiApplication app(argc, argv);
    // From the start, so the agent can read every QML warning the Studio meets.
    qq::StudioLog::install();
    QGuiApplication::setApplicationName(QStringLiteral("QQ Studio"));
    QGuiApplication::setOrganizationName(QStringLiteral("Demiurge"));
    QQuickStyle::setStyle(QStringLiteral("Material"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("QQ Studio, the QOR Engine's editor"));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("model"), QStringLiteral("A glTF 2.0 model for the new scene."));
    const QCommandLineOption project(QStringLiteral("project"), QStringLiteral("The project folder to work in."),
                                     QStringLiteral("folder"));
    const QCommandLineOption capture(QStringLiteral("capture"),
                                     QStringLiteral("Render, save the window to <file> and exit."),
                                     QStringLiteral("file"));
    const QCommandLineOption channel(QStringLiteral("launcher-channel"),
                                     QStringLiteral("The launcher's channel, given by the launcher that started this."),
                                     QStringLiteral("name"));
    const QCommandLineOption packs(QStringLiteral("asset-packs"), QStringLiteral("The Store's folder of installed items, for the asset library."),
                                   QStringLiteral("folder"));
    parser.addOption(project);
    parser.addOption(capture);
    parser.addOption(channel);
    parser.addOption(packs);
    parser.addOption({QStringLiteral("build"), QStringLiteral("Make a game of the project, starting in <scene>, and exit."), QStringLiteral("scene")});
    parser.addOption({QStringLiteral("use-pack"), QStringLiteral("Copy an installed pack into the project, and exit."), QStringLiteral("id")});
    parser.addOption({QStringLiteral("update-pack"), QStringLiteral("Update a pack the project uses, and exit."), QStringLiteral("id")});
    parser.process(app);

    const QStringList positional = parser.positionalArguments();
    const QUrl model = positional.isEmpty() ? QUrl(QStringLiteral("qrc:/samples/orb.gltf"))
                                            : QUrl::fromLocalFile(QDir().absoluteFilePath(positional.first()));
    const QUrl folder = parser.isSet(project) ? QUrl::fromLocalFile(QDir().absoluteFilePath(parser.value(project)))
                                              : QUrl();

    const bool bareStart = !parser.isSet(project) && positional.isEmpty() && !parser.isSet(capture) && !parser.isSet(channel);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("model"), model},
                                 {QStringLiteral("project"), folder},
                                 {QStringLiteral("reopenLast"), bareStart},
                                 {QStringLiteral("assetPacks"), assetPacksFrom(parser, packs)},
                                 {QStringLiteral("remembers"), !parser.isSet(capture)}});
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.loadFromModule("QQ.Studio", "Main");

    // Play's physics bodies go before their world, not in whatever order the window's teardown would take.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &engine, [&engine] {
        for (QObject *root : engine.rootObjects())
            QMetaObject::invokeMethod(root, "release");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    });

    // Logic in play that never returns would take the Studio, and what is unsaved in it, with it.
    qq::LogicWatchdog watchdog(&engine);
    QObject::connect(&watchdog, &qq::LogicWatchdog::gaveUp, &app, [&engine] {
        for (QObject *root : engine.rootObjects())
            QMetaObject::invokeMethod(root, "stop");
    });

    // The agent's tools, on the Studio's window.
    qq::McpServer mcp;
    std::unique_ptr<qq::StudioTools> tools;
    if (!engine.rootObjects().isEmpty()) {
        if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
            tools = std::make_unique<qq::StudioTools>(window, &engine, &mcp);
            // The Agent panel's loop: the same tools, called in the Studio itself. A child of the window, so the panel
            // that shows it never outlives it.
            auto *designer = new qq::DesignLoop(&mcp, window, engine.singletonInstance<qq::SceneIO *>("QQ", "SceneIO"), window);
            window->setProperty("designer", QVariant::fromValue<QObject *>(designer));
            if (!parser.isSet(capture) && !mcp.listen(qq::McpServer::defaultPipeName()))
                qWarning("Agents cannot connect to this Studio: %s", qPrintable(mcp.error()));

            // Publishing, through the launcher that started this Studio, if one did.
            auto *launcher = new qq::LauncherLink(window);
            if (parser.isSet(channel)) {
                launcher->connectTo(parser.value(channel), qq::LauncherLink::readToken());
                // It goes without asking (ADR-046): Qt's quit closes the window first, and the window would otherwise stop to ask
                // about unsaved work with nobody there to answer. What is unsaved stays in its recovery copy.
                QObject::connect(launcher, &qq::LauncherLink::lost, &app, [window] {
                    window->setProperty("closingUnasked", true);
                    QCoreApplication::quit();
                });
            } else {
                launcher->unavailable(QObject::tr("Open QQ Studio from the launcher to publish: the launcher signs "
                                                  "what you publish, and this Studio holds no key."));
            }
            window->setProperty("launcher", QVariant::fromValue<QObject *>(launcher));
        }
    }

    if (parser.isSet(capture) && !engine.rootObjects().isEmpty()) {
        if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()))
            captureWhenSettled(window, parser.value(capture));
    }
    return QGuiApplication::exec();
}
