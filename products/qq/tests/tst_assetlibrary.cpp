// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// QQ Studio's asset library (studio/assetlibrary.h), from the hostile side first: what the Store's folder of installed items may
// hold that the Studio refuses, a lock that cannot be used and so changes nothing, a copy over a file already there, an update
// that keeps what a person changed, a change cut short at each of its steps and by the kernel killing the real qq-studio, what
// the build says a game uses, and no library with no folder named. Then the real window, and last the demonstration pack
// installed by the real StoreEngine, where QOR_PRISM_BUILD names a build of Prism that has prism-test-store-install.
//
// Everything this keeps goes to folders of its own: on Linux XDG_DATA_HOME is a temporary folder for this suite and every
// program it starts, so what they keep is what it reads; elsewhere Qt's test mode.

#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
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
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QtQml/qqmlextensionplugin.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <system_error>

#include "assetlibrary.h"
#include "confinement.h"
#include "gamebuild.h"
#include "gpu.h"
#include "projectworkspace.h"
#include "sceneio.h"

static const QString chosenGpu = qq::preferHighPerformanceGpu();

Q_IMPORT_QML_PLUGIN(QQPlugin)
Q_IMPORT_QML_PLUGIN(QQ_StudioPlugin)

using qq::AssetLibrary;

namespace {

const QString kId = QStringLiteral("org.example.test.pack.art");
const QString kDemoPack = QStringLiteral("org.example.demo.pack.ember-climb-art");

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

/// A folder copied file by file, dot-files and all, into a new folder of its own.
bool copyTree(const QString &from, const QString &to)
{
    QDirIterator it(from, QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    if (!QDir().mkpath(to))
        return false;
    while (it.hasNext()) {
        it.next();
        const QString there = to + QLatin1Char('/') + QDir(from).relativeFilePath(it.filePath());
        if (it.fileInfo().isDir() ? !QDir().mkpath(there) : !QFile::copy(it.filePath(), there))
            return false;
    }
    return true;
}

QString hex(const QByteArray &bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

/// A real PNG of one colour, so a picture is what its name says.
QByteArray png(const QColor &colour, int side = 4)
{
    QImage image(side, side, QImage::Format_ARGB32);
    image.fill(colour);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

/// Every file under a folder, a link as what it says it is, and no folder: what a change must leave as it found it.
QMap<QString, QByteArray> snapshot(const QString &root)
{
    QMap<QString, QByteArray> files;
    QDirIterator it(root, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        const QString relative = QDir(root).relativeFilePath(info.filePath());
        if (info.isSymLink())
            files.insert(relative, "link to " + info.symLinkTarget().toUtf8());
        else if (info.isFile())
            files.insert(relative, readBytes(info.filePath()));
    }
    return files;
}

/// A folder as the Store's engine leaves it: installed.json, and items/<id>/ with a manifest, its signature and its files.
struct Store {
    QTemporaryDir dir;
    QJsonObject items;

    QString path() const { return dir.path(); }
    QString folderOf(const QString &id) const { return dir.filePath(QStringLiteral("items/") + id); }
    QString fileOf(const QString &id, const QString &path) const { return folderOf(id) + QLatin1Char('/') + path; }

    void writeRecord() const
    {
        writeFile(dir.filePath(QStringLiteral("installed.json")),
                  QJsonDocument(QJsonObject{{QStringLiteral("format"), 1}, {QStringLiteral("transaction"), QString()}, {QStringLiteral("items"), items}}).toJson());
    }

    /// A pack, with its licence and notice beside whatever else it holds.
    void add(const QString &id, const QString &version, QMap<QString, QByteArray> files, const QString &kind = QStringLiteral("asset-pack"))
    {
        if (!files.contains(QStringLiteral("LICENSE.txt")))
            files.insert(QStringLiteral("LICENSE.txt"), "Apache License\nVersion 2.0, January 2004\n");
        if (!files.contains(QStringLiteral("NOTICE.txt")))
            files.insert(QStringLiteral("NOTICE.txt"), "Test art, for QQ's asset library suite.\n");
        QDir(folderOf(id)).removeRecursively();
        QJsonObject listed;
        for (auto it = files.cbegin(); it != files.cend(); ++it) {
            writeFile(fileOf(id, it.key()), it.value());
            listed.insert(it.key(), hex(it.value()));
        }
        writeManifest(id, version, listed, kind);
        writeFile(fileOf(id, QStringLiteral("manifest.sig")), QByteArray(64, '\x01'));
        items.insert(id, QJsonObject{{QStringLiteral("kind"), kind}, {QStringLiteral("name"), QStringLiteral("Test art")},
                                     {QStringLiteral("version"), version}, {QStringLiteral("developer"), QStringLiteral("org.example.test")},
                                     {QStringLiteral("installedAt"), QStringLiteral("2026-11-03T00:00:00Z")}});
        writeRecord();
    }

    void writeManifest(const QString &id, const QString &version, const QJsonObject &listed, const QString &kind = QStringLiteral("asset-pack")) const
    {
        const QJsonObject manifest{{QStringLiteral("format"), 2},
                                   {QStringLiteral("id"), id},
                                   {QStringLiteral("kind"), kind},
                                   {QStringLiteral("version"), version},
                                   {QStringLiteral("developer"), QStringLiteral("org.example.test")},
                                   {QStringLiteral("name"), QStringLiteral("Test art")},
                                   {QStringLiteral("licence"), QJsonObject{{QStringLiteral("spdx"), QStringLiteral("Apache-2.0")},
                                                                         {QStringLiteral("file"), QStringLiteral("LICENSE.txt")}}},
                                   {QStringLiteral("stores"), QJsonArray{QStringLiteral("creator")}},
                                   {QStringLiteral("platforms"), QJsonArray{QStringLiteral("linux-x86_64")}},
                                   {QStringLiteral("dependencies"), QJsonArray{}},
                                   {QStringLiteral("engines"), QJsonArray{}},
                                   {QStringLiteral("files"), listed}};
        writeFile(fileOf(id, QStringLiteral("manifest.json")), QJsonDocument(manifest).toJson());
    }

    QJsonObject manifestOf(const QString &id) const { return QJsonDocument::fromJson(readBytes(fileOf(id, QStringLiteral("manifest.json")))).object(); }

    void remove(const QString &id)
    {
        items.remove(id);
        QDir(folderOf(id)).removeRecursively();
        writeRecord();
    }
};

const QColor red(255, 0, 0), green(0, 255, 0), blue(0, 0, 255), white(255, 255, 255), amber(255, 190, 0), violet(150, 0, 255);

/// Version 1.0.0: a hero, tiles, and two the next version drops.
QMap<QString, QByteArray> firstVersion()
{
    return {{QStringLiteral("art/hero.png"), png(red)}, {QStringLiteral("art/tiles.png"), png(green)},
            {QStringLiteral("art/old.png"), png(blue)}, {QStringLiteral("art/kept.png"), png(white)}};
}

/// Version 1.1.0: a new hero, the same tiles, a torch, and neither old nor kept.
QMap<QString, QByteArray> secondVersion()
{
    return {{QStringLiteral("art/hero.png"), png(amber)}, {QStringLiteral("art/tiles.png"), png(green)},
            {QStringLiteral("art/torch.png"), png(violet)}};
}

struct Project {
    QTemporaryDir parent;
    Project() { QDir().mkpath(path()); }
    QString path() const { return parent.filePath(QStringLiteral("game")); }
    QUrl url() const { return QUrl::fromLocalFile(path()); }
    QString real() const { return qq::Confinement::realPath(path()); }
    QString file(const QString &relative) const { return QDir(path()).filePath(relative); }
    QString copy(const QString &source, const QString &id = kId) const { return file(QStringLiteral("assets/%1/%2").arg(id, source)); }
    QString lockPath() const { return file(QStringLiteral("assets.lock.json")); }
    QJsonObject lock() const { return QJsonDocument::fromJson(readBytes(lockPath())).object(); }
    QJsonObject lockedPack(const QString &id = kId) const
    {
        for (const QJsonValue &each : lock().value(QStringLiteral("packs")).toArray()) {
            if (each.toObject().value(QStringLiteral("id")).toString() == id)
                return each.toObject();
        }
        return {};
    }
    QJsonObject lockedFile(const QString &path, const QString &id = kId) const
    {
        for (const QJsonValue &each : lockedPack(id).value(QStringLiteral("files")).toArray()) {
            if (each.toObject().value(QStringLiteral("path")).toString() == path)
                return each.toObject();
        }
        return {};
    }
    void saveScene() const
    {
        writeFile(file(QStringLiteral("scenes/level.qml")), "import QQ\n\nScene {\n    name: \"level\"\n    Sun {\n    }\n}\n");
    }
};

QString inProject(const QString &source, const QString &id = kId)
{
    return QStringLiteral("assets/%1/%2").arg(id, source);
}

void trust(const QUrl &project)
{
    qq::SceneIO().trustProject(project);
}

bool ok(const QVariantMap &result)
{
    if (!result.value(QStringLiteral("ok")).toBool())
        qWarning("Refused: %s", qPrintable(result.value(QStringLiteral("problem")).toString()));
    return result.value(QStringLiteral("ok")).toBool();
}

QString problemOf(const QVariantMap &result)
{
    return result.value(QStringLiteral("problem")).toString();
}

QStringList pathsOf(const QVariant &list)
{
    QStringList paths;
    for (const QVariant &each : list.toList())
        paths.append(each.typeId() == QMetaType::QString ? each.toString() : each.toMap().value(QStringLiteral("path")).toString());
    paths.sort();
    return paths;
}

/// One open Studio, as tst_gamebuild opens it, with the library's folder.
struct Studio {
    QQmlApplicationEngine engine;
    QQuickWindow *window = nullptr;

    bool open(const QUrl &project, const QString &packs)
    {
        engine.setInitialProperties({{QStringLiteral("model"), QUrl::fromLocalFile(fixture(QStringLiteral("orb.gltf")))},
                                     {QStringLiteral("project"), project},
                                     {QStringLiteral("assetPacks"), packs}});
        engine.loadFromModule("QQ.Studio", "Main");
        if (engine.rootObjects().isEmpty())
            return false;
        window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        if (!window || !QTest::qWaitForWindowExposed(window))
            return false;
        return QTest::qWaitFor([this] { return window->property("settled").toBool(); }, 60000);
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
    QVariantMap assets() const { return get("assetState").toMap(); }
    /// By its name: a view's delegates are found through the items they are drawn in, which owns them; anything else as a child.
    QObject *child(const QString &name) const
    {
        if (QObject *found = window->findChild<QObject *>(name))
            return found;
        QList<QQuickItem *> items{window->contentItem()};
        while (!items.isEmpty()) {
            QQuickItem *item = items.takeFirst();
            if (item->objectName() == name)
                return item;
            items += item->childItems();
        }
        return nullptr;
    }
    QString notice() const { return get("notice").toString(); }
    bool shown(const QString &name) const
    {
        auto *item = qobject_cast<QQuickItem *>(child(name));
        return item && item->isVisible();
    }
    bool opened(const char *dialog) const { return child(QLatin1String(dialog)) && child(QLatin1String(dialog))->property("opened").toBool(); }
    void click(const QString &name)
    {
        auto *item = qobject_cast<QQuickItem *>(child(name));
        QVERIFY2(item, qPrintable(name));
        QTest::mouseClick(window, Qt::LeftButton, {}, item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
    }
};

/// The real qq-studio, with no window, on the library's command line: its answer, and its exit code in `code`.
QVariantMap runStudio(const QStringList &arguments, int *code = nullptr, const QProcessEnvironment &environment = QProcessEnvironment::systemEnvironment())
{
    QProcess studio;
    QProcessEnvironment env = environment;
    env.insert(QStringLiteral("ASAN_OPTIONS"), env.value(QStringLiteral("ASAN_OPTIONS")) + QStringLiteral(":detect_leaks=0"));
    studio.setProcessEnvironment(env);
    studio.start(QStringLiteral(QQ_STUDIO), arguments);
    if (!studio.waitForFinished(120000))
        return {};
    if (code)
        *code = studio.exitCode();
    for (const QByteArray &line : studio.readAllStandardOutput().split('\n')) {
        if (line.startsWith("QQ-ASSETS "))
            return QJsonDocument::fromJson(line.mid(10)).object().toVariantMap();
    }
    qWarning("qq-studio said: %s", studio.readAllStandardError().constData());
    return {};
}

/// A program of the Prism build QOR_PRISM_BUILD names, or "".
QString prismProgram(const QString &name)
{
    const QString build = qEnvironmentVariable("QOR_PRISM_BUILD");
    if (build.isEmpty())
        return {};
    const QString usual = QDir(build).filePath(QStringLiteral("tests/") + name);
    if (QFileInfo(usual).isExecutable())
        return usual;
    QDirIterator it(build, {name}, QDir::Files | QDir::Executable, QDirIterator::Subdirectories);
    return it.hasNext() ? it.next() : QString();
}

/// The real StoreEngine, through prism-test-store-install: 0 done.
int storeEngine(const QString &program, const QStringList &arguments)
{
    QProcess install;
    install.setProcessChannelMode(QProcess::MergedChannels);
    install.start(program, arguments);
    if (!install.waitForFinished(120000))
        return -1;
    if (install.exitCode() != 0)
        qWarning("prism-test-store-install said: %s", install.readAll().constData());
    return install.exitCode();
}

enum class Plant { LinkInIt, FileNotListed, WrongDigest, HugeFile, ClimbingPath, FolderIsALink, ProgramAsPicture, ModelNamesOutside,
                   ManifestOfAnother, RecordIsHuge, Claimed, MarkupIsNotText, ManifestIsALink, TwoUris };

}  // namespace

class TstAssetLibrary : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QQuickStyle::setStyle(QStringLiteral("Material"));
        QGuiApplication::setQuitOnLastWindowClosed(false);
        QVERIFY(QFileInfo(QStringLiteral(QQ_STUDIO)).isExecutable());
    }

    // ── What the Store's folder may hold ──

    void aGoodPackIsReadWithAllItSays()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        QStringList refused;
        const QList<AssetLibrary::Pack> packs = AssetLibrary::readInstalled(store.path(), &refused);
        QVERIFY2(refused.isEmpty(), qPrintable(refused.join(QLatin1Char('\n'))));
        QCOMPARE(packs.size(), 1);
        const AssetLibrary::Pack &pack = packs.first();
        QCOMPARE(pack.id, kId);
        QCOMPARE(pack.version, QStringLiteral("1.0.0"));
        QCOMPARE(pack.developer, QStringLiteral("org.example.test"));
        QCOMPARE(pack.licence, QStringLiteral("Apache-2.0"));
        QCOMPARE(pack.files.size(), 6);
        QCOMPARE(pack.file(QStringLiteral("art/hero.png"))->kind, QStringLiteral("picture"));
        QCOMPARE(pack.file(QStringLiteral("LICENSE.txt"))->kind, QStringLiteral("text"));
        QCOMPARE(QString::fromLatin1(pack.manifestSha256.toHex()), hex(readBytes(store.fileOf(kId, QStringLiteral("manifest.json")))));
        // A model naming its buffer beside it, and one carrying it as data, are a pack's too.
        Store models;
        models.add(kId, QStringLiteral("1.0.0"),
                   {{QStringLiteral("orb.gltf"), readBytes(fixture(QStringLiteral("orb.gltf")))}});
        const QList<AssetLibrary::Pack> withModel = AssetLibrary::readInstalled(models.path(), &refused);
        QVERIFY2(withModel.size() == 1, qPrintable(refused.join(QLatin1Char('\n'))));
    }

    void aPlantedPackIsRefused_data()
    {
        QTest::addColumn<Plant>("plant");
        QTest::addColumn<QString>("said");
        QTest::newRow("a link among its files") << Plant::LinkInIt << QStringLiteral("link");
        QTest::newRow("a file its manifest does not list") << Plant::FileNotListed << QStringLiteral("does not list");
        QTest::newRow("a digest that does not match") << Plant::WrongDigest << QStringLiteral("not what its manifest says");
        QTest::newRow("a file larger than a pack may hold") << Plant::HugeFile << QStringLiteral("larger than it may be");
        QTest::newRow("a path that climbs") << Plant::ClimbingPath << QStringLiteral("may not hold");
        QTest::newRow("its folder a link") << Plant::FolderIsALink << QStringLiteral("link");
        QTest::newRow("a program named as a picture") << Plant::ProgramAsPicture << QStringLiteral("not what its name says");
        QTest::newRow("a model naming a file outside") << Plant::ModelNamesOutside << QStringLiteral("not beside it");
        QTest::newRow("a manifest of another version") << Plant::ManifestOfAnother << QStringLiteral("does not say it is this pack");
        QTest::newRow("a record too large") << Plant::RecordIsHuge << QStringLiteral("larger than it may be");
        QTest::newRow("a record the Store did not check") << Plant::Claimed << QStringLiteral("has not checked it");
        QTest::newRow("a licence with a control character") << Plant::MarkupIsNotText << QStringLiteral("not plain text");
        QTest::newRow("its manifest a link") << Plant::ManifestIsALink << QStringLiteral("link");
        QTest::newRow("a model naming two files in one place") << Plant::TwoUris << QStringLiteral("twice");
    }

    void aPlantedPackIsRefused()
    {
        QFETCH(Plant, plant);
        QFETCH(QString, said);
        Store store;
        QTemporaryDir elsewhere;
        writeFile(elsewhere.filePath(QStringLiteral("secret.png")), png(red));
        QMap<QString, QByteArray> files = firstVersion();
        const QString gltf = QStringLiteral(R"({"asset":{"version":"2.0"},"buffers":[{"uri":"%1","byteLength":4}]})");
        if (plant == Plant::ModelNamesOutside)
            files.insert(QStringLiteral("art/orb.gltf"), gltf.arg(QStringLiteral("../../outside.bin")).toUtf8());
        if (plant == Plant::TwoUris)
            files.insert(QStringLiteral("art/orb.gltf"),
                         QByteArray(R"({"asset":{"version":"2.0"},"buffers":[{"uri":"data:application/octet-stream;base64,AAAA","uri":"../../x.bin","byteLength":4}]})"));
        if (plant == Plant::ProgramAsPicture)
            files.insert(QStringLiteral("art/tool.png"), QByteArray("\x7f" "ELF\x02\x01\x01", 7) + QByteArray(64, '\0'));
        if (plant == Plant::MarkupIsNotText)
            files.insert(QStringLiteral("LICENSE.txt"), "Apache License\x1b[31m\n");
        store.add(kId, QStringLiteral("1.0.0"), files);
        QJsonObject listed = store.manifestOf(kId).value(QStringLiteral("files")).toObject();

        switch (plant) {
        case Plant::LinkInIt:
            QFile::remove(store.fileOf(kId, QStringLiteral("art/hero.png")));
            if (!makeLink(elsewhere.filePath(QStringLiteral("secret.png")), store.fileOf(kId, QStringLiteral("art/hero.png"))))
                QSKIP("This user cannot make symbolic links here.");
            listed.insert(QStringLiteral("art/hero.png"), hex(png(red)));
            store.writeManifest(kId, QStringLiteral("1.0.0"), listed);
            break;
        case Plant::FileNotListed:
            writeFile(store.fileOf(kId, QStringLiteral("art/extra.png")), png(red));
            break;
        case Plant::WrongDigest:
            writeFile(store.fileOf(kId, QStringLiteral("art/hero.png")), png(blue));
            break;
        case Plant::HugeFile: {
            QFile huge(store.fileOf(kId, QStringLiteral("art/huge.png")));
            QVERIFY(huge.open(QIODevice::WriteOnly));
            huge.write("\x89PNG\r\n\x1a\n");
            QVERIFY(huge.resize(AssetLibrary::largestPack + 1));
            huge.close();
            listed.insert(QStringLiteral("art/huge.png"), hex("not the bytes"));
            store.writeManifest(kId, QStringLiteral("1.0.0"), listed);
            break;
        }
        case Plant::ClimbingPath:
            writeFile(store.dir.filePath(QStringLiteral("items/outside.png")), png(red));
            listed.insert(QStringLiteral("../outside.png"), hex(png(red)));
            store.writeManifest(kId, QStringLiteral("1.0.0"), listed);
            break;
        case Plant::FolderIsALink: {
            const QString real = store.dir.filePath(QStringLiteral("elsewhere-pack"));
            QVERIFY(QDir().rename(store.folderOf(kId), real));
            if (!makeLink(real, store.folderOf(kId)))
                QSKIP("This user cannot make symbolic links here.");
            break;
        }
        case Plant::ManifestOfAnother:
            store.writeManifest(kId, QStringLiteral("2.0.0"), listed);
            break;
        case Plant::RecordIsHuge:
            writeFile(store.dir.filePath(QStringLiteral("installed.json")),
                      QJsonDocument(QJsonObject{{QStringLiteral("items"), store.items}}).toJson() + QByteArray(AssetLibrary::largestRecord, ' '));
            break;
        case Plant::Claimed: {
            QJsonObject entry = store.items.value(kId).toObject();
            entry.insert(QStringLiteral("claimed"), true);
            store.items.insert(kId, entry);
            store.writeRecord();
            break;
        }
        case Plant::ManifestIsALink: {
            const QString real = elsewhere.filePath(QStringLiteral("manifest.json"));
            QVERIFY(QFile::rename(store.fileOf(kId, QStringLiteral("manifest.json")), real));
            if (!makeLink(real, store.fileOf(kId, QStringLiteral("manifest.json"))))
                QSKIP("This user cannot make symbolic links here.");
            break;
        }
        case Plant::ModelNamesOutside:
        case Plant::ProgramAsPicture:
        case Plant::MarkupIsNotText:
        case Plant::TwoUris:
            break;
        }

        QStringList refused;
        QVERIFY(AssetLibrary::readInstalled(store.path(), &refused).isEmpty());
        QCOMPARE(refused.size(), 1);
        QVERIFY2(refused.first().contains(said), qPrintable(refused.first()));

        // And nothing of it reaches a project.
        Project project;
        project.saveScene();
        const auto before = snapshot(project.path());
        const QVariantMap used = AssetLibrary::usePack(project.path(), store.path(), kId, {}, false);
        QVERIFY(!used.value(QStringLiteral("ok")).toBool());
        QVERIFY2(problemOf(used).contains(QStringLiteral("cannot be used")) || problemOf(used).contains(QStringLiteral("not installed")),
                 qPrintable(problemOf(used)));
        QCOMPARE(snapshot(project.path()), before);
    }

    void aGameIsNoPack()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion(), QStringLiteral("game"));
        QStringList refused;
        QVERIFY(AssetLibrary::readInstalled(store.path(), &refused).isEmpty());
        QVERIFY(refused.isEmpty());
        Project project;
        QVERIFY(!AssetLibrary::usePack(project.path(), store.path(), kId, {}, false).value(QStringLiteral("ok")).toBool());
        QVERIFY(!QFileInfo::exists(project.lockPath()));
    }

