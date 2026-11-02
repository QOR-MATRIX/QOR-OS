// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// Does a project become a game that plays, and stays apart from the project? The real build over real folders, the real
// Studio window as a person works it, and the real QQ Player on what was built, judged by the pixels it draws. And the
// hostile side: what cannot go in a game refuses the build and leaves the last game as it was, a file swapped behind its back
// is not followed, and a build stopped dead at any step loses no game, in a real process that really dies.
//
// What the Library then does with a built game is in tests/prism/tst_library.cpp, and tools/dev/first-journey.py goes from one
// to the other with the real programs.

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QMap>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSemaphore>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QtQml/qqmlextensionplugin.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <system_error>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "confinement.h"
#include "gamebuild.h"
#include "gpu.h"
#include "sceneio.h"
#include "store.h"

static const QString chosenGpu = qq::preferHighPerformanceGpu();

Q_IMPORT_QML_PLUGIN(QQPlugin)
Q_IMPORT_QML_PLUGIN(QQ_StudioPlugin)

namespace {

QString fixture(const QString &name)
{
    return QDir(QStringLiteral(QQ_FIXTURES)).filePath(name);
}

bool writeFile(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
}

QByteArray readBytes(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

bool makeLink(const QString &target, const QString &where)
{
    QDir().mkpath(QFileInfo(where).absolutePath());
    std::error_code error;
    std::filesystem::create_symlink(target.toStdString(), where.toStdString(), error);
    return !error;
}

/// A scene whose whole sky is this colour, so whose it is can be read from any frame of it.
QByteArray skyScene(const QString &name, const QString &colour)
{
    return QStringLiteral("import QQ\n\nScene {\n    name: \"%1\"\n    skyTop: \"%2\"\n    skyHorizon: \"%2\"\n}\n")
        .arg(name, colour).toUtf8();
}

/// Every file under a folder, hidden ones too, by what it holds.
QMap<QString, QByteArray> contentsOf(const QString &folder)
{
    QMap<QString, QByteArray> all;
    QDirIterator it(folder, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString found = it.next();
        all.insert(QDir(folder).relativeFilePath(found), readBytes(found));
    }
    return all;
}

/// What a build left in the folder of games besides games.
QStringList leftovers(const QString &games)
{
    return QDir(games).entryList({QStringLiteral(".*")}, QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
}

/// The copies of one game moved aside for a build, by name.
QStringList oldCopiesOf(const QString &games, const QString &name)
{
    return QDir(games).entryList({QStringLiteral(".old-%1-*").arg(name)}, QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
}

/// What a person's own list of projects says, as SceneIO keeps it: `path` is on it.
void listAsTrusted(const QString &dataFolder, const QString &path)
{
    QVERIFY(writeFile(QDir(dataFolder).filePath(QStringLiteral("qor-os/own-projects.json")),
                      QJsonDocument(QJsonArray{qq::Confinement::realPath(path)}).toJson()));
}

/// A project on disk, with a little of everything a game may hold and everything it may not.
struct Project {
    QTemporaryDir parent;
    QString name = QStringLiteral("orb-run");

    QString path() const { return parent.filePath(name); }
    QUrl url() const { return QUrl::fromLocalFile(path()); }
    QString file(const QString &relative) const { return QDir(path()).filePath(relative); }

    void fill() const
    {
        writeFile(file(QStringLiteral("scenes/level.qml")), skyScene(QStringLiteral("level"), QStringLiteral("#ff0000")));
        writeFile(file(QStringLiteral("logic/rule.qml")), "import QQ\n\nLogic {\n}\n");
        writeFile(file(QStringLiteral("prefabs/crate.qml")), "import QQ\n\nGroup {\n}\n");
        writeFile(file(QStringLiteral("assets/sub/tile.png")), "a tile");
    }
    void fillWithWhatStaysBehind() const
    {
        writeFile(file(QStringLiteral(".git/config")), "[remote]\n");
        writeFile(file(QStringLiteral(".env")), "TOKEN=not-for-a-game\n");
        writeFile(file(QStringLiteral("notes.txt")), "a person's notes\n");
        writeFile(file(QStringLiteral("game.json")), "{\"format\": 1, \"title\": \"planted\"}");
        writeFile(file(QStringLiteral("assets/.hidden")), "hidden\n");
        writeFile(file(QStringLiteral("logic/.rule.qml.swp")), "an editor's\n");
    }
};

/// The games a test makes go to its own folder, and what QStandardPaths says for it.
QVariantMap build(const Project &project, const QString &games, const QString &scene = QStringLiteral("scenes/level.qml"))
{
    return qq::GameBuild::make(project.path(), scene, games);
}

/// The real Player, on a data folder of its own. Its exit code, or -1 if it did not finish.
int runPlayer(const QStringList &arguments, QByteArray *said = nullptr)
{
    QTemporaryDir data;
    QProcess player;
    player.setProgram(QStringLiteral(QQ_PLAYER));
    player.setArguments(arguments);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("XDG_DATA_HOME"), data.path());
    player.setProcessEnvironment(environment);
    player.setProcessChannelMode(QProcess::MergedChannels);
    player.start();
    const bool finished = player.waitForFinished(120000);
    if (said)
        *said = player.readAll();
    if (!finished) {
        player.kill();
        player.waitForFinished();
        return -1;
    }
    return player.exitStatus() == QProcess::NormalExit ? player.exitCode() : -1;
}

/// How much of a frame the Player draws of a scene is mostly of this channel (0 red, 1 green, 2 blue), from 0 to 1.
double shareOf(const QImage &image, int channel)
{
    qint64 hits = 0;
    qint64 all = 0;
    for (int y = 0; y < image.height(); y += 4) {
        for (int x = 0; x < image.width(); x += 4) {
            const QColor c = image.pixelColor(x, y);
            const int values[] = {c.red(), c.green(), c.blue()};
            const int other = qMax(values[(channel + 1) % 3], values[(channel + 2) % 3]);
            if (values[channel] > 60 && values[channel] > other * 2)
                ++hits;
            ++all;
        }
    }
    return all ? double(hits) / double(all) : 0;
}

/// The colour the sky of this scene's first frame is, as the Player draws it, or -1 if it is neither: 0 red, 1 green, 2 blue.
int skyOf(const QString &scene)
{
    QTemporaryDir out;
    const QString frame = out.filePath(QStringLiteral("first.png"));
    QByteArray said;
    const int code = runPlayer({scene, QStringLiteral("--capture"), frame}, &said);
    if (code != 0) {
        qWarning("The Player said, of %s, %d: %s", qPrintable(scene), code, said.constData());
        return -1;
    }
    const QImage image(frame);
    if (image.isNull())
        return -1;
    for (int channel = 0; channel < 3; ++channel) {
        if (shareOf(image, channel) > 0.5)
            return channel;
    }
    qWarning("Of %s, no colour holds the frame: red %.2f, green %.2f, blue %.2f", qPrintable(scene), shareOf(image, 0), shareOf(image, 1),
             shareOf(image, 2));
    return -1;
}

/// A build that stops at one of its steps until it is told to go on, on a thread of its own.
struct HeldBuild {
    QSemaphore reached;
    QSemaphore go;
    QVariantMap result;
    std::unique_ptr<QThread> thread;

    qq::GameBuild::Limits limitsHoldingAt(const QByteArray &stage)
    {
        qq::GameBuild::Limits limits;
        limits.step = [this, stage](const char *at) {
            if (stage == at) {
                reached.release();
                go.acquire();
            }
        };
        return limits;
    }
    void start(const QString &project, const QString &scene, const QString &games, const QByteArray &stage)
    {
        const qq::GameBuild::Limits limits = limitsHoldingAt(stage);
        thread.reset(QThread::create([=, this] { result = qq::GameBuild::make(project, scene, games, limits); }));
        thread->start();
    }
    bool hasReachedIt() { return reached.tryAcquire(1, 60000); }
    void finish()
    {
        go.release();
        thread->wait();
    }
    ~HeldBuild()
    {
        if (thread && thread->isRunning())
            finish();
    }
};

/// One open Studio.
struct Studio {
    QQmlApplicationEngine engine;
    QQuickWindow *window = nullptr;

    bool open(const QUrl &project)
    {
        engine.setInitialProperties({{QStringLiteral("model"), QUrl::fromLocalFile(fixture(QStringLiteral("orb.gltf")))},
                                     {QStringLiteral("project"), project}});
        engine.loadFromModule("QQ.Studio", "Main");
        if (engine.rootObjects().isEmpty())
            return false;
        window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        if (!window || !QTest::qWaitForWindowExposed(window))
            return false;
        if (!QTest::qWaitFor([this] { return window->property("settled").toBool(); }, 60000))
            return false;
        [[maybe_unused]] const bool hasKeyboard = QTest::qWaitForWindowActive(window, 15000);
        return true;
    }

    ~Studio()
    {
        if (window)
            QMetaObject::invokeMethod(window, "release");
        window = nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    QVariant call(const char *function, const QVariant &a = {}, const QVariant &b = {})
    {
        QVariant result;
        if (!a.isValid())
            QMetaObject::invokeMethod(window, function, Q_RETURN_ARG(QVariant, result));
        else if (!b.isValid())
            QMetaObject::invokeMethod(window, function, Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, a));
        else
            QMetaObject::invokeMethod(window, function, Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, a), Q_ARG(QVariant, b));
        return result;
    }

    QVariant get(const char *name) const { return window->property(name); }
    QObject *child(const char *name) const { return window->findChild<QObject *>(QLatin1String(name)); }
    QObject *scene() const { return get("scene").value<QObject *>(); }
    QString notice() const { return get("notice").toString(); }
    bool building() const { return child("gameBuild")->property("busy").toBool(); }

    void click(const char *objectName)
    {
        auto *item = window->findChild<QQuickItem *>(QLatin1String(objectName));
        QVERIFY2(item, objectName);
        QTest::mouseClick(window, Qt::LeftButton, {}, item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
    }

    /// The whole sky is this colour now, with what the Studio does after an edit.
    void paintSky(const QString &colour)
    {
        scene()->setProperty("skyTop", QColor(colour));
        scene()->setProperty("skyHorizon", QColor(colour));
        call("refresh");
    }

    /// Build is pressed, and has finished.
    void pressBuild()
    {
        call("say", QString());
        click("buildButton");
        QTRY_VERIFY2_WITH_TIMEOUT(notice().startsWith(QStringLiteral("Built")) || get("noticeIsProblem").toBool(),
                                  qPrintable(notice()), 60000);
        QTRY_VERIFY(!building());
    }

    /// How much of the top of the world, as the window shows it now, is mostly of this channel (shareOf).
    double skyShownOf(int channel)
    {
        auto *view = window->findChild<QQuickItem *>(QStringLiteral("viewport"));
        if (!view)
            return -1;
        window->update();
        QTest::qWait(200);
        const QRectF band = view->mapRectToScene(QRectF(0, 0, view->width(), view->height() / 6));
        const QImage frame = window->grabWindow();
        return shareOf(frame.copy(band.toRect()), channel);
    }
};

}  // namespace

class TstGameBuild : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QQuickStyle::setStyle(QStringLiteral("Material"));
        // Everything kept goes to the test's own folders, never a person's.
        QStandardPaths::setTestModeEnabled(true);
        QGuiApplication::setQuitOnLastWindowClosed(false);
    }

    void init() { QDir(qq::GameBuild::gamesFolder()).removeRecursively(); }
    void cleanupTestCase() { QDir(qq::GameBuild::gamesFolder()).removeRecursively(); }

