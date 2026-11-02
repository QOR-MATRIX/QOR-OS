// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// Does a project become a game that plays, and stays apart from the project? The real build over real folders, the real
// Studio window as a person works it, and the real QQ Player on what was built, judged by the pixels it draws. And the
// hostile side: what cannot go in a game refuses the build, reads nothing it should not, and leaves the last game as it was.
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
#include <QMap>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

#include <filesystem>
#include <system_error>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
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
        click("buildButton");
        QTRY_VERIFY2_WITH_TIMEOUT(notice().startsWith(QStringLiteral("Built")) || get("noticeIsProblem").toBool(),
                                  qPrintable(notice()), 60000);
        QTRY_VERIFY(!building());
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

        // The project gone, the game plays on.
        QVERIFY(QDir(project.path()).removeRecursively());
        QCOMPARE(skyOf(played), 1);
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
        QTest::newRow("a name a system would read as more") << QStringLiteral("name-colon") << level;
        QTest::newRow("a folder that is a file") << QStringLiteral("assets-file") << level;
        QTest::newRow("a scene outside") << QString() << QStringLiteral("outside");
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
        const QString secret = outside.filePath(QStringLiteral("secret.txt"));
        QVERIFY(writeFile(secret, "what no game should carry"));

        QTemporaryDir games;
        const QVariantMap before = build(project, games.path());
        QVERIFY(before.value(QStringLiteral("ok")).toBool());
        const QString folder = before.value(QStringLiteral("folder")).toString();
        const QMap<QString, QByteArray> held = contentsOf(folder);

        bool planted = true;
        if (plant == QStringLiteral("link-out"))
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

        qq::GameBuild::Limits few;
        few.files = 2;
        QVERIFY(!qq::GameBuild::make(project.path(), QStringLiteral("scenes/level.qml"), games.path(), few).value(QStringLiteral("ok")).toBool());
        QCOMPARE(contentsOf(folder), held);
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

            // Edit, and do not build: Play here has the edit, and the game has not.
            studio.paintSky(QStringLiteral("#00ff00"));
            QVERIFY(studio.call("saveScene").toBool());
            QCOMPARE(skyOf(QDir(firstFolder).filePath(relative)), 0);
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
        // Opening it asked already: the person said not yet.
        QTRY_VERIFY(question->property("opened").toBool() || !question->property("visible").toBool());
        QMetaObject::invokeMethod(question, "close");
        QTRY_VERIFY(!question->property("visible").toBool());
        const QString game = qq::GameBuild::folderNameFor(qq::Confinement::realPath(project.path()));

        studio.click("buildButton");
        QTRY_VERIFY2(question->property("opened").toBool(), "the trust question was not asked");
        QVERIFY(!studio.building());
        QVERIFY(!QDir(qq::GameBuild::gamesFolder()).exists(game));
    }

    /// With no display at all, as a build machine has none: it starts nothing graphical.
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
            environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("no-such-platform"));
            environment.insert(QStringLiteral("QT_FORCE_STDERR_LOGGING"), QStringLiteral("1"));
            environment.insert(QStringLiteral("XDG_DATA_HOME"), data.path());
            studio.setProcessEnvironment(environment);
            studio.setProcessChannelMode(QProcess::MergedChannels);
            studio.start();
            if (!studio.waitForFinished(60000)) {
                studio.kill();
                studio.waitForFinished();
                return -1;
            }
            *said = studio.readAll();
            return studio.exitStatus() == QProcess::NormalExit ? studio.exitCode() : -1;
        };
        const auto answer = [](const QByteArray &said) {
            const qsizetype at = said.indexOf("QQ-BUILD ");
            return at < 0 ? QJsonObject() : QJsonDocument::fromJson(said.mid(at + 9).split('\n').first()).object();
        };

        QByteArray said;
        QCOMPARE(run({QStringLiteral("--project"), project.path(), QStringLiteral("--build"), QStringLiteral("scenes/level.qml")}, &said), 0);
        const QJsonObject built = answer(said);
        QVERIFY2(built.value(QStringLiteral("ok")).toBool(), said.constData());
        QVERIFY(built.value(QStringLiteral("folder")).toString().startsWith(data.path() + QStringLiteral("/qor-os/games/")));
        QVERIFY(QFile::exists(built.value(QStringLiteral("scene")).toString()));

        QCOMPARE(run({QStringLiteral("--project"), project.path(), QStringLiteral("--build"), QStringLiteral("scenes/gone.qml")}, &said), 3);
        const QJsonObject refused = answer(said);
        QVERIFY2(!refused.value(QStringLiteral("ok")).toBool(), said.constData());
        QVERIFY(!refused.value(QStringLiteral("problem")).toString().isEmpty());
#endif
    }
};

QTEST_MAIN(TstGameBuild)

#include "tst_gamebuild.moc"