    // ── Using a pack ──

    void usingAPackCopiesItAndLocksIt()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        const QVariantMap used = AssetLibrary::usePack(project.path(), store.path(), kId, {}, false);
        QVERIFY(ok(used));
        QCOMPARE(pathsOf(used.value(QStringLiteral("copied"))).size(), 6);
        for (const QString &source : {QStringLiteral("art/hero.png"), QStringLiteral("art/tiles.png"), QStringLiteral("LICENSE.txt"), QStringLiteral("NOTICE.txt")})
            QCOMPARE(readBytes(project.copy(source)), readBytes(store.fileOf(kId, source)));

        const QJsonObject lock = project.lock();
        QCOMPARE(lock.value(QStringLiteral("format")).toInt(), 1);
        const QJsonObject pack = project.lockedPack();
        QCOMPARE(pack.value(QStringLiteral("version")).toString(), QStringLiteral("1.0.0"));
        QCOMPARE(pack.value(QStringLiteral("developer")).toString(), QStringLiteral("org.example.test"));
        QCOMPARE(pack.value(QStringLiteral("licence")).toString(), QStringLiteral("Apache-2.0"));
        QCOMPARE(pack.value(QStringLiteral("whole")).toBool(), true);
        const QJsonObject provenance = pack.value(QStringLiteral("provenance")).toObject();
        QCOMPARE(provenance.value(QStringLiteral("manifest")).toString(), hex(readBytes(store.fileOf(kId, QStringLiteral("manifest.json")))));
        QCOMPARE(QByteArray::fromBase64(provenance.value(QStringLiteral("signature")).toString().toLatin1()), QByteArray(64, '\x01'));
        const QJsonObject hero = project.lockedFile(inProject(QStringLiteral("art/hero.png")));
        QCOMPARE(hero.value(QStringLiteral("source")).toString(), QStringLiteral("art/hero.png"));
        QCOMPARE(hero.value(QStringLiteral("sha256")).toString(), hex(png(red)));
        QCOMPARE(hero.value(QStringLiteral("version")).toString(), QStringLiteral("1.0.0"));
        // Read back as a stranger's, it is the same lock, and the same bytes written again.
        const AssetLibrary::Lock read = AssetLibrary::readLock(project.real());
        QVERIFY2(read.problem.isEmpty(), qPrintable(read.problem));
        QCOMPARE(AssetLibrary::lockBytes(read.packs, read.change), readBytes(project.lockPath()));
        // Nothing of the change is left behind: no journal, no staging folder.
        QVERIFY(!QFileInfo::exists(AssetLibrary::journalFor(project.real())));
        QCOMPARE(QDir(project.path()).entryList({QStringLiteral(".qq-assets-*")}, QDir::AllEntries | QDir::Hidden).size(), 0);
        // The project lists them as its own.
        const QVariantList assets = qq::SceneIO().project(project.url()).value(QStringLiteral("assets")).toList();
        QVERIFY(std::any_of(assets.cbegin(), assets.cend(), [](const QVariant &each) {
            return each.toMap().value(QStringLiteral("path")).toString() == inProject(QStringLiteral("art/hero.png"));
        }));
    }

    void usingOneFileBringsItsLicenceToo()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {QStringLiteral("art/hero.png")}, false)));
        QCOMPARE(snapshot(project.file(QStringLiteral("assets/") + kId)).keys(),
                 (QStringList{QStringLiteral("LICENSE.txt"), QStringLiteral("NOTICE.txt"), QStringLiteral("art/hero.png")}));
        QCOMPARE(project.lockedPack().value(QStringLiteral("whole")).toBool(), false);
        // A file not in the pack is refused, and changes nothing.
        const auto before = snapshot(project.path());
        QVERIFY(!AssetLibrary::usePack(project.path(), store.path(), kId, {QStringLiteral("../../etc/passwd")}, false).value(QStringLiteral("ok")).toBool());
        QCOMPARE(snapshot(project.path()), before);
        // More of it later joins the same entry.
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {QStringLiteral("art/tiles.png")}, false)));
        QCOMPARE(project.lockedPack().value(QStringLiteral("files")).toArray().size(), 4);
    }

    void aFileAlreadyThereIsReplacedOnlyWhenSaid()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        writeFile(project.copy(QStringLiteral("art/hero.png")), png(blue));
        const auto before = snapshot(project.path());
        const QVariantMap asked = AssetLibrary::usePack(project.path(), store.path(), kId, {}, false);
        QVERIFY(!asked.value(QStringLiteral("ok")).toBool());
        QCOMPARE(pathsOf(asked.value(QStringLiteral("conflicts"))), QStringList{inProject(QStringLiteral("art/hero.png"))});
        QCOMPARE(snapshot(project.path()), before);
        // The same bytes already there are no conflict.
        Project same;
        writeFile(same.copy(QStringLiteral("art/hero.png")), png(red));
        QVERIFY(ok(AssetLibrary::usePack(same.path(), store.path(), kId, {}, false)));
        // Said, it is replaced.
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, true)));
        QCOMPARE(readBytes(project.copy(QStringLiteral("art/hero.png"))), png(red));
        // Something in the way that is not a file is never replaced.
        Project folder;
        QDir().mkpath(folder.copy(QStringLiteral("art/hero.png")));
        QVERIFY(!AssetLibrary::usePack(folder.path(), store.path(), kId, {}, true).value(QStringLiteral("ok")).toBool());
        QVERIFY(!QFileInfo::exists(folder.lockPath()));
    }

    void aCopyThroughALinkIsRefused()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        QTemporaryDir outside;
        QDir().mkpath(project.file(QStringLiteral("assets")));
        if (!makeLink(outside.path(), project.file(QStringLiteral("assets/") + kId)))
            QSKIP("This user cannot make symbolic links here.");
        const QVariantMap used = AssetLibrary::usePack(project.path(), store.path(), kId, {}, false);
        QVERIFY(!used.value(QStringLiteral("ok")).toBool());
        QVERIFY2(problemOf(used).contains(QStringLiteral("link")), qPrintable(problemOf(used)));
        QVERIFY(QDir(outside.path()).isEmpty());
        QVERIFY(!QFileInfo::exists(project.lockPath()));
    }

    // ── The lock, read as a stranger's ──

    void aBrokenLockChangesNothing_data()
    {
        QTest::addColumn<QString>("broken");
        for (const char *kind : {"climbs", "a link", "huge", "another format", "outside assets", "through a linked folder", "not JSON",
                                 "a path twice", "a bad digest", "the lock a folder"})
            QTest::newRow(kind) << QString::fromLatin1(kind);
    }

    void aBrokenLockChangesNothing()
    {
        QFETCH(QString, broken);
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        project.saveScene();
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {QStringLiteral("art/hero.png")}, false)));
        QVERIFY(AssetLibrary::readLock(project.real()).problem.isEmpty());
        store.add(kId, QStringLiteral("1.1.0"), secondVersion());

        QJsonObject lock = project.lock();
        QJsonArray packs = lock.value(QStringLiteral("packs")).toArray();
        QJsonObject pack = packs.first().toObject();
        QJsonArray files = pack.value(QStringLiteral("files")).toArray();
        const auto plantFile = [&](const QString &key, const QJsonValue &value) {
            QJsonObject file = files.first().toObject();
            file.insert(key, value);
            files.append(file);
            pack.insert(QStringLiteral("files"), files);
            packs.replace(0, pack);
            lock.insert(QStringLiteral("packs"), packs);
            writeFile(project.lockPath(), QJsonDocument(lock).toJson());
        };
        QTemporaryDir outside;
        if (broken == QStringLiteral("climbs")) {
            plantFile(QStringLiteral("path"), QStringLiteral("assets/%1/../../scenes/level.qml").arg(kId));
        } else if (broken == QStringLiteral("a link")) {
            const QString real = outside.filePath(QStringLiteral("lock.json"));
            QVERIFY(QFile::rename(project.lockPath(), real));
            if (!makeLink(real, project.lockPath()))
                QSKIP("This user cannot make symbolic links here.");
        } else if (broken == QStringLiteral("huge")) {
            writeFile(project.lockPath(), QJsonDocument(lock).toJson() + QByteArray(AssetLibrary::largestLock, ' '));
        } else if (broken == QStringLiteral("another format")) {
            lock.insert(QStringLiteral("format"), 2);
            writeFile(project.lockPath(), QJsonDocument(lock).toJson());
        } else if (broken == QStringLiteral("outside assets")) {
            plantFile(QStringLiteral("path"), QStringLiteral("scenes/level.qml"));
        } else if (broken == QStringLiteral("through a linked folder")) {
            const QString folder = project.file(QStringLiteral("assets/") + kId);
            QVERIFY(QDir().rename(folder, outside.filePath(QStringLiteral("pack"))));
            if (!makeLink(outside.filePath(QStringLiteral("pack")), folder))
                QSKIP("This user cannot make symbolic links here.");
        } else if (broken == QStringLiteral("not JSON")) {
            writeFile(project.lockPath(), "{ \"format\": 1, \"packs\": [ ");
        } else if (broken == QStringLiteral("a path twice")) {
            plantFile(QStringLiteral("version"), QStringLiteral("1.0.0"));
        } else if (broken == QStringLiteral("a bad digest")) {
            plantFile(QStringLiteral("sha256"), QStringLiteral("not a digest"));
        } else if (broken == QStringLiteral("the lock a folder")) {
            QVERIFY(QFile::remove(project.lockPath()));
            QVERIFY(QDir().mkpath(project.lockPath() + QStringLiteral("/inside")));
        }
        const AssetLibrary::Lock read = AssetLibrary::readLock(project.real());
        QVERIFY(read.found);
        QVERIFY2(!read.problem.isEmpty(), "the lock was believed");
        QVERIFY(read.packs.isEmpty());

        const auto before = snapshot(project.path());
        const auto outsideBefore = snapshot(outside.path());
        const QVariantMap used = AssetLibrary::usePack(project.path(), store.path(), kId, {}, true);
        QVERIFY(!used.value(QStringLiteral("ok")).toBool());
        QVERIFY2(problemOf(used).contains(QStringLiteral("assets.lock.json cannot be used")), qPrintable(problemOf(used)));
        QVERIFY(!AssetLibrary::updatePack(project.path(), store.path(), kId, {}).value(QStringLiteral("ok")).toBool());
        QVERIFY(!AssetLibrary::planUpdate(project.path(), store.path(), kId).value(QStringLiteral("ok")).toBool());
        AssetLibrary library;
        library.setFolder(store.path());
        QVERIFY(!library.state(project.url()).value(QStringLiteral("lockProblem")).toString().isEmpty());
        // The build says so, and makes no game that would not say what it uses.
        QTemporaryDir games;
        const QVariantMap built = qq::GameBuild::make(project.path(), QStringLiteral("scenes/level.qml"), games.path());
        QVERIFY(!built.value(QStringLiteral("ok")).toBool());
        QVERIFY2(problemOf(built).contains(QStringLiteral("assets.lock.json")), qPrintable(problemOf(built)));
        QCOMPARE(snapshot(project.path()), before);
        QCOMPARE(snapshot(outside.path()), outsideBefore);
    }

    void aProjectWithNoLockIsAsBefore()
    {
        Project project;
        project.saveScene();
        const AssetLibrary::Lock lock = AssetLibrary::readLock(project.real());
        QVERIFY(!lock.found);
        QVERIFY(lock.problem.isEmpty());
        QTemporaryDir games;
        const QVariantMap built = qq::GameBuild::make(project.path(), QStringLiteral("scenes/level.qml"), games.path());
        QVERIFY(ok(built));
        const QJsonObject game = QJsonDocument::fromJson(readBytes(built.value(QStringLiteral("folder")).toString() + QStringLiteral("/game.json"))).object();
        QCOMPARE(game.value(QStringLiteral("uses")).toArray().size(), 0);
        QVERIFY(!QFileInfo::exists(project.lockPath()));
    }

    // ── Updating ──

    void anUpdateKeepsWhatThePersonChanged()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));
        // The person changes the tiles, which the new version still has, and the kept one, which it drops.
        writeFile(project.copy(QStringLiteral("art/tiles.png")), png(blue, 8));
        writeFile(project.copy(QStringLiteral("art/kept.png")), png(blue, 8));
        store.add(kId, QStringLiteral("1.1.0"), secondVersion());

        // Asked first, and the plan changes nothing.
        const auto before = snapshot(project.path());
        const QVariantMap plan = AssetLibrary::planUpdate(project.path(), store.path(), kId);
        QVERIFY(ok(plan));
        QCOMPARE(snapshot(project.path()), before);
        QCOMPARE(plan.value(QStringLiteral("from")).toString(), QStringLiteral("1.0.0"));
        QCOMPARE(plan.value(QStringLiteral("to")).toString(), QStringLiteral("1.1.0"));
        QCOMPARE(pathsOf(plan.value(QStringLiteral("replaced"))), QStringList{inProject(QStringLiteral("art/hero.png"))});
        QCOMPARE(pathsOf(plan.value(QStringLiteral("added"))), QStringList{inProject(QStringLiteral("art/torch.png"))});
        QCOMPARE(pathsOf(plan.value(QStringLiteral("removed"))), QStringList{inProject(QStringLiteral("art/old.png"))});
        QCOMPARE(pathsOf(plan.value(QStringLiteral("changed"))),
                 (QStringList{inProject(QStringLiteral("art/kept.png")), inProject(QStringLiteral("art/tiles.png"))}));
        QString besideTiles;
        for (const QVariant &each : plan.value(QStringLiteral("changed")).toList()) {
            const QVariantMap changed = each.toMap();
            if (changed.value(QStringLiteral("path")) == inProject(QStringLiteral("art/tiles.png")))
                besideTiles = changed.value(QStringLiteral("beside")).toString();
            else
                QCOMPARE(changed.value(QStringLiteral("beside")).toString(), QString());  // dropped by the new version: nothing to put beside
        }
        QCOMPARE(besideTiles, inProject(QStringLiteral("art/tiles-1.1.0.png")));

        const QVariantMap updated = AssetLibrary::updatePack(project.path(), store.path(), kId, {inProject(QStringLiteral("art/tiles.png"))});
        QVERIFY(ok(updated));
        // What the person changed is as they left it, the new version beside it.
        QCOMPARE(readBytes(project.copy(QStringLiteral("art/tiles.png"))), png(blue, 8));
        QCOMPARE(readBytes(project.copy(QStringLiteral("art/tiles-1.1.0.png"))), png(green));
        QCOMPARE(readBytes(project.copy(QStringLiteral("art/kept.png"))), png(blue, 8));
        // Everything else is the new version's.
        QCOMPARE(readBytes(project.copy(QStringLiteral("art/hero.png"))), png(amber));
        QCOMPARE(readBytes(project.copy(QStringLiteral("art/torch.png"))), png(violet));
        QVERIFY(!QFileInfo::exists(project.copy(QStringLiteral("art/old.png"))));
        // And the lock says so: the pack's new version, each copy from the version it came from.
        QCOMPARE(project.lockedPack().value(QStringLiteral("version")).toString(), QStringLiteral("1.1.0"));
        QCOMPARE(project.lockedFile(inProject(QStringLiteral("art/hero.png"))).value(QStringLiteral("version")).toString(), QStringLiteral("1.1.0"));
        QCOMPARE(project.lockedFile(inProject(QStringLiteral("art/tiles.png"))).value(QStringLiteral("version")).toString(), QStringLiteral("1.0.0"));
        QCOMPARE(project.lockedFile(inProject(QStringLiteral("art/tiles.png"))).value(QStringLiteral("sha256")).toString(), hex(png(green)));
        QCOMPARE(project.lockedFile(inProject(QStringLiteral("art/tiles-1.1.0.png"))).value(QStringLiteral("source")).toString(), QStringLiteral("art/tiles.png"));
        QVERIFY(project.lockedFile(inProject(QStringLiteral("art/old.png"))).isEmpty());
        QVERIFY(!project.lockedFile(inProject(QStringLiteral("art/kept.png"))).isEmpty());
        // Nothing more to update.
        QVERIFY(!AssetLibrary::planUpdate(project.path(), store.path(), kId).value(QStringLiteral("ok")).toBool());
    }

    void anUpdateSkipsWhatIsNotChosenAndBringsBackNothingDeleted()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {QStringLiteral("art/hero.png"), QStringLiteral("art/tiles.png")}, false)));
        writeFile(project.copy(QStringLiteral("art/hero.png")), png(blue, 8));
        QVERIFY(QFile::remove(project.copy(QStringLiteral("art/tiles.png"))));
        store.add(kId, QStringLiteral("1.1.0"), secondVersion());
        const QVariantMap updated = AssetLibrary::updatePack(project.path(), store.path(), kId, {});
        QVERIFY(ok(updated));
        QCOMPARE(pathsOf(updated.value(QStringLiteral("gone"))), QStringList{inProject(QStringLiteral("art/tiles.png"))});
        QCOMPARE(readBytes(project.copy(QStringLiteral("art/hero.png"))), png(blue, 8));
        QVERIFY(!QFileInfo::exists(project.copy(QStringLiteral("art/hero-1.1.0.png"))));
        QVERIFY(!QFileInfo::exists(project.copy(QStringLiteral("art/tiles.png"))));
        // Only some of it was used, so what is new in the pack stays in the pack.
        QVERIFY(!QFileInfo::exists(project.copy(QStringLiteral("art/torch.png"))));
        QCOMPARE(project.lockedPack().value(QStringLiteral("version")).toString(), QStringLiteral("1.1.0"));
        QVERIFY(project.lockedFile(inProject(QStringLiteral("art/tiles.png"))).isEmpty());
    }

    // ── Cut short ──

    void aChangeCutShortIsPutRight_data()
    {
        QTest::addColumn<QString>("operation");
        QTest::addColumn<QString>("step");
        QTest::addColumn<bool>("made");
        // "replace": Replace them, over a copy the person changed, of the very version the lock already has.
        for (const char *operation : {"use", "update", "replace"}) {
            QTest::newRow(qPrintable(QStringLiteral("%1, staged").arg(QLatin1String(operation)))) << QString::fromLatin1(operation) << QStringLiteral("staged") << false;
            QTest::newRow(qPrintable(QStringLiteral("%1, set aside").arg(QLatin1String(operation)))) << QString::fromLatin1(operation) << QStringLiteral("aside") << false;
            QTest::newRow(qPrintable(QStringLiteral("%1, placed").arg(QLatin1String(operation)))) << QString::fromLatin1(operation) << QStringLiteral("placed") << false;
            QTest::newRow(qPrintable(QStringLiteral("%1, locked").arg(QLatin1String(operation)))) << QString::fromLatin1(operation) << QStringLiteral("locked") << true;
        }
    }

    /// Stopped dead at each step by a process of its own, as a power cut or a kill would, then looked at again: all of it or none.
    void aChangeCutShortIsPutRight()
    {
        QFETCH(QString, operation);
        QFETCH(QString, step);
        QFETCH(bool, made);
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        if (operation == QStringLiteral("use")) {
            // Something there before: set aside, and back if the change does not stand.
            writeFile(project.copy(QStringLiteral("art/hero.png")), png(blue, 8));
        } else if (operation == QStringLiteral("replace")) {
            QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));
            writeFile(project.copy(QStringLiteral("art/hero.png")), png(blue, 8));
        } else {
            QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));
            writeFile(project.copy(QStringLiteral("art/tiles.png")), png(blue, 8));
            store.add(kId, QStringLiteral("1.1.0"), secondVersion());
        }
        const auto before = snapshot(project.path());

        QProcess dying;
        dying.setProcessChannelMode(QProcess::MergedChannels);
        dying.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--die-at"), step, operation, project.path(), store.path()});
        QVERIFY(dying.waitForFinished(120000));
        QVERIFY2(dying.exitCode() == 42, dying.readAll().constData());
        // It died with the change under way: its journal is there, in the person's store.
        QVERIFY(QFileInfo::exists(AssetLibrary::journalFor(project.real())));

        const QString said = AssetLibrary::recover(project.real());
        QVERIFY2(said.contains(made ? QStringLiteral("finished") : QStringLiteral("put back")), qPrintable(said));
        QVERIFY(!QFileInfo::exists(AssetLibrary::journalFor(project.real())));
        QCOMPARE(QDir(project.path()).entryList({QStringLiteral(".qq-assets-*")}, QDir::AllEntries | QDir::Hidden).size(), 0);
        if (!made) {
            QCOMPARE(snapshot(project.path()), before);
        } else if (operation == QStringLiteral("use") || operation == QStringLiteral("replace")) {
            QCOMPARE(readBytes(project.copy(QStringLiteral("art/hero.png"))), png(red));
            QCOMPARE(project.lockedPack().value(QStringLiteral("version")).toString(), QStringLiteral("1.0.0"));
        } else {
            QCOMPARE(readBytes(project.copy(QStringLiteral("art/hero.png"))), png(amber));
            QCOMPARE(readBytes(project.copy(QStringLiteral("art/tiles.png"))), png(blue, 8));
            QCOMPARE(readBytes(project.copy(QStringLiteral("art/tiles-1.1.0.png"))), png(green));
            QCOMPARE(project.lockedPack().value(QStringLiteral("version")).toString(), QStringLiteral("1.1.0"));
        }
        QVERIFY(AssetLibrary::readLock(project.real()).problem.isEmpty());
        // Looked at again, nothing more happens.
        const auto settled = snapshot(project.path());
        QCOMPARE(AssetLibrary::recover(project.real()), QString());
        QCOMPARE(snapshot(project.path()), settled);
    }

    /// A journal is the person's, never the project's: one planted in a project is a file like any other, and one in the
    /// person's store naming what a change could not have made moves nothing.
    void aPlantedJournalMovesNothing()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));
        const QString tag = QStringLiteral("0123456789abcdef0123456789abcdef");
        writeFile(project.file(QStringLiteral(".qq-assets-") + tag + QStringLiteral("/new/assets/x.png")), png(red));
        writeFile(project.file(QStringLiteral("assets.journal.json")), "{}");
        const auto before = snapshot(project.path());
        AssetLibrary library;
        library.setFolder(store.path());
        QCOMPARE(library.state(project.url()).value(QStringLiteral("recovered")).toString(), QString());
        QCOMPARE(snapshot(project.path()), before);

        // In the person's own store, a journal naming the project's scenes: refused, and the scene stays.
        project.saveScene();
        const auto withScene = snapshot(project.path());
        writeFile(AssetLibrary::journalFor(project.real()),
                  QJsonDocument(QJsonObject{{QStringLiteral("format"), 1}, {QStringLiteral("project"), project.real()}, {QStringLiteral("tag"), tag},
                                            {QStringLiteral("put"), QJsonArray{QStringLiteral("scenes/level.qml")}},
                                            {QStringLiteral("remove"), QJsonArray{}},
                                            {QStringLiteral("lock"), hex("never")}}).toJson());
        const QString said = AssetLibrary::recover(project.real());
        QVERIFY2(said.contains(QStringLiteral("left as it is")), qPrintable(said));
        QCOMPARE(snapshot(project.path()), withScene);
    }