    void aGameHoldsWhatAGameCanUseAndNoMore()
    {
        Project project;
        project.fill();
        project.fillWithWhatStaysBehind();
        // Marked as a program, as a file from a download can be: what is carried is not.
        QVERIFY(QFile::setPermissions(project.file(QStringLiteral("assets/sub/tile.png")),
                                      QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QTemporaryDir games;

        const QVariantMap result = build(project, games.path());
        QVERIFY2(result.value(QStringLiteral("ok")).toBool(), qPrintable(result.value(QStringLiteral("problem")).toString()));
        const QString folder = result.value(QStringLiteral("folder")).toString();
        QCOMPARE(QFileInfo(folder).absolutePath(), QFileInfo(games.path()).absoluteFilePath());
        QCOMPARE(result.value(QStringLiteral("scene")).toString(), QDir(folder).filePath(QStringLiteral("scenes/level.qml")));
        QCOMPARE(result.value(QStringLiteral("title")).toString(), QStringLiteral("Orb Run"));
        QCOMPARE(result.value(QStringLiteral("build")).toInt(), 1);
        QCOMPARE(result.value(QStringLiteral("files")).toInt(), 4);
        QStringList leftOut = result.value(QStringLiteral("leftOut")).toStringList();
        leftOut.sort();
        QCOMPARE(leftOut, QStringList({QStringLiteral("assets/.hidden"), QStringLiteral("logic/.rule.qml.swp")}));

        const QMap<QString, QByteArray> held = contentsOf(folder);
        const QStringList expected{QStringLiteral("assets/sub/tile.png"), QStringLiteral("game.json"), QStringLiteral("logic/rule.qml"),
                                   QStringLiteral("prefabs/crate.qml"), QStringLiteral("scenes/level.qml")};
        QCOMPARE(held.keys(), expected);

        const QJsonObject manifest = QJsonDocument::fromJson(held.value(QStringLiteral("game.json"))).object();
        QCOMPARE(manifest.value(QStringLiteral("format")).toInt(), qq::GameBuild::manifestFormat);
        QCOMPARE(manifest.value(QStringLiteral("kind")).toString(), QStringLiteral("game"));
        QCOMPARE(manifest.value(QStringLiteral("title")).toString(), QStringLiteral("Orb Run"));
        QCOMPARE(manifest.value(QStringLiteral("scene")).toString(), QStringLiteral("scenes/level.qml"));
        QCOMPARE(manifest.value(QStringLiteral("build")).toInt(), 1);
        QCOMPARE(manifest.value(QStringLiteral("engine")).toObject().value(QStringLiteral("id")).toString(), QStringLiteral("qq"));
        QVERIFY(!manifest.value(QStringLiteral("engine")).toObject().value(QStringLiteral("version")).toString().isEmpty());
        QVERIFY(manifest.value(QStringLiteral("platform")).toString().contains(QLatin1Char('-')));
#ifdef Q_OS_LINUX
        QVERIFY(manifest.value(QStringLiteral("platform")).toString().startsWith(QStringLiteral("linux-")));
#endif
        // Each file, by what it holds, and nothing that says where the person's project is.
        const QJsonObject files = manifest.value(QStringLiteral("files")).toObject();
        QCOMPARE(files.size(), expected.size() - 1);
        for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
            QVERIFY2(held.contains(it.key()), qPrintable(it.key()));
            QCOMPARE(it.value().toString().toLatin1(), QCryptographicHash::hash(held.value(it.key()), QCryptographicHash::Sha256).toHex());
        }
        QVERIFY(!held.value(QStringLiteral("game.json")).contains(project.parent.path().toUtf8()));
        QVERIFY(!held.value(QStringLiteral("game.json")).contains("planted"));

#ifdef Q_OS_UNIX
        // There is no program in a game, whatever the project's files were marked as.
        QVERIFY(QFileInfo(project.file(QStringLiteral("assets/sub/tile.png"))).permissions() & QFile::ExeOwner);
        for (const QString &file : held.keys())
            QVERIFY2(!(QFileInfo(QDir(folder).filePath(file)).permissions() & QFile::ExeOwner), qPrintable(file));
#endif
        // What a player saves is the copy's own, not what the project keeps when it plays in the Studio.
        QVERIFY(qq::GameStore::gameOf(QUrl::fromLocalFile(folder)) != qq::GameStore::gameOf(project.url()));
        QVERIFY(leftovers(games.path()).isEmpty());
    }

    void whatWasBuiltPlaysAndIsNotTheProject()
    {
        Project project;
        project.fill();
        QFile::copy(fixture(QStringLiteral("orb.gltf")), project.file(QStringLiteral("assets/orb.gltf")));
        writeFile(project.file(QStringLiteral("scenes/level.qml")),
                  QByteArrayLiteral("import QQ\n\nScene {\n    name: \"level\"\n    skyTop: \"#ff0000\"\n    skyHorizon: \"#ff0000\"\n"
                                    "    Prop { name: \"Orb\"; position: Qt.vector3d(30, 30, -30); source: \"../assets/orb.gltf\" }\n"
                                    "    Behaviour { name: \"Rule\"; source: \"../logic/rule.qml\"; target: \"Orb\" }\n}\n"));
        QTemporaryDir games;
        const QVariantMap first = build(project, games.path());
        QVERIFY2(first.value(QStringLiteral("ok")).toBool(), qPrintable(first.value(QStringLiteral("problem")).toString()));
        const QString played = first.value(QStringLiteral("scene")).toString();

        // It plays, all of it: its logic builds, its model loads, and it is the copy that played.
        QTemporaryDir out;
        const QString measure = out.filePath(QStringLiteral("measure.json"));
        QByteArray said;
        QCOMPARE(runPlayer({played, QStringLiteral("--measure"), measure}, &said), 0);
        const QJsonObject measured = QJsonDocument::fromJson(readBytes(measure)).object();
        QVERIFY2(!measured.isEmpty(), said.constData());
        QCOMPARE(measured.value(QStringLiteral("problem")).toString(), QString());
        QCOMPARE(measured.value(QStringLiteral("logicErrors")).toArray(), QJsonArray());
        QCOMPARE(measured.value(QStringLiteral("unloaded")).toArray(), QJsonArray());
        QCOMPARE(measured.value(QStringLiteral("scene")).toString(), QUrl::fromLocalFile(played).toString());
#ifdef Q_OS_LINUX
        if (!said.contains("no Landlock"))
            QVERIFY2(said.contains("walled in"), said.constData());
#endif

        // Change the project, and save it. The game does not change; the project does, which shows the pixels can tell.
        QVERIFY(writeFile(project.file(QStringLiteral("scenes/level.qml")), skyScene(QStringLiteral("level"), QStringLiteral("#00ff00"))));
        QCOMPARE(skyOf(played), 0);
        QCOMPARE(skyOf(project.file(QStringLiteral("scenes/level.qml"))), 1);

        // Built again, it does, and the same game is built up.
        const QVariantMap second = build(project, games.path());
        QVERIFY2(second.value(QStringLiteral("ok")).toBool(), qPrintable(second.value(QStringLiteral("problem")).toString()));
        QCOMPARE(second.value(QStringLiteral("folder")).toString(), first.value(QStringLiteral("folder")).toString());
        QCOMPARE(second.value(QStringLiteral("build")).toInt(), 2);
        QCOMPARE(skyOf(played), 1);

        // A scene that needs its model and its logic, built; and then the project gone: the game plays on, all of it.
        QVERIFY(writeFile(project.file(QStringLiteral("scenes/level.qml")),
                          QByteArrayLiteral("import QQ\n\nScene {\n    name: \"level\"\n    skyTop: \"#0000ff\"\n    skyHorizon: \"#0000ff\"\n"
                                            "    Prop { name: \"Orb\"; position: Qt.vector3d(30, 30, -30); source: \"../assets/orb.gltf\" }\n"
                                            "    Behaviour { name: \"Rule\"; source: \"../logic/rule.qml\"; target: \"Orb\" }\n}\n")));
        QVERIFY(build(project, games.path()).value(QStringLiteral("ok")).toBool());
        QVERIFY(QDir(project.path()).removeRecursively());
        QCOMPARE(runPlayer({played, QStringLiteral("--measure"), measure}, &said), 0);
        const QJsonObject without = QJsonDocument::fromJson(readBytes(measure)).object();
        QVERIFY2(!without.isEmpty(), said.constData());
        QCOMPARE(without.value(QStringLiteral("problem")).toString(), QString());
        QCOMPARE(without.value(QStringLiteral("logicErrors")).toArray(), QJsonArray());
        QCOMPARE(without.value(QStringLiteral("unloaded")).toArray(), QJsonArray());
        QCOMPARE(skyOf(played), 2);
    }

    /// What a player saved is the copy's, and building again does not lose it.
    void buildingAgainKeepsWhatAPlayerHasSaved()
    {
        Project project;
        project.fill();
        QVERIFY(writeFile(project.file(QStringLiteral("logic/rule.qml")),
                          "import QQ\n\nLogic {\n    onStarted: save(\"plays\", saved(\"plays\", 0) + 1)\n}\n"));
        QVERIFY(writeFile(project.file(QStringLiteral("scenes/level.qml")),
                          "import QQ\n\nScene {\n    name: \"level\"\n    Behaviour { name: \"Rule\"; source: \"../logic/rule.qml\" }\n}\n"));
        QTemporaryDir games;
        const QVariantMap built = build(project, games.path());
        QVERIFY(built.value(QStringLiteral("ok")).toBool());
        const QString scene = built.value(QStringLiteral("scene")).toString();

        // One data folder for both plays, as a person has.
        QTemporaryDir data;
        QTemporaryDir out;
        QByteArray said;
        const auto play = [&] {
            QProcess player;
            QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
            environment.insert(QStringLiteral("XDG_DATA_HOME"), data.path());
            player.setProcessEnvironment(environment);
            player.setProcessChannelMode(QProcess::MergedChannels);
            player.start(QStringLiteral(QQ_PLAYER), {scene, QStringLiteral("--measure"), out.filePath(QStringLiteral("m.json"))});
            const bool finished = player.waitForFinished(120000);
            said = player.readAll();
            return finished && player.exitCode() == 0;
        };
        QVERIFY2(play(), said.constData());
        const QString game = qq::GameStore::gameOf(qq::Confinement::packageOf(QUrl::fromLocalFile(scene)));
        const QString store = QDir(data.path()).filePath(QStringLiteral("Demiurge/QQ/saves/%1.json").arg(game));
        QCOMPARE(QJsonDocument::fromJson(readBytes(store)).object().value(QStringLiteral("plays")).toInt(), 1);

        QVERIFY(writeFile(project.file(QStringLiteral("assets/more.png")), "more"));
        const QVariantMap again = build(project, games.path());
        QVERIFY(again.value(QStringLiteral("ok")).toBool());
        QCOMPARE(again.value(QStringLiteral("scene")).toString(), scene);
        QVERIFY2(play(), said.constData());
        QCOMPARE(QJsonDocument::fromJson(readBytes(store)).object().value(QStringLiteral("plays")).toInt(), 2);
    }

    /// The title a build writes is one the Library's limit lets in (tests/prism/tst_library.cpp, and first_journey joins them).
    void aLongNameIsCutToATitleTheLibraryTakes()
    {
        Project project;
        project.name = QString(120, QLatin1Char('a'));
        project.fill();
        QTemporaryDir games;
        const QVariantMap built = build(project, games.path());
        QVERIFY2(built.value(QStringLiteral("ok")).toBool(), qPrintable(built.value(QStringLiteral("problem")).toString()));
        QCOMPARE(built.value(QStringLiteral("title")).toString().size(), 80);
    }

    /// The games QQ carries are what a game looks like: nothing in them is taken for a path that is not one.
    void theSamplesBuildAsTheyAre_data()
    {
        QTest::addColumn<QString>("sample");
        QTest::addColumn<QString>("scene");
        QTest::newRow("Orb Run") << QStringLiteral("orb-run") << QStringLiteral("scenes/orb-run.qml");
        QTest::newRow("Ember Climb") << QStringLiteral("ember-climb") << QStringLiteral("scenes/ember-climb.qml");
    }
    void theSamplesBuildAsTheyAre()
    {
        QFETCH(QString, sample);
        QFETCH(QString, scene);
        QTemporaryDir games;
        const QVariantMap built = qq::GameBuild::make(QDir(QStringLiteral(QQ_SAMPLES)).filePath(sample), scene, games.path());
        QVERIFY2(built.value(QStringLiteral("ok")).toBool(), qPrintable(built.value(QStringLiteral("problem")).toString()));
    }

    void twoProjectsOfOneNameAreTwoGames()
    {
        Project one;
        Project other;
        one.fill();
        other.fill();
        QVERIFY(writeFile(one.file(QStringLiteral("logic/rule.qml")), "// the first project's\n"));
        QVERIFY(writeFile(other.file(QStringLiteral("logic/rule.qml")), "// the other project's\n"));
        QTemporaryDir games;

        const QVariantMap first = build(one, games.path());
        const QVariantMap second = build(other, games.path());
        QVERIFY(first.value(QStringLiteral("ok")).toBool());
        QVERIFY(second.value(QStringLiteral("ok")).toBool());
        const QString firstFolder = first.value(QStringLiteral("folder")).toString();
        const QString secondFolder = second.value(QStringLiteral("folder")).toString();
        QVERIFY(firstFolder != secondFolder);
        QCOMPARE(second.value(QStringLiteral("build")).toInt(), 1);
        QCOMPARE(readBytes(QDir(firstFolder).filePath(QStringLiteral("logic/rule.qml"))), QByteArray("// the first project's\n"));
        QCOMPARE(readBytes(QDir(secondFolder).filePath(QStringLiteral("logic/rule.qml"))), QByteArray("// the other project's\n"));

        // Building one leaves the other to the byte.
        const QMap<QString, QByteArray> before = contentsOf(secondFolder);
        QVERIFY(build(one, games.path()).value(QStringLiteral("ok")).toBool());
        QCOMPARE(contentsOf(secondFolder), before);
    }

    void buildingAgainLeavesNothingOfTheLastBuild()
    {
        Project project;
        project.fill();
        QTemporaryDir games;
        const QVariantMap first = build(project, games.path());
        QVERIFY(first.value(QStringLiteral("ok")).toBool());
        QCOMPARE(first.value(QStringLiteral("build")).toInt(), 1);
        const QString folder = first.value(QStringLiteral("folder")).toString();
        QVERIFY(QFile::exists(QDir(folder).filePath(QStringLiteral("assets/sub/tile.png"))));

        // What was left by a build that stopped halfway goes too, and only this game's.
        const QString name = QFileInfo(folder).fileName();
        QVERIFY(writeFile(games.filePath(QStringLiteral(".building-%1-Ab12Cd/scenes/level.qml").arg(name)), "half"));
        QVERIFY(writeFile(games.filePath(QStringLiteral(".old-%1-Ab12Cd/scenes/level.qml").arg(name)), "old"));
        QVERIFY(writeFile(games.filePath(QStringLiteral(".building-another-game-Ab12Cd/scenes/level.qml")), "another's"));

        QVERIFY(QFile::remove(project.file(QStringLiteral("assets/sub/tile.png"))));
        const QVariantMap second = build(project, games.path());
        QVERIFY(second.value(QStringLiteral("ok")).toBool());
        QCOMPARE(second.value(QStringLiteral("build")).toInt(), 2);
        QVERIFY(!QFile::exists(QDir(folder).filePath(QStringLiteral("assets/sub/tile.png"))));
        QCOMPARE(leftovers(games.path()), QStringList{QStringLiteral(".building-another-game-Ab12Cd")});
        QCOMPARE(QJsonDocument::fromJson(readBytes(QDir(folder).filePath(QStringLiteral("game.json"))))
                     .object().value(QStringLiteral("files")).toObject().size(), 3);
    }

    /// Whatever the last game said of itself, its count is only ever a count.
    void aBrokenLastBuildStartsTheCountAgain_data()
    {
        QTest::addColumn<QByteArray>("manifest");
        QTest::newRow("not json") << QByteArray("{ not json");
        QTest::newRow("no count") << QByteArray("{}");
        QTest::newRow("text") << QByteArray("{\"build\": \"7\"}");
        QTest::newRow("below one") << QByteArray("{\"build\": -3}");
        QTest::newRow("past an int") << QByteArray("{\"build\": 99999999999}");
    }
    void aBrokenLastBuildStartsTheCountAgain()
    {
        QFETCH(QByteArray, manifest);
        Project project;
        project.fill();
        QTemporaryDir games;
        const QVariantMap first = build(project, games.path());
        QVERIFY(first.value(QStringLiteral("ok")).toBool());
        QVERIFY(writeFile(QDir(first.value(QStringLiteral("folder")).toString()).filePath(QStringLiteral("game.json")), manifest));
        const QVariantMap second = build(project, games.path());
        QVERIFY(second.value(QStringLiteral("ok")).toBool());
        QCOMPARE(second.value(QStringLiteral("build")).toInt(), 1);
    }

    void whatCannotGoInRefusesTheBuild_data()
    {
        QTest::addColumn<QString>("plant");
        QTest::addColumn<QString>("scene");
        const QString level = QStringLiteral("scenes/level.qml");
        QTest::newRow("a link out of the project") << QStringLiteral("link-out") << level;
        QTest::newRow("a link that stays in it") << QStringLiteral("link-in") << level;
        QTest::newRow("a link to nothing") << QStringLiteral("link-broken") << level;
        QTest::newRow("a link for a folder inside") << QStringLiteral("link-folder") << level;
        QTest::newRow("a link for a whole folder") << QStringLiteral("link-assets") << level;
        QTest::newRow("a link for the scene") << QStringLiteral("link-scene") << level;
#ifdef Q_OS_UNIX
        QTest::newRow("a pipe") << QStringLiteral("pipe") << level;
#endif
        QTest::newRow("a name a URL would decode") << QStringLiteral("name-percent") << level;
#ifdef Q_OS_UNIX
        // On Windows this makes a stream on assets/a, and no such name: there is nothing to refuse.
        QTest::newRow("a name a system would read as more") << QStringLiteral("name-colon") << level;
#endif
        QTest::newRow("a folder that is a file") << QStringLiteral("assets-file") << level;
        QTest::newRow("a scene outside, by its full path") << QString() << QStringLiteral("outside");
        QTest::newRow("a scene that leads out by a link") << QStringLiteral("link-out-scene") << QStringLiteral("scenes/out.qml");
        QTest::newRow("a scene named with a dot") << QStringLiteral("scene-dot") << QStringLiteral("scenes/.level.qml");
        QTest::newRow("compiled QML") << QStringLiteral("file:scenes/Helper.qmlc") << level;
        QTest::newRow("compiled QML, in capitals") << QStringLiteral("file:assets/HELPER.QMLC") << level;
        QTest::newRow("a compiled script") << QStringLiteral("file:logic/lib.jsc") << level;
        QTest::newRow("a compiled module") << QStringLiteral("file:assets/deep/er/lib.mjsc") << level;
        QTest::newRow("compiled QML, named with a dot") << QStringLiteral("file:scenes/.Helper.qmlc") << level;
#ifdef Q_OS_UNIX
        if (geteuid() != 0) {
            QTest::newRow("a folder it may not read") << QStringLiteral("closed-folder") << level;
            QTest::newRow("a file it may not read") << QStringLiteral("closed-file") << level;
        }
#endif
        QTest::newRow("a scene that climbs") << QString() << QStringLiteral("scenes/../../outside.qml");
        QTest::newRow("a scene not in scenes") << QString() << QStringLiteral("logic/rule.qml");
        QTest::newRow("a scene deeper") << QStringLiteral("scene-deep") << QStringLiteral("scenes/deep/level.qml");
        QTest::newRow("a scene not there") << QString() << QStringLiteral("scenes/gone.qml");
        QTest::newRow("no scene") << QString() << QString();
    }
    void whatCannotGoInRefusesTheBuild()
    {
        QFETCH(QString, plant);
        QFETCH(QString, scene);
        Project project;
        project.fill();
        QTemporaryDir outside;
        // Named as a scene is, so nothing but where it is keeps it out.
        const QString secret = outside.filePath(QStringLiteral("secret.qml"));
        QVERIFY(writeFile(secret, skyScene(QStringLiteral("secret"), QStringLiteral("#0000ff"))));

        QTemporaryDir games;
        const QVariantMap before = build(project, games.path());
        QVERIFY(before.value(QStringLiteral("ok")).toBool());
        const QString folder = before.value(QStringLiteral("folder")).toString();
        const QMap<QString, QByteArray> held = contentsOf(folder);

        bool planted = true;
        const auto reopen = qScopeGuard([&] {
            // A folder this test closed has to be open again for its own clean-up.
            QFile::setPermissions(project.file(QStringLiteral("assets/sub")),
                                  QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        });
        if (plant.startsWith(QStringLiteral("file:")))
            planted = writeFile(project.file(plant.mid(5)), "a compiled unit, as far as anyone can tell");
        else if (plant == QStringLiteral("closed-folder"))
            planted = QFile::setPermissions(project.file(QStringLiteral("assets/sub")), QFileDevice::Permissions());
        else if (plant == QStringLiteral("closed-file"))
            planted = QFile::setPermissions(project.file(QStringLiteral("assets/sub/tile.png")), QFileDevice::Permissions());
        else if (plant == QStringLiteral("scene-dot"))
            planted = writeFile(project.file(QStringLiteral("scenes/.level.qml")), skyScene(QStringLiteral("level"), QStringLiteral("#ff0000")));
        else if (plant == QStringLiteral("link-out-scene"))
            planted = makeLink(secret, project.file(QStringLiteral("scenes/out.qml")));
        else if (plant == QStringLiteral("link-out"))
            planted = makeLink(secret, project.file(QStringLiteral("assets/stolen.txt")));
        else if (plant == QStringLiteral("link-in"))
            planted = makeLink(QStringLiteral("../logic/rule.qml"), project.file(QStringLiteral("assets/also.qml")));
        else if (plant == QStringLiteral("link-broken"))
            planted = makeLink(QStringLiteral("/no/such/file"), project.file(QStringLiteral("assets/gone")));
        else if (plant == QStringLiteral("link-folder"))
            planted = makeLink(outside.path(), project.file(QStringLiteral("assets/deep")));
        else if (plant == QStringLiteral("link-assets"))
            planted = QDir(project.file(QStringLiteral("assets"))).removeRecursively()
                   && makeLink(outside.path(), project.file(QStringLiteral("assets")));
        else if (plant == QStringLiteral("link-scene"))
            planted = QFile::remove(project.file(QStringLiteral("scenes/level.qml")))
                   && writeFile(outside.filePath(QStringLiteral("level.qml")), skyScene(QStringLiteral("level"), QStringLiteral("#0000ff")))
                   && makeLink(outside.filePath(QStringLiteral("level.qml")), project.file(QStringLiteral("scenes/level.qml")));
#ifdef Q_OS_UNIX
        else if (plant == QStringLiteral("pipe"))
            planted = mkfifo(project.file(QStringLiteral("assets/pipe")).toLocal8Bit().constData(), 0600) == 0;
#endif
        else if (plant == QStringLiteral("name-percent"))
            planted = writeFile(project.file(QStringLiteral("assets/a%2e%2e.png")), "x");
        else if (plant == QStringLiteral("name-colon"))
            planted = writeFile(project.file(QStringLiteral("assets/a:b.png")), "x");
        else if (plant == QStringLiteral("assets-file"))
            planted = QDir(project.file(QStringLiteral("assets"))).removeRecursively()
                   && writeFile(project.file(QStringLiteral("assets")), "x");
        else if (plant == QStringLiteral("scene-deep"))
            planted = writeFile(project.file(QStringLiteral("scenes/deep/level.qml")), skyScene(QStringLiteral("level"), QStringLiteral("#ff0000")));
        if (!planted)
            QSKIP("This system would not let that be made here.");

        const QString given = scene == QStringLiteral("outside") ? secret : scene;
        const QVariantMap after = qq::GameBuild::make(project.path(), given, games.path());
        QVERIFY(!after.value(QStringLiteral("ok")).toBool());
        QVERIFY(!after.value(QStringLiteral("problem")).toString().isEmpty());
        // The last game as it was, and nothing begun.
        QCOMPARE(contentsOf(folder), held);
        QVERIFY2(leftovers(games.path()).isEmpty(), qPrintable(leftovers(games.path()).join(QLatin1Char(' '))));
    }

    void noProjectFolderIsSaid()
    {
        QTemporaryDir games;
        QVERIFY(!qq::GameBuild::make(games.filePath(QStringLiteral("no-such-project")), QStringLiteral("scenes/level.qml"), games.path())
                     .value(QStringLiteral("ok")).toBool());
        QVERIFY(!qq::GameBuild::make(QString(), QStringLiteral("scenes/level.qml"), games.path()).value(QStringLiteral("ok")).toBool());
    }

    /// A project that holds the folder games go to would be built into itself.
    void aProjectHoldingTheGamesFolderIsRefused()
    {
        Project project;
        project.fill();
        const QVariantMap result = qq::GameBuild::make(project.path(), QStringLiteral("scenes/level.qml"), project.file(QStringLiteral("assets/games")));
        QVERIFY(!result.value(QStringLiteral("ok")).toBool());
    }

    void somethingInTheWayOfTheGameIsNotReplaced_data()
    {
        QTest::addColumn<bool>("link");
        QTest::newRow("a file") << false;
        QTest::newRow("a link") << true;
    }
    void somethingInTheWayOfTheGameIsNotReplaced()
    {
        QFETCH(bool, link);
        Project project;
        project.fill();
        QTemporaryDir games;
        QTemporaryDir outside;
        QVERIFY(writeFile(outside.filePath(QStringLiteral("keep.txt")), "keep"));
        const QString target = QDir(games.path()).filePath(qq::GameBuild::folderNameFor(qq::Confinement::realPath(project.path())));
        if (link) {
            if (!makeLink(outside.path(), target))
                QSKIP("This system would not let that be made here.");
        } else {
            QVERIFY(writeFile(target, "a file"));
        }
        QVERIFY(!build(project, games.path()).value(QStringLiteral("ok")).toBool());
        QCOMPARE(readBytes(outside.filePath(QStringLiteral("keep.txt"))), QByteArray("keep"));
        QVERIFY(link ? QFileInfo(target).isSymLink() : readBytes(target) == "a file");
        QVERIFY(leftovers(games.path()).isEmpty());
    }

    void aGameTooLargeIsRefusedFromWhatItHolds()
    {
        Project project;
        project.fill();
        QTemporaryDir games;
        const QVariantMap before = build(project, games.path());
        QVERIFY(before.value(QStringLiteral("ok")).toBool());
        const QString folder = before.value(QStringLiteral("folder")).toString();
        const QMap<QString, QByteArray> held = contentsOf(folder);

        QVERIFY(writeFile(project.file(QStringLiteral("assets/big.bin")), QByteArray(4096, 'x')));
        qq::GameBuild::Limits small;
        small.bytes = 1024;
        QVERIFY(!qq::GameBuild::make(project.path(), QStringLiteral("scenes/level.qml"), games.path(), small).value(QStringLiteral("ok")).toBool());
        QCOMPARE(contentsOf(folder), held);
        QVERIFY(leftovers(games.path()).isEmpty());

        // Small when the build looked, and past the limit by the time it reads: what counts is what it read.
        const QString big = project.file(QStringLiteral("assets/big.bin"));
        QVERIFY(writeFile(big, "small"));
        qq::GameBuild::Limits grown;
        grown.bytes = 1024;
        grown.step = [big](const char *at) {
            if (qstrcmp(at, "walked") == 0)
                writeFile(big, QByteArray(4096, 'x'));
        };
        QVERIFY(!qq::GameBuild::make(project.path(), QStringLiteral("scenes/level.qml"), games.path(), grown).value(QStringLiteral("ok")).toBool());
        QCOMPARE(contentsOf(folder), held);
        QVERIFY(leftovers(games.path()).isEmpty());
        QVERIFY(QFile::remove(big));

        qq::GameBuild::Limits few;
        few.files = 2;
        QVERIFY(!qq::GameBuild::make(project.path(), QStringLiteral("scenes/level.qml"), games.path(), few).value(QStringLiteral("ok")).toBool());
        QCOMPARE(contentsOf(folder), held);
    }

    // ── what a game names must be in it ──

    void whatAGameNamesMustBeInIt_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<QString>("says");
        const auto scene = [](const QByteArray &literal) {
            return QByteArray("import QQ\n\nScene {\n    name: \"level\"\n    Prop { name: \"P\"; source: \"" + literal + "\" }\n}\n");
        };
        const QString level = QStringLiteral("scenes/level.qml");
        const auto row = [&](const char *name, const QString &file, const QByteArray &text, const QString &says) {
            QTest::newRow(name) << file << text << says;
        };
        row("a model above the project", level, scene("../../shared/tree.glb"), QStringLiteral("../../shared/tree.glb"));
        row("a file URL", level, scene("file:///tmp/tree.glb"), QStringLiteral("file:///tmp/tree.glb"));
        row("a file URL in capitals", level, scene("FILE:///tmp/tree.glb"), QStringLiteral("tree.glb"));
        row("a full path", level, scene("/usr/share/tree.glb"), QStringLiteral("/usr/share/tree.glb"));
        row("a full path, with a drive", level, scene("C:/models/tree.glb"), QStringLiteral("C:/models/tree.glb"));
        row("in the project and not in the game", level, scene("../models/hero.glb"), QStringLiteral("../models/hero.glb"));
        row("beside the folders a game carries", level, scene("../notes.txt"), QStringLiteral("../notes.txt"));
        row("a dot-file", level, scene("../assets/.tex.png"), QStringLiteral("../assets/.tex.png"));
        row("not there at all", level, scene("../assets/gone.png"), QStringLiteral("../assets/gone.png"));
        row("a URL's way of climbing", level, scene("%2e%2e/%2e%2e/tree.glb"), QStringLiteral("tree.glb"));
        row("the other slash", level, scene("..\\\\..\\\\tree.glb"), QStringLiteral("tree.glb"));
        row("an escape's way of climbing", level, scene("\\x2e\\x2e/\\x2e\\x2e/tree.glb"), QStringLiteral("tree.glb"));
        row("an escaped slash", level, scene("..\\/..\\/tree.glb"), QStringLiteral("tree.glb"));
        row("climbing from inside a name", level, scene("sub/../../../tree.glb"), QStringLiteral("tree.glb"));
        row("in a prefab", QStringLiteral("prefabs/crate.qml"),
            "import QQ\n\nGroup {\n    Prop { source: \"../../shared/tree.glb\" }\n}\n", QStringLiteral("../../shared/tree.glb"));
        row("in logic, in single quotes", QStringLiteral("logic/rule.qml"),
            "import QQ\n\nLogic {\n    onStarted: play('../../x.wav')\n}\n", QStringLiteral("../../x.wav"));
        row("in logic, in back quotes", QStringLiteral("logic/rule.qml"),
            "import QQ\n\nLogic {\n    onStarted: play(`../../x.wav`)\n}\n", QStringLiteral("../../x.wav"));
        row("the start of a path made in logic", QStringLiteral("logic/rule.qml"),
            "import QQ\n\nLogic {\n    onStarted: play(`../../${name}.wav`)\n}\n", QStringLiteral("../../"));
        row("an import above", QStringLiteral("logic/rule.qml"),
            "import QQ\nimport \"../lib/x.js\" as X\n\nLogic {\n}\n", QStringLiteral("../lib/x.js"));
        row("an import of what is not there", QStringLiteral("logic/rule.qml"),
            "import QQ\nimport \"gone.js\" as X\n\nLogic {\n}\n", QStringLiteral("gone.js"));
        row("an import of a folder above", QStringLiteral("logic/rule.qml"),
            "import QQ\nimport \"../../other\"\n\nLogic {\n}\n", QStringLiteral("../../other"));
        row("a script's import", QStringLiteral("logic/lib.js"), ".import \"../../x.js\" as X\n", QStringLiteral("../../x.js"));
        row("a module's import", QStringLiteral("logic/lib.mjs"), "import { a } from \"../../x.mjs\"\n", QStringLiteral("../../x.mjs"));
        row("a module's bare import", QStringLiteral("logic/lib.mjs"), "import \"x.mjs\"\n", QStringLiteral("x.mjs"));
        row("a module's re-export", QStringLiteral("logic/lib.mjs"), "export * from \"../../y.mjs\"\n", QStringLiteral("../../y.mjs"));
        row("a module's late import", QStringLiteral("logic/lib.mjs"), "import(\"z.mjs\")\n", QStringLiteral("z.mjs"));
        row("in capitals", QStringLiteral("logic/LIB.JS"), "var x = \"/etc/passwd\"\n", QStringLiteral("/etc/passwd"));
        row("behind a quote in a pattern", QStringLiteral("logic/rule.qml"),
            "import QQ\n\nLogic {\n    function f(x) { return /\"/.test(x) }\n    property string c: \"../../outside.png\"\n}\n",
            QStringLiteral("../../outside.png"));

        const auto gltf = [](const QByteArray &uri) {
            return QByteArray("{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"uri\":\"" + uri + "\",\"byteLength\":4}]}");
        };
        const QString model = QStringLiteral("assets/m.gltf");
        row("a model's buffer above", model, gltf("../../outside.bin"), QStringLiteral("../../outside.bin"));
        row("a model's buffer by its full path", model, gltf("/etc/passwd"), QStringLiteral("/etc/passwd"));
        row("a model's buffer by a file URL", model, gltf("file:///etc/passwd"), QStringLiteral("file:///etc/passwd"));
        row("a model's buffer on the network", model, gltf("https://example.invalid/x.bin"), QStringLiteral("example.invalid"));
        row("a model's buffer not there", model, gltf("gone.bin"), QStringLiteral("gone.bin"));
        row("a model's buffer in the project and not the game", model, gltf("../models/x.bin"), QStringLiteral("../models/x.bin"));
        row("a model's buffer by the other slash", model, gltf("..\\\\..\\\\x.bin"), QStringLiteral("x.bin"));
        row("a model's buffer, percent-encoded", model, gltf("%2e%2e/%2e%2e/x.bin"), QStringLiteral("x.bin"));
        row("a model's picture", model, "{\"images\":[{\"uri\":\"../../p.png\"}]}", QStringLiteral("../../p.png"));
        row("a key written as an escape", model, "{\"buffers\":[{\"\\u0075ri\":\"../../x.bin\"}]}", QStringLiteral("../../x.bin"));
        row("one name twice, the second bad", model, "{\"buffers\":[{\"uri\":\"data:,x\",\"uri\":\"../../x.bin\"}]}", QStringLiteral("../../x.bin"));
        row("a name that is not text", model, "{\"buffers\":[{\"uri\":7}]}", QStringLiteral("assets/m.gltf"));

        // A .glb: its header, and JSON for its first chunk.
        const QByteArray json = "{\"buffers\":[{\"uri\":\"../../x.bin\"}]}";
        QByteArray glb("glTF");
        const auto word = [](quint32 value) { return QByteArray(reinterpret_cast<const char *>(&value), 4); };
        glb += word(2) + word(quint32(20 + json.size())) + word(quint32(json.size())) + "JSON" + json;
        row("a binary model's buffer", QStringLiteral("assets/m.glb"), glb, QStringLiteral("../../x.bin"));
    }
    void whatAGameNamesMustBeInIt()
    {
        QFETCH(QString, file);
        QFETCH(QByteArray, text);
        QFETCH(QString, says);
        Project project;
        project.fill();
        // There in the project, so what keeps these out is that a game does not carry them.
        QVERIFY(writeFile(project.file(QStringLiteral("models/hero.glb")), "a model"));
        QVERIFY(writeFile(project.file(QStringLiteral("models/x.bin")), "a buffer"));
        QVERIFY(writeFile(project.file(QStringLiteral("notes.txt")), "a person's notes"));
        QVERIFY(writeFile(project.file(QStringLiteral("assets/.tex.png")), "a dot-file"));
        QVERIFY(writeFile(project.file(QStringLiteral("lib/x.js")), "var x = 1\n"));
        QTemporaryDir games;
        const QVariantMap before = build(project, games.path());
        QVERIFY2(before.value(QStringLiteral("ok")).toBool(), qPrintable(before.value(QStringLiteral("problem")).toString()));
        const QString folder = before.value(QStringLiteral("folder")).toString();
        const QMap<QString, QByteArray> held = contentsOf(folder);

        QVERIFY(writeFile(project.file(file), text));
        const QVariantMap after = build(project, games.path());
        const QString problem = after.value(QStringLiteral("problem")).toString();
        QVERIFY2(!after.value(QStringLiteral("ok")).toBool(), "it built");
        // What to look at, and why.
        QVERIFY2(problem.contains(file), qPrintable(problem));
        QVERIFY2(problem.contains(says), qPrintable(problem));
        // The last game as it was, and nothing begun.
        QCOMPARE(contentsOf(folder), held);
        QVERIFY2(leftovers(games.path()).isEmpty(), qPrintable(leftovers(games.path()).join(QLatin1Char(' '))));
    }

    /// A plain string in logic that merely looks like a path may be taken for one, so the reason says so.
    void aStringTakenForAPathSaysItMayBeOnlyWords()
    {
        Project project;
        project.fill();
        QVERIFY(writeFile(project.file(QStringLiteral("logic/rule.qml")), "import QQ\n\nLogic {\n    property string hint: \"../back\"\n}\n"));
        QTemporaryDir games;
        const QString problem = build(project, games.path()).value(QStringLiteral("problem")).toString();
        QVERIFY2(problem.contains(QStringLiteral("../back")), qPrintable(problem));
        QVERIFY2(problem.contains(QStringLiteral("only words")), qPrintable(problem));
    }

    void whatAGameNamesOfItsOwnIsCarried_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<QByteArray>("text");
        const auto row = [](const char *name, const QString &file, const QByteArray &text) { QTest::newRow(name) << file << text; };
        const QString level = QStringLiteral("scenes/level.qml");
        const QString rule = QStringLiteral("logic/rule.qml");
        const QString model = QStringLiteral("assets/m.gltf");
        row("a scene's own files", level,
            "import QQ\n\nScene {\n    name: \"level\"\n    sky: \"../assets/sub/tile.png\"\n"
            "    Behaviour { source: \"../logic/rule.qml\" }\n    Prefab { source: \"../prefabs/crate.qml\" }\n}\n");
        row("a folder of the game, and the game itself", rule,
            "import QQ\n\nLogic {\n    property string a: \"../assets/sub/\"\n    property string b: \"../\"\n}\n");
        row("what comments say", rule,
            "import QQ\n// \"../../x\" and file:///etc/passwd\n/* \"/etc/passwd\"\n import \"gone.js\" */\nLogic {\n}\n");
        row("words", rule,
            "import QQ\n\nLogic {\n    property var a: [\"3/5\", \"/\", \"/5\", \"and/or\", \"file:\", \"file: a name\", \"..\", \"...\", \"a ../ b\",\n"
            "                     \"// a note\", \"/ 5\", \"https://qor.invalid/page\", \"qrc:/qt/qml/QQ/x.png\", \"C:\", \"from\"]\n"
            "    property string from: \"not an import\"\n}\n");
        row("a quote in a pattern does not hide what follows, or start anything", rule,
            "import QQ\n\nLogic {\n    function f(x) { return /\"/.test(x) ? \"a\" : \"b\" }\n    property string c: \"../assets/sub/tile.png\"\n}\n");
        row("a path made in logic, from a folder it has", rule,
            "import QQ\n\nLogic {\n    onStarted: play(`../assets/sub/${name}.png`)\n}\n");
        row("an import of its own", rule, "import QQ\nimport \"helper.js\" as H\nimport \"sub\" as S\nimport \"../logic/helper.js\" as Again\n\nLogic {\n}\n");
        row("a module's own", QStringLiteral("logic/m.mjs"), "import { a } from \"./n.mjs\"\nimport { b } from \"../logic/n.mjs\"\nimport \"qrc:/qt/qml/QQ/x.mjs\"\nexport { a, b }\n");
        row("a model's own", model, "{\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,AAAA\"},{\"uri\":\"b.bin\"},"
                                    "{\"uri\":\"sub/tile.png\"},{\"uri\":\"../assets/b.bin\"},{\"uri\":\"tile%20one.png\"}],\"name\":\"../../not a uri\"}");
        // Whatever a file ends in, it is read to its end and no further. None names anything.
        row("ends inside a string", rule, "\"abc");
        row("ends on a backslash in a string", rule, "\"abc\\");
        row("ends on a slash", rule, "a /");
        row("ends inside a comment", rule, "/* never closed");
        row("ends on an escape cut short", rule, "\"\\u00");
        row("nothing but quotes", rule, QByteArray(1 << 20, '"'));
        row("nothing but backslashes", rule, QByteArray(1 << 20, '\\'));
        row("a string a megabyte long", rule, "\"" + QByteArray(1 << 20, 'x') + "\"");
        row("a model that ends on its key", model, "{\"uri\"");
        row("a model that ends on its colon", model, "{\"uri\":");
        row("a model that ends on a space", model, "{\"uri\":   ");
        row("a binary model cut short", QStringLiteral("assets/m.glb"), "glTF");
        row("no binary model at all", QStringLiteral("assets/m.glb"), "not one");
        row("an empty file", rule, "");
        row("an empty model", model, "");
    }
    void whatAGameNamesOfItsOwnIsCarried()
    {
        QFETCH(QString, file);
        QFETCH(QByteArray, text);
        Project project;
        project.fill();
        QVERIFY(writeFile(project.file(QStringLiteral("logic/helper.js")), "var x = 1\n"));
        QVERIFY(writeFile(project.file(QStringLiteral("logic/n.mjs")), "export const a = 1, b = 2\n"));
        QVERIFY(writeFile(project.file(QStringLiteral("logic/sub/x.qml")), "import QQ\n\nLogic {\n}\n"));
        QVERIFY(writeFile(project.file(QStringLiteral("assets/b.bin")), "a buffer"));
        QVERIFY(writeFile(project.file(QStringLiteral("assets/tile one.png")), "a tile"));
        QVERIFY(writeFile(project.file(file), text));
        QTemporaryDir games;
        const QVariantMap built = build(project, games.path());
        QVERIFY2(built.value(QStringLiteral("ok")).toBool(), qPrintable(built.value(QStringLiteral("problem")).toString()));
    }