#ifdef Q_OS_LINUX
    /// The real qq-studio, updating a large pack, killed by the kernel once its journal says the change is under way; looked at
    /// again, the project is wholly the old version or wholly the new, whatever moment the kill came at.
    void theRealStudioKilledMidUpdateLosesNothing()
    {
        QMap<QString, QByteArray> first;
        QMap<QString, QByteArray> second;
        for (int i = 0; i < 1500; ++i) {
            const QString name = QStringLiteral("tiles/%1.png").arg(i, 4, 10, QLatin1Char('0'));
            first.insert(name, QByteArray("\x89PNG\r\n\x1a\n") + QByteArray::number(i) + QByteArray(512, 'a'));
            second.insert(name, QByteArray("\x89PNG\r\n\x1a\n") + QByteArray::number(i) + QByteArray(512, 'b'));
        }
        int killedUnderWay = 0;
        for (int attempt = 0; attempt < 3 && killedUnderWay == 0; ++attempt) {
            Store store;
            store.add(kId, QStringLiteral("1.0.0"), first);
            Project project;
            trust(project.url());
            QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));
            store.add(kId, QStringLiteral("1.1.0"), second);
            const QString journal = AssetLibrary::journalFor(project.real());

            QProcess studio;
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert(QStringLiteral("ASAN_OPTIONS"), env.value(QStringLiteral("ASAN_OPTIONS")) + QStringLiteral(":detect_leaks=0"));
            studio.setProcessEnvironment(env);
            studio.start(QStringLiteral(QQ_STUDIO), {QStringLiteral("--project"), project.path(), QStringLiteral("--asset-packs"), store.path(),
                                                     QStringLiteral("--update-pack"), kId});
            QVERIFY(studio.waitForStarted());
            QElapsedTimer waited;
            waited.start();
            while (!QFileInfo::exists(journal) && studio.state() == QProcess::Running && waited.elapsed() < 120000)
                QThread::usleep(200);
            const bool underWay = QFileInfo::exists(journal) && studio.state() == QProcess::Running;
            studio.kill();
            QVERIFY(studio.waitForFinished(30000));
            if (underWay && studio.exitStatus() == QProcess::CrashExit)
                ++killedUnderWay;

            // Looked at again by the next Studio.
            AssetLibrary library;
            library.setFolder(store.path());
            const QVariantMap state = library.state(project.url());
            qInfo("Attempt %d: killed %s; on reopening: %s", attempt, underWay ? "with the change under way" : "after it had finished",
                  qPrintable(state.value(QStringLiteral("recovered")).toString()));
            QVERIFY(!QFileInfo::exists(journal));
            QCOMPARE(QDir(project.path()).entryList({QStringLiteral(".qq-assets-*")}, QDir::AllEntries | QDir::Hidden).size(), 0);
            const QString version = project.lockedPack().value(QStringLiteral("version")).toString();
            QVERIFY(version == QStringLiteral("1.0.0") || version == QStringLiteral("1.1.0"));
            const QMap<QString, QByteArray> &expected = version == QStringLiteral("1.0.0") ? first : second;
            int wrong = 0;
            for (auto it = expected.cbegin(); it != expected.cend(); ++it)
                wrong += readBytes(project.copy(it.key())) != it.value() ? 1 : 0;
            QCOMPARE(wrong, 0);
        }
        QVERIFY2(killedUnderWay > 0, "The kill never came while the change was under way");
    }
#endif

    // ── Gone from the Store, and no library at all ──

    void aRemovedPackLeavesTheProjectAsItIs()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));
        store.remove(kId);
        const auto before = snapshot(project.path());
        AssetLibrary library;
        library.setFolder(store.path());
        const QVariantMap state = library.state(project.url());
        QCOMPARE(state.value(QStringLiteral("packs")).toList().size(), 0);
        const QVariantList missing = state.value(QStringLiteral("missing")).toList();
        QCOMPARE(missing.size(), 1);
        QCOMPARE(missing.first().toMap().value(QStringLiteral("id")).toString(), kId);
        QCOMPARE(missing.first().toMap().value(QStringLiteral("version")).toString(), QStringLiteral("1.0.0"));
        const QVariantMap plan = AssetLibrary::planUpdate(project.path(), store.path(), kId);
        QVERIFY(!plan.value(QStringLiteral("ok")).toBool());
        QVERIFY2(problemOf(plan).contains(QStringLiteral("stay as they are")), qPrintable(problemOf(plan)));
        QCOMPARE(snapshot(project.path()), before);
    }

    void noLibraryWithoutAFolder()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        project.saveScene();
        trust(project.url());
        const auto before = snapshot(project.path());

        AssetLibrary nothing;
        QVERIFY(!nothing.available());
        QCOMPARE(nothing.state(project.url()).value(QStringLiteral("available")).toBool(), false);
        QVERIFY(!nothing.use(project.url(), kId, {}, false).value(QStringLiteral("ok")).toBool());

        {
            Studio s;
            QVERIFY(s.open(project.url(), QString()));
            QCOMPARE(s.assets().value(QStringLiteral("available")).toBool(), false);
            QVERIFY(!s.shown(QStringLiteral("libraryPanel")));
        }
        // The real program, with neither the option nor the variable, has no library either.
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove(QStringLiteral("QOR_ASSET_PACKS"));
        int code = 0;
        QVariantMap answer = runStudio({QStringLiteral("--project"), project.path(), QStringLiteral("--use-pack"), kId}, &code, env);
        QCOMPARE(code, 3);
        QVERIFY2(problemOf(answer).contains(QStringLiteral("No folder")), qPrintable(problemOf(answer)));
        QCOMPARE(snapshot(project.path()), before);
        // With the variable Prism sets, it has.
        env.insert(QStringLiteral("QOR_ASSET_PACKS"), store.path());
        answer = runStudio({QStringLiteral("--project"), project.path(), QStringLiteral("--use-pack"), kId}, &code, env);
        QVERIFY2(code == 0, qPrintable(problemOf(answer)));
        QCOMPARE(readBytes(project.copy(QStringLiteral("art/hero.png"))), png(red));
        // And a project nobody has said they trust is refused with nobody there to ask.
        Project stranger;
        answer = runStudio({QStringLiteral("--project"), stranger.path(), QStringLiteral("--asset-packs"), store.path(), QStringLiteral("--use-pack"), kId}, &code, env);
        QCOMPARE(code, 3);
        QVERIFY(QDir(stranger.path()).isEmpty());
    }

    // ── The build ──

    void theBuiltGameSaysWhatItUses()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        project.saveScene();
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {QStringLiteral("art/hero.png")}, false)));
        QTemporaryDir games;
        const QVariantMap built = qq::GameBuild::make(project.path(), QStringLiteral("scenes/level.qml"), games.path());
        QVERIFY(ok(built));
        const QString folder = built.value(QStringLiteral("folder")).toString();
        const QJsonObject game = QJsonDocument::fromJson(readBytes(folder + QStringLiteral("/game.json"))).object();
        // What the Library reads is as it was.
        QCOMPARE(game.value(QStringLiteral("format")).toInt(), 1);
        QCOMPARE(game.value(QStringLiteral("kind")).toString(), QStringLiteral("game"));
        const QJsonArray uses = game.value(QStringLiteral("uses")).toArray();
        QCOMPARE(uses.size(), 1);
        const QJsonObject pack = uses.first().toObject();
        QCOMPARE(pack.value(QStringLiteral("id")).toString(), kId);
        QCOMPARE(pack.value(QStringLiteral("version")).toString(), QStringLiteral("1.0.0"));
        QCOMPARE(pack.value(QStringLiteral("licence")).toString(), QStringLiteral("Apache-2.0"));
        QCOMPARE(pack.value(QStringLiteral("developer")).toString(), QStringLiteral("org.example.test"));
        QCOMPARE(pack.value(QStringLiteral("provenance")).toObject().value(QStringLiteral("manifest")).toString(),
                 hex(readBytes(store.fileOf(kId, QStringLiteral("manifest.json")))));
        QStringList carried;
        for (const QJsonValue &each : pack.value(QStringLiteral("files")).toArray())
            carried.append(each.toString());
        carried.sort();
        QCOMPARE(carried, (QStringList{inProject(QStringLiteral("LICENSE.txt")), inProject(QStringLiteral("NOTICE.txt")), inProject(QStringLiteral("art/hero.png"))}));
        // The copies, the licence and notice among them, are carried like any asset.
        for (const QString &each : std::as_const(carried)) {
            QCOMPARE(readBytes(folder + QLatin1Char('/') + each), readBytes(project.file(each)));
            QVERIFY(game.value(QStringLiteral("files")).toObject().contains(each));
        }
        // The lock itself is the project's, not the game's.
        QVERIFY(!QFileInfo::exists(folder + QStringLiteral("/assets.lock.json")));
    }

    // ── The window ──

    void theWindowUsesUpdatesAndReopens()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        project.saveScene();
        trust(project.url());
        writeFile(project.copy(QStringLiteral("art/hero.png")), png(blue, 8));
        {
            Studio s;
            QVERIFY(s.open(project.url(), store.path()));
            QVERIFY(s.shown(QStringLiteral("libraryPanel")));
            const QVariantList packs = s.assets().value(QStringLiteral("packs")).toList();
            QCOMPARE(packs.size(), 1);
            QCOMPARE(packs.first().toMap().value(QStringLiteral("licence")).toString(), QStringLiteral("Apache-2.0"));
            QVERIFY(s.child(QStringLiteral("use:") + kId));
            // Its files are shown on asking, and only the rows in view are made.
            QVERIFY(!s.child(QStringLiteral("useFile:") + kId + QStringLiteral("/art/hero.png")));
            s.click(QStringLiteral("files:") + kId);
            QTRY_VERIFY(s.child(QStringLiteral("useFile:") + kId + QStringLiteral("/art/hero.png")));

            // A file already there that differs: asked, and Leave changes nothing.
            const auto before = snapshot(project.path());
            QVERIFY(!s.call("useAssets", kId, QStringList()).toBool());
            QTRY_VERIFY(s.opened("conflictDialog"));
            QVERIFY(s.child(QStringLiteral("conflictText"))->property("text").toString().contains(inProject(QStringLiteral("art/hero.png"))));
            s.click(QStringLiteral("conflictLeave"));
            QTRY_VERIFY(!s.opened("conflictDialog"));
            QCOMPARE(snapshot(project.path()), before);
            // Replace does it.
            QVERIFY(!s.call("useAssets", kId, QStringList()).toBool());
            QTRY_VERIFY(s.opened("conflictDialog"));
            s.click(QStringLiteral("conflictReplace"));
            QTRY_VERIFY(QFileInfo::exists(project.lockPath()));
            QCOMPARE(readBytes(project.copy(QStringLiteral("art/hero.png"))), png(red));
            QCOMPARE(s.assets().value(QStringLiteral("packs")).toList().first().toMap().value(QStringLiteral("used")).toString(), QStringLiteral("1.0.0"));

            // The pack moves on; the person has changed the tiles.
            writeFile(project.copy(QStringLiteral("art/tiles.png")), png(blue, 8));
            store.add(kId, QStringLiteral("1.1.0"), secondVersion());
            s.call("refreshAssets");
            QVERIFY(s.assets().value(QStringLiteral("packs")).toList().first().toMap().value(QStringLiteral("update")).toBool());
            QVERIFY(s.shown(QStringLiteral("update:") + kId));
            // Cancel changes nothing.
            const auto beforeUpdate = snapshot(project.path());
            QVERIFY(s.call("offerUpdate", kId).toBool());
            QTRY_VERIFY(s.opened("updateDialog"));
            s.click(QStringLiteral("updateCancel"));
            QTRY_VERIFY(!s.opened("updateDialog"));
            QCOMPARE(snapshot(project.path()), beforeUpdate);
            // Update, with the new tiles beside the person's, as offered.
            QVERIFY(s.call("offerUpdate", kId).toBool());
            QTRY_VERIFY(s.opened("updateDialog"));
            QVERIFY(s.child(QStringLiteral("beside:") + inProject(QStringLiteral("art/tiles.png"))));
            s.click(QStringLiteral("updateGo"));
            QTRY_COMPARE(project.lockedPack().value(QStringLiteral("version")).toString(), QStringLiteral("1.1.0"));
            QCOMPARE(readBytes(project.copy(QStringLiteral("art/tiles.png"))), png(blue, 8));
            QCOMPARE(readBytes(project.copy(QStringLiteral("art/tiles-1.1.0.png"))), png(green));
            QVERIFY2(s.notice().contains(QStringLiteral("keeping 1")), qPrintable(s.notice()));
        }
        // Reopened: what the project uses comes from its lock; gone from the Store, the copies stay and the library says so.
        store.remove(kId);
        {
            Studio s;
            QVERIFY(s.open(project.url(), store.path()));
            const QVariantList missing = s.assets().value(QStringLiteral("missing")).toList();
            QCOMPARE(missing.size(), 1);
            QCOMPARE(missing.first().toMap().value(QStringLiteral("version")).toString(), QStringLiteral("1.1.0"));
            QVERIFY(s.child(QStringLiteral("missingPack")));
            QVERIFY(QFileInfo::exists(project.copy(QStringLiteral("art/hero.png"))));
        }
        // A broken lock is said in the window, and the window changes nothing.
        writeFile(project.lockPath(), "{ \"format\": 9 }");
        const auto broken = snapshot(project.path());
        {
            Studio s;
            QVERIFY(s.open(project.url(), store.path()));
            QVERIFY(!s.assets().value(QStringLiteral("lockProblem")).toString().isEmpty());
            QTRY_VERIFY(s.shown(QStringLiteral("lockProblem")));
        }
        QCOMPARE(snapshot(project.path()), broken);
    }


    // ── Recovery reads only what it made (the review of 3 November) ──

    /// A journal that names copies the project holds, with no staging folder beside them (cleared by git clean, a restore, or
    /// another folder put in that place): nothing is moved, the journal goes, and that is said.
    void aJournalWithNoStagingFolderMovesNothing()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));
        const auto before = snapshot(project.path());
        writeFile(AssetLibrary::journalFor(project.real()),
                  QJsonDocument(QJsonObject{{QStringLiteral("format"), 1}, {QStringLiteral("project"), project.real()},
                                            {QStringLiteral("tag"), QStringLiteral("0123456789abcdef0123456789abcdef")},
                                            {QStringLiteral("identity"), qq::ProjectWorkspace::folderIdentity(project.real())},
                                            {QStringLiteral("put"), QJsonArray{inProject(QStringLiteral("art/hero.png")), inProject(QStringLiteral("art/tiles.png"))}},
                                            {QStringLiteral("placed"), QJsonObject{{inProject(QStringLiteral("art/hero.png")), hex(png(red))},
                                                                                   {inProject(QStringLiteral("art/tiles.png")), hex(png(green))}}},
                                            {QStringLiteral("remove"), QJsonArray{}},
                                            {QStringLiteral("lock"), hex("never this lock")}}).toJson());
        const QString said = AssetLibrary::recover(project.real());
        QVERIFY2(said.contains(QStringLiteral("nothing was moved")), qPrintable(said));
        QVERIFY(!QFileInfo::exists(AssetLibrary::journalFor(project.real())));
        QCOMPARE(snapshot(project.path()), before);
    }

    /// Cut short with its copies placed, then the person edits one before the project is looked at again: their edit stays.
    void aCopyEditedAfterACrashIsKept()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));
        store.add(kId, QStringLiteral("1.1.0"), secondVersion());
        QProcess dying;
        dying.setProcessChannelMode(QProcess::MergedChannels);
        dying.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--die-at"), QStringLiteral("placed"), QStringLiteral("update"), project.path(), store.path()});
        QVERIFY(dying.waitForFinished(120000));
        QVERIFY2(dying.exitCode() == 42, dying.readAll().constData());
        QCOMPARE(readBytes(project.copy(QStringLiteral("art/hero.png"))), png(amber));
        writeFile(project.copy(QStringLiteral("art/hero.png")), png(violet, 8));
        const QString said = AssetLibrary::recover(project.real());
        QVERIFY2(!said.isEmpty(), "nothing was said");
        QCOMPARE(readBytes(project.copy(QStringLiteral("art/hero.png"))), png(violet, 8));
        // The rest is as it was before the update.
        QCOMPARE(readBytes(project.copy(QStringLiteral("art/old.png"))), png(blue));
        QCOMPARE(project.lockedPack().value(QStringLiteral("version")).toString(), QStringLiteral("1.0.0"));
    }

    /// Cut short, then another folder is put where the project was, staging folder and all: it is not the project the journal
    /// was written for, and nothing of it is touched.
    void anotherFolderInItsPlaceIsNotTouched()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));
        store.add(kId, QStringLiteral("1.1.0"), secondVersion());
        QProcess dying;
        dying.setProcessChannelMode(QProcess::MergedChannels);
        dying.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--die-at"), QStringLiteral("placed"), QStringLiteral("update"), project.path(), store.path()});
        QVERIFY(dying.waitForFinished(120000));
        QVERIFY2(dying.exitCode() == 42, dying.readAll().constData());
        // A copy of it, made new, put where it was.
        const QString away = project.path() + QStringLiteral(".away");
        QVERIFY(QDir().rename(project.path(), away));
        QVERIFY(copyTree(away, project.path()));
        const auto before = snapshot(project.path());
        const QString said = AssetLibrary::recover(project.real());
        QVERIFY2(said.contains(QStringLiteral("not the folder")), qPrintable(said));
        QCOMPARE(snapshot(project.path()), before);
        QVERIFY(!QFileInfo::exists(AssetLibrary::journalFor(project.real())));
    }

    /// The person changes a copy while an update is on its way, after it was planned: it is not written over.
    void aCopyChangedDuringAnUpdateIsKept()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));
        store.add(kId, QStringLiteral("1.1.0"), secondVersion());
        const QString hero = project.copy(QStringLiteral("art/hero.png"));
        const QVariantMap updated = AssetLibrary::updatePack(project.path(), store.path(), kId, {}, [&hero](const char *at) {
            if (qstrcmp(at, "staged") == 0)
                writeFile(hero, png(violet, 8));
        });
        QCOMPARE(readBytes(hero), png(violet, 8));
        QVERIFY(!updated.value(QStringLiteral("ok")).toBool());
        QCOMPARE(project.lockedPack().value(QStringLiteral("version")).toString(), QStringLiteral("1.0.0"));
        QCOMPARE(readBytes(project.copy(QStringLiteral("art/old.png"))), png(blue));
    }


    /// Every change writes a lock of its own: a change of nothing in it is still told apart from the lock before.
    void everyLockIsItsOwn()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));
        const QByteArray first = readBytes(project.lockPath());
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));
        const QByteArray second = readBytes(project.lockPath());
        QVERIFY(first != second);
        QCOMPARE(AssetLibrary::readLock(project.real()).packs.first().files.size(), 6);
        // A planted change that is not a tag is not a lock.
        QJsonObject lock = project.lock();
        lock.insert(QStringLiteral("change"), QStringLiteral("not a tag"));
        writeFile(project.lockPath(), QJsonDocument(lock).toJson());
        QVERIFY(!AssetLibrary::readLock(project.real()).problem.isEmpty());
    }

    /// While a change or a build holds a project, the other waits: a build is refused while a change runs, and a change while a
    /// build does. And a store that cannot be written is said to be that.
    void aChangeAndABuildNeverMeet()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        project.saveScene();
        {
            const AssetLibrary::Hold hold(project.real());
            QVERIFY(hold.held());
            QTemporaryDir games;
            const QVariantMap built = qq::GameBuild::make(project.path(), QStringLiteral("scenes/level.qml"), games.path());
            QVERIFY(!built.value(QStringLiteral("ok")).toBool());
            QVERIFY2(problemOf(built).contains(QStringLiteral("changing or building")), qPrintable(problemOf(built)));
            const QVariantMap used = AssetLibrary::usePack(project.path(), store.path(), kId, {}, false);
            QVERIFY(!used.value(QStringLiteral("ok")).toBool());
            QVERIFY2(problemOf(used).contains(QStringLiteral("changing or building")), qPrintable(problemOf(used)));
            QVERIFY(!QFileInfo::exists(project.lockPath()));
        }
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));