    /// Past what a build will look through, it says so, and does not read it.
    void whatIsTooLargeToLookThroughIsRefused_data()
    {
        QTest::addColumn<QString>("file");
        QTest::newRow("logic") << QStringLiteral("logic/rule.qml");
        QTest::newRow("a script") << QStringLiteral("logic/big.js");
    }
    void whatIsTooLargeToLookThroughIsRefused()
    {
        QFETCH(QString, file);
        Project project;
        project.fill();
        QVERIFY(writeFile(project.file(file), QByteArray(17 * 1024 * 1024, ' ')));
        QTemporaryDir games;
        const QVariantMap built = build(project, games.path());
        QVERIFY(!built.value(QStringLiteral("ok")).toBool());
        QVERIFY2(built.value(QStringLiteral("problem")).toString().contains(file), qPrintable(built.value(QStringLiteral("problem")).toString()));
        QVERIFY(leftovers(games.path()).isEmpty());
    }

    void whatIsLeftOutIsCountedAndNotListedForever()
    {
        Project project;
        project.fill();
        for (int i = 0; i < 300; ++i)
            QVERIFY(writeFile(project.file(QStringLiteral("assets/.left-%1").arg(i)), "x"));
        QTemporaryDir games;
        const QVariantMap built = build(project, games.path());
        QVERIFY(built.value(QStringLiteral("ok")).toBool());
        QCOMPARE(built.value(QStringLiteral("leftOut")).toStringList().size(), 100);
        QCOMPARE(built.value(QStringLiteral("leftOutCount")).toInt(), 300);
    }