#ifdef Q_OS_LINUX
        // In a program whose store is a file, so no folder can be made there.
        QTemporaryDir broken;
        writeFile(broken.filePath(QStringLiteral("data")), "a file where a folder would be");
        QProcess child;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("XDG_DATA_HOME"), broken.filePath(QStringLiteral("data")));
        child.setProcessEnvironment(env);
        child.setProcessChannelMode(QProcess::MergedChannels);
        child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--use-here"), project.path(), store.path()});
        QVERIFY(child.waitForFinished(60000));
        const QByteArray said = child.readAll();
        QVERIFY2(said.contains("cannot be written"), said.constData());
        QVERIFY2(!said.contains("Another QQ Studio"), said.constData());
#endif
    }

    /// A tree of empty folders in a pack is refused at once, however many there are.
    void aPackFullOfEmptyFoldersIsRefused()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        for (int i = 0; i < 3000; ++i)
            QDir().mkpath(store.fileOf(kId, QStringLiteral("empty/%1").arg(i)));
        QElapsedTimer clock;
        clock.start();
        QStringList refused;
        QVERIFY(AssetLibrary::readInstalled(store.path(), &refused).isEmpty());
        QCOMPARE(refused.size(), 1);
        QVERIFY2(refused.first().contains(QStringLiteral("folder its manifest does not list")) || refused.first().contains(QStringLiteral("more than")),
                 qPrintable(refused.first()));
    }

    /// Thumbnails come from the checked bytes, as their name's format, and only up to 4096 pixels a side.
    void aThumbnailIsDrawnOnlyFromCheckedBytes()
    {
        Store store;
        QImage wide(5000, 1, QImage::Format_ARGB32);
        wide.fill(red);
        QByteArray wideBytes;
        QBuffer buffer(&wideBytes);
        buffer.open(QIODevice::WriteOnly);
        wide.save(&buffer, "PNG");
        QMap<QString, QByteArray> files = firstVersion();
        files.insert(QStringLiteral("art/wide.png"), wideBytes);
        store.add(kId, QStringLiteral("1.0.0"), files);
        AssetLibrary library;
        library.setFolder(store.path());
        QVERIFY(AssetLibrary::readInstalled(store.path()).size() == 1);
        qq::AssetThumbnails thumbnails;
        QSize size;
        const QImage hero = thumbnails.requestImage(kId + QStringLiteral("/1.0.0/art/hero.png"), &size, QSize(40, 40));
        QCOMPARE(hero.size(), QSize(4, 4));
        QCOMPARE(hero.pixelColor(0, 0), red);
        // Wider than a thumbnail may be: no picture, and nothing decoded.
        QVERIFY(thumbnails.requestImage(kId + QStringLiteral("/1.0.0/art/wide.png"), &size, QSize(40, 40)).isNull());
        // Changed after it was checked: no picture.
        writeFile(store.fileOf(kId, QStringLiteral("art/hero.png")), png(blue));
        QVERIFY(thumbnails.requestImage(kId + QStringLiteral("/1.0.0/art/hero.png"), &size, QSize(40, 40)).isNull());
        // Another version than the one listed, or a file it does not hold: none either.
        QVERIFY(thumbnails.requestImage(kId + QStringLiteral("/9.9.9/art/tiles.png"), &size, QSize(40, 40)).isNull());
        QVERIFY(thumbnails.requestImage(kId + QStringLiteral("/1.0.0/../../secret.png"), &size, QSize(40, 40)).isNull());
    }

    /// A planted journal naming copies the project holds, of another folder that was in this place: refused, and nothing moved.
    void aJournalForAnotherFolderMovesNothing()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));
        const QString tag = QStringLiteral("0123456789abcdef0123456789abcdef");
        QDir().mkpath(project.file(QStringLiteral(".qq-assets-") + tag + QStringLiteral("/old")));
        const auto before = snapshot(project.path());
        writeFile(AssetLibrary::journalFor(project.real()),
                  QJsonDocument(QJsonObject{{QStringLiteral("format"), 1}, {QStringLiteral("project"), project.real()},
                                            {QStringLiteral("tag"), tag}, {QStringLiteral("identity"), QStringLiteral("1:2")},
                                            {QStringLiteral("put"), QJsonArray{inProject(QStringLiteral("art/hero.png"))}},
                                            {QStringLiteral("placed"), QJsonObject{{inProject(QStringLiteral("art/hero.png")), hex(png(red))}}},
                                            {QStringLiteral("remove"), QJsonArray{}},
                                            {QStringLiteral("lock"), hex("never this lock")}}).toJson());
        const QString said = AssetLibrary::recover(project.real());
        QVERIFY2(said.contains(QStringLiteral("not the folder")), qPrintable(said));
        QCOMPARE(snapshot(project.path()), before);
    }

    // ── Beside names ──

    void aNewVersionBesideNeverBreaksTheLock()
    {
        Store store;
        const QString longPath = QString(200, QLatin1Char('a')) + QLatin1Char('/') + QString(200, QLatin1Char('b')) + QLatin1Char('/')
                                 + QString(106, QLatin1Char('c')) + QStringLiteral(".png");
        QCOMPARE(longPath.size(), 512);
        store.add(kId, QStringLiteral("1.0.0"), {{longPath, png(red)}});
        Project project;
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));
        writeFile(project.copy(longPath), png(blue, 8));
        store.add(kId, QStringLiteral("1.1.0"), {{longPath, png(green)}});
        const QVariantMap plan = AssetLibrary::planUpdate(project.path(), store.path(), kId);
        QStringList beside;
        for (const QVariant &each : plan.value(QStringLiteral("changed")).toList())
            beside.append(each.toMap().value(QStringLiteral("path")).toString());
        AssetLibrary::updatePack(project.path(), store.path(), kId, beside);
        // Whatever it did, the project's lock can still be read, so the project can still be built.
        const AssetLibrary::Lock lock = AssetLibrary::readLock(project.real());
        QVERIFY2(lock.problem.isEmpty(), qPrintable(lock.problem));
        QCOMPARE(readBytes(project.copy(longPath)), png(blue, 8));
    }

    void aBesideNameNeverMeetsANewFile()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));
        writeFile(project.copy(QStringLiteral("art/tiles.png")), png(blue, 8));
        QMap<QString, QByteArray> next = secondVersion();
        next.insert(QStringLiteral("art/tiles-1.1.0.png"), png(violet));
        store.add(kId, QStringLiteral("1.1.0"), next);
        const QVariantMap updated = AssetLibrary::updatePack(project.path(), store.path(), kId, {inProject(QStringLiteral("art/tiles.png"))});
        QVERIFY(ok(updated));
        QCOMPARE(readBytes(project.copy(QStringLiteral("art/tiles-1.1.0.png"))), png(violet));
        QCOMPARE(pathsOf(updated.value(QStringLiteral("beside"))), QStringList{inProject(QStringLiteral("art/tiles-1.1.0-2.png"))});
        QCOMPARE(readBytes(project.copy(QStringLiteral("art/tiles-1.1.0-2.png"))), png(green));
    }

    void aBesideCopyIsThePersonsFromThenOn()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));
        writeFile(project.copy(QStringLiteral("art/tiles.png")), png(blue, 8));
        store.add(kId, QStringLiteral("1.1.0"), secondVersion());
        QVERIFY(ok(AssetLibrary::updatePack(project.path(), store.path(), kId, {inProject(QStringLiteral("art/tiles.png"))})));
        QCOMPARE(readBytes(project.copy(QStringLiteral("art/tiles-1.1.0.png"))), png(green));
        QMap<QString, QByteArray> third = secondVersion();
        third.insert(QStringLiteral("art/tiles.png"), png(amber, 6));
        store.add(kId, QStringLiteral("1.2.0"), third);
        QVERIFY(ok(AssetLibrary::updatePack(project.path(), store.path(), kId, {})));
        QCOMPARE(project.lockedPack().value(QStringLiteral("version")).toString(), QStringLiteral("1.2.0"));
        QCOMPARE(readBytes(project.copy(QStringLiteral("art/tiles-1.1.0.png"))), png(green));
    }

    // ── What the panel says and shows ──

    void thePanelSaysTheDeveloperIsNotCheckedAndShowsCheckedBytes()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        AssetLibrary library;
        library.setFolder(store.path());
        const QVariantMap pack = library.state(project.url()).value(QStringLiteral("packs")).toList().value(0).toMap();
        QVERIFY2(pack.value(QStringLiteral("provenance")).toString().contains(QStringLiteral("not checked here")),
                 qPrintable(pack.value(QStringLiteral("provenance")).toString()));
        for (const QVariant &each : pack.value(QStringLiteral("files")).toList()) {
            const QVariantMap file = each.toMap();
            if (file.value(QStringLiteral("kind")) == QStringLiteral("picture"))
                QVERIFY2(file.value(QStringLiteral("url")).toString().startsWith(QStringLiteral("image://")), qPrintable(file.value(QStringLiteral("url")).toString()));
        }
    }

    void aSpoofingNameOrPathIsRefused_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("path");
        QTest::newRow("a right-to-left override in its name") << QStringLiteral("Art \u202Egnp.exe") << QStringLiteral("art/hero.png");
        QTest::newRow("a zero-width joiner in a path") << QStringLiteral("Test art") << QStringLiteral("art/he\u200Dro.png");
        QTest::newRow("a path not in NFC") << QStringLiteral("Test art") << QStringLiteral("art/cafe\u0301.png");
    }

    void aSpoofingNameOrPathIsRefused()
    {
        QFETCH(QString, name);
        QFETCH(QString, path);
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), {{path, png(red)}});
        QJsonObject manifest = store.manifestOf(kId);
        manifest.insert(QStringLiteral("name"), name);
        writeFile(store.fileOf(kId, QStringLiteral("manifest.json")), QJsonDocument(manifest).toJson());
        QStringList refused;
        QVERIFY(AssetLibrary::readInstalled(store.path(), &refused).isEmpty());
        QCOMPARE(refused.size(), 1);
    }

    void itemsThroughALinkIsRefused()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        const QString elsewhere = store.dir.filePath(QStringLiteral("elsewhere"));
        QVERIFY(QDir().rename(store.dir.filePath(QStringLiteral("items")), elsewhere));
        if (!makeLink(elsewhere, store.dir.filePath(QStringLiteral("items"))))
            QSKIP("This user cannot make symbolic links here.");
        QStringList refused;
        QVERIFY(AssetLibrary::readInstalled(store.path(), &refused).isEmpty());
        QVERIFY(!refused.isEmpty());
    }

    // ── Trust, in the window ──

    /// A project from elsewhere: Use and Update both ask for trust first, and change nothing until it is given.
    void anUntrustedProjectIsChangedOnlyOnceTrusted()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        project.saveScene();
        Project other;
        QVERIFY(ok(AssetLibrary::usePack(other.path(), store.path(), kId, {}, false)));
        // A stranger's project, its lock naming the pack; never trusted here.
        QVERIFY(QDir(project.path()).removeRecursively());
        QVERIFY(copyTree(other.path(), project.path()));
        project.saveScene();
        store.add(kId, QStringLiteral("1.1.0"), secondVersion());
        const auto before = snapshot(project.path());
        {
            Studio s;
            QVERIFY(s.open(project.url(), store.path()));
            QVERIFY(!s.get("projectTrusted").toBool());
            QVERIFY(!s.call("useAssets", kId, QStringList()).toBool());
            QTRY_VERIFY(s.opened("trustDialog"));
            s.click(QStringLiteral("keepClosed"));
            QTRY_VERIFY(!s.opened("trustDialog"));
            s.call("offerUpdate", kId);
            QVERIFY(!s.opened("updateDialog"));
            QTRY_VERIFY(s.opened("trustDialog"));
            s.click(QStringLiteral("keepClosed"));
            QTRY_VERIFY(!s.opened("trustDialog"));
        }
        QCOMPARE(snapshot(project.path()), before);
    }

    /// The update's question names the pack, which is someone else's text: its header is plain text, never markup.
    void theUpdatesQuestionIsPlainText()
    {
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), firstVersion());
        Project project;
        project.saveScene();
        trust(project.url());
        QVERIFY(ok(AssetLibrary::usePack(project.path(), store.path(), kId, {}, false)));
        store.add(kId, QStringLiteral("1.1.0"), secondVersion());
        QJsonObject manifest = store.manifestOf(kId);
        manifest.insert(QStringLiteral("name"), QStringLiteral("<b>Bold</b> art"));
        writeFile(store.fileOf(kId, QStringLiteral("manifest.json")), QJsonDocument(manifest).toJson());
        Studio s;
        QVERIFY(s.open(project.url(), store.path()));
        QVERIFY(s.call("offerUpdate", kId).toBool());
        QTRY_VERIFY(s.opened("updateDialog"));
        auto *header = s.child(QStringLiteral("updateDialog"))->property("header").value<QQuickItem *>();
        QVERIFY(header);
        QList<QQuickItem *> items{header};
        bool plain = false;
        while (!items.isEmpty()) {
            QQuickItem *item = items.takeFirst();
            if (item->property("text").toString().contains(QStringLiteral("<b>Bold</b>")))
                plain = item->property("textFormat").toInt() == 0;  // Text.PlainText
            items += item->childItems();
        }
        QVERIFY2(plain, "The pack's name is in a header that is not plain text");
        s.click(QStringLiteral("updateCancel"));
    }

    /// What the window's thread pays for the largest pack: 8,190 files and 32 MB, listed, used whole, listed again, planned and
    /// updated. Measured only when QQ_ASSETLIBRARY_MEASURE is set: a number, not a check.
    void measureTheLargestPack()
    {
        if (qEnvironmentVariableIsEmpty("QQ_ASSETLIBRARY_MEASURE"))
            QSKIP("QQ_ASSETLIBRARY_MEASURE is not set");
        QMap<QString, QByteArray> first;
        QMap<QString, QByteArray> second;
        for (int i = 0; i < 8190; ++i) {
            const QString name = QStringLiteral("tiles/%1/%2.png").arg(i / 256, 2, 10, QLatin1Char('0')).arg(i, 4, 10, QLatin1Char('0'));
            first.insert(name, QByteArray("\x89PNG\r\n\x1a\n") + QByteArray::number(i) + QByteArray(4000, 'a'));
            second.insert(name, QByteArray("\x89PNG\r\n\x1a\n") + QByteArray::number(i) + QByteArray(4000, 'b'));
        }
        Store store;
        store.add(kId, QStringLiteral("1.0.0"), first);
        Project project;
        AssetLibrary library;
        library.setFolder(store.path());
        QElapsedTimer clock;
        const auto took = [&clock](const char *what) {
            qInfo("MEASURE %s: %lld ms", what, qint64(clock.elapsed()));
            clock.restart();
        };
        clock.start();
        library.state(project.url());
        took("list, first time");
        library.state(project.url());
        took("list, again");
        QVERIFY(library.use(project.url(), kId, {}, false).value(QStringLiteral("ok")).toBool());
        took("use whole");
        library.state(project.url());
        took("list after use");
        store.add(kId, QStringLiteral("1.1.0"), second);
        clock.restart();
        library.state(project.url());
        took("list the new version");
        QVERIFY(library.plan(project.url(), kId).value(QStringLiteral("ok")).toBool());
        took("plan the update");
        QVERIFY(library.update(project.url(), kId, {}).value(QStringLiteral("ok")).toBool());
        took("update");
        library.state(project.url());
        took("list after update");
    }

    // ── The demonstration pack, installed by the real Store ──

    void theDemonstrationPackFromTheRealStore()
    {
        const QString install = prismProgram(QStringLiteral("prism-test-store-install"));
        if (install.isEmpty())
            QSKIP("QOR_PRISM_BUILD names no build of Prism with prism-test-store-install");
        const QString demo = QStringLiteral(QOR_STORE_DEMO);
        QTemporaryDir store;
        const QString key = demo + QStringLiteral("/store.pub");
        QCOMPARE(storeEngine(install, {store.path(), key, demo + QStringLiteral("/v1/catalog.json"), kDemoPack}), 0);

        Project project;
        project.saveScene();
        trust(project.url());
        const QString hero = project.copy(QStringLiteral("art/hero.png"), kDemoPack);
        const QString tiles = project.copy(QStringLiteral("art/tiles.png"), kDemoPack);
        QString game;
        {
            Studio s;
            QVERIFY(s.open(project.url(), store.path()));
            const QVariantList packs = s.assets().value(QStringLiteral("packs")).toList();
            QCOMPARE(packs.size(), 1);
            const QVariantMap pack = packs.first().toMap();
            QCOMPARE(pack.value(QStringLiteral("name")).toString(), QStringLiteral("Ember Climb's art"));
            QCOMPARE(pack.value(QStringLiteral("version")).toString(), QStringLiteral("1.0.0"));
            QCOMPARE(pack.value(QStringLiteral("licence")).toString(), QStringLiteral("Apache-2.0"));
            // Where it came from, as the Store's signed listing says and its engine keeps it in installed.json.
            QCOMPARE(pack.value(QStringLiteral("provenance")).toString(), QStringLiteral("From Demiurge, QOR-MATRIX/demiurge-chain, through the Store"));
            QVERIFY(s.call("useAssets", kDemoPack, QStringList()).toBool());

            // The lock is right: every file as the Store's manifest lists it, and the manifest's own digest.
            const QJsonObject manifest = QJsonDocument::fromJson(readBytes(store.filePath(QStringLiteral("items/%1/manifest.json").arg(kDemoPack)))).object();
            const QJsonObject listed = manifest.value(QStringLiteral("files")).toObject();
            QCOMPARE(project.lockedPack(kDemoPack).value(QStringLiteral("files")).toArray().size(), listed.size());
            for (auto it = listed.begin(); it != listed.end(); ++it) {
                QCOMPARE(project.lockedFile(inProject(it.key(), kDemoPack), kDemoPack).value(QStringLiteral("sha256")).toString(), it.value().toString());
                QCOMPARE(hex(readBytes(project.copy(it.key(), kDemoPack))), it.value().toString());
            }
            QCOMPARE(project.lockedPack(kDemoPack).value(QStringLiteral("provenance")).toObject().value(QStringLiteral("manifest")).toString(),
                     hex(readBytes(store.filePath(QStringLiteral("items/%1/manifest.json").arg(kDemoPack)))));
            const QJsonObject provenance = project.lockedPack(kDemoPack).value(QStringLiteral("provenance")).toObject();
            const QJsonObject record = QJsonDocument::fromJson(readBytes(store.filePath(QStringLiteral("installed.json")))).object()
                                           .value(QStringLiteral("items")).toObject().value(kDemoPack).toObject();
            QCOMPARE(record.value(QStringLiteral("provenanceFrom")).toString(), QStringLiteral("listing"));
            for (const char *key : {"origin", "author", "source", "permission"})
                QCOMPARE(provenance.value(QLatin1String(key)).toString(), record.value(QStringLiteral("provenance")).toObject().value(QLatin1String(key)).toString());
            QCOMPARE(provenance.value(QStringLiteral("origin")).toString(), QStringLiteral("QOR-MATRIX/demiurge-chain"));

            // The project is edited with the copy, saved and built.
            QVERIFY(s.call("openScene", QStringLiteral("level")).toBool());
            QVERIFY(s.call("add", QStringLiteral("Sprite"), QVariantMap{{QStringLiteral("name"), QStringLiteral("Hero")},
                                                                         {QStringLiteral("source"), QUrl::fromLocalFile(hero)}})
                        .value<QObject *>());
            s.call("refresh");
            QVERIFY(s.call("saveScene").toBool());
            QVERIFY(readBytes(project.file(QStringLiteral("scenes/level.qml"))).contains("art/hero.png"));
            s.call("say", QString(), false);
            s.click(QStringLiteral("buildButton"));
            QTRY_VERIFY_WITH_TIMEOUT(s.notice().startsWith(QStringLiteral("Built")) || s.get("noticeIsProblem").toBool(), 60000);
            QVERIFY2(s.notice().startsWith(QStringLiteral("Built")), qPrintable(s.notice()));
            QTRY_VERIFY(!s.child(QStringLiteral("gameBuild"))->property("busy").toBool());
            game = qq::GameBuild::gamesFolder() + QLatin1Char('/') + qq::GameBuild::folderNameFor(project.real());
            const QJsonArray uses = QJsonDocument::fromJson(readBytes(game + QStringLiteral("/game.json"))).object().value(QStringLiteral("uses")).toArray();
            QCOMPARE(uses.size(), 1);
            QCOMPARE(uses.first().toObject().value(QStringLiteral("licence")).toString(), QStringLiteral("Apache-2.0"));
            QCOMPARE(readBytes(game + QLatin1Char('/') + inProject(QStringLiteral("art/hero.png"), kDemoPack)), readBytes(hero));

            // The person changes the tiles; the Store moves on to 1.1.0, and the Studio updates, keeping them.
            writeFile(tiles, png(blue, 8));
            QCOMPARE(storeEngine(install, {store.path(), key, demo + QStringLiteral("/v2/catalog.json"), kDemoPack}), 0);
            s.call("refreshAssets");
            QVERIFY(s.assets().value(QStringLiteral("packs")).toList().first().toMap().value(QStringLiteral("update")).toBool());
            QVERIFY(s.call("offerUpdate", kDemoPack).toBool());
            QTRY_VERIFY(s.opened("updateDialog"));
            s.click(QStringLiteral("updateGo"));
            QTRY_COMPARE(project.lockedPack(kDemoPack).value(QStringLiteral("version")).toString(), QStringLiteral("1.1.0"));
            QCOMPARE(readBytes(tiles), png(blue, 8));
            QCOMPARE(hex(readBytes(project.copy(QStringLiteral("art/tiles-1.1.0.png"), kDemoPack))),
                     QStringLiteral("7800f5a54f87f8cf334bc02a6859f5700628e94bcf8d4684c7359dbc26e8a426"));
            QVERIFY(QFileInfo::exists(project.copy(QStringLiteral("art/torch.png"), kDemoPack)));
            QVERIFY(QFileInfo::exists(project.copy(QStringLiteral("art/ember.png"), kDemoPack)));
        }
        // A restart shows it all: the pack at 1.1.0 from the lock, and the scene still using the copy.
        {
            Studio s;
            QVERIFY(s.open(project.url(), store.path()));
            QVERIFY(s.call("openScene", QStringLiteral("level")).toBool());
            const QVariantMap pack = s.assets().value(QStringLiteral("packs")).toList().first().toMap();
            QCOMPARE(pack.value(QStringLiteral("used")).toString(), QStringLiteral("1.1.0"));
            QCOMPARE(pack.value(QStringLiteral("update")).toBool(), false);
            QObject *scene = s.get("scene").value<QObject *>();
            QObject *sprite = nullptr;
            for (const QVariant &each : qq::SceneIO().entities(scene)) {
                QObject *entity = each.value<QObject *>();
                if (entity && entity->property("name").toString() == QStringLiteral("Hero"))
                    sprite = entity;
            }
            QVERIFY(sprite);
            QCOMPARE(qq::Confinement::realPath(qq::SceneIO().resolvedUrl(sprite, sprite->property("source").toUrl()).toLocalFile()),
                     qq::Confinement::realPath(hero));
        }
        // Removed from the Store, by the Store: the copies and the lock stay, and the library says so.
        const auto before = snapshot(project.path());
        QCOMPARE(storeEngine(install, {store.path(), key, QStringLiteral("--remove"), kDemoPack}), 0);
        {
            Studio s;
            QVERIFY(s.open(project.url(), store.path()));
            QCOMPARE(s.assets().value(QStringLiteral("missing")).toList().size(), 1);
        }
        QCOMPARE(snapshot(project.path()), before);
        QDir(game).removeRecursively();
    }
};