    // ── two builds, and a build stopped dead ──

    void aSecondBuildOfOneGameIsRefused()
    {
        Project project;
        project.fill();
        QTemporaryDir games;
        HeldBuild first;
        first.start(project.path(), QStringLiteral("scenes/level.qml"), games.path(), "walked");
        QVERIFY(first.hasReachedIt());

        // Not a moment's wait: the person is told, and the first goes on whole.
        const QVariantMap second = build(project, games.path());
        QVERIFY(!second.value(QStringLiteral("ok")).toBool());
        QVERIFY2(second.value(QStringLiteral("problem")).toString().contains(QStringLiteral("already being built")),
                 qPrintable(second.value(QStringLiteral("problem")).toString()));

        // Another game is no matter.
        Project other;
        other.fill();
        QVERIFY(build(other, games.path()).value(QStringLiteral("ok")).toBool());

        first.finish();
        QVERIFY2(first.result.value(QStringLiteral("ok")).toBool(), qPrintable(first.result.value(QStringLiteral("problem")).toString()));
        QVERIFY(QFile::exists(QDir(first.result.value(QStringLiteral("folder")).toString()).filePath(QStringLiteral("scenes/level.qml"))));
        QVERIFY2(leftovers(games.path()).isEmpty(), qPrintable(leftovers(games.path()).join(QLatin1Char(' '))));

        // And with it done, it can be built again.
        QCOMPARE(build(project, games.path()).value(QStringLiteral("build")).toInt(), 2);
    }

    /// At every point that matters, a real process that really dies, with no chance to tidy anything.
    void aBuildStoppedDeadLosesNoGame_data()
    {
        QTest::addColumn<QString>("stage");
        QTest::addColumn<bool>("gameThere");
        QTest::addColumn<int>("buildAfterIt");
        QTest::newRow("before anything is copied") << QStringLiteral("walked") << true << 1;
        QTest::newRow("with everything copied") << QStringLiteral("copied") << true << 1;
        QTest::newRow("between the two renames") << QStringLiteral("moved-aside") << false << 1;
        QTest::newRow("with the new game in place") << QStringLiteral("in-place") << true << 2;
    }
    void aBuildStoppedDeadLosesNoGame()
    {
        QFETCH(QString, stage);
        QFETCH(bool, gameThere);
        QFETCH(int, buildAfterIt);
        Project project;
        project.fill();
        QTemporaryDir games;
        const QVariantMap first = build(project, games.path());
        QVERIFY(first.value(QStringLiteral("ok")).toBool());
        const QString folder = first.value(QStringLiteral("folder")).toString();
        const QMap<QString, QByteArray> held = contentsOf(folder);

        // This program again, building and then dying at that step.
        QProcess dying;
        dying.start(QCoreApplication::applicationFilePath(),
                    {QStringLiteral("--die-building-at"), stage, project.path(), QStringLiteral("scenes/level.qml"), games.path()});
        QVERIFY(dying.waitForFinished(120000));
        QCOMPARE(dying.exitCode(), 42);
        QCOMPARE(QDir(folder).exists(), gameThere);

        // A build that is then refused still leaves the last game as it was.
        QVERIFY(!build(project, games.path(), QStringLiteral("scenes/gone.qml")).value(QStringLiteral("ok")).toBool());
        QVERIFY2(QDir(folder).exists(), "the game was left hidden");
        QCOMPARE(QJsonDocument::fromJson(readBytes(QDir(folder).filePath(QStringLiteral("game.json")))).object().value(QStringLiteral("build")).toInt(), buildAfterIt);
        if (buildAfterIt == 1)
            QCOMPARE(contentsOf(folder), held);

        // The next good build counts on from it, and nothing is left of the one that died.
        const QVariantMap next = build(project, games.path());
        QVERIFY2(next.value(QStringLiteral("ok")).toBool(), qPrintable(next.value(QStringLiteral("problem")).toString()));
        QCOMPARE(next.value(QStringLiteral("build")).toInt(), buildAfterIt + 1);
        QVERIFY2(leftovers(games.path()).isEmpty(), qPrintable(leftovers(games.path()).join(QLatin1Char(' '))));
    }