namespace {

/// A change to a project's assets that dies at one of its steps, leaving everything as it stands.
int dieAt(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
#ifndef Q_OS_LINUX
    QStandardPaths::setTestModeEnabled(true);
#endif
    const QByteArray step = argv[2];
    const auto steps = [step](const char *at) {
        if (step == at)
            std::_Exit(42);
    };
    const QString operation = QString::fromLocal8Bit(argv[3]);
    const QString project = QString::fromLocal8Bit(argv[4]);
    const QString packs = QString::fromLocal8Bit(argv[5]);
    QVariantMap result;
    if (operation == QStringLiteral("use") || operation == QStringLiteral("replace")) {
        result = AssetLibrary::usePack(project, packs, kId, {}, true, steps);
    } else {
        const QVariantMap plan = AssetLibrary::planUpdate(project, packs, kId);
        QStringList changed;
        for (const QVariant &each : plan.value(QStringLiteral("changed")).toList())
            changed.append(each.toMap().value(QStringLiteral("path")).toString());
        result = AssetLibrary::updatePack(project, packs, kId, changed, steps);
    }
    qWarning("The change did not stop where it was told to: %s", qPrintable(result.value(QStringLiteral("problem")).toString()));
    return 1;
}

}  // namespace

/// A pack used here, and what was said: for a store that cannot be written.
int useHere(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QVariantMap result = AssetLibrary::usePack(QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3]), kId, {}, false);
    std::fputs(qPrintable(result.value(QStringLiteral("problem")).toString()), stdout);
    return result.value(QStringLiteral("ok")).toBool() ? 0 : 3;
}