    /// The second rename failing puts the first back.
    void aGameThatCannotBePutInPlaceIsPutBack()
    {
        Project project;
        project.fill();
        QTemporaryDir games;
        const QVariantMap first = build(project, games.path());
        QVERIFY(first.value(QStringLiteral("ok")).toBool());
        const QString folder = first.value(QStringLiteral("folder")).toString();
        const QMap<QString, QByteArray> held = contentsOf(folder);
        const QString name = QFileInfo(folder).fileName();
        const QString gamesPath = games.path();

        qq::GameBuild::Limits limits;
        limits.step = [gamesPath, name](const char *at) {
            if (qstrcmp(at, "moved-aside") != 0)
                return;
            // What was to go into place has gone.
            for (const QString &found : QDir(gamesPath).entryList({QStringLiteral(".building-%1-*").arg(name)}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
                QDir(QDir(gamesPath).filePath(found)).removeRecursively();
        };
        const QVariantMap second = qq::GameBuild::make(project.path(), QStringLiteral("scenes/level.qml"), games.path(), limits);
        QVERIFY(!second.value(QStringLiteral("ok")).toBool());
        QVERIFY(!second.value(QStringLiteral("problem")).toString().isEmpty());
        QCOMPARE(contentsOf(folder), held);
        QVERIFY(oldCopiesOf(games.path(), name).isEmpty());
    }

    /// What is left in the games folder under a leftover's name is only ever looked at, whatever it leads to.
    void aLinkAmongTheLeftoversIsNotFollowed()
    {
        Project project;
        project.fill();
        QTemporaryDir games;
        QTemporaryDir outside;
        QVERIFY(writeFile(outside.filePath(QStringLiteral("keep.txt")), "keep"));
        const QVariantMap first = build(project, games.path());
        const QString folder = first.value(QStringLiteral("folder")).toString();
        const QString name = QFileInfo(folder).fileName();
        if (!makeLink(outside.path(), games.filePath(QStringLiteral(".old-%1-link").arg(name))))
            QSKIP("This system would not let that be made here.");
        QVERIFY(makeLink(outside.path(), games.filePath(QStringLiteral(".building-%1-link").arg(name))));

        // With the game there, and with it gone: a link is never put back as the game.
        QVERIFY(build(project, games.path()).value(QStringLiteral("ok")).toBool());
        QCOMPARE(readBytes(outside.filePath(QStringLiteral("keep.txt"))), QByteArray("keep"));
        QVERIFY(QDir(folder).removeRecursively());
        QFile::remove(games.filePath(QStringLiteral(".old-%1-link").arg(name)));
        QVERIFY(makeLink(outside.path(), games.filePath(QStringLiteral(".old-%1-link").arg(name))));
        QVERIFY(!build(project, games.path(), QStringLiteral("scenes/gone.qml")).value(QStringLiteral("ok")).toBool());
        QVERIFY(!QFileInfo(folder).isSymLink());
        QCOMPARE(readBytes(outside.filePath(QStringLiteral("keep.txt"))), QByteArray("keep"));
    }

    /// What the last game says is read for a number, and never through a link or from a pipe.
    void theLastGamesManifestIsAStrangers_data()
    {
        QTest::addColumn<QString>("what");
        QTest::newRow("a link") << QStringLiteral("link");
#ifdef Q_OS_UNIX
        QTest::newRow("a pipe") << QStringLiteral("pipe");
#endif
    }
    void theLastGamesManifestIsAStrangers()
    {
        QFETCH(QString, what);
        Project project;
        project.fill();
        QTemporaryDir games;
        QTemporaryDir outside;
        const QVariantMap first = build(project, games.path());
        const QString manifest = QDir(first.value(QStringLiteral("folder")).toString()).filePath(QStringLiteral("game.json"));
        QVERIFY(QFile::remove(manifest));
        if (what == QStringLiteral("link")) {
            QVERIFY(writeFile(outside.filePath(QStringLiteral("game.json")), "{\"build\": 99}"));
            if (!makeLink(outside.filePath(QStringLiteral("game.json")), manifest))
                QSKIP("This system would not let that be made here.");
        }
#ifdef Q_OS_UNIX
        else {
            QCOMPARE(mkfifo(manifest.toLocal8Bit().constData(), 0600), 0);
        }
#endif
        // A pipe read would hang here, and this case with it.
        const QVariantMap second = build(project, games.path());
        QVERIFY2(second.value(QStringLiteral("ok")).toBool(), qPrintable(second.value(QStringLiteral("problem")).toString()));
        QCOMPARE(second.value(QStringLiteral("build")).toInt(), 1);
    }

    // ── a file swapped behind the build's back ──

    void aFileSwappedAfterTheLookIsNotFollowed_data()
    {
        QTest::addColumn<QString>("how");
        QTest::newRow("for a link out") << QStringLiteral("link");
#ifdef Q_OS_UNIX
        QTest::newRow("for a pipe") << QStringLiteral("pipe");
        QTest::newRow("its folder, for a link out") << QStringLiteral("folder");
#endif
        QTest::newRow("gone") << QStringLiteral("gone");
    }
    void aFileSwappedAfterTheLookIsNotFollowed()
    {
#ifdef Q_OS_WIN
        QSKIP("Windows has its best check and a stated limit (gamebuild.h): a swap in that gap would pass.");
#endif
        QFETCH(QString, how);
        Project project;
        project.fill();
        QTemporaryDir games;
        QTemporaryDir outside;
        const QString secret = outside.filePath(QStringLiteral("secret.txt"));
        QVERIFY(writeFile(secret, "what no game should carry"));
        QVERIFY(writeFile(outside.filePath(QStringLiteral("tile.png")), "what no game should carry"));
        const QVariantMap before = build(project, games.path());
        QVERIFY(before.value(QStringLiteral("ok")).toBool());
        const QString folder = before.value(QStringLiteral("folder")).toString();
        const QMap<QString, QByteArray> held = contentsOf(folder);

        const QString tile = project.file(QStringLiteral("assets/sub/tile.png"));
        const QString sub = project.file(QStringLiteral("assets/sub"));
        const QString outsidePath = outside.path();
        qq::GameBuild::Limits limits;
        limits.step = [=](const char *at) {
            if (qstrcmp(at, "walked") != 0)
                return;
            if (how == QStringLiteral("folder")) {
                QDir(sub).removeRecursively();
                makeLink(outsidePath, sub);
                return;
            }
            QFile::remove(tile);
            if (how == QStringLiteral("link"))
                makeLink(secret, tile);
#ifdef Q_OS_UNIX
            else if (how == QStringLiteral("pipe"))
                mkfifo(tile.toLocal8Bit().constData(), 0600);
#endif
        };
        const QVariantMap after = qq::GameBuild::make(project.path(), QStringLiteral("scenes/level.qml"), games.path(), limits);
        QVERIFY2(!after.value(QStringLiteral("ok")).toBool(), "it built");
        QVERIFY2(after.value(QStringLiteral("problem")).toString().contains(QStringLiteral("tile.png")),
                 qPrintable(after.value(QStringLiteral("problem")).toString()));
        QCOMPARE(contentsOf(folder), held);
        QVERIFY(leftovers(games.path()).isEmpty());
    }

    /// A real compiled unit, made by Qt's own tool, beside a source that says something else. Were it read, the program behind the
    /// wall would be the unit and not the source, and its contents are not checked as source is: the Player keeps to the source.
    void thePlayerRunsNoCompiledUnit()
    {
        const QString tool = QDir(QLibraryInfo::path(QLibraryInfo::LibraryExecutablesPath)).filePath(QStringLiteral("qmlcachegen"));
        if (!QFile::exists(tool))
            QSKIP("This kit has no qmlcachegen to make a compiled unit with.");
        QTemporaryDir game;
        const QString helper = game.filePath(QStringLiteral("scenes/Helper.qml"));
        QVERIFY(writeFile(helper, "import QtQml\nQtObject {\n    Component.onCompleted: console.log(\"THE-COMPILED-UNIT-RAN\")\n}\n"));
        QVERIFY(writeFile(game.filePath(QStringLiteral("scenes/level.qml")),
                          "import QQ\n\nScene {\n    name: \"level\"\n    Helper {}\n}\n"));
        QProcess compile;
        compile.setProcessChannelMode(QProcess::MergedChannels);
        compile.start(tool, {QStringLiteral("--only-bytecode"), helper, QStringLiteral("-o"), helper + QLatin1Char('c')});
        QVERIFY(compile.waitForFinished(60000));
        QVERIFY2(compile.exitCode() == 0 && QFile::exists(helper + QLatin1Char('c')), compile.readAll().constData());

        // The source says something else now, and still looks as it did to anything that goes by its time.
        const QDateTime when = QFileInfo(helper).lastModified();
        QVERIFY(writeFile(helper, "import QtQml\nQtObject {\n    Component.onCompleted: console.log(\"THE-SOURCE-RAN\")\n}\n"));
        QFile touched(helper);
        QVERIFY(touched.open(QIODevice::ReadWrite));
        QVERIFY(touched.setFileTime(when, QFileDevice::FileModificationTime));
        touched.close();

        qputenv("QT_FORCE_STDERR_LOGGING", "1");
        QTemporaryDir out;
        const QString measure = out.filePath(QStringLiteral("measure.json"));
        QByteArray said;
        QCOMPARE(runPlayer({game.filePath(QStringLiteral("scenes/level.qml")), QStringLiteral("--measure"), measure}, &said), 0);
        QVERIFY2(!said.contains("THE-COMPILED-UNIT-RAN"), said.constData());
        QVERIFY2(said.contains("THE-SOURCE-RAN"), said.constData());
        // And it says so, for a check that does not plant a unit.
        const QJsonObject measured = QJsonDocument::fromJson(readBytes(measure)).object();
        QCOMPARE(measured.value(QStringLiteral("qmlDiskCache")).toBool(true), false);
    }

    /// The first journey, as far as the Studio and the Player go: open a project, import a model, edit, save, build, play
    /// what was built; another project; back to the first; and the Studio started again.
    void theJourneyThroughTheStudio()
    {
        Project first;
        Project second;
        first.name = QStringLiteral("first-game");
        second.name = QStringLiteral("second-game");
        QVERIFY(QDir().mkpath(first.path()));
        QVERIFY(QDir().mkpath(second.path()));
        qq::SceneIO().trustProject(first.url());
        qq::SceneIO().trustProject(second.url());
        const QString firstFolder = QDir(qq::GameBuild::gamesFolder()).filePath(qq::GameBuild::folderNameFor(qq::Confinement::realPath(first.path())));
        const QString secondFolder = QDir(qq::GameBuild::gamesFolder()).filePath(qq::GameBuild::folderNameFor(qq::Confinement::realPath(second.path())));

        {
            Studio studio;
            QVERIFY(studio.open(first.url()));
            // Import: what the model chooser does with the file a person picks. It has a name of its own, because every new
            // scene starts with the Studio's orb, in any project: what must stay in this one is what was imported into it.
            QTemporaryDir picked;
            const QString imported = picked.filePath(QStringLiteral("only-in-first.gltf"));
            QVERIFY(QFile::copy(fixture(QStringLiteral("orb.gltf")), imported));
            QVERIFY(studio.call("add", QStringLiteral("Prop"),
                                QVariantMap{{QStringLiteral("name"), QStringLiteral("Imported")},
                                            {QStringLiteral("source"), QUrl::fromLocalFile(imported)}}).isValid());
            studio.paintSky(QStringLiteral("#ff0000"));
            QVERIFY(studio.get("dirty").toBool());
            studio.pressBuild();
            QVERIFY2(!studio.get("noticeIsProblem").toBool(), qPrintable(studio.notice()));
            QVERIFY2(studio.notice().contains(QStringLiteral("build 1")), qPrintable(studio.notice()));

            // Build saved first: what was on the screen is on the disk, and the model came into the project and the game.
            QVERIFY(!studio.get("dirty").toBool());
            const QString sceneName = studio.get("sceneName").toString();
            const QString relative = QStringLiteral("scenes/%1.qml").arg(sceneName);
            QVERIFY(QFile::exists(first.file(relative)));
            QVERIFY(QFile::exists(first.file(QStringLiteral("assets/only-in-first.gltf"))));
            QCOMPARE(readBytes(QDir(firstFolder).filePath(relative)), readBytes(first.file(relative)));
            QCOMPARE(readBytes(QDir(firstFolder).filePath(QStringLiteral("assets/only-in-first.gltf"))), readBytes(imported));
            QCOMPARE(skyOf(QDir(firstFolder).filePath(relative)), 0);

            // Edit, and do not build. Play is pressed: it runs the project as it stands, edit and all, and the world on the
            // screen shows it, while what was built plays as it was built.
            studio.paintSky(QStringLiteral("#00ff00"));
            QVERIFY(studio.call("saveScene").toBool());
            studio.click("playButton");
            QTRY_VERIFY2(studio.get("playing").toBool(), qPrintable(studio.notice()));
            QObject *playing = studio.get("playScene").value<QObject *>();
            QVERIFY(playing);
            QCOMPARE(playing->property("skyTop").value<QColor>(), QColor(QStringLiteral("#00ff00")));
            QTRY_VERIFY2_WITH_TIMEOUT(studio.skyShownOf(1) > 0.5, "Play does not show the edit", 30000);
            QCOMPARE(skyOf(QDir(firstFolder).filePath(relative)), 0);
            QVERIFY(studio.get("playing").toBool());
            studio.call("stop");
            QTRY_VERIFY(!studio.get("playing").toBool());
            QCOMPARE(skyOf(first.file(relative)), 1);

            // Another project: its own game, with nothing of the first in it.
            QVERIFY(studio.call("switchProject", second.url()).toBool());
            QTRY_VERIFY(studio.get("project").toUrl() == second.url());
            studio.paintSky(QStringLiteral("#0000ff"));
            studio.pressBuild();
            QVERIFY2(!studio.get("noticeIsProblem").toBool(), qPrintable(studio.notice()));
            const QString secondRelative = QStringLiteral("scenes/%1.qml").arg(studio.get("sceneName").toString());
            QCOMPARE(skyOf(QDir(secondFolder).filePath(secondRelative)), 2);
            QCOMPARE(skyOf(QDir(firstFolder).filePath(relative)), 0);
            QVERIFY(!QFile::exists(QDir(secondFolder).filePath(QStringLiteral("assets/only-in-first.gltf"))));
            QVERIFY(!QFile::exists(second.file(QStringLiteral("assets/only-in-first.gltf"))));
            QVERIFY(!readBytes(QDir(secondFolder).filePath(secondRelative)).contains("Imported"));

            // Back to the first, as it was left: the green sky saved, not yet in its game.
            QVERIFY(studio.call("switchProject", first.url()).toBool());
            QTRY_VERIFY(studio.get("project").toUrl() == first.url());
            QTRY_COMPARE(studio.get("sceneName").toString(), sceneName);
            QCOMPARE(studio.scene()->property("skyTop").value<QColor>(), QColor(QStringLiteral("#00ff00")));
        }

        // Started again: the game is still there, and is still what was built.
        QCOMPARE(skyOf(QDir(firstFolder).filePath(QStringLiteral("scenes/untitled.qml"))), 0);
        {
            Studio studio;
            QVERIFY(studio.open(first.url()));
            QTRY_COMPARE(studio.scene()->property("skyTop").value<QColor>(), QColor(QStringLiteral("#00ff00")));
            studio.pressBuild();
            QVERIFY2(studio.notice().contains(QStringLiteral("build 2")), qPrintable(studio.notice()));
            QCOMPARE(skyOf(QDir(firstFolder).filePath(QStringLiteral("scenes/untitled.qml"))), 1);
        }
    }

    /// The owner's "edit, save, build": a scene that was saved, edited, and Build pressed. The game has the edit.
    void anEditedSavedSceneIsWhatIsBuilt()
    {
        Project project;
        project.fill();
        qq::SceneIO().trustProject(project.url());
        Studio studio;
        QVERIFY(studio.open(project.url()));
        QVERIFY(studio.call("openScene", QStringLiteral("level")).toBool());
        QVERIFY(!studio.get("dirty").toBool());
        studio.paintSky(QStringLiteral("#00ff00"));
        QVERIFY(studio.get("dirty").toBool());
        studio.pressBuild();
        QVERIFY2(!studio.get("noticeIsProblem").toBool(), qPrintable(studio.notice()));
        QVERIFY(!studio.get("dirty").toBool());
        const QString folder = QDir(qq::GameBuild::gamesFolder()).filePath(qq::GameBuild::folderNameFor(qq::Confinement::realPath(project.path())));
        QVERIFY(readBytes(QDir(folder).filePath(QStringLiteral("scenes/level.qml"))).contains("#00ff00"));
        QCOMPARE(readBytes(QDir(folder).filePath(QStringLiteral("scenes/level.qml"))), readBytes(project.file(QStringLiteral("scenes/level.qml"))));
        QCOMPARE(skyOf(QDir(folder).filePath(QStringLiteral("scenes/level.qml"))), 1);
    }

    /// A scene with nothing to save that names a model from elsewhere: Build brings it in, as Save would, rather than refusing.
    void aModelFromOutsideComesInWithNothingToSave()
    {
        Project project;
        project.fill();
        qq::SceneIO().trustProject(project.url());
        QTemporaryDir elsewhere;
        const QString model = elsewhere.filePath(QStringLiteral("from-outside.gltf"));
        QVERIFY(QFile::copy(fixture(QStringLiteral("orb.gltf")), model));
        QVERIFY(writeFile(project.file(QStringLiteral("scenes/level.qml")),
                          "import QQ\n\nScene {\n    name: \"level\"\n    Prop {\n        name: \"Far\"\n        source: \""
                              + QUrl::fromLocalFile(model).toString().toUtf8() + "\"\n    }\n}\n"));
        Studio studio;
        QVERIFY(studio.open(project.url()));
        QVERIFY(studio.call("openScene", QStringLiteral("level")).toBool());
        QVERIFY(!studio.get("dirty").toBool());
        studio.pressBuild();
        QVERIFY2(!studio.get("noticeIsProblem").toBool(), qPrintable(studio.notice()));
        QVERIFY(QFile::exists(project.file(QStringLiteral("assets/from-outside.gltf"))));
        QVERIFY(readBytes(project.file(QStringLiteral("scenes/level.qml"))).contains("../assets/from-outside.gltf"));
        const QString folder = QDir(qq::GameBuild::gamesFolder()).filePath(qq::GameBuild::folderNameFor(qq::Confinement::realPath(project.path())));
        QVERIFY(QFile::exists(QDir(folder).filePath(QStringLiteral("assets/from-outside.gltf"))));
        QVERIFY(!studio.get("dirty").toBool());
    }

    /// Inside the project but not among what a game carries: adopting leaves it, so the build says what stays behind.
    void aModelBesideTheGamesFoldersIsSaid()
    {
        Project project;
        project.fill();
        qq::SceneIO().trustProject(project.url());
        QDir().mkpath(project.file(QStringLiteral("models")));
        QVERIFY(QFile::copy(fixture(QStringLiteral("orb.gltf")), project.file(QStringLiteral("models/hero.gltf"))));
        QVERIFY(writeFile(project.file(QStringLiteral("scenes/level.qml")),
                          "import QQ\n\nScene {\n    name: \"level\"\n    Prop {\n        name: \"Hero\"\n        source: \"../models/hero.gltf\"\n    }\n}\n"));
        Studio studio;
        QVERIFY(studio.open(project.url()));
        QVERIFY(studio.call("openScene", QStringLiteral("level")).toBool());
        studio.pressBuild();
        QVERIFY(studio.get("noticeIsProblem").toBool());
        QVERIFY2(studio.notice().contains(QStringLiteral("../models/hero.gltf")), qPrintable(studio.notice()));
        QVERIFY(!QDir(qq::GameBuild::gamesFolder()).exists(qq::GameBuild::folderNameFor(qq::Confinement::realPath(project.path()))));
    }

    /// A project just opened shows a new scene nobody has touched. That is no game of it.
    void aNewUntouchedSceneIsNotBuiltOverAProjectsOwn()
    {
        Project project;
        project.fill();
        qq::SceneIO().trustProject(project.url());
        Studio studio;
        QVERIFY(studio.open(project.url()));
        QCOMPARE(studio.get("sceneName").toString(), QStringLiteral("untitled"));
        const QMap<QString, QByteArray> before = contentsOf(project.path());
        studio.pressBuild();
        QVERIFY(studio.get("noticeIsProblem").toBool());
        QVERIFY2(studio.notice().contains(QStringLiteral("Open the scene the game starts in")), qPrintable(studio.notice()));
        QCOMPARE(contentsOf(project.path()), before);
        QVERIFY(!QDir(qq::GameBuild::gamesFolder()).exists(qq::GameBuild::folderNameFor(qq::Confinement::realPath(project.path()))));

        // With work in it, it is a scene like any other.
        studio.paintSky(QStringLiteral("#00ff00"));
        studio.pressBuild();
        QVERIFY2(!studio.get("noticeIsProblem").toBool(), qPrintable(studio.notice()));
    }

    /// Logic that does not build is said by the Studio's own check, and no game is made of it.
    void codeThatDoesNotBuildRefusesAStudioBuild_data()
    {
        QTest::addColumn<QString>("file");
        QTest::newRow("logic") << QStringLiteral("logic/rule.qml");
        QTest::newRow("a prefab") << QStringLiteral("prefabs/crate.qml");
        QTest::newRow("another scene") << QStringLiteral("scenes/other.qml");
        QTest::newRow("logic in a folder") << QStringLiteral("logic/sub/deep.qml");
    }
    void codeThatDoesNotBuildRefusesAStudioBuild()
    {
        QFETCH(QString, file);
        Project project;
        project.fill();
        qq::SceneIO().trustProject(project.url());
        QVERIFY(writeFile(project.file(file), "import QQ\n\nLogic {\n    this is not QML\n"));
        Studio studio;
        QVERIFY(studio.open(project.url()));
        QVERIFY(studio.call("openScene", QStringLiteral("level")).toBool());
        studio.pressBuild();
        QVERIFY(studio.get("noticeIsProblem").toBool());
        QVERIFY2(studio.notice().contains(file), qPrintable(studio.notice()));
        QVERIFY(!QDir(qq::GameBuild::gamesFolder()).exists(qq::GameBuild::folderNameFor(qq::Confinement::realPath(project.path()))));
    }

    /// Typed and not built: Build saves it, as it saves anything, then says what is wrong with it, and does not say "Built".
    void logicTypedThatDoesNotBuildIsSavedAndRefused()
    {
        Project project;
        project.fill();
        qq::SceneIO().trustProject(project.url());
        Studio studio;
        QVERIFY(studio.open(project.url()));
        QVERIFY(studio.call("openScene", QStringLiteral("level")).toBool());
        QVERIFY(QMetaObject::invokeMethod(studio.child("logicEditor"), "open", Q_ARG(QVariant, QStringLiteral("rule"))));
        const QByteArray typed = "import QQ\n\nLogic {\n    this is not QML\n";
        studio.child("logicText")->setProperty("text", QString::fromUtf8(typed));
        studio.pressBuild();
        QVERIFY(studio.get("noticeIsProblem").toBool());
        QVERIFY2(studio.notice().contains(QStringLiteral("logic/rule.qml")), qPrintable(studio.notice()));
        QCOMPARE(readBytes(project.file(QStringLiteral("logic/rule.qml"))), typed);
        QVERIFY(!QDir(qq::GameBuild::gamesFolder()).exists(qq::GameBuild::folderNameFor(qq::Confinement::realPath(project.path()))));
    }

    /// A save that fails stops the build, and the last game stays as it was.
    void aSaveThatFailsStopsTheBuild_data()
    {
        QTest::addColumn<bool>("logic");
        QTest::newRow("the scene") << false;
        QTest::newRow("the logic") << true;
    }
    void aSaveThatFailsStopsTheBuild()
    {
        QFETCH(bool, logic);
        Project project;
        project.fill();
        qq::SceneIO().trustProject(project.url());
        Studio studio;
        QVERIFY(studio.open(project.url()));
        QVERIFY(studio.call("openScene", QStringLiteral("level")).toBool());
        studio.pressBuild();
        QVERIFY2(!studio.get("noticeIsProblem").toBool(), qPrintable(studio.notice()));
        const QString folder = QDir(qq::GameBuild::gamesFolder()).filePath(qq::GameBuild::folderNameFor(qq::Confinement::realPath(project.path())));
        const QMap<QString, QByteArray> held = contentsOf(folder);

        // A folder in the file's place, on any system: what is written whole then cannot be put there.
        if (logic) {
            QVERIFY(QMetaObject::invokeMethod(studio.child("logicEditor"), "open", Q_ARG(QVariant, QStringLiteral("rule"))));
            studio.child("logicText")->setProperty("text", QStringLiteral("// typed\n"));
            QVERIFY(QFile::remove(project.file(QStringLiteral("logic/rule.qml"))));
            QVERIFY(QDir().mkpath(project.file(QStringLiteral("logic/rule.qml"))));
        } else {
            studio.paintSky(QStringLiteral("#00ff00"));
            QVERIFY(QFile::remove(project.file(QStringLiteral("scenes/level.qml"))));
            QVERIFY(QDir().mkpath(project.file(QStringLiteral("scenes/level.qml"))));
        }
        studio.pressBuild();
        QVERIFY(studio.get("noticeIsProblem").toBool());
        QVERIFY(!studio.building());
        QCOMPARE(contentsOf(folder), held);
    }

    /// With a build running: nothing that would change what it is reading, or put its answer in another project.
    void nothingIsChangedWhileABuildRuns()
    {
        Project project;
        Project other;
        project.fill();
        other.name = QStringLiteral("other-game");
        other.fill();
        qq::SceneIO().trustProject(project.url());
        qq::SceneIO().trustProject(other.url());
        Studio studio;
        QVERIFY(studio.open(project.url()));
        QVERIFY(studio.call("openScene", QStringLiteral("level")).toBool());
        QVERIFY(QMetaObject::invokeMethod(studio.child("logicEditor"), "open", Q_ARG(QVariant, QStringLiteral("rule"))));

        HeldBuild held;
        auto *builder = qobject_cast<qq::GameBuild *>(studio.child("gameBuild"));
        QVERIFY(builder);
        builder->setLimits(held.limitsHoldingAt("walked"));
        studio.click("buildButton");
        QVERIFY(held.hasReachedIt());
        QTRY_VERIFY(studio.building());

        const QByteArray sceneOnDisk = readBytes(project.file(QStringLiteral("scenes/level.qml")));
        const QByteArray logicOnDisk = readBytes(project.file(QStringLiteral("logic/rule.qml")));

        // Another project: no, and why.
        QVERIFY(!studio.call("switchProject", other.url()).toBool());
        QCOMPARE(studio.get("project").toUrl(), project.url());
        QVERIFY(studio.get("noticeIsProblem").toBool());
        QVERIFY2(studio.notice().contains(QStringLiteral("being built")), qPrintable(studio.notice()));

        // Saving the scene: no, and why.
        studio.paintSky(QStringLiteral("#00ff00"));
        studio.call("say", QString());
        QVERIFY(!studio.call("saveScene").toBool());
        QVERIFY(studio.get("noticeIsProblem").toBool());
        QVERIFY2(studio.notice().contains(QStringLiteral("being built")), qPrintable(studio.notice()));
        QCOMPARE(readBytes(project.file(QStringLiteral("scenes/level.qml"))), sceneOnDisk);

        // Saving the logic: no, and why.
        studio.child("logicText")->setProperty("text", QStringLiteral("// typed\n"));
        studio.call("say", QString());
        QVariant saved;
        QVERIFY(QMetaObject::invokeMethod(studio.child("logicEditor"), "save", Q_RETURN_ARG(QVariant, saved)));
        QVERIFY(!saved.toBool());
        QVERIFY(studio.get("noticeIsProblem").toBool());
        QVERIFY2(studio.notice().contains(QStringLiteral("being built")), qPrintable(studio.notice()));
        QCOMPARE(readBytes(project.file(QStringLiteral("logic/rule.qml"))), logicOnDisk);
        QVERIFY(studio.child("logicEditor")->property("dirty").toBool());

        // It ends where it began, saying so there, with what was on the disk when it was pressed.
        held.go.release();
        QTRY_VERIFY(!studio.building());
        QTRY_VERIFY2(studio.notice().startsWith(QStringLiteral("Built")), qPrintable(studio.notice()));
        QCOMPARE(studio.get("project").toUrl(), project.url());
        const QString folder = QDir(qq::GameBuild::gamesFolder()).filePath(qq::GameBuild::folderNameFor(qq::Confinement::realPath(project.path())));
        QCOMPARE(readBytes(QDir(folder).filePath(QStringLiteral("scenes/level.qml"))), sceneOnDisk);

        // And then all of it may be done again.
        builder->setLimits({});
        QVERIFY(studio.call("saveScene").toBool());
        QVariant savedNow;
        QVERIFY(QMetaObject::invokeMethod(studio.child("logicEditor"), "save", Q_RETURN_ARG(QVariant, savedNow)));
        QVERIFY(savedNow.toBool());
        QVERIFY(studio.call("switchProject", other.url()).toBool());
    }

    void whatIsUnsavedInTheLogicEditorIsSavedAndBuilt()
    {
        Project project;
        project.fill();
        qq::SceneIO().trustProject(project.url());
        Studio studio;
        QVERIFY(studio.open(project.url()));
        QVERIFY(studio.call("saveScene").toBool());
        QVERIFY(QMetaObject::invokeMethod(studio.child("logicEditor"), "open", Q_ARG(QVariant, QStringLiteral("rule"))));
        const QByteArray typed = "import QQ\n\nLogic {\n    // typed, and not saved\n}\n";
        studio.child("logicText")->setProperty("text", QString::fromUtf8(typed));
        QVERIFY(studio.child("logicEditor")->property("dirty").toBool());
        studio.pressBuild();
        QVERIFY2(!studio.get("noticeIsProblem").toBool(), qPrintable(studio.notice()));
        const QString folder = QDir(qq::GameBuild::gamesFolder()).filePath(qq::GameBuild::folderNameFor(qq::Confinement::realPath(project.path())));
        QCOMPARE(readBytes(project.file(QStringLiteral("logic/rule.qml"))), typed);
        QCOMPARE(readBytes(QDir(folder).filePath(QStringLiteral("logic/rule.qml"))), typed);
    }

    void aRefusedBuildIsSaid()
    {
        Project project;
        project.fill();
        qq::SceneIO().trustProject(project.url());
        QTemporaryDir outside;
        if (!makeLink(outside.path(), project.file(QStringLiteral("assets/deep"))))
            QSKIP("This system would not let that be made here.");
        Studio studio;
        QVERIFY(studio.open(project.url()));
        QVERIFY(studio.call("openScene", QStringLiteral("level")).toBool());
        studio.pressBuild();
        QVERIFY(studio.get("noticeIsProblem").toBool());
        QVERIFY2(studio.notice().contains(QStringLiteral("assets/deep")), qPrintable(studio.notice()));
        QVERIFY(leftovers(qq::GameBuild::gamesFolder()).isEmpty());
    }

    void noBuildWhilePlaying()
    {
        Project project;
        project.fill();
        qq::SceneIO().trustProject(project.url());
        Studio studio;
        QVERIFY(studio.open(project.url()));
        QVERIFY(studio.call("saveScene").toBool());
        studio.call("play");
        QTRY_VERIFY(studio.get("playing").toBool());
        QVERIFY(!studio.child("buildButton")->property("enabled").toBool());
        studio.call("buildGame");
        QVERIFY(!studio.building());
        QTest::qWait(50);
        QVERIFY(!QDir(qq::GameBuild::gamesFolder()).exists(qq::GameBuild::folderNameFor(qq::Confinement::realPath(project.path()))));
        studio.call("stop");
        QTRY_VERIFY(!studio.get("playing").toBool());
    }

    void aProjectFromElsewhereIsAskedAboutFirst()
    {
        Project project;
        project.fill();
        Studio studio;
        QVERIFY(studio.open(project.url()));
        QVERIFY(!studio.get("projectTrusted").toBool());
        QObject *question = studio.child("trustDialog");
        // Nothing was remembered of it, so nothing of it needed opening, and nothing has asked yet.
        QVERIFY(!question->property("visible").toBool());
        const QString game = qq::GameBuild::folderNameFor(qq::Confinement::realPath(project.path()));
        const QMap<QString, QByteArray> before = contentsOf(project.path());
        studio.paintSky(QStringLiteral("#00ff00"));

        // Asked, and kept closed: nothing is built, nothing is saved in it, and it is still not trusted.
        studio.click("buildButton");
        QTRY_VERIFY2(question->property("opened").toBool(), "the trust question was not asked");
        QVERIFY(!studio.building());
        studio.click("keepClosed");
        QTRY_VERIFY(!question->property("visible").toBool());
        QTest::qWait(50);
        QVERIFY(!studio.building());
        QVERIFY(!QDir(qq::GameBuild::gamesFolder()).exists(game));
        QCOMPARE(contentsOf(project.path()), before);
        QVERIFY(!studio.get("projectTrusted").toBool());

        // Asked again, and trusted: the build goes on from there, with nothing else pressed.
        studio.click("buildButton");
        QTRY_VERIFY2(question->property("opened").toBool(), "the trust question was not asked");
        studio.click("trustProject");
        QTRY_VERIFY(!question->property("visible").toBool());
        QTRY_VERIFY2_WITH_TIMEOUT(studio.notice().startsWith(QStringLiteral("Built")), qPrintable(studio.notice()), 60000);
        QVERIFY(QDir(qq::GameBuild::gamesFolder()).exists(game));
        QVERIFY(studio.get("projectTrusted").toBool());
    }

    /// Code only in prefabs/ is a project's code like any other: the folder is asked about, and placing one waits for that.
    void aProjectWithOnlyPrefabsIsAskedAbout()
    {
        Project project;
        QVERIFY(writeFile(project.file(QStringLiteral("prefabs/coin.qml")),
                          "import QtQml\nimport QQ\nGroup {\n    Component.onCompleted: Qt.application.domain = \"the-prefab-ran\"\n}\n"));
        QVERIFY(!qq::SceneIO().projectTrusted(project.url()));
        // Asking did not put it on the list.
        QVERIFY(!qq::SceneIO().projectTrusted(project.url()));

        QCoreApplication::setOrganizationDomain({});
        Studio studio;
        QVERIFY(studio.open(project.url()));
        QVERIFY(!studio.get("projectTrusted").toBool());
        QVERIFY(!studio.child("trustDialog")->property("visible").toBool());
        QVariantMap file{{QStringLiteral("kind"), QStringLiteral("prefab")},
                         {QStringLiteral("name"), QStringLiteral("coin")},
                         {QStringLiteral("url"), QUrl::fromLocalFile(project.file(QStringLiteral("prefabs/coin.qml"))).toString()}};
        studio.call("useFile", file);
        QTRY_VERIFY2(studio.child("trustDialog")->property("opened").toBool(), "the trust question was not asked");
        QCOMPARE(QCoreApplication::organizationDomain(), QString());
        studio.click("keepClosed");
        QTRY_VERIFY(!studio.child("trustDialog")->property("visible").toBool());
        QCOMPARE(QCoreApplication::organizationDomain(), QString());

        studio.call("useFile", file);
        QTRY_VERIFY(studio.child("trustDialog")->property("opened").toBool());
        studio.click("trustProject");
        QTRY_COMPARE(QCoreApplication::organizationDomain(), QStringLiteral("the-prefab-ran"));
        QCoreApplication::setOrganizationDomain({});
    }

    /// Saving asks first, so this is what asks when there is nothing to save: trust withdrawn from a scene already saved.
    void trustWithdrawnIsAskedAboutWithNothingToSave()
    {
        Project project;
        project.fill();
        qq::SceneIO().trustProject(project.url());
        Studio studio;
        QVERIFY(studio.open(project.url()));
        QVERIFY(studio.call("saveScene").toBool());
        QVERIFY(!studio.get("dirty").toBool());
        QVERIFY(studio.window->setProperty("projectTrusted", false));
        const QString game = qq::GameBuild::folderNameFor(qq::Confinement::realPath(project.path()));

        studio.click("buildButton");
        QTRY_VERIFY2(studio.child("trustDialog")->property("opened").toBool(), "the trust question was not asked");
        QVERIFY(!studio.building());
        QVERIFY(!QDir(qq::GameBuild::gamesFolder()).exists(game));
    }

    /// With no display at all, as a build machine has none: it starts nothing graphical. Its answer is on standard output, with
    /// nothing else asked of it, and a project not on the person's list is refused, with no way past that from here.
    void theStudioBuildsWithNoWindow()
    {
#ifndef Q_OS_LINUX
        QSKIP("Where its games go is set here by XDG_DATA_HOME, Linux's.");
#else
        Project project;
        project.fill();
        QTemporaryDir data;
        const auto run = [&](const QStringList &arguments, QByteArray *said) {
            QProcess studio;
            studio.setProgram(QStringLiteral(QQ_STUDIO));
            studio.setArguments(arguments);
            QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
            environment.remove(QStringLiteral("DISPLAY"));
            environment.remove(QStringLiteral("WAYLAND_DISPLAY"));
            environment.remove(QStringLiteral("QT_FORCE_STDERR_LOGGING"));
            environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("no-such-platform"));
            environment.insert(QStringLiteral("XDG_DATA_HOME"), data.path());
            studio.setProcessEnvironment(environment);
            studio.start();
            if (!studio.waitForFinished(60000)) {
                studio.kill();
                studio.waitForFinished();
                return -1;
            }
            *said = studio.readAllStandardOutput();
            return studio.exitStatus() == QProcess::NormalExit ? studio.exitCode() : -1;
        };
        const auto answer = [](const QByteArray &said) {
            return said.startsWith("QQ-BUILD ") ? QJsonDocument::fromJson(said.mid(9).split('\n').first()).object() : QJsonObject();
        };
        const QStringList building{QStringLiteral("--project"), project.path(), QStringLiteral("--build"), QStringLiteral("scenes/level.qml")};
        const QString games = data.filePath(QStringLiteral("qor-os/games"));
        const QString list = data.filePath(QStringLiteral("qor-os/own-projects.json"));

        // Not on the person's list: refused, in words, and it does not go on the list by being asked for.
        QByteArray said;
        QCOMPARE(run(building, &said), 3);
        const QJsonObject untrusted = answer(said);
        QVERIFY2(!untrusted.value(QStringLiteral("ok")).toBool(true), said.constData());
        QVERIFY2(untrusted.value(QStringLiteral("problem")).toString().contains(QStringLiteral("trust")), said.constData());
        QVERIFY(!QFile::exists(list));
        QVERIFY(QDir(games).isEmpty());

        // An empty folder too, which opening in the Studio would take as the person's own.
        QTemporaryDir empty;
        QCOMPARE(run({QStringLiteral("--project"), empty.path(), QStringLiteral("--build"), QStringLiteral("scenes/level.qml")}, &said), 3);
        QVERIFY(!QFile::exists(list));

        listAsTrusted(data.path(), project.path());
        QCOMPARE(run(building, &said), 0);
        const QJsonObject built = answer(said);
        QVERIFY2(built.value(QStringLiteral("ok")).toBool(), said.constData());
        QVERIFY(built.value(QStringLiteral("folder")).toString().startsWith(data.path() + QStringLiteral("/qor-os/games/")));
        QVERIFY(QFile::exists(built.value(QStringLiteral("scene")).toString()));
        // It says what it did not do.
        QCOMPARE(built.value(QStringLiteral("compiled")).toBool(true), false);
        QVERIFY2(built.value(QStringLiteral("note")).toString().contains(QStringLiteral("compiled")), said.constData());

        // The other way of writing it asks for the same thing, and opens no window: this platform could not.
        QCOMPARE(run({QStringLiteral("--project"), project.path(), QStringLiteral("--build=scenes/level.qml")}, &said), 0);
        QCOMPARE(answer(said).value(QStringLiteral("build")).toInt(), 2);
        QCOMPARE(run({QStringLiteral("--project"), project.path(), QStringLiteral("--build=")}, &said), 3);
        QVERIFY(!answer(said).value(QStringLiteral("problem")).toString().isEmpty());

        QCOMPARE(run({QStringLiteral("--project"), project.path(), QStringLiteral("--build"), QStringLiteral("scenes/gone.qml")}, &said), 3);
        const QJsonObject refused = answer(said);
        QVERIFY2(!refused.value(QStringLiteral("ok")).toBool(true), said.constData());
        QVERIFY(!refused.value(QStringLiteral("problem")).toString().isEmpty());

        // Code that does not build is not looked at from here, and the answer says so rather than pretending.
        QVERIFY(writeFile(project.file(QStringLiteral("scenes/level.qml")), "garbage"));
        QCOMPARE(run(building, &said), 0);
        QCOMPARE(answer(said).value(QStringLiteral("compiled")).toBool(true), false);
#endif
    }

    /// Two programs at once, each its own process: the second says so and takes nothing from the first.
    void aBuildInAnotherProgramIsRefusedToo()
    {
#ifndef Q_OS_LINUX
        QSKIP("Where its games go is set here by XDG_DATA_HOME, Linux's.");
#else
        Project project;
        project.fill();
        QTemporaryDir data;
        listAsTrusted(data.path(), project.path());
        HeldBuild first;
        first.start(project.path(), QStringLiteral("scenes/level.qml"), data.filePath(QStringLiteral("qor-os/games")), "walked");
        QVERIFY(first.hasReachedIt());

        QProcess studio;
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("XDG_DATA_HOME"), data.path());
        studio.setProcessEnvironment(environment);
        studio.start(QStringLiteral(QQ_STUDIO), {QStringLiteral("--project"), project.path(), QStringLiteral("--build"), QStringLiteral("scenes/level.qml")});
        QVERIFY(studio.waitForFinished(60000));
        const QByteArray said = studio.readAllStandardOutput();
        QCOMPARE(studio.exitCode(), 3);
        QVERIFY2(said.contains("already being built"), said.constData());

        first.finish();
        QVERIFY2(first.result.value(QStringLiteral("ok")).toBool(), qPrintable(first.result.value(QStringLiteral("problem")).toString()));
#endif
    }
};

namespace {

/// Build, and die at this step, leaving everything as it stands: what a power cut or a kill does to a build.
[[maybe_unused]] int dieBuilding(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QByteArray stage = argv[2];
    qq::GameBuild::Limits limits;
    limits.step = [stage](const char *at) {
        if (stage == at)
            std::_Exit(42);
    };
    const QVariantMap result = qq::GameBuild::make(QString::fromLocal8Bit(argv[3]), QString::fromLocal8Bit(argv[4]), QString::fromLocal8Bit(argv[5]), limits);
    qWarning("The build did not stop where it was told to: %s", qPrintable(result.value(QStringLiteral("problem")).toString()));
    return 1;
}

}  // namespace

int main(int argc, char *argv[])
{
    if (argc == 6 && qstrcmp(argv[1], "--die-building-at") == 0)
        return dieBuilding(argc, argv);
    QGuiApplication app(argc, argv);
    QCoreApplication::setAttribute(Qt::AA_Use96Dpi, true);
    TstGameBuild test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "tst_gamebuild.moc"