int main(int argc, char *argv[])
{
    // As the Studio names itself, so what this keeps is where qq-studio keeps it.
    QCoreApplication::setApplicationName(QStringLiteral("QQ Studio"));
    QCoreApplication::setOrganizationName(QStringLiteral("Demiurge"));
#ifdef Q_OS_LINUX
    // One folder for this suite and every program it starts, gone with it; a child is handed the same.
    static QTemporaryDir kept;
    if (qEnvironmentVariableIsEmpty("QQ_ASSETLIBRARY_TEST_HOME")) {
        qputenv("QQ_ASSETLIBRARY_TEST_HOME", kept.path().toUtf8());
        qputenv("XDG_DATA_HOME", (kept.path() + QStringLiteral("/data")).toUtf8());
        qputenv("XDG_CONFIG_HOME", (kept.path() + QStringLiteral("/config")).toUtf8());
    }
#endif
    if (argc == 6 && qstrcmp(argv[1], "--die-at") == 0)
        return dieAt(argc, argv);
    if (argc == 4 && qstrcmp(argv[1], "--use-here") == 0)
        return useHere(argc, argv);
    QGuiApplication app(argc, argv);
    QCoreApplication::setAttribute(Qt::AA_Use96Dpi, true);
#ifndef Q_OS_LINUX
    QStandardPaths::setTestModeEnabled(true);
#endif
    TstAssetLibrary test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "tst_assetlibrary.moc"
