// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// Catalogue format 2, the package checks and the one engine behind two storefronts (QOR-0021), from the hostile side
// first. Format 1's own tests stay in tst_store.cpp, untouched.
//
// Every hostile archive and catalogue is made here, beside the assertion that refuses it, and signed with a key made
// for the test (testsigner.h), so that it reaches the check it is for. Each assertion names the reason it expects: a
// package refused for some other reason would otherwise pass while proving nothing.
#include "archive.h"
#include "catalog.h"
#include "filerules.h"
#include "package.h"
#include "signature.h"
#include "store.h"
#include "storeengine.h"
#include "testshop.h"

#include <QtCore/QBuffer>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QLockFile>
#include <QtCore/QRandomGenerator>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QTemporaryDir>
#include <QtCore/QThread>
#include <QtCore/QTimeZone>
#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <algorithm>

#ifdef Q_OS_UNIX
#include <sys/resource.h>
#endif

using namespace qor::store;
using namespace teststore;
using namespace Qt::StringLiterals;

namespace {

const QString kSource = QStringLiteral(QOR_SOURCE_DIR);
const QString kCatalog = QStringLiteral(QOR_STORE_CATALOG);
const QString kStorePub = QStringLiteral(QOR_STORE_PUB);
const QString kFixtures = QStringLiteral(QOR_STORE_FIXTURES);
const QString kDemo = QStringLiteral(QOR_STORE_DEMO);
const QString kLicences = QStringLiteral(QOR_STORE_LICENCES);

const QByteArray kJpg = QByteArray::fromHex("ffd8ffe0") + "a picture";
const QByteArray kWebp = QByteArrayLiteral("RIFF....WEBPa picture");
const QByteArray kWav = QByteArrayLiteral("RIFF....WAVEa sound");
const QByteArray kOgg = QByteArrayLiteral("OggSa sound");
const QByteArray kFlac = QByteArrayLiteral("fLaCa sound");
const QByteArray kMp3 = QByteArrayLiteral("ID3a sound");
const QByteArray kHdr = QByteArrayLiteral("#?RADIANCE\n");

// A one-item format 1 catalogue, with `change` applied to the item.
CatalogResult parseFormat1(const std::function<void(QJsonObject &)> &change)
{
    const TestSigner store;
    const TestSigner developer;
    QJsonObject item { { u"id"_s, u"org.example.theme.test"_s },
                       { u"kind"_s, u"theme"_s },
                       { u"name"_s, u"A theme"_s },
                       { u"developer"_s, kDeveloper },
                       { u"version"_s, u"1.0.0"_s },
                       { u"package"_s, QJsonObject { { u"file"_s, u"packages/t.qorpkg"_s },
                                                     { u"size"_s, 10 },
                                                     { u"sha256"_s, hexDigest("x") } } } };
    change(item);
    const QJsonObject root {
        { u"format"_s, 1 },
        { u"store"_s, u"A test store"_s },
        { u"developers"_s,
          QJsonObject {
              { kDeveloper, QJsonObject { { u"name"_s, u"Example"_s },
                                          { u"key"_s, QString::fromLatin1(developer.publicKey().toBase64()) } } } } },
        { u"items"_s, QJsonArray { item } }
    };
    const QByteArray bytes = QJsonDocument(root).toJson();
    return parseCatalog(bytes, store.sign(bytes), store.publicKey());
}

// An engine with creator features on, as every test here that is not about the switch wants.
class CreatorStore : public StoreEngine
{
public:
    CreatorStore(const QString &dataDir, const QByteArray &storeKey) : StoreEngine(dataDir, storeKey)
    {
        setCreatorFeatures(true);
    }
};

// An engine that can be handed a catalogue, to reach the walls behind the ones that normally stop it first, and told a
// later stage has come, to prove what does not depend on the stage.
class PlantedEngine : public StoreEngine
{
public:
    using StoreEngine::adoptCatalog;
    using StoreEngine::StoreEngine;
    using Stage = CommitStage;
    QSet<QString> laterKinds;
    std::function<bool(Stage)> onStage; // what an install does as it reaches each moment; false stands in for failing

protected:
    bool takesKind(const QString &kind) const override
    {
        return laterKinds.contains(kind) || StoreEngine::takesKind(kind);
    }
    bool reachedStage(CommitStage stage) override { return onStage ? onStage(stage) : true; }
    bool saveInstalled(const QMap<QString, Installed> &records, const QString &transaction) const override
    {
        return !refuseRecords && StoreEngine::saveInstalled(records, transaction);
    }
    void syncFolder(const QString &path) const override
    {
        const QString relative = QDir(dataDir()).relativeFilePath(QDir::cleanPath(path));
        events.append(u"sync "_s + (relative.isEmpty() ? u"."_s : relative));
        StoreEngine::syncFolder(path);
    }
    qint64 heldBytesLimit() const override { return heldLimit >= 0 ? heldLimit : StoreEngine::heldBytesLimit(); }

public:
    bool refuseRecords = false; // as a disk would that is full or gone read-only
    qint64 heldLimit = -1;      // what plans may hold together, if not the engine's own limit
    mutable QStringList events; // each folder made durable, as "sync <path in the data folder>", and what a test adds
};

// A copy of a folder, as a crash at this moment would leave it.
void copyFolder(const QString &from, const QString &to)
{
    QDir().mkpath(to);
    const QDir root(from);
    QDirIterator walk(from, QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (walk.hasNext()) {
        const QString source = walk.next();
        const QString target = to + u'/' + root.relativeFilePath(source);
        if (QFileInfo(source).fileName() == u".lock"_s)
            continue; // a crash leaves no holder of it, and a copy of this live one would look held
        if (QFileInfo(source).isDir())
            QDir().mkpath(target);
        else {
            QDir().mkpath(QFileInfo(target).absolutePath());
            QFile::copy(source, target);
        }
    }
}

// What is left in an install folder beside what is installed. Nothing should be, once an engine has started on it.
QStringList leftovers(const QString &dataDir)
{
    QStringList found;
    const QStringList names =
        QDir(dataDir + u"/items"_s).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
    for (const QString &name : names)
        if (name.startsWith(u".staging-"_s) || name.contains(u".old-"_s))
            found.append(name);
    if (QFile::exists(dataDir + u"/plan-journal.json"_s))
        found.append(u"plan-journal.json"_s);
    return found;
}

CatalogItem plantedItem(const QString &id, const QString &kind)
{
    CatalogItem item;
    item.format = 2;
    item.id = id;
    item.kind = kind;
    item.name = u"Planted"_s;
    item.version = u"1.0.0"_s;
    item.developer = kDeveloper;
    item.entitlement = u"free"_s;
    item.platforms = { currentPlatform() };
    item.engines = { VersionRange { u"qq"_s, u"0.1.0"_s, u"1.0.0"_s } };
    item.packageFile = u"packages/none.qorpkg"_s;
    item.packageSize = 1;
    item.packageUnpacked = 1;
    item.placements = { Placement { kindMayBePlacedIn(kind, View::Player) ? View::Player : View::Creator,
                                    u"Things"_s } };
    return item;
}

Catalog plantedCatalog(const QList<CatalogItem> &items)
{
    Catalog catalog;
    catalog.format = 2;
    catalog.sequence = 1;
    catalog.expires = QDate(2099, 12, 31);
    catalog.items = items;
    return catalog;
}

// Lays a real package beside a shop's catalogue and returns the listing for it, as a later stage's parser would. The
// parser of this stage keeps such a kind a draft, which is what planting gets past.
CatalogItem plantedPackage(const Shop &shop, const Pack &pack, const QList<Placement> &placements)
{
    const QByteArray package = pack.package(shop.developer);
    writeFile(shop.dir.path() + u"/packages/%1-%2.qorpkg"_s.arg(pack.id, pack.version), package);
    CatalogItem item = plantedItem(pack.id, pack.kind);
    item.version = pack.version;
    item.placements = placements;
    item.licence = Licence { pack.spdx, pack.licenceFile };
    item.platforms.clear();
    for (const QJsonValue &platform : pack.platforms)
        item.platforms.append(platform.toString());
    item.engines.clear();
    for (const QJsonValue &engine : pack.engines) {
        const QJsonObject range = engine.toObject();
        item.engines.append(VersionRange { range.value(u"id"_s).toString(), range.value(u"min"_s).toString(),
                                           range.value(u"below"_s).toString() });
    }
    for (const QJsonValue &dependency : pack.dependencies) {
        const QJsonObject range = dependency.toObject();
        item.dependencies.append(VersionRange { range.value(u"id"_s).toString(), range.value(u"min"_s).toString(),
                                                range.value(u"below"_s).toString() });
    }
    item.packageFile = u"packages/%1-%2.qorpkg"_s.arg(pack.id, pack.version);
    item.packageSize = package.size();
    item.packageSha256 = QCryptographicHash::hash(package, QCryptographicHash::Sha256);
    item.packageUnpacked = 0;
    for (const RawEntry &entry : pack.entries(shop.developer))
        item.packageUnpacked += entry.data.size();
    if (!pack.edition.isEmpty())
        item.edition = Edition { pack.edition.value(u"group"_s).toString(), pack.edition.value(u"role"_s).toString() };
    return item;
}

void listDeveloper(Catalog *catalog, const Shop &shop)
{
    catalog->developers.insert(kDeveloper, Developer { kDeveloper, u"Example"_s, true, shop.developer.publicKey() });
}

QStringList idsOf(const QList<CatalogItem> &items)
{
    QStringList ids;
    for (const CatalogItem &item : items)
        ids.append(item.id);
    return ids;
}

QString folderOfNames(const QString &folder)
{
    QStringList names = QDir(folder).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name);
    return names.join(u' ');
}

const QByteArray kOkModel =
    QByteArrayLiteral("{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"uri\":\"model.bin\"}]}");

// A .glb holding `json` as its first chunk.
QByteArray glbHolding(const QByteArray &json)
{
    QByteArray glb("glTF");
    put32(glb, 2);
    put32(glb, quint32(12 + 8 + json.size()));
    put32(glb, quint32(json.size()));
    glb.append("JSON");
    glb.append(json);
    return glb;
}

// Rewrites a package with stored entries, unlike tst_store.cpp's rezip, whose deflated ones now stop at the door.
QByteArray rezipStored(const QByteArray &archive, const std::function<void(QMap<QString, QByteArray> &)> &edit)
{
    QBuffer in;
    in.setData(archive);
    in.open(QIODevice::ReadOnly);
    QZipReader reader(&in);
    QMap<QString, QByteArray> entries;
    for (const QZipReader::FileInfo &info : reader.fileInfoList())
        entries.insert(info.filePath, reader.fileData(info.filePath));
    edit(entries);
    QList<RawEntry> raw;
    for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
        RawEntry entry;
        entry.name = it.key().toLatin1();
        entry.data = it.value();
        raw.append(entry);
    }
    return forgeArchive(raw);
}

} // namespace

class TestStoreFormat2 : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ── Archives: bombs ──────────────────────────────────────────────────────

    // 64 MB of zeros in a few kilobytes, claiming to unpack to one. The reader has no way to inflate it.
    void refusesADeflateBomb()
    {
        const TestSigner developer;
        const Pack pack;
        QList<RawEntry> entries = pack.entries(developer);
        const QByteArray zeros(64 * 1024 * 1024, '\0');
        const QByteArray zlib = qCompress(zeros, 9);
        RawEntry bomb;
        bomb.name = "art/bomb.png";
        bomb.data =
            zlib.mid(6, zlib.size() - 10); // the raw deflate stream, without qCompress's length and zlib's frame
        bomb.method = 8;
        bomb.unpacked = 1024;
        entries.append(bomb);
        QVERIFY(bomb.data.size() < 1024 * 1024);

        const PackageResult opened = openPackage(forgeArchive(entries), developer.publicKey());
        QVERIFY(!opened.ok);
        QVERIFY2(opened.error.contains(u"compressed"_s), qPrintable(opened.error));
        QVERIFY(opened.contents.isEmpty());
    }

    // Both formats: every existing package is stored, so nothing real is compressed.
    void refusesAnyCompressedEntry()
    {
        const CatalogResult catalog = parseCatalog(readFile(kCatalog + u"/catalog.json"_s),
                                                   readFile(kCatalog + u"/catalog.json.sig"_s), publicKey(kStorePub));
        QVERIFY(catalog.ok);
        const CatalogItem *clock = catalog.catalog.item(u"dev.qorsync.widget.clock"_s);
        QVERIFY(clock);
        const QByteArray key = catalog.catalog.developers.value(clock->developer).key;
        const QByteArray package = readFile(kCatalog + u'/' + clock->packageFile);
        QVERIFY2(openPackage(package, key).ok, qPrintable(openPackage(package, key).error));

        // The very same bytes, deflated by Qt's own writer: nothing changed but the compression.
        QBuffer in;
        in.setData(package);
        in.open(QIODevice::ReadOnly);
        QZipReader reader(&in);
        QBuffer out;
        out.open(QIODevice::WriteOnly);
        {
            QZipWriter writer(&out);
            for (const QZipReader::FileInfo &info : reader.fileInfoList())
                writer.addFile(info.filePath, reader.fileData(info.filePath));
        }
        const PackageResult deflated = openPackage(out.data(), key);
        QVERIFY(!deflated.ok);
        QVERIFY2(deflated.error.contains(u"compressed"_s), qPrintable(deflated.error));
    }

    void refusesAnEntryThatIsNotWhatItClaims_data()
    {
        QTest::addColumn<QByteArray>("archive");
        QTest::addColumn<QString>("reason");
        const QByteArray big(1000, 'x');

        RawEntry bothHeaders;
        bothHeaders.name = "a.png";
        bothHeaders.data = big;
        bothHeaders.unpacked = 10;
        QTest::newRow("stored, and larger than its unpacked size") << forgeArchive({ bothHeaders }) << u"claims"_s;

        RawEntry smaller;
        smaller.name = "a.png";
        smaller.data = big;
        smaller.packed = 10;
        smaller.unpacked = 10;
        QTest::newRow("headers both small, bytes large") << forgeArchive({ smaller }) << u"after the last"_s;

        RawEntry directory;
        directory.name = "a.png";
        directory.data = big;
        directory.centralUnpacked = 10;
        QTest::newRow("the directory differs from the entry") << forgeArchive({ directory }) << u"claims"_s;

        RawEntry differentName;
        differentName.name = "evil.dll";
        differentName.centralName = "good.png";
        differentName.data = big;
        QTest::newRow("the directory names it differently") << forgeArchive({ differentName }) << u"different name"_s;

        RawEntry beyond;
        beyond.name = "a.png";
        beyond.data = big;
        beyond.packed = 0x7fffffff;
        beyond.unpacked = 0x7fffffff;
        QTest::newRow("runs off the end of the archive") << forgeArchive({ beyond }) << u"bytes run past"_s;

        RawEntry zip64;
        zip64.name = "a.png";
        zip64.data = big;
        zip64.packed = 0xffffffff;
        zip64.unpacked = 0xffffffff;
        QTest::newRow("a zip64 marker") << forgeArchive({ zip64 }) << u"bytes run past"_s;
    }
    void refusesAnEntryThatIsNotWhatItClaims()
    {
        QFETCH(QByteArray, archive);
        QFETCH(QString, reason);
        const ArchiveResult read = readStoredArchive(archive, kMaximumPackageContent);
        QVERIFY(!read.ok);
        QVERIFY2(read.error.contains(reason), qPrintable(read.error));
    }

    // Stored entries sharing one run of bytes unpack to far more than the archive holds, with no compression at all.
    void refusesOverlappingEntries()
    {
        const QByteArray shared(1024 * 1024, 'x');
        RawEntry first;
        first.name = "a.png";
        first.data = shared;
        QList<RawEntry> entries { first };
        for (int i = 0; i < 100; ++i) {
            RawEntry another;
            another.name = "b" + QByteArray::number(i) + ".png";
            another.centralOffset = 0;
            entries.append(another);
        }
        const ArchiveResult read = readStoredArchive(forgeArchive(entries), kMaximumPackageContent);
        QVERIFY(!read.ok);
        QVERIFY2(read.error.contains(u"overlap"_s), qPrintable(read.error));
    }

    // What is counted is what is there. Exactly the limit passes, and one byte under it does not.
    void boundsWhatIsUnpackedAsItIsRead()
    {
        const TestSigner developer;
        Pack pack;
        pack.files.insert(u"art/large.png"_s, kPng + QByteArray(1024 * 1024, 'x'));
        const QByteArray package = pack.package(developer);
        qint64 total = 0;
        for (const RawEntry &entry : pack.entries(developer))
            total += entry.data.size();

        const PackageResult fits = openPackage(package, developer.publicKey(), total);
        QVERIFY2(fits.ok, qPrintable(fits.error));
        QCOMPARE(fits.unpacked, total);

        const PackageResult over = openPackage(package, developer.publicKey(), total - 1);
        QVERIFY(!over.ok);
        QVERIFY2(over.error.contains(u"too large"_s), qPrintable(over.error));

        // The default, over the real thing, and past what any header says.
        Pack huge;
        huge.files.insert(u"art/huge.png"_s, kPng + QByteArray(kMaximumPackageContent, 'x'));
        const PackageResult tooHuge = openPackage(huge.package(developer), developer.publicKey());
        QVERIFY(!tooHuge.ok);
        QVERIFY2(tooHuge.error.contains(u"too large"_s), qPrintable(tooHuge.error));
    }

    void refusesTooManyEntries()
    {
        QList<RawEntry> entries;
        for (int i = 0; i < 4097; ++i) {
            RawEntry entry;
            entry.name = "f" + QByteArray::number(i) + ".png";
            entries.append(entry);
        }
        const ArchiveResult read = readStoredArchive(forgeArchive(entries), kMaximumPackageContent);
        QVERIFY(!read.ok);
        QVERIFY2(read.error.contains(u"too many"_s), qPrintable(read.error));
    }

    // ── Archives: names ──────────────────────────────────────────────────────

    void refusesUnsafeNames_data()
    {
        QTest::addColumn<QByteArray>("name");
        const QList<QByteArray> names {
            // Windows' device names, with and without an extension, in any case, at any depth.
            "CON", "con", "Con.txt", "PRN", "AUX.wav", "NUL", "nul.png", "COM1", "com9.png", "LPT1", "lpt9.txt",
            "art/CON/hero.png", "art/NUL.png",
            // Names Windows would change on the way to the disk.
            "a.png.", "art./a.png", "a.png ", "art /a.png",
            // Names that leave the folder, or are not one name on every system.
            "../a.png", "a/../b.png", "/a.png", "C:/a.png", "c:a.png", "a\\b.png", "a.png:stream", "a//b.png", "a/",
            ".hidden.png"
        };
        for (const QByteArray &name : names)
            QTest::addRow("%s", name.constData()) << name;
        QTest::newRow("two dots inside a name") << QByteArray("a..b.png");
        QTest::newRow("a newline after the name") << QByteArray("a.png\n");
        QTest::newRow("a NUL in the name") << QByteArray("a\0b.png", 7);
        QTest::newRow("Latin-1 in the name") << QByteArray("caf\xe9.png");
        QTest::newRow("empty") << QByteArray("");
        QTest::newRow("very long") << QByteArray(QByteArray(201, 'a') + ".png");
    }
    void refusesUnsafeNames()
    {
        QFETCH(QByteArray, name);
        const TestSigner developer;
        Pack pack;
        pack.files.insert(QString::fromLatin1(name), kPng);
        const PackageResult opened = openPackage(pack.package(developer), developer.publicKey());
        QVERIFY(!opened.ok);
        QVERIFY2(opened.error.contains(u"unsafe"_s), qPrintable(opened.error));
    }

    void acceptsOrdinaryNames()
    {
        for (const QString &path :
             { u"LICENSE.txt"_s, u"art/hero.png"_s, u"a/b/c/d.wav"_s, u"console.png"_s, u"auxiliary.png"_s,
               u"nullable.txt"_s, u"com10.png"_s, u"lpt.txt"_s, u"_x.png"_s, u"my-art_2.v1.png"_s })
            QVERIFY2(isSafePackagePath(path), qPrintable(path));
    }

    void refusesCollidingNames_data()
    {
        QTest::addColumn<QStringList>("names");
        QTest::newRow("the same name twice") << QStringList { u"a.png"_s, u"a.png"_s };
        QTest::newRow("names differing in case") << QStringList { u"a.png"_s, u"A.PNG"_s };
        QTest::newRow("folders differing in case") << QStringList { u"art/a.png"_s, u"ART/b.png"_s };
        // Not the pack's own art/: each of these two has to be seen in its own order to prove its own check.
        QTest::newRow("a file, then a folder of that name") << QStringList { u"stuff"_s, u"stuff/a.png"_s };
        QTest::newRow("a folder, then a file of that name") << QStringList { u"stuff/a.png"_s, u"stuff"_s };
        QTest::newRow("a file and a folder differing in case") << QStringList { u"art/a.png"_s, u"ART"_s };
        QTest::newRow("the manifest, in other case") << QStringList { u"MANIFEST.JSON"_s };
    }
    void refusesCollidingNames()
    {
        QFETCH(QStringList, names);
        const TestSigner developer;
        const Pack pack;
        QList<RawEntry> entries = pack.entries(developer);
        for (const QString &name : names) {
            RawEntry entry;
            entry.name = name.toLatin1();
            entry.data = kPng;
            entries.append(entry);
        }
        const PackageResult opened = openPackage(forgeArchive(entries), developer.publicKey());
        QVERIFY(!opened.ok);
        QVERIFY2(opened.error.contains(u"land on one file"_s), qPrintable(opened.error));
    }

    void refusesOtherShapes_data()
    {
        QTest::addColumn<QByteArray>("archive");
        QTest::addColumn<QString>("reason");
        RawEntry plain;
        plain.name = "a.png";
        plain.data = kPng;
        const auto entry = [&plain](const std::function<void(RawEntry &)> &change) {
            RawEntry changed = plain;
            change(changed);
            return forgeArchive({ changed });
        };
        const auto shaped = [&plain](const std::function<void(ArchiveShape &)> &change) {
            ArchiveShape shape;
            change(shape);
            return forgeArchive({ plain }, shape);
        };

        QTest::newRow("a symbolic link") << entry([](RawEntry &e) { e.mode = 0120777; }) << u"a link or a folder"_s;
        QTest::newRow("a symbolic link that says it came from DOS") << entry([](RawEntry &e) {
            e.mode = 0120777;
            e.host = 0;
        }) << u"a link or a folder"_s;
        QTest::newRow("a folder by its Unix attributes")
            << entry([](RawEntry &e) { e.mode = 040755; }) << u"a link or a folder"_s;
        QTest::newRow("a folder by its DOS attributes")
            << entry([](RawEntry &e) { e.dosAttributes = 0x10; }) << u"marked as a folder"_s;
        QTest::newRow("from a system not known") << entry([](RawEntry &e) { e.host = 19; }) << u"does not know"_s;
        QTest::newRow("encrypted") << entry([](RawEntry &e) { e.flags = 1; }) << u"a flag"_s;
        QTest::newRow("sizes given after the data") << entry([](RawEntry &e) { e.flags = 8; }) << u"a flag"_s;
        QTest::newRow("a method not known") << entry([](RawEntry &e) { e.method = 99; }) << u"compressed"_s;
        QTest::newRow("an extra field in the directory")
            << entry([](RawEntry &e) { e.centralExtra = "xx"; }) << u"extra field in the directory"_s;
        QTest::newRow("a comment on an entry")
            << entry([](RawEntry &e) { e.centralComment = "xx"; }) << u"has a comment"_s;
        QTest::newRow("an entry on another disk") << entry([](RawEntry &e) { e.diskStart = 1; }) << u"another disk"_s;

        // One header differing from the other, each field alone.
        QTest::newRow("flags differ") << entry([](RawEntry &e) { e.localFlags = 0x0800; }) << u"different flags"_s;
        QTest::newRow("method differs") << entry([](RawEntry &e) { e.localMethod = 8; }) << u"different method"_s;
        QTest::newRow("CRC differs") << entry([](RawEntry &e) { e.localCrc = 1; }) << u"different CRC"_s;
        QTest::newRow("stored size differs")
            << entry([](RawEntry &e) { e.localPacked = 1; }) << u"different stored size"_s;
        QTest::newRow("unpacked size differs")
            << entry([](RawEntry &e) { e.localUnpacked = 1; }) << u"different unpacked size"_s;
        QTest::newRow("name length differs")
            << entry([](RawEntry &e) { e.localNameLength = 1; }) << u"different name length"_s;
        QTest::newRow("an extra field in its own header")
            << entry([](RawEntry &e) { e.localExtra = "xx"; }) << u"extra field in its own header"_s;
        QTest::newRow("a signature wrong on the entry")
            << entry([](RawEntry &e) { e.localSignature = 0x12345678; }) << u"entry's header has the wrong signature"_s;
        QTest::newRow("a signature wrong in the directory")
            << entry([](RawEntry &e) { e.centralSignature = 0x12345678; })
            << u"directory entry has the wrong signature"_s;
        QTest::newRow("a directory name running past the directory")
            << entry([](RawEntry &e) { e.centralNameLength = 0x7000; }) << u"runs past the directory"_s;
        QTest::newRow("a name running past the entries") << entry([](RawEntry &e) {
            e.data.clear();
            e.centralName = QByteArray(190, 'a');
            e.localNameLength = 190;
        }) << u"name runs past the entries"_s;
        RawEntry ghost;
        ghost.name = "b.png";
        ghost.data = kPng;
        ghost.centralOnly = true;
        QTest::newRow("a directory entry with nothing in the entries")
            << forgeArchive({ plain, ghost }) << u"header runs past the entries"_s;

        // The end record.
        QTest::newRow("bytes before the first entry")
            << shaped([](ArchiveShape &s) { s.leader = "leading"; }) << u"overlap"_s;
        QTest::newRow("bytes after the end")
            << shaped([](ArchiveShape &s) { s.trailer = "trailing"; }) << u"does not finish with an end record"_s;
        QTest::newRow("a signature wrong on the end record")
            << shaped([](ArchiveShape &s) { s.endSignature = 0x12345678; }) << u"does not finish with an end record"_s;
        QTest::newRow("says it has a comment")
            << shaped([](ArchiveShape &s) { s.commentLength = 5; }) << u"says it has a comment"_s;
        QTest::newRow("on another disk") << shaped([](ArchiveShape &s) { s.disk = 1; }) << u"spans disks"_s;
        QTest::newRow("its directory on another disk")
            << shaped([](ArchiveShape &s) { s.centralDisk = 1; }) << u"spans disks"_s;
        QTest::newRow("its two counts differ")
            << shaped([](ArchiveShape &s) { s.thisDiskCount = 2; }) << u"two counts of entries disagree"_s;
        QTest::newRow("a count that is too high")
            << shaped([](ArchiveShape &s) { s.count = 2; }) << u"finishes before its count"_s;
        QTest::newRow("a count that is too low")
            << shaped([](ArchiveShape &s) { s.count = 0; }) << u"holds more than its count"_s;
        QTest::newRow("a directory of the wrong size")
            << shaped([](ArchiveShape &s) { s.centralSize = 1; }) << u"does not finish where the end record starts"_s;
        QTest::newRow("a directory in the wrong place")
            << shaped([](ArchiveShape &s) { s.centralStart = 1; }) << u"does not finish where the end record starts"_s;

        QTest::newRow("no entries, no manifest") << forgeArchive({}) << u"no signed manifest"_s;
        QTest::newRow("not an archive, and short") << QByteArray("not a zip") << u"too short"_s;
        QTest::newRow("not an archive, and long") << QByteArray(4096, 'x') << u"does not finish with an end record"_s;
        QTest::newRow("nothing") << QByteArray() << u"too short"_s;
    }
    void refusesOtherShapes()
    {
        QFETCH(QByteArray, archive);
        QFETCH(QString, reason);
        const TestSigner developer;
        const PackageResult opened = openPackage(archive, developer.publicKey());
        QVERIFY(!opened.ok);
        QVERIFY2(opened.error.contains(reason), qPrintable(opened.error));
    }

    // Nothing here is a proof by itself. Under the sanitizers, a reader that reaches outside the buffer stops the run.
    void survivesDamage()
    {
        const TestSigner developer;
        const Pack pack;
        const QByteArray package = pack.package(developer);
        const PackageResult original = openPackage(package, developer.publicKey());
        QVERIFY2(original.ok, qPrintable(original.error));

        for (qsizetype length = 0; length < package.size(); ++length)
            QVERIFY(!openPackage(package.left(length), developer.publicKey()).ok);

        QRandomGenerator random(0x51f15e);
        for (int round = 0; round < 3000; ++round) {
            QByteArray damaged = package;
            const int changes = 1 + random.bounded(4);
            for (int i = 0; i < changes; ++i) {
                // Half the time in the tables at the end, where the offsets and sizes are.
                const qsizetype from = random.bounded(2) ? 0 : package.size() - 200;
                const qsizetype at = from + random.bounded(package.size() - from);
                damaged[at] = char(random.bounded(256));
            }
            const PackageResult opened = openPackage(damaged, developer.publicKey());
            // What opens anyway is what was signed: only fields that say nothing about the content can change.
            if (opened.ok)
                QCOMPARE(opened.contents, original.contents);
        }
    }

    // ── Packages: what an asset pack may hold ────────────────────────────────

    void aPackRefuses_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<QString>("reason");

        // A QML file copied into a project's logic/ runs.
        QTest::newRow("qml") << u"logic/rules.qml"_s << QByteArray("import QQ\nLogic {}\n") << u"may not hold"_s;
        QTest::newRow("qml, in capitals")
            << u"logic/RULES.QML"_s << QByteArray("import QQ\nLogic {}\n") << u"may not hold"_s;
        QTest::newRow("qmldir") << u"qmldir"_s << QByteArray("plugin evil\n") << u"may not hold"_s;
        QTest::newRow("compiled qml") << u"a.qmlc"_s << QByteArray("x") << u"may not hold"_s;
        QTest::newRow("javascript") << u"a.js"_s << QByteArray("x") << u"may not hold"_s;
        QTest::newRow("a module") << u"a.mjs"_s << QByteArray("x") << u"may not hold"_s;
        QTest::newRow("a shader") << u"a.frag"_s << QByteArray("x") << u"may not hold"_s;
        QTest::newRow("a compiled shader") << u"a.qsb"_s << QByteArray("x") << u"may not hold"_s;
        QTest::newRow("svg, which can hold script") << u"a.svg"_s << QByteArray("<svg/>") << u"may not hold"_s;
        QTest::newRow("a dll") << u"a.dll"_s << QByteArray("MZ") << u"may not hold"_s;
        QTest::newRow("a shared object") << u"liba.so"_s
                                         << QByteArray("\x7f"
                                                       "ELF")
                                         << u"may not hold"_s;
        QTest::newRow("a program") << u"a.exe"_s << QByteArray("MZ") << u"may not hold"_s;
        QTest::newRow("a shell script") << u"run.sh"_s << QByteArray("#!/bin/sh\n") << u"may not hold"_s;
        QTest::newRow("a Python script") << u"run.py"_s << QByteArray("print()\n") << u"may not hold"_s;
        QTest::newRow("a PowerShell script") << u"run.ps1"_s << QByteArray("x") << u"may not hold"_s;
        QTest::newRow("a batch file") << u"run.bat"_s << QByteArray("x") << u"may not hold"_s;
        QTest::newRow("a file of no known kind") << u"mystery.xyz"_s << QByteArray("x") << u"may not hold"_s;
        QTest::newRow("a file with no extension") << u"mystery"_s << QByteArray("x") << u"may not hold"_s;
        QTest::newRow("a level, which is not on the list") << u"level.tmj"_s << QByteArray("{}") << u"may not hold"_s;
        QTest::newRow("json, which is not on the list") << u"a.json"_s << QByteArray("{}") << u"may not hold"_s;

        // A name is not enough.
        for (const char *name : { "a.jpg", "a.jpeg", "a.webp", "a.ogg", "a.flac", "a.mp3", "a.ktx", "a.ktx2" })
            QTest::addRow("%s, of other bytes", name)
                << QString::fromLatin1(name) << QByteArray("MZ\x90") << u"not what its name says"_s;
        QTest::newRow("a RIFF that is no picture")
            << u"a.webp"_s << QByteArray("RIFF....WAVE") << u"not what its name says"_s;
        QTest::newRow("a RIFF cut short") << u"a.wav"_s << QByteArray("RIFF") << u"not what its name says"_s;
        QTest::newRow("a C1 control in text") << u"a.txt"_s << QByteArray("a\xc2\x85z") << u"not plain text"_s;
        QTest::newRow("a dll named as a picture")
            << u"art/a.png"_s << QByteArray("MZ\x90") << u"not what its name says"_s;
        QTest::newRow("QML named as a sound") << u"a.wav"_s << QByteArray("import QQ\n") << u"not what its name says"_s;
        QTest::newRow("a script named as a picture")
            << u"a.hdr"_s << QByteArray("#!/bin/sh\n") << u"not what its name says"_s;
        QTest::newRow("an empty picture") << u"a.png"_s << QByteArray() << u"not what its name says"_s;
        QTest::newRow("a buffer no model names") << u"a.bin"_s << QByteArray("data") << u"not named by a model"_s;
        QTest::newRow("binary named as text") << u"a.txt"_s << QByteArray("MZ\x90\x00", 4) << u"not plain text"_s;
        QTest::newRow("a script named as text") << u"a.txt"_s << QByteArray("#!/bin/sh\n") << u"not plain text"_s;
        QTest::newRow("text that is not UTF-8") << u"a.txt"_s << QByteArray("caf\xe9\n") << u"not plain text"_s;
        QTest::newRow("a character cut off at the end") << u"a.txt"_s << QByteArray("caf\xc3") << u"not plain text"_s;
    }
    void aPackRefuses()
    {
        QFETCH(QString, path);
        QFETCH(QByteArray, data);
        QFETCH(QString, reason);
        const TestSigner developer;
        Pack pack;
        pack.files.insert(path, data);
        const PackageResult opened = openPackage(pack.package(developer), developer.publicKey());
        QVERIFY(!opened.ok);
        QVERIFY2(opened.error.contains(reason), qPrintable(opened.error));
    }

    void aPackTakesWhatIsPassive()
    {
        const TestSigner developer;
        Pack pack;
        pack.files.insert(u"a.png"_s, kPng);
        pack.files.insert(u"a.jpg"_s, kJpg);
        pack.files.insert(u"a.jpeg"_s, kJpg);
        pack.files.insert(u"a.webp"_s, kWebp);
        pack.files.insert(u"a.hdr"_s, kHdr);
        pack.files.insert(u"a.wav"_s, kWav);
        pack.files.insert(u"a.ogg"_s, kOgg);
        pack.files.insert(u"a.flac"_s, kFlac);
        pack.files.insert(u"a.mp3"_s, kMp3);
        pack.files.insert(u"a.ktx"_s, QByteArray::fromHex("ab4b5458203131bb0d0a1a0a"));
        pack.files.insert(u"a.ktx2"_s, QByteArray::fromHex("ab4b5458203230bb0d0a1a0a"));
        pack.files.insert(u"b.mp3"_s, QByteArray::fromHex("fffb9064"));
        pack.files.insert(u"NOTICE.txt"_s, "A notice.\n");
        pack.files.insert(u"m/model.gltf"_s, kOkModel);
        pack.files.insert(u"m/model.bin"_s, "data");
        pack.files.insert(u"b.glb"_s, glbHolding("{\"asset\":{\"version\":\"2.0\"}}"));
        const PackageResult opened = openPackage(pack.package(developer), developer.publicKey());
        QVERIFY2(opened.ok, qPrintable(opened.error));
        QCOMPARE(opened.contents.value(u"m/model.bin"_s), QByteArray("data"));
    }

    void aModelNamesOnlyWhatIsBesideIt_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<QByteArray>("model");
        QTest::addColumn<QString>("reason");
        const auto gltf = [](const char *uri) -> QByteArray {
            return QByteArray("{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"uri\":\"") + uri + "\"}]}";
        };
        QTest::newRow("climbing") << u"m/a.gltf"_s << gltf("../../../secret.bin") << u"outside the package"_s;
        QTest::newRow("climbing, with the dots encoded")
            << u"m/a.gltf"_s << gltf("%2e%2e/secret.bin") << u"outside the package"_s;
        QTest::newRow("absolute") << u"m/a.gltf"_s << gltf("/etc/passwd") << u"outside the package"_s;
        QTest::newRow("a drive") << u"m/a.gltf"_s << gltf("C:/secret.bin") << u"outside the package"_s;
        QTest::newRow("a file URL") << u"m/a.gltf"_s << gltf("file:///etc/passwd") << u"outside the package"_s;
        QTest::newRow("the network") << u"m/a.gltf"_s << gltf("https://example.org/a.bin") << u"outside the package"_s;
        QTest::newRow("a file that is not there") << u"m/a.gltf"_s << gltf("missing.bin") << u"outside the package"_s;
        QTest::newRow("data of another kind")
            << u"m/a.gltf"_s << gltf("data:text/html,x") << u"of a kind a model may not"_s;
        QTest::newRow("an image climbing")
            << u"m/a.gltf"_s << QByteArray("{\"asset\":{},\"images\":[{\"uri\":\"../secret.png\"}]}")
            << u"outside the package"_s;
        QTest::newRow("deep in an extension")
            << u"m/a.gltf"_s
            << QByteArray("{\"asset\":{},\"extensions\":{\"X_y\":{\"things\":[{\"uri\":\"../secret.bin\"}]}}}")
            << u"outside the package"_s;
        QTest::newRow("climbing, in a glb")
            << u"m/a.glb"_s << glbHolding("{\"asset\":{},\"buffers\":[{\"uri\":\"../secret.bin\"}]}")
            << u"outside the package"_s;
        QTest::newRow("a picture of a kind a model may not carry")
            << u"m/a.gltf"_s << gltf("data:image/svg+xml;base64,AA==") << u"of a kind a model may not"_s;
        QTest::newRow("a script") << u"m/a.gltf"_s << gltf("data:application/javascript;base64,AA==")
                                  << u"of a kind a model may not"_s;
        QTest::newRow("data of a good kind, not in base64")
            << u"m/a.gltf"_s << gltf("data:image/png,x") << u"of a kind a model may not"_s;
        QTest::newRow("a uri named twice, the first climbing")
            << u"m/a.gltf"_s << QByteArray("{\"asset\":{},\"images\":[{\"uri\":\"../../x\",\"uri\":\"secret.bin\"}]}")
            << u"names one thing twice"_s;
        QTest::newRow("a uri named twice, one with its letters escaped")
            << u"m/a.gltf"_s
            << QByteArray("{\"asset\":{},\"images\":[{\"\\u0075ri\":\"../../x\",\"uri\":\"secret.bin\"}]}")
            << u"names one thing twice"_s;
        QTest::newRow("a uri that is no text")
            << u"m/a.gltf"_s << QByteArray("{\"asset\":{},\"images\":[{\"uri\":7}]}") << u"other than text"_s;
        QTest::newRow("not JSON") << u"m/a.gltf"_s << QByteArray("not a model") << u"not a model"_s;
        QTest::newRow("JSON, not a model") << u"m/a.gltf"_s << QByteArray("{}") << u"not a model"_s;
        QTest::newRow("a glb cut short") << u"m/a.glb"_s << QByteArray("glTF") << u"not a model"_s;
        QByteArray longer = glbHolding("{\"asset\":{}}");
        longer[12] = char(0x7f);
        QTest::newRow("a glb whose chunk runs past its end") << u"m/a.glb"_s << longer << u"not a model"_s;
        QByteArray wrongLength = glbHolding("{\"asset\":{}}");
        wrongLength[8] = char(0x7f);
        QTest::newRow("a glb that gives the wrong length") << u"m/a.glb"_s << wrongLength << u"not a model"_s;
    }
    void aModelNamesOnlyWhatIsBesideIt()
    {
        QFETCH(QString, path);
        QFETCH(QByteArray, model);
        QFETCH(QString, reason);
        const TestSigner developer;
        Pack pack;
        pack.files.insert(path, model);
        pack.files.insert(u"secret.bin"_s, "a buffer it might have named"); // beside the climb, and in the package
        const PackageResult opened = openPackage(pack.package(developer), developer.publicKey());
        QVERIFY(!opened.ok);
        QVERIFY2(opened.error.contains(reason), qPrintable(opened.error));
    }

    void aModelMayCarryPicturesAndBuffers_data()
    {
        QTest::addColumn<QString>("uri");
        for (const QString &uri :
             { u"data:application/octet-stream;base64,AA=="_s, u"data:application/gltf-buffer;base64,AA=="_s,
               u"data:image/png;base64,AA=="_s, u"data:image/jpeg;base64,AA=="_s, u"data:image/webp;base64,AA=="_s,
               u"data:image/ktx2;base64,AA=="_s })
            QTest::addRow("%s", qPrintable(uri)) << uri;
    }
    void aModelMayCarryPicturesAndBuffers()
    {
        QFETCH(QString, uri);
        const TestSigner developer;
        Pack pack;
        pack.files.insert(
            u"a.gltf"_s,
            QJsonDocument(QJsonObject { { u"asset"_s, QJsonObject {} },
                                        { u"images"_s, QJsonArray { QJsonObject { { u"uri"_s, uri } } } } })
                .toJson());
        const PackageResult opened = openPackage(pack.package(developer), developer.publicKey());
        QVERIFY2(opened.ok, qPrintable(opened.error));
    }

    void aModelMayCarryItsDataOrNameItsNeighbour()
    {
        const TestSigner developer;
        Pack pack;
        pack.files.insert(u"a.gltf"_s, "{\"asset\":{},\"buffers\":[{\"uri\":\"data:application/"
                                       "octet-stream;base64,AA==\"}],\"images\":[{\"uri\":\"art/hero.png\"}]}");
        const PackageResult opened = openPackage(pack.package(developer), developer.publicKey());
        QVERIFY2(opened.ok, qPrintable(opened.error));
    }

    void aBufferThatIsAProgramIsRefused_data()
    {
        QTest::addColumn<QByteArray>("data");
        QTest::newRow("Windows") << QByteArray("MZ\x90");
        QTest::newRow("Linux") << QByteArray("\x7f"
                                             "ELF");
        QTest::newRow("a script") << QByteArray("#!/bin/sh");
        QTest::newRow("an archive") << QByteArray("PK\x03\x04");
    }
    void aBufferThatIsAProgramIsRefused()
    {
        QFETCH(QByteArray, data);
        const TestSigner developer;
        Pack pack;
        pack.files.insert(u"model.gltf"_s, kOkModel);
        pack.files.insert(u"model.bin"_s, data);
        const PackageResult opened = openPackage(pack.package(developer), developer.publicKey());
        QVERIFY(!opened.ok);
        QVERIFY2(opened.error.contains(u"is a program"_s), qPrintable(opened.error));
    }

    // ── Packages: licences ───────────────────────────────────────────────────

    void aLicenceHoldsNoMarkup_data()
    {
        QTest::addColumn<QByteArray>("text");
        // A Qt Text guesses at rich text, and rich text fetches pictures.
        QTest::newRow("a picture") << QByteArray("Free.\n<img src=\"https://example.org/seen.png\">\n");
        QTest::newRow("a picture, in capitals") << QByteArray("<IMG SRC=x>");
        QTest::newRow("a link") << QByteArray("Free, <a href=\"https://example.org\">honest</a>.");
        QTest::newRow("a page") << QByteArray("<html><body>Free.</body></html>");
        QTest::newRow("Qt's own tag") << QByteArray("<qt>Free.</qt>");
        QTest::newRow("a closing tag alone") << QByteArray("Free.</b>");
        QTest::newRow("a comment") << QByteArray("Free.<!-- hidden -->");
        QTest::newRow("a doctype") << QByteArray("<!DOCTYPE html>");
        // The review's: what a pattern with a list of names missed, and Qt or a browser would still act on.
        QTest::newRow("a quoted < in an attribute")
            << QByteArray("<img title=\"<\" src=\"https://example.org/seen.png\">");
        QTest::newRow("U+00A0 after the name") << QByteArray("<img\xc2\xa0src=\"https://example.org/seen.png\">");
        QTest::newRow("U+2003 before the name") << QByteArray("<\xe2\x80\x83img src=x>");
        QTest::newRow("an unterminated tag on a later line")
            << QByteArray("A line.\nlater <img src=\"https://example.org/seen.png\"");
        QTest::newRow("&lt; on the first line, which turns rich text on") << QByteArray("&lt; and no tag at all\n");
        QTest::newRow("<caption>, in Qt's table") << QByteArray("<caption>x</caption>");
        QTest::newRow("svg with no space") << QByteArray("<svg/onload=alert(1)>");
        QTest::newRow("img with no space") << QByteArray("<img/src=x onerror=alert(1)>");
        QTest::newRow("details, for browsers") << QByteArray("<details ontoggle=alert(1)>");
        QTest::newRow("a browser's name, in capitals") << QByteArray("<SVG/onload=alert(1)>");
        QTest::newRow("a browser's name, in mixed case") << QByteArray("<DeTaIlS ontoggle=alert(1)>");
        QTest::newRow("a browser's name, then U+00A0") << QByteArray("<svg\xc2\xa0onload=alert(1)>");
        QTest::newRow("a name in quotes, as Qt's parser reads one") << QByteArray("<\"img\" src=x>");
        QTest::newRow("a name with an entity, as Qt's parser reads one") << QByteArray("<&#105;mg src=x>");
        QTest::newRow("a name in the Kelvin sign, which lower-cases to k") << QByteArray("<\xe2\x84\xaa"
                                                                                         "bd>x");
        QTest::newRow("a space after the bracket, as Qt's parser reads it") << QByteArray("A line.\n< b >x");
        QTest::newRow("spaces around a closing slash") << QByteArray("A line.\n< / b >");
        QTest::newRow("an XML declaration") << QByteArray("<?xml version=\"1.0\"?>");
        // The recheck's: a short list of names is no wall. A tag that carries a value is markup, whatever its name.
        QTest::newRow("a style on a tag Qt does not know")
            << QByteArray("<x style=\"background-image:url(https://example.org/seen.png)\">y</x>");
        QTest::newRow("image, which a browser takes for img")
            << QByteArray("<image src=\"https://example.org/seen.png\">");
        QTest::newRow("an event handler on a tag nobody knows") << QByteArray("<x onmouseover=\"alert(1)\">y</x>");
        QTest::newRow("a value straight after the name") << QByteArray("<x/style=color:red>y");
        QTest::newRow("a value, and nothing to end the tag") << QByteArray("y <x style=color:red");
        QTest::newRow("a value after a name longer than any element's")
            << QByteArray("<abcdefghijklmnopqrstuvwxyz style=color:red>y");
        QTest::newRow("a value after plain words") << QByteArray("<one line to give style=color:red>");
        QTest::newRow("a value after a stray <= before it") << QByteArray("a <= b, then <x style=color:red>y");
        // Qt reads a quoted word in a tag as one word, `>` and all, so the tag goes on past it.
        QTest::newRow("a quoted key holding >, then a value")
            << QByteArray("<x \">\" style=\"background-image:url(https://example.org/seen.png)\">y");
        QTest::newRow("a single-quoted key holding >, then a value") << QByteArray("<x '>' style=color:red>y");
        QTest::newRow("a single-quoted key a backslash keeps open, then a value")
            << QByteArray("<x 'a\\'>' style=color:red>y");
        // With no value to give it away, the name alone, read as Qt's parser reads it.
        QTest::newRow("a browser's name, with no value") << QByteArray("<plaintext>y");
        QTest::newRow("a browser's name, with no value, in capitals") << QByteArray("<PLAINTEXT>y");
        QTest::newRow("a browser's name, with no value, then U+00A0") << QByteArray("<plaintext\xc2\xa0y>");
        QTest::newRow("a browser's name, with no value, then a slash") << QByteArray("<plaintext/y>");
        QTest::newRow("a name in quotes, with no value") << QByteArray("<\"b\">bold");
        QTest::newRow("a name with an entity, and no value") << QByteArray("<&#98;>bold");
    }
    void aLicenceHoldsNoMarkup()
    {
        QFETCH(QByteArray, text);
        const TestSigner developer;
        // Where it stands, and on a later line, where Qt::mightBeRichText does not look and the scan has to find it.
        // Only what is about the first line is not markup further down.
        const bool firstLineOnly = QByteArray(QTest::currentDataTag()).contains("on the first line");
        for (const QByteArray &prefix : { QByteArray(), QByteArray("A line of text.\n") }) {
            if (firstLineOnly && !prefix.isEmpty())
                continue;
            Pack pack;
            pack.files.insert(u"LICENSE.txt"_s, QByteArray(prefix + text));
            const PackageResult opened = openPackage(pack.package(developer), developer.publicKey());
            QVERIFY2(!opened.ok, prefix.constData());
            QVERIFY2(opened.error.contains(u"markup"_s), qPrintable(opened.error));
        }
    }

    void aLicenceMayHoldAngleBracketsAsWords()
    {
        const TestSigner developer;
        Pack pack;
        pack.files.insert(u"LICENSE.txt"_s,
                          "<one line to give the program's name and a brief idea of what it does.>\n"
                          "Copyright (C) <year> <name of author>, <https://www.gnu.org/licenses/>.\n\f\n1 < 2 > 1\n"
                          "<signature of Ty Coon>, a <- c, <> and <=, and the odd stray < at the end <\n"
                          "Copyright (c) <YEAR>, <OWNER>. Write to <someone@example.org> or <https://example.org/x>.\n"
                          "a = b, then <copyright holders> and <organization>, and c = d");
        const PackageResult opened = openPackage(pack.package(developer), developer.publicKey());
        QVERIFY2(opened.ok, qPrintable(opened.error));
    }

    // The licences people use, as their stewards publish them (SPDX's texts, as Qt carries them): none is markup.
    void aRealLicenceIsNotMarkup_data()
    {
        QTest::addColumn<QString>("file");
        const QStringList names { u"GPL-2.0-only"_s,  u"GPL-3.0-only"_s, u"LGPL-2.1-or-later"_s,
                                  u"LGPL-3.0-only"_s, u"MPL-2.0"_s,      u"Apache-2.0"_s,
                                  u"MIT"_s,           u"BSD-2-Clause"_s, u"BSD-3-Clause"_s };
        for (const QString &name : names)
            QTest::newRow(qPrintable(name)) << QString(kLicences + u'/' + name + u".txt"_s);
    }
    void aRealLicenceIsNotMarkup()
    {
        QFETCH(QString, file);
        const QByteArray text = readFile(file);
        QVERIFY2(text.size() > 500, qPrintable(file));
        QVERIFY(text.contains('<') || !QByteArray(QTest::currentDataTag()).contains("GPL"));
        const TestSigner developer;
        Pack pack;
        pack.files.insert(u"LICENSE.txt"_s, text);
        const PackageResult opened = openPackage(pack.package(developer), developer.publicKey());
        QVERIFY2(opened.ok, qPrintable(opened.error));
    }

    // At the limit, whatever the text is, in one pass. The pattern this replaced took quadratic time in `<` and
    // whitespace, on the interface's thread: days at this size, and then reporting no markup when its limit fired.
    void hostileTextIsReadInOnePass_data()
    {
        QTest::addColumn<QByteArray>("head");
        QTest::addColumn<QByteArray>("unit");
        QTest::addColumn<QByteArray>("tail");
        QTest::addColumn<bool>("markup");
        QTest::newRow("< then spaces") << QByteArray("<") << QByteArray(" ") << QByteArray("x") << false;
        QTest::newRow("< then spaces, then a tag")
            << QByteArray("<") << QByteArray(" ") << QByteArray("<img src=x>") << true;
        QTest::newRow("< then every kind of whitespace")
            << QByteArray("<") << QByteArray("\t\n\r\f ") << QByteArray("x") << false;
        QTest::newRow("nothing but <") << QByteArray() << QByteArray("<") << QByteArray() << false;
        QTest::newRow("< and a space, over and over") << QByteArray() << QByteArray("< ") << QByteArray() << false;
        QTest::newRow("a word Qt must be asked about, over and over")
            << QByteArray() << QByteArray("<x ") << QByteArray() << false;
        QTest::newRow("a closing slash, over and over") << QByteArray() << QByteArray("</") << QByteArray() << false;
        QTest::newRow("a quote, over and over") << QByteArray() << QByteArray("<\"") << QByteArray() << false;
        QTest::newRow("one long name") << QByteArray("<") << QByteArray("a") << QByteArray(">") << false;
        QTest::newRow("no close on the first line") << QByteArray("x <") << QByteArray("y") << QByteArray() << false;
        QTest::newRow("the tag at the very end") << QByteArray() << QByteArray("x") << QByteArray("<b>") << true;
        QTest::newRow("a quoted key left open, over and over")
            << QByteArray() << QByteArray("<x \"") << QByteArray() << false;
        QTest::newRow("a quoted key holding >, over and over")
            << QByteArray() << QByteArray("<x '>' ") << QByteArray() << false;
        QTest::newRow("a name over and over, and a value at the very end")
            << QByteArray() << QByteArray("<x ") << QByteArray("=") << true;
        QTest::newRow("a value first, then a name over and over")
            << QByteArray("=") << QByteArray("<x ") << QByteArray() << false;
        QTest::newRow("a name and an end, over and over, and a value at the very end")
            << QByteArray() << QByteArray("<x>") << QByteArray("<x =") << true;
    }
    void hostileTextIsReadInOnePass()
    {
        QFETCH(QByteArray, head);
        QFETCH(QByteArray, unit);
        QFETCH(QByteArray, tail);
        QFETCH(bool, markup);
        const TestSigner developer;
        Pack pack;
        // Room for the manifest, its signature and the licence inside the limit.
        const qsizetype room = kMaximumPackageContent - 8 * 1024;
        QByteArray text = head;
        text.reserve(room);
        text += unit.repeated((room - head.size() - tail.size()) / unit.size());
        text += tail;
        pack.files.insert(u"NOTES.txt"_s, text);
        const QByteArray package = pack.package(developer);

        QElapsedTimer timer;
        timer.start();
        const PackageResult opened = openPackage(package, developer.publicKey());
        const qint64 elapsed = timer.elapsed();
        qInfo("%s: %lld bytes in %lld ms", QTest::currentDataTag(), qint64(text.size()), elapsed);
        QVERIFY2(elapsed < 60000, QByteArray::number(elapsed).constData());
        QCOMPARE(opened.ok, !markup);
        if (markup)
            QVERIFY2(opened.error.contains(u"markup"_s), qPrintable(opened.error));
        else
            QVERIFY2(opened.ok, qPrintable(opened.error));
    }

    // What else in openPackage is paid for on the interface's thread, at the limit.
    void hostileJsonIsReadInBoundedTime_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QByteArray>("head");
        QTest::addColumn<QByteArray>("unit");
        QTest::addColumn<QByteArray>("tail");
        QTest::newRow("a model holding a long array")
            << u"a.gltf"_s << u"asset-pack"_s << QByteArray("{\"asset\":{},\"x\":[0") << QByteArray(",0")
            << QByteArray("]}");
        QTest::newRow("a model holding many objects")
            << u"a.gltf"_s << u"asset-pack"_s << QByteArray("{\"asset\":{},\"x\":[{}") << QByteArray(",{}")
            << QByteArray("]}");
        QTest::newRow("a game's level holding a long array")
            << u"level.tmj"_s << u"game"_s << QByteArray("[0") << QByteArray(",0") << QByteArray("]");
    }
    void hostileJsonIsReadInBoundedTime()
    {
        QFETCH(QString, name);
        QFETCH(QString, kind);
        QFETCH(QByteArray, head);
        QFETCH(QByteArray, unit);
        QFETCH(QByteArray, tail);
        const TestSigner developer;
        Pack pack = kind == u"game"_s ? gamePack() : Pack();
        const qsizetype room = kMaximumPackageContent - 8 * 1024;
        QByteArray text = head;
        text.reserve(room);
        text += unit.repeated((room - head.size() - tail.size()) / unit.size());
        text += tail;
        pack.files.insert(name, text);
        const QByteArray package = pack.package(developer);
        QElapsedTimer timer;
        timer.start();
        const PackageResult opened = openPackage(package, developer.publicKey());
        const qint64 elapsed = timer.elapsed();
        qInfo("%s: %lld bytes in %lld ms, ok=%d: %s", QTest::currentDataTag(), qint64(text.size()), elapsed,
              int(opened.ok), qPrintable(opened.error));
        QVERIFY2(elapsed < 60000, QByteArray::number(elapsed).constData());
    }

    // The other thing an install pays for on the interface's thread: each of the most files a package may hold, written
    // safely.
    void manyFilesAreInstalledInBoundedTime()
    {
        Pack pack;
        for (int i = 0; i < 4090; ++i)
            pack.files.insert(u"art/a%1/b%2.png"_s.arg(i % 64).arg(i), kPng);
        Shop shop;
        shop.add(pack);
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QElapsedTimer timer;
        timer.start();
        const Outcome outcome = installAndWait(store, pack.id);
        const qint64 installing = timer.elapsed();
        timer.restart();
        QVERIFY2(outcome.ok, qPrintable(outcome.error));
        QVERIFY(store.isIntact(pack.id));
        qInfo("%d files: %lld ms to install, %lld ms to check", int(pack.files.size()), installing,
              qint64(timer.elapsed()));
        QVERIFY2(installing < 120000, QByteArray::number(installing).constData());
    }

    void aPackageMustHoldItsLicence()
    {
        const TestSigner developer;
        Pack pack;
        pack.files.remove(u"LICENSE.txt"_s);
        const PackageResult opened = openPackage(pack.package(developer), developer.publicKey());
        QVERIFY(!opened.ok);
        QVERIFY2(opened.error.contains(u"licence"_s), qPrintable(opened.error));
    }

    // ── Packages: what a game may hold ───────────────────────────────────────

    void aGameRefuses_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<QByteArray>("data");
        QTest::newRow("a dll") << u"a.dll"_s << QByteArray("MZ");
        QTest::newRow("a shared object") << u"liba.so"_s
                                         << QByteArray("\x7f"
                                                       "ELF");
        QTest::newRow("a program") << u"a.exe"_s << QByteArray("MZ");
        QTest::newRow("a shell script") << u"run.sh"_s << QByteArray("#!/bin/sh\n");
        QTest::newRow("the script that made its art") << u"make_art.py"_s << QByteArray("print()\n");
        QTest::newRow("a qmldir, which can name a plugin") << u"qmldir"_s << QByteArray("plugin evil\n");
        QTest::newRow("compiled QML") << u"a.qmlc"_s << QByteArray("x");
        QTest::newRow("a file of no known kind") << u"mystery.xyz"_s << QByteArray("x");
    }
    void aGameRefuses()
    {
        QFETCH(QString, path);
        QFETCH(QByteArray, data);
        const TestSigner developer;
        Pack game = gamePack();
        game.files.insert(path, data);
        const PackageResult opened = openPackage(game.package(developer), developer.publicKey());
        QVERIFY(!opened.ok);
        QVERIFY2(opened.error.contains(u"kind game may not hold"_s), qPrintable(opened.error));
    }

    void aGamesTextAndLevelsAreWhatTheySay_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<QString>("reason");
        QTest::newRow("QML with a NUL") << u"a.qml"_s << QByteArray("import QQ\0", 10) << u"not plain text"_s;
        QTest::newRow("QML that is not UTF-8") << u"a.qml"_s << QByteArray("caf\xe9") << u"not plain text"_s;
        QTest::newRow("a module that starts a program")
            << u"a.mjs"_s << QByteArray("#!/bin/sh\n") << u"not plain text"_s;
        QTest::newRow("JavaScript that is binary") << u"a.js"_s << QByteArray("MZ\x90") << u"not plain text"_s;
        QTest::newRow("a level that is not JSON") << u"a.tmj"_s << QByteArray("not json") << u"is not JSON"_s;
        QTest::newRow("a tileset that is not JSON") << u"a.tsj"_s << QByteArray("{") << u"is not JSON"_s;
        QTest::newRow("JSON that is not JSON") << u"a.json"_s << QByteArray("[1,") << u"is not JSON"_s;
    }
    void aGamesTextAndLevelsAreWhatTheySay()
    {
        QFETCH(QString, path);
        QFETCH(QByteArray, data);
        QFETCH(QString, reason);
        const TestSigner developer;
        Pack game = gamePack();
        game.files.insert(path, data);
        const PackageResult opened = openPackage(game.package(developer), developer.publicKey());
        QVERIFY(!opened.ok);
        QVERIFY2(opened.error.contains(reason), qPrintable(opened.error));
    }

    void aGameTakesItsScenesAndLogic()
    {
        const TestSigner developer;
        Pack game = gamePack();
        game.files.insert(u"scenes/a.qml"_s, "import QQ\nScene {}\n");
        game.files.insert(u"logic/rules.qml"_s, "import QQ\nLogic {}\n");
        game.files.insert(u"logic/rules.mjs"_s, "export const a = 1;\n");
        game.files.insert(u"assets/level.tmj"_s, "{}");
        game.files.insert(u"assets/tiles.tsj"_s, "{}");
        game.files.insert(u"assets/a.wav"_s, kWav);
        const PackageResult opened = openPackage(game.package(developer), developer.publicKey());
        QVERIFY2(opened.ok, qPrintable(opened.error));
    }

    // ── Catalogues: where a package may be ───────────────────────────────────

    void aPackageFileIsPlainAndRelative_data()
    {
        QTest::addColumn<QString>("file");
        for (const QString &file : { u"/etc/passwd"_s, u"C:/x"_s, u"c:\\x"_s, u"C:x"_s, u"\\\\server\\x"_s,
                                     u"//server/x"_s, u"https://example.org/x"_s, u"file:///etc/passwd"_s, u"../x"_s,
                                     u"a/../b"_s, u"a\\b"_s, u""_s, u"packages/CON"_s, u"packages/x."_s })
            QTest::addRow("%s", qPrintable(file.isEmpty() ? u"empty"_s : file)) << file;
    }
    void aPackageFileIsPlainAndRelative()
    {
        QFETCH(QString, file);
        // Format 1.
        const CatalogResult first = parseFormat1([&file](QJsonObject &item) {
            QJsonObject package = item.value(u"package"_s).toObject();
            package.insert(u"file"_s, file);
            item.insert(u"package"_s, package);
        });
        QVERIFY(!first.ok);
        QVERIFY2(first.error.contains(u"not valid"_s), qPrintable(first.error));

        // Format 2.
        const Pack pack;
        Shop shop;
        QJsonObject &listing = shop.add(pack);
        QJsonObject package = listing.value(u"package"_s).toObject();
        package.insert(u"file"_s, file);
        listing.insert(u"package"_s, package);
        const CatalogResult second = shop.parse();
        QVERIFY(!second.ok);
        QVERIFY2(second.error.contains(u"package file"_s), qPrintable(second.error));
    }

    // ── Catalogues: no price ─────────────────────────────────────────────────

    void formatTwoRefusesAnyPrice_data()
    {
        QTest::addColumn<QJsonValue>("price");
        QTest::newRow("zero") << QJsonValue(0);
        QTest::newRow("nonzero") << QJsonValue(9.99);
        QTest::newRow("text") << QJsonValue(u"0"_s);
        QTest::newRow("null") << QJsonValue(QJsonValue::Null);
        QTest::newRow("an object") << QJsonValue(QJsonObject {});
    }
    void formatTwoRefusesAnyPrice()
    {
        QFETCH(QJsonValue, price);
        const Pack pack;
        Shop shop;
        QJsonObject &listing = shop.add(pack);
        listing.insert(u"price"_s, price);
        const CatalogResult result = shop.parse();
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(u"refuses any price"_s), qPrintable(result.error));
    }

    // Being a draft is no way round it: a price refuses the catalogue on any listing, under any field.
    void formatTwoRefusesAPriceAnywhere_data()
    {
        QTest::addColumn<QJsonObject>("listing");
        const QJsonObject free { { u"type"_s, u"free"_s } };
        QTest::newRow("on a kind not taken yet")
            << QJsonObject { { u"id"_s, u"org.example.tool.one"_s }, { u"kind"_s, u"tool"_s }, { u"price"_s, 0 } };
        QTest::newRow("on a listing with no kind")
            << QJsonObject { { u"id"_s, u"org.example.bad.one"_s }, { u"price"_s, 1 } };
        QTest::newRow("in its entitlement")
            << QJsonObject { { u"id"_s, u"org.example.bad.one"_s },
                             { u"kind"_s, u"game"_s },
                             { u"entitlement"_s, QJsonObject { { u"type"_s, u"paid"_s }, { u"price"_s, 5 } } } };
        QTest::newRow("in a placement") << QJsonObject {
            { u"id"_s, u"org.example.bad.one"_s },
            { u"kind"_s, u"game"_s },
            { u"entitlement"_s, free },
            { u"placements"_s, QJsonArray { QJsonObject { { u"store"_s, u"player"_s }, { u"price"_s, 0 } } } }
        };
        QTest::newRow("in its package") << QJsonObject { { u"id"_s, u"org.example.bad.one"_s },
                                                         { u"kind"_s, u"game"_s },
                                                         { u"package"_s, QJsonObject { { u"price"_s, 0 } } } };
    }
    void formatTwoRefusesAPriceAnywhere()
    {
        QFETCH(QJsonObject, listing);
        Shop shop;
        shop.items.append(listing);
        const CatalogResult result = shop.parse();
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(u"refuses any price"_s), qPrintable(result.error));
    }

    void formatOneRefusesAPriceThatIsNotZero_data()
    {
        QTest::addColumn<QJsonValue>("price");
        QTest::newRow("nonzero") << QJsonValue(9.99);
        QTest::newRow("one") << QJsonValue(1);
        QTest::newRow("negative") << QJsonValue(-1);
        QTest::newRow("the smallest there is") << QJsonValue(1e-300);
        QTest::newRow("text that toDouble would read as nothing") << QJsonValue(u"9.99"_s);
        QTest::newRow("null") << QJsonValue(QJsonValue::Null);
        QTest::newRow("true") << QJsonValue(true);
    }
    void formatOneRefusesAPriceThatIsNotZero()
    {
        QFETCH(QJsonValue, price);
        const CatalogResult result = parseFormat1([&price](QJsonObject &item) { item.insert(u"price"_s, price); });
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(u"price"_s), qPrintable(result.error));
        QVERIFY(result.catalog.items.isEmpty());
    }

    void formatOneStillTakesZeroOrNoPrice()
    {
        const CatalogResult zero = parseFormat1([](QJsonObject &item) { item.insert(u"price"_s, 0); });
        QVERIFY2(zero.ok, qPrintable(zero.error));
        QCOMPARE(zero.catalog.items.constFirst().toVariant().value(u"price"_s).toInt(), 0);
        const CatalogResult absent = parseFormat1([](QJsonObject &) { });
        QVERIFY2(absent.ok, qPrintable(absent.error));
    }

    // ── Catalogues: kinds ────────────────────────────────────────────────────

    // What this stage does not take parses and stays a draft: it is in no view and nothing installs it, and the rest
    // of the catalogue stands.
    void aKindNotTakenYetStaysADraft_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QStringList>("stores");
        const QStringList player { u"player"_s };
        const QStringList creator { u"creator"_s };
        const QStringList both { u"player"_s, u"creator"_s };
        for (const QString &kind : { u"demo"_s, u"expansion"_s, u"soundtrack"_s, u"mod"_s })
            QTest::addRow("%s", qPrintable(kind)) << kind << player;
        for (const QString &kind : { u"template"_s, u"project"_s, u"plugin"_s, u"tool"_s })
            QTest::addRow("%s", qPrintable(kind)) << kind << creator;
        // These two may be placed in either or both, so none of these is a contradiction.
        for (const QString &kind : { u"utility"_s, u"creator-app"_s }) {
            QTest::addRow("%s, for players", qPrintable(kind)) << kind << player;
            QTest::addRow("%s, for creators", qPrintable(kind)) << kind << creator;
            QTest::addRow("%s, in both", qPrintable(kind)) << kind << both;
        }
    }
    void aKindNotTakenYetStaysADraft()
    {
        QFETCH(QString, kind);
        QFETCH(QStringList, stores);
        const Pack good;
        Shop shop;
        shop.add(good);
        const QString id = u"org.example.later.one"_s;
        QJsonArray placements;
        for (const QString &store : stores)
            placements.append(placementOf(store, u"Later"_s));
        shop.items.append(classifiedListing(id, kind, placements));

        const CatalogResult result = shop.parse();
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(idsOf(result.catalog.items), QStringList { good.id });
        QVERIFY(!result.catalog.item(id));
        QVERIFY(result.catalog.draft(id));
        QCOMPARE(result.catalog.draft(id)->code, u"kind-not-taken-yet"_s);

        // And the engine says so, rather than saying nothing.
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        for (const View view : { View::Player, View::Creator })
            QVERIFY(!idsOf(store.listings(view)).contains(id));
        const Outcome refused = installAndWait(store, id);
        QVERIFY(!refused.started);
        QVERIFY(!refused.ok);
        QVERIFY2(refused.error.contains(kind), qPrintable(refused.error));
        QVERIFY(store.installed().isEmpty());
        QVERIFY(!QFileInfo::exists(store.installDirectory(id)));
    }

    // What is unclassified or contradictory is kept out of every view and refused by install, with a reason a tool can
    // read, and the rest of the catalogue loads and installs.
    void whatIsUnclassifiedOrContradictoryIsADraft_data()
    {
        QTest::addColumn<QJsonObject>("set");
        QTest::addColumn<QStringList>("removed");
        QTest::addColumn<QString>("code");
        const auto placed = [](const QString &store, const QString &category) -> QJsonValue {
            return QJsonArray { placementOf(store, category) };
        };
        const auto set = [](const QString &field, const QJsonValue &value) { return QJsonObject { { field, value } }; };
        const QStringList none;

        QTest::newRow("no kind") << QJsonObject {} << QStringList { u"kind"_s } << u"unknown-kind"_s;
        QTest::newRow("a kind no store knows") << set(u"kind"_s, u"virus"_s) << none << u"unknown-kind"_s;
        QTest::newRow("a theme, which is format 1's") << set(u"kind"_s, u"theme"_s) << none << u"unknown-kind"_s;
        QTest::newRow("a widget, which is format 1's") << set(u"kind"_s, u"widget"_s) << none << u"unknown-kind"_s;
        QTest::newRow("a kind that is no text") << set(u"kind"_s, 7) << none << u"unknown-kind"_s;

        QTest::newRow("no placements") << QJsonObject {} << QStringList { u"placements"_s } << u"no-placements"_s;
        QTest::newRow("placements empty") << set(u"placements"_s, QJsonArray {}) << none << u"no-placements"_s;
        QTest::newRow("placements not a list") << set(u"placements"_s, u"creator"_s) << none << u"no-placements"_s;
        QTest::newRow("a placement of nothing") << set(u"placements"_s, QJsonArray { 1 }) << none << u"bad-placement"_s;
        QTest::newRow("a placement with more to it") << set(
            u"placements"_s,
            QJsonArray { QJsonObject { { u"store"_s, u"creator"_s }, { u"category"_s, u"Art"_s }, { u"rank"_s, 1 } } })
                                                     << none << u"bad-placement"_s;
        QTest::newRow("a store that does not exist")
            << set(u"placements"_s, placed(u"secret"_s, u"Art"_s)) << none << u"unknown-store"_s;
        QTest::newRow("no store") << set(u"placements"_s, QJsonArray { QJsonObject { { u"category"_s, u"Art"_s } } })
                                  << none << u"unknown-store"_s;
        QTest::newRow("an empty category")
            << set(u"placements"_s, placed(u"creator"_s, u""_s)) << none << u"empty-category"_s;
        QTest::newRow("a blank category")
            << set(u"placements"_s, placed(u"creator"_s, u"   "_s)) << none << u"empty-category"_s;
        QTest::newRow("no category") << set(u"placements"_s,
                                            QJsonArray { QJsonObject { { u"store"_s, u"creator"_s } } })
                                     << none << u"empty-category"_s;
        QTest::newRow("one category twice") << set(
            u"placements"_s, QJsonArray { placementOf(u"creator"_s, u"Art"_s), placementOf(u"creator"_s, u"Art"_s) })
                                            << none << u"duplicate-placement"_s;

        QTest::newRow("a creator's kind in the player store")
            << set(u"placements"_s, placed(u"player"_s, u"Art"_s)) << none << u"placement-contradicts-kind"_s;
        QTest::newRow("a creator's kind in both") << set(
            u"placements"_s, QJsonArray { placementOf(u"creator"_s, u"Art"_s), placementOf(u"player"_s, u"Art"_s) })
                                                  << none << u"placement-contradicts-kind"_s;
        QJsonObject gameForCreators = set(u"kind"_s, u"game"_s);
        gameForCreators.insert(u"placements"_s, placed(u"creator"_s, u"Games"_s));
        QTest::newRow("a game only in the creator store") << gameForCreators << none << u"placement-contradicts-kind"_s;
        QJsonObject gameInBoth = set(u"kind"_s, u"game"_s);
        gameInBoth.insert(u"placements"_s,
                          QJsonArray { placementOf(u"player"_s, u"Games"_s), placementOf(u"creator"_s, u"Games"_s) });
        QTest::newRow("a game in both") << gameInBoth << none << u"placement-contradicts-kind"_s;

        const QString notFree = u"entitlement-not-free"_s;
        QTest::newRow("no entitlement") << QJsonObject {} << QStringList { u"entitlement"_s } << notFree;
        QTest::newRow("a paid entitlement")
            << set(u"entitlement"_s, QJsonObject { { u"type"_s, u"paid"_s } }) << none << notFree;
        QTest::newRow("a subscription") << set(u"entitlement"_s, QJsonObject { { u"type"_s, u"subscription"_s } })
                                        << none << notFree;
        QTest::newRow("free, with more to it")
            << set(u"entitlement"_s, QJsonObject { { u"type"_s, u"free"_s }, { u"until"_s, u"2027-01-01"_s } }) << none
            << notFree;
        QTest::newRow("an entitlement that is only a word") << set(u"entitlement"_s, u"free"_s) << none << notFree;

        QTest::newRow("no platforms") << QJsonObject {} << QStringList { u"platforms"_s } << u"no-platforms"_s;
        QTest::newRow("platforms empty") << set(u"platforms"_s, QJsonArray {}) << none << u"no-platforms"_s;
        QTest::newRow("platforms not a list") << set(u"platforms"_s, u"linux-x86_64"_s) << none << u"no-platforms"_s;
        QTest::newRow("a platform of no name")
            << set(u"platforms"_s, QJsonArray { u""_s }) << none << u"no-platforms"_s;
        QTest::newRow("a platform that climbs")
            << set(u"platforms"_s, QJsonArray { u"../x"_s }) << none << u"no-platforms"_s;

        QTest::newRow("an executable pack") << set(u"executable"_s, true) << none << u"executable-not-taken"_s;
        QJsonObject executableGame = set(u"kind"_s, u"game"_s);
        executableGame.insert(u"placements"_s, placed(u"player"_s, u"Games"_s));
        executableGame.insert(u"executable"_s, true);
        QTest::newRow("an executable game") << executableGame << none << u"executable-not-taken"_s;
        QTest::newRow("not saying if it is executable")
            << QJsonObject {} << QStringList { u"executable"_s } << u"executable-not-said"_s;
        QTest::newRow("executable, in words") << set(u"executable"_s, u"no"_s) << none << u"executable-not-said"_s;

        const QString edition = u"edition-contradicts-kind"_s;
        const auto editionOf = [](const QString &group, const QString &role) {
            return QJsonObject { { u"group"_s, group }, { u"role"_s, role } };
        };
        QTest::newRow("a pack that says it is a game's edition")
            << set(u"edition"_s, editionOf(u"org.example.one"_s, u"game"_s)) << none << edition;
        QTest::newRow("a pack that says it is an editable project")
            << set(u"edition"_s, editionOf(u"org.example.one"_s, u"editable-project"_s)) << none << edition;
        QJsonObject gameAsProject = set(u"kind"_s, u"game"_s);
        gameAsProject.insert(u"placements"_s, placed(u"player"_s, u"Games"_s));
        gameAsProject.insert(u"edition"_s, editionOf(u"org.example.one"_s, u"editable-project"_s));
        QTest::newRow("a game that says it is an editable project") << gameAsProject << none << edition;
        QJsonObject unknownRole = gameAsProject;
        unknownRole.insert(u"edition"_s, editionOf(u"org.example.one"_s, u"collector"_s));
        QTest::newRow("a role no edition has") << unknownRole << none << edition;
        QJsonObject noGroup = gameAsProject;
        noGroup.insert(u"edition"_s, editionOf(u""_s, u"game"_s));
        QTest::newRow("an edition of no group") << noGroup << none << edition;
    }
    void whatIsUnclassifiedOrContradictoryIsADraft()
    {
        QFETCH(QJsonObject, set);
        QFETCH(QStringList, removed);
        QFETCH(QString, code);
        Pack good;
        good.id = u"org.example.pack.good"_s;
        Pack bad;
        bad.id = u"org.example.pack.bad"_s;
        Shop shop;
        shop.add(good);
        QJsonObject &listing = shop.add(bad);
        for (auto it = set.begin(); it != set.end(); ++it)
            listing.insert(it.key(), it.value());
        for (const QString &field : removed)
            listing.remove(field);

        const CatalogResult result = shop.parse();
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(!result.catalog.item(bad.id));
        const Draft *draft = result.catalog.draft(bad.id);
        QVERIFY(draft);
        QCOMPARE(draft->code, code);
        QVERIFY(draft->reason.contains(bad.id));
        QCOMPARE(idsOf(result.catalog.items), QStringList { good.id });

        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        for (const View view : { View::Player, View::Creator }) {
            QVERIFY(!idsOf(store.listings(view)).contains(bad.id));
            QVERIFY(!idsOf(store.search(view, QString())).contains(bad.id));
            QVERIFY(!idsOf(store.featured(view)).contains(bad.id));
        }
        const Outcome refused = installAndWait(store, bad.id);
        QVERIFY(!refused.started);
        QVERIFY(!refused.ok);
        QVERIFY2(refused.error.contains(bad.id), qPrintable(refused.error));
        QVERIFY(!QFileInfo::exists(store.installDirectory(bad.id)));

        const Outcome installed = installAndWait(store, good.id);
        QVERIFY2(installed.ok, qPrintable(installed.error));
    }

    // A listing that says what it is and is then malformed is not a draft. It is what a store should not have signed.
    void whatIsMalformedOrHostileRefusesTheWholeCatalogue()
    {
        Shop shop;
        const Pack good;
        shop.add(good);
        Pack bad;
        bad.id = u"org.example.pack.bad"_s;
        QJsonObject &listing = shop.add(bad);
        QJsonObject package = listing.value(u"package"_s).toObject();
        package.insert(u"file"_s, u"C:/x"_s);
        listing.insert(u"package"_s, package);
        const CatalogResult result = shop.parse();
        QVERIFY(!result.ok);
        QVERIFY(result.catalog.items.isEmpty());
        QVERIFY(result.catalog.drafts.isEmpty());
    }

    // One that cannot be named cannot be a draft, and two with one id are not one install.
    void aListingNeedsAnIdentityOfItsOwn()
    {
        for (const QJsonObject &bad : { QJsonObject { { u"kind"_s, u"game"_s } },
                                        QJsonObject { { u"id"_s, u"Not An Id"_s }, { u"kind"_s, u"game"_s } },
                                        QJsonObject { { u"id"_s, u"org.example.pack.art"_s } } }) {
            const Pack good;
            Shop shop;
            shop.add(good);
            shop.items.append(bad);
            const CatalogResult result = shop.parse();
            QVERIFY(!result.ok);
            QVERIFY2(result.error.contains(u"not valid"_s), qPrintable(result.error));
        }
    }

    // ── Catalogues: fields ───────────────────────────────────────────────────

    // Format 2 knows its fields, so a paid field cannot come in under another name.
    void anUnknownFieldIsRefused_data()
    {
        QTest::addColumn<QString>("where");
        QTest::newRow("in the catalogue") << u"root"_s;
        QTest::newRow("in an item") << u"item"_s;
        QTest::newRow("in a package") << u"package"_s;
        QTest::newRow("in a provenance") << u"provenance"_s;
        QTest::newRow("in a licence") << u"licence"_s;
    }
    void anUnknownFieldIsRefused()
    {
        QFETCH(QString, where);
        const Pack pack;
        Shop shop;
        QJsonObject &listing = shop.add(pack);
        const auto extended = [&listing](const QString &field) {
            QJsonObject object = listing.value(field).toObject();
            object.insert(u"cost"_s, 1);
            listing.insert(field, object);
        };
        QJsonObject root;
        if (where == u"root"_s)
            root.insert(u"currency"_s, u"USD"_s);
        else if (where == u"item"_s)
            listing.insert(u"cost"_s, 1);
        else
            extended(where);
        const CatalogResult result = shop.parse(root);
        QVERIFY(!result.ok);
        QVERIFY2(!result.error.isEmpty(), "an empty reason");
    }

    void everyRequiredFieldIsRequired_data()
    {
        QTest::addColumn<QString>("field");
        for (const QString &field :
             { u"name"_s, u"developer"_s, u"version"_s, u"summary"_s, u"description"_s, u"released"_s, u"licence"_s,
               u"provenance"_s, u"engines"_s, u"dependencies"_s, u"previews"_s, u"package"_s })
            QTest::addRow("%s", qPrintable(field)) << field;
    }
    void everyRequiredFieldIsRequired()
    {
        QFETCH(QString, field);
        const Pack pack;
        Shop shop;
        shop.add(pack).remove(field);
        const CatalogResult result = shop.parse();
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(pack.id), qPrintable(result.error));
    }

    void fieldsHoldWhatTheySay_data()
    {
        QTest::addColumn<QString>("field");
        QTest::addColumn<QJsonValue>("value");
        const QJsonObject provenance { { u"origin"_s, u"here"_s },
                                       { u"author"_s, u"someone"_s },
                                       { u"source"_s, u"x"_s } };
        const auto licence = [](const QString &spdx, const QString &file) {
            return QJsonObject { { u"spdx"_s, spdx }, { u"file"_s, file } };
        };
        QTest::newRow("no provenance's permission") << u"provenance"_s << QJsonValue(provenance);
        QTest::newRow("a licence that says nothing")
            << u"licence"_s << QJsonValue(licence(u"NOASSERTION"_s, u"LICENSE.txt"_s));
        QTest::newRow("a licence of no SPDX shape")
            << u"licence"_s << QJsonValue(licence(u"<b>MIT</b>"_s, u"LICENSE.txt"_s));
        QTest::newRow("a licence file that climbs")
            << u"licence"_s << QJsonValue(licence(u"MIT"_s, u"../LICENSE.txt"_s));
        QTest::newRow("a licence file that is not text")
            << u"licence"_s << QJsonValue(licence(u"MIT"_s, u"art/hero.png"_s));
        QTest::newRow("engines empty") << u"engines"_s << QJsonValue(QJsonArray {});
        QTest::newRow("an engine with no range")
            << u"engines"_s << QJsonValue(QJsonArray { QJsonObject { { u"id"_s, u"qq"_s } } });
        QTest::newRow("an engine range backwards")
            << u"engines"_s << QJsonValue(QJsonArray { rangeOf(u"qq"_s, u"2.0.0"_s, u"1.0.0"_s) });
        QTest::newRow("an engine range holding nothing")
            << u"engines"_s << QJsonValue(QJsonArray { rangeOf(u"qq"_s, u"1.0.0"_s, u"1.0.0"_s) });
        QTest::newRow("an engine named oddly")
            << u"engines"_s << QJsonValue(QJsonArray { rangeOf(u"../qq"_s, u"1.0.0"_s, u"2.0.0"_s) });
        QTest::newRow("an engine twice") << u"engines"_s
                                         << QJsonValue(QJsonArray { rangeOf(u"qq"_s, u"1.0.0"_s, u"2.0.0"_s),
                                                                    rangeOf(u"qq"_s, u"2.0.0"_s, u"3.0.0"_s) });
        QTest::newRow("a dependency with a bad id")
            << u"dependencies"_s << QJsonValue(QJsonArray { rangeOf(u"Not An Id"_s, u"1.0.0"_s, u"2.0.0"_s) });
        QTest::newRow("a preview that climbs")
            << u"previews"_s
            << QJsonValue(QJsonArray { QJsonObject { { u"file"_s, u"../a.png"_s }, { u"sha256"_s, hexDigest("x") } } });
        QTest::newRow("a preview that is no picture")
            << u"previews"_s
            << QJsonValue(QJsonArray { QJsonObject { { u"file"_s, u"a.svg"_s }, { u"sha256"_s, hexDigest("x") } } });
        QTest::newRow("a preview whose digest is not one")
            << u"previews"_s
            << QJsonValue(QJsonArray { QJsonObject { { u"file"_s, u"a.png"_s }, { u"sha256"_s, u"zz"_s } } });
        QTest::newRow("released on no such day") << u"released"_s << QJsonValue(u"2026-13-40"_s);
        QTest::newRow("a version of no shape") << u"version"_s << QJsonValue(u"1.0"_s);
        QTest::newRow("a developer not listed") << u"developer"_s << QJsonValue(u"org.other"_s);
        QTest::newRow("a name that is blank") << u"name"_s << QJsonValue(u"   "_s);
        QTest::newRow("tags that are not text") << u"tags"_s << QJsonValue(QJsonArray { 1 });
    }
    void fieldsHoldWhatTheySay()
    {
        QFETCH(QString, field);
        QFETCH(QJsonValue, value);
        const Pack pack;
        Shop shop;
        shop.add(pack).insert(field, value);
        const CatalogResult result = shop.parse();
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(pack.id), qPrintable(result.error));
    }

    void aPackageSizeIsBounded_data()
    {
        QTest::addColumn<QString>("field");
        QTest::addColumn<qint64>("value");
        QTest::newRow("no size") << u"size"_s << qint64(0);
        QTest::newRow("a negative size") << u"size"_s << qint64(-1);
        QTest::newRow("an enormous size") << u"size"_s << kMaximumPackageArchive + 1;
        QTest::newRow("nothing unpacked") << u"unpacked"_s << qint64(0);
        QTest::newRow("an enormous unpacked") << u"unpacked"_s << kMaximumPackageContent + 1;
    }
    void aPackageSizeIsBounded()
    {
        QFETCH(QString, field);
        QFETCH(qint64, value);
        const Pack pack;
        Shop shop;
        QJsonObject &listing = shop.add(pack);
        QJsonObject package = listing.value(u"package"_s).toObject();
        package.insert(field, value);
        listing.insert(u"package"_s, package);
        const CatalogResult result = shop.parse();
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(u"out of bounds"_s), qPrintable(result.error));
    }

    // Every list in a listing has a limit, and a catalogue past one is refused or, for what classifies it, a draft.
    void aListHasALimit_data()
    {
        QTest::addColumn<QString>("field");
        QTest::addColumn<QJsonValue>("value");
        QTest::addColumn<QString>("reason");
        const auto many = [](int count, const std::function<QJsonValue(int)> &element) {
            QJsonArray array;
            for (int i = 0; i < count; ++i)
                array.append(element(i));
            return QJsonValue(array);
        };
        QTest::newRow("tags") << u"tags"_s << many(33, [](int) { return QJsonValue(u"a tag"_s); }) << u"tags"_s;
        QTest::newRow("previews") << u"previews"_s << many(9, [](int i) {
            return QJsonValue(QJsonObject { { u"file"_s, u"p%1.png"_s.arg(i) }, { u"sha256"_s, hexDigest("x") } });
        }) << u"previews"_s;
        QTest::newRow("dependencies") << u"dependencies"_s << many(33, [](int i) {
            return QJsonValue(rangeOf(u"org.example.pack.p%1"_s.arg(i), u"1.0.0"_s, u"2.0.0"_s));
        }) << u"too long"_s;
        QTest::newRow("engines") << u"engines"_s << many(33, [](int i) {
            return QJsonValue(rangeOf(u"engine%1"_s.arg(i), u"1.0.0"_s, u"2.0.0"_s));
        }) << u"too long"_s;
        QTest::newRow("a name too long") << u"name"_s << QJsonValue(QString(8193, u'x')) << u"no name"_s;
        QTest::newRow("a summary too long") << u"summary"_s << QJsonValue(QString(8193, u'x')) << u"no summary"_s;
        QTest::newRow("whatsNew that is no text") << u"whatsNew"_s << QJsonValue(7) << u"whatsNew"_s;
        QTest::newRow("featured that is no answer") << u"featured"_s << QJsonValue(u"yes"_s) << u"featured"_s;
    }
    void aListHasALimit()
    {
        QFETCH(QString, field);
        QFETCH(QJsonValue, value);
        QFETCH(QString, reason);
        const Pack pack;
        Shop shop;
        shop.add(pack).insert(field, value);
        const CatalogResult result = shop.parse();
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(reason), qPrintable(result.error));
    }

    void whatClassifiesAListingHasALimitToo_data()
    {
        QTest::addColumn<QString>("field");
        QTest::addColumn<QJsonValue>("value");
        QTest::addColumn<QString>("code");
        QJsonArray placements;
        for (int i = 0; i < 17; ++i)
            placements.append(placementOf(u"creator"_s, u"Category %1"_s.arg(i)));
        QTest::newRow("placements") << u"placements"_s << QJsonValue(placements) << u"bad-placement"_s;
        QJsonArray platforms;
        for (int i = 0; i < 17; ++i)
            platforms.append(u"system-%1"_s.arg(i));
        QTest::newRow("platforms") << u"platforms"_s << QJsonValue(platforms) << u"no-platforms"_s;
    }
    void whatClassifiesAListingHasALimitToo()
    {
        QFETCH(QString, field);
        QFETCH(QJsonValue, value);
        QFETCH(QString, code);
        const Pack pack;
        Shop shop;
        shop.add(pack).insert(field, value);
        const CatalogResult result = shop.parse();
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(result.catalog.draft(pack.id));
        QCOMPARE(result.catalog.draft(pack.id)->code, code);
    }

    void aCatalogueHasALimitOnItsItems()
    {
        Shop shop;
        for (int i = 0; i < 4097; ++i)
            shop.items.append(QJsonObject { { u"id"_s, u"org.example.item.i%1"_s.arg(i) }, { u"kind"_s, u"tool"_s } });
        const CatalogResult result = shop.parse();
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(u"too many items"_s), qPrintable(result.error));
    }

    void aCatalogueNamesItsStoreAndItsDevelopers_data()
    {
        QTest::addColumn<QJsonObject>("root");
        QTest::addColumn<QString>("reason");
        const auto developer = [](const QJsonObject &entry) {
            return QJsonObject { { u"developers"_s, QJsonObject { { kDeveloper, entry } } } };
        };
        const QString key = QString::fromLatin1(QByteArray(65, '\x04').toBase64());
        QTest::newRow("no store") << QJsonObject { { u"store"_s, u""_s } } << u"does not name its store"_s;
        QTest::newRow("a developer with no name")
            << developer({ { u"verified"_s, true }, { u"key"_s, key } }) << u"no valid key"_s;
        QTest::newRow("a developer with a short key")
            << developer({ { u"name"_s, u"Example"_s }, { u"verified"_s, true }, { u"key"_s, u"AAAA"_s } })
            << u"no valid key"_s;
        QTest::newRow("a developer with no answer to verified")
            << developer({ { u"name"_s, u"Example"_s }, { u"key"_s, key } }) << u"whether it is verified"_s;
        QTest::newRow("a developer with a field about money")
            << developer({ { u"name"_s, u"Example"_s }, { u"verified"_s, true }, { u"key"_s, key }, { u"fee"_s, 1 } })
            << u"does not know: fee"_s;
    }
    void aCatalogueNamesItsStoreAndItsDevelopers()
    {
        QFETCH(QJsonObject, root);
        QFETCH(QString, reason);
        const Pack pack;
        Shop shop;
        shop.add(pack);
        const CatalogResult result = shop.parse(root);
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(reason), qPrintable(result.error));
    }

    // Not an empty catalogue: signed, it would use up its sequence and drop the keys what is installed is checked
    // against.
    void aCatalogueWithoutItsListsIsRefused_data()
    {
        QTest::addColumn<int>("format");
        QTest::addColumn<QJsonValue>("items");
        QTest::addColumn<QJsonValue>("developers");
        QTest::addColumn<QString>("reason");
        const QJsonValue absent(QJsonValue::Undefined);
        const QJsonValue noItems { QJsonArray {} };
        const QJsonValue noDevelopers { QJsonObject {} };
        for (const int format : { 1, 2 }) {
            const QByteArray tag = "format " + QByteArray::number(format);
            QTest::addRow("%s, items absent", tag.constData())
                << format << absent << noDevelopers << u"no list of items"_s;
            QTest::addRow("%s, items a word", tag.constData())
                << format << QJsonValue(u"none"_s) << noDevelopers << u"no list of items"_s;
            QTest::addRow("%s, developers absent", tag.constData())
                << format << noItems << absent << u"no list of developers"_s;
            QTest::addRow("%s, developers a list", tag.constData())
                << format << noItems << QJsonValue(QJsonArray {}) << u"no list of developers"_s;
        }
    }
    void aCatalogueWithoutItsListsIsRefused()
    {
        QFETCH(int, format);
        QFETCH(QJsonValue, items);
        QFETCH(QJsonValue, developers);
        QFETCH(QString, reason);
        QJsonObject root { { u"format"_s, format }, { u"store"_s, u"A store"_s } };
        if (format == 2) {
            root.insert(u"sequence"_s, 3);
            root.insert(u"expires"_s, u"2099-01-01"_s);
        }
        if (!items.isUndefined())
            root.insert(u"items"_s, items);
        if (!developers.isUndefined())
            root.insert(u"developers"_s, developers);
        const TestSigner store;
        const QByteArray bytes = QJsonDocument(root).toJson();
        const CatalogResult result = parseCatalog(bytes, store.sign(bytes), store.publicKey());
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(reason), qPrintable(result.error));
    }

    // Lists that are there and empty are a catalogue with nothing in it, which is a thing a store may say.
    void aCatalogueMayBeEmpty()
    {
        const Shop shop;
        const CatalogResult result = shop.parse();
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(result.catalog.items.isEmpty());
    }

    // `$` matches before a trailing newline, so each of these once passed, and `x` and `x\n` were two ids.
    void aTrailingNewlineIsNotPartOfAnything()
    {
        QVERIFY(!isSafePackagePath(u"a.png\n"_s));
        QVERIFY(!isValidItemId(u"org.example.pack.art\n"_s));
        QVERIFY(!isValidItemId(u"org.example.pack.old-"_s + QString(32, u'a') + u'\n'));
        QVERIFY(!isValidVersion(u"1.0.0\n"_s));

        const Pack pack;
        Shop shop;
        QJsonObject &listing = shop.add(pack);
        QJsonObject withDigest = listing.value(u"package"_s).toObject();
        const QString digest = withDigest.value(u"sha256"_s).toString();
        withDigest.insert(u"sha256"_s, QString(digest + u'\n'));
        listing.insert(u"package"_s, withDigest);
        CatalogResult result = shop.parse();
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(u"SHA-256"_s), qPrintable(result.error));

        withDigest.insert(u"sha256"_s, digest);
        listing.insert(u"package"_s, withDigest);
        listing.insert(u"released"_s, u"2026-11-01\n"_s);
        result = shop.parse();
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(u"released"_s), qPrintable(result.error));

        listing.insert(u"released"_s, u"2026-11-01"_s);
        result = shop.parse({ { u"expires"_s, u"2099-12-31\n"_s } });
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(u"expiry"_s), qPrintable(result.error));

        listing.insert(u"engines"_s, QJsonArray { rangeOf(u"qq\n"_s, u"1.0.0"_s, u"2.0.0"_s) });
        result = shop.parse();
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(u"engines"_s), qPrintable(result.error));

        listing.insert(u"engines"_s, QJsonArray { rangeOf(u"qq"_s, u"1.0.0"_s, u"2.0.0"_s) });
        listing.insert(u"platforms"_s, QJsonArray { u"linux-x86_64\n"_s });
        result = shop.parse();
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(result.catalog.draft(pack.id));
        QCOMPARE(result.catalog.draft(pack.id)->code, u"no-platforms"_s);
    }

    void aFormatOneSizeIsBounded_data()
    {
        QTest::addColumn<qint64>("size");
        QTest::newRow("none") << qint64(0);
        QTest::newRow("negative") << qint64(-1);
        QTest::newRow("past what a package may be") << kMaximumPackageArchive + 1;
        QTest::newRow("near what a number holds") << (qint64(1) << 52);
    }
    void aFormatOneSizeIsBounded()
    {
        QFETCH(qint64, size);
        const CatalogResult result = parseFormat1([size](QJsonObject &item) {
            QJsonObject package = item.value(u"package"_s).toObject();
            package.insert(u"size"_s, size);
            item.insert(u"package"_s, package);
        });
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(u"not valid"_s), qPrintable(result.error));
    }

    void aCatalogueSurvivesDamage()
    {
        const Pack pack;
        Shop shop;
        shop.add(pack);
        const QByteArray bytes = shop.json();
        QRandomGenerator random(0xca7a106);
        for (int round = 0; round < 500; ++round) {
            QByteArray damaged = bytes;
            for (int i = 0; i < 1 + random.bounded(8); ++i)
                damaged[random.bounded(damaged.size())] = char(random.bounded(256));
            // Signed, so it reaches the parser: the store's key is the only wall this does not test.
            const CatalogResult result = parseCatalog(damaged, shop.store.sign(damaged), shop.store.publicKey());
            if (result.ok && result.catalog.format == 2)
                QVERIFY(result.catalog.sequence >= 1);
        }
    }

    void anUnknownFormatIsRefused()
    {
        // How an older client treats format 2: its check is `format != 1`, and format 2 is a plain integer.
        const QByteArray demo = readFile(kDemo + u"/v1/catalog.json"_s);
        QCOMPARE(QJsonDocument::fromJson(demo).object().value(u"format"_s).toInt(), 2);

        const Pack pack;
        Shop shop;
        shop.add(pack);
        for (const QJsonValue &format : { QJsonValue(3), QJsonValue(0), QJsonValue(u"2"_s), QJsonValue(2.5) }) {
            const CatalogResult result = shop.parse({ { u"format"_s, format } });
            QVERIFY(!result.ok);
            QCOMPARE(result.error, u"Unsupported catalogue format."_s);
        }
    }

    // ── Catalogues: expiry and sequence ──────────────────────────────────────

    void anExpiredCatalogueIsRefused_data()
    {
        QTest::addColumn<QString>("expires");
        QTest::addColumn<QDate>("today");
        QTest::addColumn<bool>("taken");
        const QDate day(2026, 11, 1);
        QTest::newRow("the day before it expires") << u"2026-11-02"_s << day << true;
        QTest::newRow("the last day it is good for") << u"2026-11-01"_s << day << true;
        QTest::newRow("the day after") << u"2026-10-31"_s << day << false;
        QTest::newRow("long ago") << u"2020-01-01"_s << day << false;
    }
    void anExpiredCatalogueIsRefused()
    {
        QFETCH(QString, expires);
        QFETCH(QDate, today);
        QFETCH(bool, taken);
        const Pack pack;
        Shop shop;
        shop.add(pack);
        shop.expires = expires;
        const CatalogResult result = shop.parse({}, today);
        QCOMPARE(result.ok, taken);
        if (!taken)
            QVERIFY2(result.error.contains(u"expired"_s), qPrintable(result.error));
    }

    void anExpiredCatalogueIsRefusedByTheEngineByTheRealClock()
    {
        const Pack pack;
        Shop shop;
        shop.add(pack);
        shop.expires = u"2020-01-01"_s;
        QTemporaryDir data;
        StoreEngine store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Failed);
        QVERIFY2(store.error().contains(u"expired"_s), qPrintable(store.error()));
        QCOMPARE(store.lastSequence(), 0); // an expired catalogue teaches nothing
    }

    void anExpiryIsADate_data()
    {
        QTest::addColumn<QJsonValue>("expires");
        QTest::newRow("no such day") << QJsonValue(u"2099-13-40"_s);
        QTest::newRow("no date") << QJsonValue(u"soon"_s);
        QTest::newRow("a date and more") << QJsonValue(u"2099-01-01T00:00:00Z"_s);
        QTest::newRow("a number") << QJsonValue(20990101);
        QTest::newRow("nothing") << QJsonValue(QJsonValue::Null);
    }
    void anExpiryIsADate()
    {
        QFETCH(QJsonValue, expires);
        const Pack pack;
        Shop shop;
        shop.add(pack);
        const CatalogResult result = shop.parse({ { u"expires"_s, expires } });
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(u"expiry"_s), qPrintable(result.error));
    }

    void aSequenceIsAPositiveInteger_data()
    {
        QTest::addColumn<QJsonValue>("sequence");
        QTest::newRow("zero") << QJsonValue(0);
        QTest::newRow("negative") << QJsonValue(-1);
        QTest::newRow("a fraction") << QJsonValue(1.5);
        QTest::newRow("text") << QJsonValue(u"1"_s);
        QTest::newRow("nothing") << QJsonValue(QJsonValue::Null);
        QTest::newRow("far too large to be a whole number") << QJsonValue(1e300);
        QTest::newRow("just past what JSON holds exactly") << QJsonValue(qint64(1) << 54);
    }
    void aSequenceIsAPositiveInteger()
    {
        QFETCH(QJsonValue, sequence);
        const Pack pack;
        Shop shop;
        shop.add(pack);
        const CatalogResult result = shop.parse({ { u"sequence"_s, sequence } });
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(u"sequence"_s), qPrintable(result.error));
    }

    // A catalogue has no audience any more: each listing says where it goes, so a store cannot be told one by a field.
    void aCatalogueHasNoAudience()
    {
        const Pack pack;
        Shop shop;
        shop.add(pack);
        const CatalogResult result = shop.parse({ { u"audience"_s, u"creator"_s } });
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(u"audience"_s), qPrintable(result.error));
    }

    // An older copy is still signed: only remembering what was seen tells them apart.
    void aReplayedLowerSequenceIsRefusedAndStaysRefused()
    {
        const Pack pack;
        Shop shop;
        shop.add(pack);
        QTemporaryDir data;
        const QByteArray key = shop.store.publicKey();
        const QString catalogue = shop.dir.path() + u"/catalog.json"_s;
        {
            CreatorStore store(data.path(), key);
            shop.sequence = 5;
            QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
            QCOMPARE(store.lastSequence(), 5);

            // The same catalogue again is no replay: a refresh fetches it every time.
            QCOMPARE(refreshed(store, QUrl::fromLocalFile(catalogue)), StoreEngine::State::Ready);

            shop.sequence = 4;
            QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Failed);
            QVERIFY2(store.error().contains(u"older"_s), qPrintable(store.error()));
            QCOMPARE(store.catalog().sequence, 5); // what was held is still held
            QCOMPARE(store.lastSequence(), 5);

            // Another catalogue under a number already used is no more to be trusted than an older one.
            shop.sequence = 5;
            shop.items.first().insert(u"summary"_s, u"Another summary."_s);
            QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Failed);
            QVERIFY2(store.error().contains(u"differs"_s), qPrintable(store.error()));

            shop.sequence = 6;
            QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
            QCOMPARE(store.lastSequence(), 6);
        }
        // It is remembered in the store's own folder, across a restart.
        QVERIFY(QFileInfo::exists(data.path() + u"/catalog-state.json"_s));
        CreatorStore again(data.path(), key);
        QCOMPARE(again.lastSequence(), 6);
        shop.sequence = 5;
        QCOMPARE(refreshed(again, shop.publish()), StoreEngine::State::Failed);
        QVERIFY2(again.error().contains(u"older"_s), qPrintable(again.error()));
    }

    // Format 1 has no sequence, so once a store has taken format 2 an older format must not be the way round it.
    void anOlderFormatCannotReplaceAFormatTwoCatalogue()
    {
        const Pack pack;
        Shop shop;
        shop.add(pack);
        const QByteArray bytes = QJsonDocument(QJsonObject { { u"format"_s, 1 },
                                                             { u"store"_s, u"An older store"_s },
                                                             { u"developers"_s, QJsonObject {} },
                                                             { u"items"_s, QJsonArray {} } })
                                     .toJson();
        QTemporaryDir older;
        writeFile(older.path() + u"/catalog.json"_s, bytes);
        writeFile(older.path() + u"/catalog.json.sig"_s, shop.store.sign(bytes));
        const QUrl olderUrl = QUrl::fromLocalFile(older.path() + u"/catalog.json"_s);

        // Before any format 2 catalogue it is taken, as it always was.
        {
            QTemporaryDir data;
            StoreEngine store(data.path(), shop.store.publicKey());
            QCOMPARE(refreshed(store, olderUrl), StoreEngine::State::Ready);
        }

        QTemporaryDir data;
        const QByteArray key = shop.store.publicKey();
        {
            StoreEngine store(data.path(), key);
            QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
            QCOMPARE(refreshed(store, olderUrl), StoreEngine::State::Failed);
            QVERIFY2(store.error().contains(u"format 1 has no sequence"_s), qPrintable(store.error()));
            QCOMPARE(store.catalog().format, 2); // what was held is still held
        }
        // And after a restart, since the sequence is remembered.
        StoreEngine again(data.path(), key);
        QCOMPARE(refreshed(again, olderUrl), StoreEngine::State::Failed);
    }

    void formatOneKnowsNoSequenceAndKeepsWorking()
    {
        QTemporaryDir data;
        const QByteArray key = publicKey(kFixtures + u"/store.pub"_s);
        StoreEngine store(data.path(), key);
        // An older catalogue after a newer one is fine here, as it always was: this limit closes for format 2 only.
        QCOMPARE(refreshed(store, QUrl::fromLocalFile(kFixtures + u"/v2/catalog.json"_s)), StoreEngine::State::Ready);
        QCOMPARE(refreshed(store, QUrl::fromLocalFile(kFixtures + u"/v1/catalog.json"_s)), StoreEngine::State::Ready);
        QVERIFY(!QFileInfo::exists(data.path() + u"/catalog-state.json"_s));
    }

    // ── Swaps ────────────────────────────────────────────────────────────────

    void anOlderSignedPackageSwappedInIsRefused()
    {
        Pack older;
        older.version = u"1.0.0"_s;
        Pack newer;
        newer.version = u"1.1.0"_s;
        Shop shop;
        const QByteArray olderBytes = older.package(shop.developer);
        shop.add(newer);
        writeFile(shop.dir.path() + u"/packages/%1-1.1.0.qorpkg"_s.arg(newer.id), olderBytes);

        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        const Outcome outcome = installAndWait(store, newer.id);
        QVERIFY(!outcome.ok);
        QVERIFY2(outcome.error.contains(u"does not match"_s) || outcome.error.contains(u"larger"_s),
                 qPrintable(outcome.error));
        QVERIFY(store.installed().isEmpty());
    }

    // Each of these opens cleanly, with a valid signature, and is the wrong package.
    void theManifestMustMirrorTheListing_data()
    {
        QTest::addColumn<QString>("what");
        for (const QString &what :
             { u"id"_s, u"version"_s, u"kind alone"_s, u"kind and stores"_s, u"edition"_s, u"licence's SPDX"_s,
               u"licence's file"_s, u"dependencies"_s, u"developer"_s, u"platforms"_s, u"engines"_s })
            QTest::addRow("%s", qPrintable(what)) << what;
    }
    void theManifestMustMirrorTheListing()
    {
        QFETCH(QString, what);
        Pack listed;
        Pack actual;
        if (what == u"id"_s) {
            actual.id = u"org.example.pack.other"_s;
        } else if (what == u"version"_s) {
            actual.version = u"9.0.0"_s;
        } else if (what == u"kind alone"_s) {
            actual.kind = u"plugin"_s; // the same store, and holding nothing a plugin may not
        } else if (what == u"platforms"_s) {
            actual.platforms = QJsonArray { currentPlatform(), u"some-other-system"_s };
        } else if (what == u"engines"_s) {
            actual.engines = QJsonArray { rangeOf(u"qq"_s, u"0.1.0"_s, u"2.0.0"_s) };
        } else if (what == u"kind and stores"_s) {
            actual.kind = u"game"_s;
            actual.stores = { u"player"_s };
        } else if (what == u"edition"_s) {
            listed.kind = actual.kind = u"game"_s;
            listed.stores = actual.stores = { u"player"_s };
            listed.edition = { { u"group"_s, u"org.example.one"_s }, { u"role"_s, u"game"_s } };
            actual.edition = { { u"group"_s, u"org.example.two"_s }, { u"role"_s, u"game"_s } };
        } else if (what == u"licence's SPDX"_s) {
            actual.spdx = u"MIT"_s;
        } else if (what == u"licence's file"_s) {
            actual.files.insert(u"OTHER.txt"_s, kLicence);
            actual.licenceFile = u"OTHER.txt"_s;
        } else if (what == u"dependencies"_s) {
            actual.dependencies = QJsonArray { rangeOf(u"org.example.pack.other"_s, u"1.0.0"_s, u"2.0.0"_s) };
        } else if (what == u"developer"_s) {
            actual.developer = u"org.other"_s;
        }
        Shop shop;
        const QByteArray package = actual.package(shop.developer);
        // What the listing says is `listed`, of the very package's size, digest and contents.
        qint64 unpacked = 0;
        for (const RawEntry &entry : actual.entries(shop.developer))
            unpacked += entry.data.size();
        QJsonObject &listing = shop.addPackage(listed, package);
        QJsonObject packageField = listing.value(u"package"_s).toObject();
        packageField.insert(u"unpacked"_s, unpacked);
        listing.insert(u"package"_s, packageField);

        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        const Outcome outcome = installAndWait(store, listed.id);
        QVERIFY(!outcome.ok);
        QVERIFY2(outcome.error.contains(u"not the one the catalogue lists"_s), qPrintable(outcome.error));
        QVERIFY(store.installed().isEmpty());
    }

    // A format 2 manifest knows its fields, as its catalogue does.
    void aManifestIsClosed_data()
    {
        QTest::addColumn<QString>("field");
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("a price") << u"price"_s << QJsonValue(0);
        QTest::newRow("anything else") << u"audience"_s << QJsonValue(u"player"_s);
    }
    void aManifestIsClosed()
    {
        QFETCH(QString, field);
        QFETCH(QJsonValue, value);
        const TestSigner developer;
        const Pack pack;
        QJsonObject manifest = QJsonDocument::fromJson(pack.manifest()).object();
        manifest.insert(field, value);
        const QByteArray bytes = QJsonDocument(manifest).toJson();
        QList<RawEntry> entries = pack.entries(developer);
        entries[0].data = bytes;
        entries[1].data = developer.sign(bytes);
        const PackageResult opened = openPackage(forgeArchive(entries), developer.publicKey());
        QVERIFY(!opened.ok);
        QVERIFY2(opened.error.contains(u"manifest is not valid"_s), qPrintable(opened.error));
    }

    void aManifestListsOnlyWhatMayBeListed_data()
    {
        QTest::addColumn<QString>("listed");
        QTest::addColumn<QString>("digest");
        QTest::newRow("a path that climbs") << u"../evil.png"_s << hexDigest(kPng);
        QTest::newRow("the manifest itself") << u"manifest.json"_s << hexDigest(kPng);
        QTest::newRow("the signature") << u"manifest.sig"_s << hexDigest(kPng);
        QTest::newRow("a digest of the wrong length") << u"art/more.png"_s << u"abcd"_s;
    }
    void aManifestListsOnlyWhatMayBeListed()
    {
        QFETCH(QString, listed);
        QFETCH(QString, digest);
        const TestSigner developer;
        const Pack pack;
        QJsonObject manifest = QJsonDocument::fromJson(pack.manifest()).object();
        QJsonObject files = manifest.value(u"files"_s).toObject();
        files.insert(listed, digest);
        manifest.insert(u"files"_s, files);
        const QByteArray bytes = QJsonDocument(manifest).toJson();
        QList<RawEntry> entries = pack.entries(developer);
        entries[0].data = bytes;
        entries[1].data = developer.sign(bytes);
        const PackageResult opened = openPackage(forgeArchive(entries), developer.publicKey());
        QVERIFY(!opened.ok);
        QVERIFY2(opened.error.contains(u"lists an invalid file"_s), qPrintable(opened.error));
    }

    void aManifestHasALimit()
    {
        const TestSigner developer;
        const Pack pack;
        QJsonObject manifest = QJsonDocument::fromJson(pack.manifest()).object();
        manifest.insert(u"name"_s, QString(5 * 1024 * 1024, u'x'));
        const QByteArray bytes = QJsonDocument(manifest).toJson();
        QList<RawEntry> entries = pack.entries(developer);
        entries[0].data = bytes;
        entries[1].data = developer.sign(bytes);
        const PackageResult opened = openPackage(forgeArchive(entries), developer.publicKey());
        QVERIFY(!opened.ok);
        QVERIFY2(opened.error.contains(u"manifest is too large"_s), qPrintable(opened.error));
    }

    void aFormatOneManifestCannotBeANewKind()
    {
        Pack pack;
        pack.format = 1;
        const TestSigner developer;
        const PackageResult opened = openPackage(pack.package(developer), developer.publicKey());
        QVERIFY(!opened.ok);
        QVERIFY2(opened.error.contains(u"manifest is not valid"_s), qPrintable(opened.error));
    }

    void aFormatTwoManifestSaysWhatItIsWithoutContradiction_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QStringList>("stores");
        QTest::addColumn<QJsonObject>("edition");
        const QStringList player { u"player"_s };
        const QStringList creator { u"creator"_s };
        const QStringList both { u"creator"_s, u"player"_s };
        const QJsonObject none;
        QTest::newRow("a theme") << u"theme"_s << creator << none;
        QTest::newRow("a kind no store knows") << u"virus"_s << creator << none;
        QTest::newRow("no store") << u"asset-pack"_s << QStringList {} << none;
        QTest::newRow("a store that does not exist") << u"asset-pack"_s << QStringList { u"secret"_s } << none;
        QTest::newRow("a pack for players") << u"asset-pack"_s << player << none;
        QTest::newRow("a pack for both") << u"asset-pack"_s << both << none;
        QTest::newRow("a game for creators") << u"game"_s << creator << none;
        QTest::newRow("a game for both") << u"game"_s << both << none;
        QTest::newRow("one store twice") << u"utility"_s << QStringList { u"player"_s, u"player"_s } << none;
        QTest::newRow("a game that says it is a project")
            << u"game"_s << player
            << QJsonObject { { u"group"_s, u"org.example.one"_s }, { u"role"_s, u"editable-project"_s } };
    }
    void aFormatTwoManifestSaysWhatItIsWithoutContradiction()
    {
        QFETCH(QString, kind);
        QFETCH(QStringList, stores);
        QFETCH(QJsonObject, edition);
        Pack pack;
        pack.kind = kind;
        pack.stores = stores;
        pack.edition = edition;
        const TestSigner developer;
        const PackageResult opened = openPackage(pack.package(developer), developer.publicKey());
        QVERIFY(!opened.ok);
        QVERIFY2(opened.error.contains(u"manifest is not valid"_s), qPrintable(opened.error));
    }

    // A kind with no rules of its own gets the strictest there are. It is the catalogue and install that keep it from
    // being installed at all; this is what stands behind them when a stage opens.
    void aPackageOfAKindNotTakenYetHoldsNothingThatRuns_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QStringList>("stores");
        for (const QString &kind : { u"plugin"_s, u"tool"_s, u"template"_s, u"project"_s, u"creator-app"_s })
            QTest::addRow("%s", qPrintable(kind)) << kind << QStringList { u"creator"_s };
        for (const QString &kind : { u"mod"_s, u"expansion"_s, u"demo"_s, u"soundtrack"_s, u"utility"_s })
            QTest::addRow("%s", qPrintable(kind)) << kind << QStringList { u"player"_s };
    }
    void aPackageOfAKindNotTakenYetHoldsNothingThatRuns()
    {
        QFETCH(QString, kind);
        QFETCH(QStringList, stores);
        const TestSigner developer;
        for (const char *path : { "plugin.dll", "libplugin.so", "run.sh", "logic/rules.qml", "qmldir" }) {
            Pack pack;
            pack.kind = kind;
            pack.stores = stores;
            pack.files.insert(QString::fromLatin1(path), "x");
            const PackageResult opened = openPackage(pack.package(developer), developer.publicKey());
            QVERIFY(!opened.ok);
            QVERIFY2(opened.error.contains(u"may not hold"_s), qPrintable(opened.error));
        }
    }

    void whatIsUnpackedIsWhatTheCatalogueSays()
    {
        const Pack pack;
        qint64 real = 0;
        {
            Shop shop;
            for (const RawEntry &entry : pack.entries(shop.developer))
                real += entry.data.size();
            // It claims one byte fewer than the package holds.
            QJsonObject &listing = shop.add(pack);
            QJsonObject field = listing.value(u"package"_s).toObject();
            field.insert(u"unpacked"_s, real - 1);
            listing.insert(u"package"_s, field);
            QTemporaryDir data;
            CreatorStore store(data.path(), shop.store.publicKey());
            QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
            const Outcome outcome = installAndWait(store, pack.id);
            QVERIFY(!outcome.ok);
            QVERIFY2(outcome.error.contains(u"too large"_s), qPrintable(outcome.error));
        }
        {
            Shop shop;
            // It claims one byte more.
            QJsonObject &listing = shop.add(pack);
            QJsonObject field = listing.value(u"package"_s).toObject();
            field.insert(u"unpacked"_s, real + 1);
            listing.insert(u"package"_s, field);
            QTemporaryDir data;
            CreatorStore store(data.path(), shop.store.publicKey());
            QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
            const Outcome outcome = installAndWait(store, pack.id);
            QVERIFY(!outcome.ok);
            QVERIFY2(outcome.error.contains(u"not the one"_s), qPrintable(outcome.error));
        }
    }

    // ── What is installed stays what was signed ──────────────────────────────

    void anEditedPackIsNotIntact()
    {
        const Pack pack;
        Shop shop;
        shop.add(pack);
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, pack.id).ok);
        QVERIFY(store.isIntact(pack.id));

        const QString folder = store.installDirectory(pack.id);
        const QString hero = folder + u"/art/hero.png"_s;
        const QByteArray original = readFile(hero);

        writeFile(hero, original + "edited");
        QVERIFY(!store.isIntact(pack.id));
        writeFile(hero, original);
        QVERIFY(store.isIntact(pack.id));

        // Added: exactly what a pack must never bring into a project.
        writeFile(folder + u"/logic/rules.qml"_s, "import QQ\nLogic {}\n");
        QVERIFY(!store.isIntact(pack.id));
        QVERIFY(QDir(folder + u"/logic"_s).removeRecursively());
        QVERIFY(store.isIntact(pack.id));

        QVERIFY(QFile::remove(hero));
        QVERIFY(!store.isIntact(pack.id));
    }

    // Its signature is good, and its developer signed it, in its day. It is still not what is installed.
    void anOlderSignedCopyInTheInstallFolderIsNotIntact()
    {
        Pack older;
        older.version = u"1.0.0"_s;
        Pack newer;
        newer.version = u"1.1.0"_s;
        newer.files.insert(u"art/new.png"_s, kPng);
        Shop shop;
        shop.add(newer);
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, newer.id).ok);

        const QString folder = store.installDirectory(newer.id);
        QVERIFY(QDir(folder).removeRecursively());
        const QByteArray manifest = older.manifest();
        writeFile(folder + u"/manifest.json"_s, manifest);
        writeFile(folder + u"/manifest.sig"_s, shop.developer.sign(manifest));
        for (auto it = older.files.cbegin(); it != older.files.cend(); ++it)
            writeFile(folder + u'/' + it.key(), it.value());

        QVERIFY(verifyInstalled(folder, shop.developer.publicKey()));
        QVERIFY(!store.isIntact(newer.id));
    }

    // ── Views: one catalogue, two storefronts, and no mixing ─────────────────

    void aViewHoldsOnlyWhatIsPlacedInIt()
    {
        Pack pack;
        pack.category = u"Tiles"_s;
        Pack game = gamePack();
        game.category = u"Platformers"_s;
        Shop shop;
        QJsonObject &packListing = shop.add(pack);
        packListing.insert(u"name"_s, u"Forge tiles"_s);
        packListing.insert(u"featured"_s, true);
        QJsonObject &gameListing = shop.add(game);
        gameListing.insert(u"name"_s, u"Climb"_s);
        gameListing.insert(u"featured"_s, true);

        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);

        const QStringList justGame { game.id };
        const QStringList justPack { pack.id };
        QCOMPARE(idsOf(store.listings(View::Player)), justGame);
        QCOMPARE(idsOf(store.featured(View::Player)), justGame);
        QCOMPARE(idsOf(store.search(View::Player, QString())), justGame);
        QCOMPARE(idsOf(store.search(View::Player, u"climb"_s)), justGame);
        QCOMPARE(store.categoryCounts(View::Player), (QMap<QString, int> { { u"Platformers"_s, 1 } }));

        QCOMPARE(idsOf(store.listings(View::Creator)), justPack);
        QCOMPARE(idsOf(store.featured(View::Creator)), justPack);
        QCOMPARE(idsOf(store.search(View::Creator, u"forge"_s)), justPack);
        QCOMPARE(store.categoryCounts(View::Creator), (QMap<QString, int> { { u"Tiles"_s, 1 } }));
    }

    // A search, a featured list or a count that finds nothing here answers with nothing, however much is in the other.
    void anEmptyAnswerDoesNotFallBackToTheOtherStore()
    {
        Pack pack;
        pack.category = u"Tiles"_s;
        Pack game = gamePack();
        game.category = u"Platformers"_s;
        {
            Shop onlyCreator;
            onlyCreator.add(pack).insert(u"featured"_s, true);
            QTemporaryDir data;
            CreatorStore store(data.path(), onlyCreator.store.publicKey());
            QCOMPARE(refreshed(store, onlyCreator.publish()), StoreEngine::State::Ready);
            QCOMPARE(store.listings(View::Creator).size(), 1);
            QVERIFY(store.listings(View::Player).isEmpty());
            QVERIFY(store.featured(View::Player).isEmpty());
            QVERIFY(store.search(View::Player, QString()).isEmpty());
            QVERIFY(store.search(View::Player, u"pack"_s).isEmpty());
            QVERIFY(store.categoryCounts(View::Player).isEmpty());
        }
        {
            Shop onlyPlayer;
            onlyPlayer.add(game).insert(u"featured"_s, true);
            QTemporaryDir data;
            CreatorStore store(data.path(), onlyPlayer.store.publicKey());
            QCOMPARE(refreshed(store, onlyPlayer.publish()), StoreEngine::State::Ready);
            QCOMPARE(store.listings(View::Player).size(), 1);
            QVERIFY(store.listings(View::Creator).isEmpty());
            QVERIFY(store.featured(View::Creator).isEmpty());
            QVERIFY(store.search(View::Creator, QString()).isEmpty());
            QVERIFY(store.categoryCounts(View::Creator).isEmpty());
        }
        {
            // Both are full: a miss in one is still a miss, and "nothing featured" is not "everything".
            Shop both;
            both.add(pack).insert(u"featured"_s, true);
            both.add(game);
            QTemporaryDir data;
            CreatorStore store(data.path(), both.store.publicKey());
            QCOMPARE(refreshed(store, both.publish()), StoreEngine::State::Ready);
            QVERIFY(store.featured(View::Player).isEmpty());
            QCOMPARE(store.featured(View::Creator).size(), 1);
            QVERIFY(store.search(View::Player, u"tiles"_s).isEmpty());
            QCOMPARE(store.search(View::Creator, u"tiles"_s).size(), 1);
        }
    }

    void aFormatOneCatalogueIsInNoView()
    {
        QTemporaryDir data;
        CreatorStore store(data.path(), publicKey(kFixtures + u"/store.pub"_s));
        QCOMPARE(refreshed(store, QUrl::fromLocalFile(kFixtures + u"/v1/catalog.json"_s)), StoreEngine::State::Ready);
        QCOMPARE(store.catalog().items.size(), 2);
        QCOMPARE(store.catalog().formatOneItems().size(), 2);
        for (const View view : { View::Player, View::Creator }) {
            QVERIFY(store.listings(view).isEmpty());
            QVERIFY(store.search(view, QString()).isEmpty());
            QVERIFY(store.categoryCounts(view).isEmpty());
        }
    }

    // What the shell's Store page lists, for it knows no view: a format 2 listing must not reach it by accident.
    void thisSystemIsNamedAsListingsNameIt()
    {
        static const QRegularExpression shape(u"^(linux|windows|macos)-[a-z0-9_]+\\z"_s);
        QVERIFY2(shape.match(currentPlatform()).hasMatch(), qPrintable(currentPlatform()));
    }

    void aFormatTwoListingIsNotAFormatOneItem()
    {
        const Pack pack;
        Shop shop;
        shop.add(pack);
        const CatalogResult result = shop.parse();
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.catalog.items.size(), 1);
        QVERIFY(result.catalog.formatOneItems().isEmpty());
    }

    // ── One install ──────────────────────────────────────────────────────────

    void thereIsOneEngineAndOneInstallFolder()
    {
        const Pack pack;
        const Pack game = gamePack();
        Shop shop;
        shop.add(pack);
        shop.add(game);
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(store.dataDir(), data.path());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, pack.id).ok);
        QVERIFY(installAndWait(store, game.id).ok);
        QCOMPARE(store.installDirectory(pack.id), QString(data.path() + u"/items/"_s + pack.id));
        QCOMPARE(store.installDirectory(game.id), QString(data.path() + u"/items/"_s + game.id));
        QVERIFY(QFileInfo::exists(store.installDirectory(pack.id) + u"/manifest.json"_s));
        QVERIFY(QFileInfo::exists(store.installDirectory(game.id) + u"/manifest.json"_s));
        QVERIFY(!QFileInfo::exists(data.path() + u"/player"_s));
        QVERIFY(!QFileInfo::exists(data.path() + u"/creator"_s));
        QCOMPARE(StoreEngine(data.path(), shop.store.publicKey()).installed().size(), 2);
    }

    // One package in two storefronts. `utility` is a draft at this stage, so this runs on an engine told a later stage
    // has come: nothing about listing it twice and installing it once depends on the stage.
    void aUtilityInBothStoresIsListedInEachAndInstalledOnce()
    {
        Pack utility;
        utility.id = u"org.example.utility.one"_s;
        utility.kind = u"utility"_s;
        utility.stores = { u"creator"_s, u"player"_s };
        Shop shop;
        Catalog catalog = plantedCatalog({ plantedPackage(
            shop, utility, { Placement { View::Player, u"Tools"_s }, Placement { View::Creator, u"Pipelines"_s } }) });
        catalog.items.first().name = u"Converter"_s;
        catalog.items.first().featured = true;
        listDeveloper(&catalog, shop);

        QTemporaryDir data;
        PlantedEngine store(data.path(), shop.store.publicKey());
        store.laterKinds = { utility.kind };
        store.setCatalogUrl(QUrl::fromLocalFile(shop.dir.path() + u"/catalog.json"_s));
        store.adoptCatalog(catalog);

        // Creator features off: it is a player's too, so it is there for a player, and installs.
        const QStringList justIt { utility.id };
        QCOMPARE(idsOf(store.listings(View::Player)), justIt);
        QVERIFY(store.listings(View::Creator).isEmpty());
        Outcome outcome = installAndWait(store, utility.id);
        QVERIFY2(outcome.ok, qPrintable(outcome.error));

        // On: the same id in both, each in its own category, and a word from one is not found through the other.
        store.setCreatorFeatures(true);
        QCOMPARE(idsOf(store.listings(View::Player)), justIt);
        QCOMPARE(idsOf(store.listings(View::Creator)), justIt);
        QCOMPARE(idsOf(store.featured(View::Player)), justIt);
        QCOMPARE(idsOf(store.featured(View::Creator)), justIt);
        QCOMPARE(store.categoryCounts(View::Player), (QMap<QString, int> { { u"Tools"_s, 1 } }));
        QCOMPARE(store.categoryCounts(View::Creator), (QMap<QString, int> { { u"Pipelines"_s, 1 } }));
        QVERIFY(store.search(View::Player, u"pipelines"_s).isEmpty());
        QVERIFY(store.search(View::Creator, u"tools"_s).isEmpty());

        // Asked for again, from whichever store: still one install.
        outcome = installAndWait(store, utility.id);
        QVERIFY2(outcome.ok, qPrintable(outcome.error));
        QCOMPARE(store.installed().size(), 1);
        QCOMPARE(QDir(data.path() + u"/items"_s).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden),
                 QStringList { utility.id });
        QVERIFY(store.isIntact(utility.id));
    }

    // Its developer signed for one store, and the catalogue placed it in two.
    void aManifestMirrorsTheStoresItIsPlacedIn()
    {
        Pack utility;
        utility.id = u"org.example.utility.one"_s;
        utility.kind = u"utility"_s;
        utility.stores = { u"player"_s };
        Shop shop;
        Catalog catalog = plantedCatalog({ plantedPackage(
            shop, utility, { Placement { View::Player, u"Tools"_s }, Placement { View::Creator, u"Pipelines"_s } }) });
        listDeveloper(&catalog, shop);
        QTemporaryDir data;
        PlantedEngine store(data.path(), shop.store.publicKey());
        store.laterKinds = { utility.kind };
        store.setCreatorFeatures(true);
        store.setCatalogUrl(QUrl::fromLocalFile(shop.dir.path() + u"/catalog.json"_s));
        store.adoptCatalog(catalog);
        const Outcome outcome = installAndWait(store, utility.id);
        QVERIFY(!outcome.ok);
        QVERIFY2(outcome.error.contains(u"not the one the catalogue lists"_s), qPrintable(outcome.error));
        QVERIFY(store.installed().isEmpty());
    }

    // ── Install's own walls ──────────────────────────────────────────────────

    // Whatever got into its catalogue, install takes only what this stage takes. Reached past the parser that stops it.
    void installRefusesWhatThisStageDoesNotTake_data()
    {
        QTest::addColumn<QString>("kind");
        for (const QString &kind : { u"demo"_s, u"expansion"_s, u"soundtrack"_s, u"mod"_s, u"utility"_s, u"template"_s,
                                     u"project"_s, u"plugin"_s, u"tool"_s, u"creator-app"_s, u"theme"_s, u"widget"_s })
            QTest::addRow("%s", qPrintable(kind)) << kind;
    }
    void installRefusesWhatThisStageDoesNotTake()
    {
        QFETCH(QString, kind);
        QTemporaryDir data;
        PlantedEngine store(data.path(), QByteArray(65, '\x04'));
        store.setCreatorFeatures(true);
        const QString id = u"org.example.planted.one"_s;
        store.adoptCatalog(plantedCatalog({ plantedItem(id, kind) }));
        QVERIFY(store.catalog().item(id));

        const Outcome outcome = installAndWait(store, id);
        QVERIFY(!outcome.started);
        QVERIFY(!outcome.ok);
        QVERIFY2(outcome.error.contains(u"does not take"_s), qPrintable(outcome.error));
        QVERIFY(!store.isBusy(id));
        QVERIFY(store.installed().isEmpty());
    }

    void installRefusesWhatIsExecutableExpiredNotFreeOrPlacedNowhere()
    {
        QTemporaryDir data;
        PlantedEngine store(data.path(), QByteArray(65, '\x04'));
        store.setCreatorFeatures(true);
        const QString id = u"org.example.planted.one"_s;

        CatalogItem executable = plantedItem(id, u"asset-pack"_s);
        executable.executable = true;
        store.adoptCatalog(plantedCatalog({ executable }));
        Outcome outcome = installAndWait(store, id);
        QVERIFY(!outcome.started);
        QVERIFY2(outcome.error.contains(u"executable"_s), qPrintable(outcome.error));

        CatalogItem paid = plantedItem(id, u"asset-pack"_s);
        paid.entitlement = u"paid"_s;
        store.adoptCatalog(plantedCatalog({ paid }));
        outcome = installAndWait(store, id);
        QVERIFY(!outcome.started);
        QVERIFY2(outcome.error.contains(u"not free"_s), qPrintable(outcome.error));

        CatalogItem nowhere = plantedItem(id, u"asset-pack"_s);
        nowhere.placements.clear();
        store.adoptCatalog(plantedCatalog({ nowhere }));
        outcome = installAndWait(store, id);
        QVERIFY(!outcome.started);
        QVERIFY2(outcome.error.contains(u"placed in no store"_s), qPrintable(outcome.error));

        // A catalogue that was good when it arrived and has run out since.
        Catalog expired = plantedCatalog({ plantedItem(id, u"asset-pack"_s) });
        expired.expires = QDate(2020, 1, 1);
        store.adoptCatalog(expired);
        outcome = installAndWait(store, id);
        QVERIFY(!outcome.started);
        QVERIFY2(outcome.error.contains(u"expired"_s), qPrintable(outcome.error));
    }

    void oneEngineTakesEitherFormat()
    {
        const Pack pack;
        Shop shop;
        shop.add(pack);
        {
            QTemporaryDir data;
            StoreEngine store(data.path(), shop.store.publicKey());
            QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
            QCOMPARE(store.catalog().format, 2);
        }
        {
            QTemporaryDir data;
            StoreEngine store(data.path(), publicKey(kFixtures + u"/store.pub"_s));
            QCOMPARE(refreshed(store, QUrl::fromLocalFile(kFixtures + u"/v1/catalog.json"_s)),
                     StoreEngine::State::Ready);
            QCOMPARE(store.catalog().format, 1);
            QVERIFY(installAndWait(store, u"org.example.theme.test"_s).ok);
        }
    }

    // ── Creator features ─────────────────────────────────────────────────────

    // Hiding is not access control (QOR-0021): off, a creator-only item is refused from C++, with no page involved.
    void withCreatorFeaturesOffTheCreatorStoreIsAbsentAndRefused()
    {
        Pack pack;
        pack.category = u"Tiles"_s;
        const Pack game = gamePack();
        Shop shop;
        QJsonObject &packListing = shop.add(pack);
        packListing.insert(u"featured"_s, true);
        packListing.insert(u"previews"_s, QJsonArray { QJsonObject { { u"file"_s, u"previews/p.png"_s },
                                                                     { u"sha256"_s, hexDigest(kPng) } } });
        writeFile(shop.dir.path() + u"/previews/p.png"_s, kPng);
        shop.add(game);
        QTemporaryDir data;
        StoreEngine store(data.path(), shop.store.publicKey());
        QVERIFY(!store.creatorFeatures()); // off until it is told otherwise
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);

        QVERIFY(store.listings(View::Creator).isEmpty());
        QVERIFY(store.featured(View::Creator).isEmpty());
        QVERIFY(store.search(View::Creator, QString()).isEmpty());
        QVERIFY(store.categoryCounts(View::Creator).isEmpty());
        // It is in the catalogue all the same, which is why asking the engine, and not the hiding, is what counts.
        QVERIFY(store.catalog().item(pack.id));

        const Outcome refused = installAndWait(store, pack.id);
        QVERIFY(!refused.started);
        QVERIFY(!refused.ok);
        QVERIFY2(refused.error.contains(u"creator features are off"_s), qPrintable(refused.error));
        QVERIFY(store.installed().isEmpty());
        QVERIFY(!QFileInfo::exists(store.installDirectory(pack.id)));
        QVERIFY(!store.fetchPreview(pack.id, 0)); // a preview is part of the store that carries it

        // A player's things are not touched by it.
        QCOMPARE(idsOf(store.listings(View::Player)), QStringList { game.id });
        const Outcome played = installAndWait(store, game.id);
        QVERIFY2(played.ok, qPrintable(played.error));

        // On, and the person still uses the player view as players do.
        store.setCreatorFeatures(true);
        QCOMPARE(idsOf(store.listings(View::Creator)), QStringList { pack.id });
        QCOMPARE(idsOf(store.listings(View::Player)), QStringList { game.id });
        const Outcome taken = installAndWait(store, pack.id);
        QVERIFY2(taken.ok, qPrintable(taken.error));
        QSignalSpy fetched(&store, &StoreEngine::previewFetched);
        QVERIFY(store.fetchPreview(pack.id, 0));
        QTRY_COMPARE(fetched.count(), 1);
        QVERIFY(fetched.at(0).at(2).toBool());
    }

    // The switch may go off while the package is on its way.
    void anInstallUnderWayStopsWhenCreatorFeaturesGoOff()
    {
        const Pack pack;
        Shop shop;
        shop.add(pack);
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QSignalSpy finished(&store, &StoreEngine::finished);
        QVERIFY(store.install(pack.id));
        store.setCreatorFeatures(false);
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(!finished.at(0).at(1).toBool());
        QVERIFY2(finished.at(0).at(2).toString().contains(u"creator features are off"_s),
                 qPrintable(finished.at(0).at(2).toString()));
        QVERIFY(store.installed().isEmpty());
        QVERIFY(!QFileInfo::exists(store.installDirectory(pack.id)));
        QVERIFY(!store.isBusy(pack.id));
    }

    // A game that depends on a pack must not carry it past the switch.
    void aDependencyDoesNotCarryACreatorItemPastTheSwitch()
    {
        const Pack pack;
        Pack game = gamePack();
        game.dependencies.append(rangeOf(pack.id, u"1.0.0"_s, u"2.0.0"_s));
        Shop shop;
        shop.add(pack);
        shop.add(game);
        QTemporaryDir data;
        StoreEngine store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);

        Outcome outcome = installAndWait(store, game.id);
        QVERIFY(!outcome.started);
        QVERIFY(!outcome.ok);
        QVERIFY2(outcome.error.contains(u"creator features are off"_s), qPrintable(outcome.error));
        QVERIFY(store.installed().isEmpty());
        QVERIFY(!store.isBusy(pack.id));

        store.setCreatorFeatures(true);
        outcome = installAndWait(store, game.id);
        QVERIFY2(outcome.ok, qPrintable(outcome.error));
        QCOMPARE(outcome.order, QStringList({ pack.id, game.id }));
    }

    // "Disabling creator features preserves installed apps, purchases and project data."
    void turningCreatorFeaturesOffRemovesNothing()
    {
        Pack v1;
        const Pack game = gamePack();
        Shop shop;
        shop.add(v1);
        shop.add(game);
        QTemporaryDir base;
        const QString dataDir = base.path() + u"/store"_s;
        const QByteArray key = shop.store.publicKey();
        const QString projectData = base.path() + u"/my-project/assets/hero.png"_s;
        writeFile(projectData, kPng);

        CreatorStore store(dataDir, key);
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, v1.id).ok);
        QVERIFY(installAndWait(store, game.id).ok);
        const QByteArray hero = readFile(store.installDirectory(v1.id) + u"/art/hero.png"_s);

        store.setCreatorFeatures(false);
        QCOMPARE(store.installed().size(), 2);
        QVERIFY(store.isIntact(v1.id));
        QVERIFY(store.isIntact(game.id));
        QCOMPARE(readFile(store.installDirectory(v1.id) + u"/art/hero.png"_s), hero);
        QCOMPARE(readFile(projectData), kPng);

        // Across a restart too, where the switch starts off.
        {
            StoreEngine restarted(dataDir, key);
            QVERIFY(!restarted.creatorFeatures());
            QCOMPARE(refreshed(restarted, QUrl::fromLocalFile(shop.dir.path() + u"/catalog.json"_s)),
                     StoreEngine::State::Ready);
            QCOMPARE(restarted.installed().size(), 2);
            QVERIFY(restarted.isIntact(v1.id));
        }

        // An update waits for the switch, and what is there stays as it is meanwhile.
        Pack v2 = v1;
        v2.version = u"1.1.0"_s;
        shop.items.clear();
        shop.add(v2);
        shop.add(game);
        shop.sequence = 2;
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        const Outcome waiting = installAndWait(store, v2.id);
        QVERIFY(!waiting.started);
        QVERIFY2(waiting.error.contains(u"creator features are off"_s), qPrintable(waiting.error));
        QCOMPARE(store.installed().value(v1.id).version, u"1.0.0"_s);
        QVERIFY(store.isIntact(v1.id));

        store.setCreatorFeatures(true);
        QVERIFY(installAndWait(store, v2.id).ok);
        QCOMPARE(store.installed().value(v1.id).version, u"1.1.0"_s);
    }

    // ── Editions: a game and its editable project ────────────────────────────

    // At this stage the project is simply a draft, and holding the game changes nothing about that.
    void holdingAGameDoesNotUnlockItsProject()
    {
        const QJsonObject asGame { { u"group"_s, u"org.example.one"_s }, { u"role"_s, u"game"_s } };
        const QJsonObject asProject { { u"group"_s, u"org.example.one"_s }, { u"role"_s, u"editable-project"_s } };
        Pack game = gamePack();
        game.edition = asGame;
        const QString projectId = u"org.example.project.one"_s;
        QJsonObject project =
            classifiedListing(projectId, u"project"_s, QJsonArray { placementOf(u"creator"_s, u"Projects"_s) });
        project.insert(u"edition"_s, asProject);
        Shop shop;
        shop.add(game);
        shop.items.append(project);

        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(store.catalog().draft(projectId));
        const Outcome installed = installAndWait(store, game.id);
        QVERIFY2(installed.ok, qPrintable(installed.error));
        QCOMPARE(installed.order, QStringList { game.id });
        QVERIFY(!store.installed().contains(projectId));
        QVERIFY(!idsOf(store.listings(View::Creator)).contains(projectId));

        const Outcome refused = installAndWait(store, projectId);
        QVERIFY(!refused.started);
        QVERIFY(!refused.ok);
        QVERIFY(!store.installed().contains(projectId));
    }

    // And when a later stage takes projects, each is still its own package, held on its own, either way round.
    void editionsAreSeparateEvenWhenBothCanBeInstalled()
    {
        Pack game = gamePack();
        game.edition = { { u"group"_s, u"org.example.one"_s }, { u"role"_s, u"game"_s } };
        Pack project;
        project.id = u"org.example.project.one"_s;
        project.kind = u"project"_s;
        project.edition = { { u"group"_s, u"org.example.one"_s }, { u"role"_s, u"editable-project"_s } };
        Shop shop;
        Catalog catalog =
            plantedCatalog({ plantedPackage(shop, game, { Placement { View::Player, u"Games"_s } }),
                             plantedPackage(shop, project, { Placement { View::Creator, u"Projects"_s } }) });
        listDeveloper(&catalog, shop);

        QTemporaryDir data;
        PlantedEngine store(data.path(), shop.store.publicKey());
        store.laterKinds = { project.kind };
        store.setCatalogUrl(QUrl::fromLocalFile(shop.dir.path() + u"/catalog.json"_s));
        store.adoptCatalog(catalog);

        // Holding the game does not get the project past the switch.
        Outcome outcome = installAndWait(store, game.id);
        QVERIFY2(outcome.ok, qPrintable(outcome.error));
        QVERIFY(!store.installed().contains(project.id));
        outcome = installAndWait(store, project.id);
        QVERIFY(!outcome.started);
        QVERIFY2(outcome.error.contains(u"creator features are off"_s), qPrintable(outcome.error));

        // Nor does either plan bring the other, with or without it held.
        QMap<QString, InstalledVersion> holdingGame { { game.id, { game.version, {} } } };
        QMap<QString, InstalledVersion> holdingProject { { project.id, { project.version, {} } } };
        QCOMPARE(resolveDependencies(store.catalog(), project.id, holdingGame).order, QStringList { project.id });
        QCOMPARE(resolveDependencies(store.catalog(), game.id, holdingProject).order, QStringList { game.id });

        // On, it is its own install. Removing the game leaves it, and it did not need the game.
        store.setCreatorFeatures(true);
        outcome = installAndWait(store, project.id);
        QVERIFY2(outcome.ok, qPrintable(outcome.error));
        QCOMPARE(outcome.order, QStringList { project.id });
        QVERIFY(store.remove(game.id));
        QVERIFY(store.isIntact(project.id));
        QVERIFY(!store.installed().contains(game.id));
    }

    // Depending on another edition would bring it along, so it is a contradiction wherever it comes from.
    void anEditionCannotDependOnAnother()
    {
        const QJsonObject asGame { { u"group"_s, u"org.example.one"_s }, { u"role"_s, u"game"_s } };
        const QJsonObject asProject { { u"group"_s, u"org.example.one"_s }, { u"role"_s, u"editable-project"_s } };
        const QString projectId = u"org.example.project.one"_s;

        Pack game = gamePack();
        game.edition = asGame;
        game.dependencies.append(rangeOf(projectId, u"1.0.0"_s, u"2.0.0"_s));
        QJsonObject project =
            classifiedListing(projectId, u"project"_s, QJsonArray { placementOf(u"creator"_s, u"Projects"_s) });
        project.insert(u"edition"_s, asProject);
        Pack other;
        other.id = u"org.example.pack.other"_s;
        Shop shop;
        shop.add(game);
        shop.add(other);
        shop.items.append(project);
        const CatalogResult result = shop.parse();
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(!result.catalog.item(game.id));
        QVERIFY(result.catalog.draft(game.id));
        QCOMPARE(result.catalog.draft(game.id)->code, u"edition-dependency"_s);
        QVERIFY(result.catalog.item(other.id));

        // The resolver refuses it by itself as well: it must not rest on the parser having looked.
        CatalogItem plantedGame = plantedItem(game.id, u"game"_s);
        plantedGame.edition = Edition { u"org.example.one"_s, u"game"_s };
        plantedGame.dependencies.append(VersionRange { projectId, u"1.0.0"_s, u"2.0.0"_s });
        CatalogItem plantedProject = plantedItem(projectId, u"project"_s);
        plantedProject.edition = Edition { u"org.example.one"_s, u"editable-project"_s };
        const Catalog catalog = plantedCatalog({ plantedGame, plantedProject });
        const InstallPlan plan = resolveDependencies(catalog, game.id, {});
        QVERIFY(!plan.ok);
        QVERIFY2(plan.error.contains(u"editions"_s), qPrintable(plan.error));
        // Even with the other edition already held.
        const InstallPlan held = resolveDependencies(
            catalog, game.id, QMap<QString, InstalledVersion> { { projectId, { u"1.0.0"_s, {} } } });
        QVERIFY(!held.ok);
        QVERIFY2(held.error.contains(u"editions"_s), qPrintable(held.error));
    }

    // However indirectly: through a pack in between, it still brings the other edition along.
    void anEditionCannotReachAnotherThroughSomethingElse()
    {
        const QJsonObject asGame { { u"group"_s, u"org.example.one"_s }, { u"role"_s, u"game"_s } };
        Pack first = gamePack();
        first.id = u"org.example.game.first"_s;
        first.edition = asGame;
        Pack middle;
        middle.id = u"org.example.pack.middle"_s;
        Pack second = gamePack();
        second.id = u"org.example.game.second"_s;
        second.edition = asGame;
        first.dependencies.append(rangeOf(middle.id, u"1.0.0"_s, u"2.0.0"_s));
        middle.dependencies.append(rangeOf(second.id, u"1.0.0"_s, u"2.0.0"_s));
        Shop shop;
        shop.add(first);
        shop.add(middle);
        shop.add(second);
        const CatalogResult result = shop.parse();
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(!result.catalog.item(first.id));
        QVERIFY(result.catalog.draft(first.id));
        QCOMPARE(result.catalog.draft(first.id)->code, u"edition-dependency"_s);
        QVERIFY(result.catalog.item(middle.id));
        QVERIFY(result.catalog.item(second.id));

        // The resolver sees it too, with no parser having looked.
        CatalogItem plantedFirst = plantedItem(first.id, u"game"_s);
        plantedFirst.edition = Edition { u"org.example.one"_s, u"game"_s };
        plantedFirst.dependencies.append(VersionRange { middle.id, u"1.0.0"_s, u"2.0.0"_s });
        CatalogItem plantedMiddle = plantedItem(middle.id, u"asset-pack"_s);
        plantedMiddle.dependencies.append(VersionRange { second.id, u"1.0.0"_s, u"2.0.0"_s });
        CatalogItem plantedSecond = plantedItem(second.id, u"game"_s);
        plantedSecond.edition = Edition { u"org.example.one"_s, u"game"_s };
        const InstallPlan plan =
            resolveDependencies(plantedCatalog({ plantedFirst, plantedMiddle, plantedSecond }), first.id, {});
        QVERIFY(!plan.ok);
        QVERIFY2(plan.error.contains(u"editions"_s), qPrintable(plan.error));
    }

    // ── Dependencies ─────────────────────────────────────────────────────────

    void aDependencyCycleRefusesTheCatalogue_data()
    {
        QTest::addColumn<QList<QStringList>>("edges");
        QTest::newRow("itself") << QList<QStringList> { { u"org.example.pack.a"_s, u"org.example.pack.a"_s } };
        QTest::newRow("two") << QList<QStringList> { { u"org.example.pack.a"_s, u"org.example.pack.b"_s },
                                                     { u"org.example.pack.b"_s, u"org.example.pack.a"_s } };
        QTest::newRow("three") << QList<QStringList> { { u"org.example.pack.a"_s, u"org.example.pack.b"_s },
                                                       { u"org.example.pack.b"_s, u"org.example.pack.c"_s },
                                                       { u"org.example.pack.c"_s, u"org.example.pack.a"_s } };
    }
    void aDependencyCycleRefusesTheCatalogue()
    {
        QFETCH(QList<QStringList>, edges);
        Shop shop;
        Catalog planted = plantedCatalog({});
        QMap<QString, Pack> packs;
        for (const QStringList &edge : edges) {
            packs[edge.at(0)].id = edge.at(0);
            packs[edge.at(0)].dependencies.append(rangeOf(edge.at(1), u"1.0.0"_s, u"2.0.0"_s));
            packs[edge.at(1)].id = edge.at(1);
        }
        for (const Pack &pack : packs) {
            shop.add(pack);
            CatalogItem item = plantedItem(pack.id, pack.kind);
            for (const QJsonValue &dependency : pack.dependencies) {
                const QJsonObject range = dependency.toObject();
                item.dependencies.append(VersionRange { range.value(u"id"_s).toString(),
                                                        range.value(u"min"_s).toString(),
                                                        range.value(u"below"_s).toString() });
            }
            planted.items.append(item);
        }
        const CatalogResult result = shop.parse();
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(u"depend on each other"_s), qPrintable(result.error));

        // The resolver finds it by itself as well: it must not rest on the parser having looked.
        const InstallPlan plan = resolveDependencies(planted, packs.firstKey(), {});
        QVERIFY(!plan.ok);
        QVERIFY2(plan.error.contains(u"depend on each other"_s), qPrintable(plan.error));
    }

    // However a catalogue happens to order them: a limit that depended on the order would be no limit.
    void aDependencyChainHasALimit_data()
    {
        QTest::addColumn<int>("length");
        QTest::addColumn<bool>("deepestFirst");
        QTest::addColumn<bool>("taken");
        QTest::newRow("32, top first") << 32 << false << true;
        QTest::newRow("32, deepest first") << 32 << true << true;
        QTest::newRow("33, top first") << 33 << false << false;
        QTest::newRow("33, deepest first") << 33 << true << false;
        QTest::newRow("40, deepest first") << 40 << true << false;
    }
    void aDependencyChainHasALimit()
    {
        QFETCH(int, length);
        QFETCH(bool, deepestFirst);
        QFETCH(bool, taken);
        Shop shop;
        Catalog planted = plantedCatalog({});
        for (int n = 0; n < length; ++n) {
            const int i = deepestFirst ? length - 1 - n : n;
            Pack pack;
            pack.id = u"org.example.pack.p%1"_s.arg(i);
            CatalogItem item = plantedItem(pack.id, pack.kind);
            if (i < length - 1) {
                pack.dependencies.append(rangeOf(u"org.example.pack.p%1"_s.arg(i + 1), u"1.0.0"_s, u"2.0.0"_s));
                item.dependencies.append(VersionRange { u"org.example.pack.p%1"_s.arg(i + 1), u"1.0.0"_s, u"2.0.0"_s });
            }
            shop.add(pack);
            planted.items.append(item);
        }
        const CatalogResult result = shop.parse();
        QCOMPARE(result.ok, taken);
        if (!taken)
            QVERIFY2(result.error.contains(u"too deep"_s), qPrintable(result.error));

        // The resolver holds to it by itself, with no parser having looked.
        const InstallPlan plan = resolveDependencies(planted, u"org.example.pack.p0"_s, {});
        QCOMPARE(plan.ok, taken);
        if (!taken)
            QVERIFY2(plan.error.contains(u"too deep"_s), qPrintable(plan.error));
    }

    void aMissingDependencyIsRefused()
    {
        Pack pack;
        pack.dependencies.append(rangeOf(u"org.example.pack.gone"_s, u"1.0.0"_s, u"2.0.0"_s));
        Shop shop;
        shop.add(pack);
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        const Outcome outcome = installAndWait(store, pack.id);
        QVERIFY(!outcome.started);
        QVERIFY(!outcome.ok);
        QVERIFY2(outcome.error.contains(u"does not list"_s), qPrintable(outcome.error));
        QVERIFY(store.installed().isEmpty());
    }

    void aDependencyOutOfItsRangeIsRefused_data()
    {
        QTest::addColumn<QString>("listed");
        QTest::newRow("below the range") << u"0.9.9"_s;
        QTest::newRow("at the upper bound, which is outside") << u"2.0.0"_s;
        QTest::newRow("above the range") << u"3.0.0"_s;
    }
    void aDependencyOutOfItsRangeIsRefused()
    {
        QFETCH(QString, listed);
        Pack base;
        base.id = u"org.example.pack.base"_s;
        base.version = listed;
        Pack pack;
        pack.dependencies.append(rangeOf(base.id, u"1.0.0"_s, u"2.0.0"_s));
        Shop shop;
        shop.add(base);
        shop.add(pack);
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        const Outcome outcome = installAndWait(store, pack.id);
        QVERIFY(!outcome.started);
        QVERIFY2(outcome.error.contains(u"this store lists"_s), qPrintable(outcome.error));
        QVERIFY(store.installed().isEmpty());
    }

    void dependenciesInstallFirstAndOnlyWhenNeeded()
    {
        Pack c;
        c.id = u"org.example.pack.c"_s;
        Pack b;
        b.id = u"org.example.pack.b"_s;
        b.dependencies.append(rangeOf(c.id, u"1.0.0"_s, u"2.0.0"_s));
        Pack d;
        d.id = u"org.example.pack.d"_s;
        d.dependencies.append(rangeOf(c.id, u"1.0.0"_s, u"2.0.0"_s));
        Pack a;
        a.id = u"org.example.pack.a"_s;
        a.dependencies.append(rangeOf(b.id, u"1.0.0"_s, u"2.0.0"_s));
        a.dependencies.append(rangeOf(d.id, u"1.0.0"_s, u"2.0.0"_s));
        Shop shop;
        shop.add(a);
        shop.add(b);
        shop.add(c);
        shop.add(d);

        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        // Diamond: c once, and before both of what needs it.
        const Outcome outcome = installAndWait(store, a.id);
        QVERIFY2(outcome.ok, qPrintable(outcome.error));
        QCOMPARE(outcome.order, QStringList({ c.id, b.id, d.id, a.id }));
        QCOMPARE(store.installed().size(), 4);
        for (const Pack *pack : { &a, &b, &c, &d }) {
            QVERIFY(store.isIntact(pack->id));
            QVERIFY(!store.isBusy(pack->id));
        }
        // What is there and in range is not fetched again.
        const InstallPlan again = resolveDependencies(
            store.catalog(), a.id,
            QMap<QString, InstalledVersion> { { b.id, { b.version, {} } }, { d.id, { d.version, {} } } });
        QVERIFY2(again.ok, qPrintable(again.error));
        QCOMPARE(again.order, QStringList({ a.id }));
    }

    // ── All or nothing ───────────────────────────────────────────────────────

    // What the review found: base moved, the step after it failed, and base stayed moved under what needed it.
    void aFailedUpdateMovesNothing_data()
    {
        QTest::addColumn<QString>("damaged");
        QTest::newRow("the first step fails") << u"org.example.pack.base"_s;
        QTest::newRow("the last step fails") << u"org.example.pack.dep"_s;
    }
    void aFailedUpdateMovesNothing()
    {
        QFETCH(QString, damaged);
        Pack base;
        base.id = u"org.example.pack.base"_s;
        Pack dependent;
        dependent.id = u"org.example.pack.dep"_s;
        dependent.dependencies.append(rangeOf(base.id, u"1.0.0"_s, u"2.0.0"_s));
        Pack unrelated;
        unrelated.id = u"org.example.pack.other"_s;
        Shop shop;
        shop.add(base);
        shop.add(dependent);
        shop.add(unrelated);
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, dependent.id).ok);
        const QByteArray baseManifest = readFile(store.installDirectory(base.id) + u"/manifest.json"_s);

        Pack base2 = base;
        base2.version = u"2.0.0"_s;
        Pack dependent2 = dependent;
        dependent2.version = u"2.0.0"_s;
        dependent2.dependencies = QJsonArray { rangeOf(base.id, u"2.0.0"_s, u"3.0.0"_s) };
        shop.items.clear();
        shop.add(base2);
        shop.add(dependent2);
        shop.add(unrelated);
        writeFile(shop.dir.path() + u"/packages/%1-2.0.0.qorpkg"_s.arg(damaged), "damaged");
        shop.sequence = 2;
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);

        const Outcome outcome = installAndWait(store, dependent2.id);
        QVERIFY(outcome.started);
        QVERIFY(!outcome.ok);
        QVERIFY(!outcome.error.isEmpty());
        QVERIFY(!store.isBusy(base.id));
        QVERIFY(!store.isBusy(dependent.id));

        // Exactly as it was, and on disk too.
        QCOMPARE(store.installed().value(base.id).version, u"1.0.0"_s);
        QCOMPARE(store.installed().value(dependent.id).version, u"1.0.0"_s);
        QCOMPARE(readFile(store.installDirectory(base.id) + u"/manifest.json"_s), baseManifest);
        QVERIFY(store.isIntact(base.id));
        QVERIFY(store.isIntact(dependent.id));
        QVERIFY(leftovers(store.dataDir()).isEmpty());
        CreatorStore after(data.path(), shop.store.publicKey());
        QCOMPARE(after.installed().value(base.id).version, u"1.0.0"_s);

        // And nothing else is held up by it.
        const Outcome other = installAndWait(store, unrelated.id);
        QVERIFY2(other.ok, qPrintable(other.error));
    }

    // Refused before anything moves: the dependency was the review's second way to the same place.
    void aListingOlderThanWhatIsInstalledMovesNothing()
    {
        Pack base;
        base.id = u"org.example.pack.base"_s;
        Pack dependent;
        dependent.id = u"org.example.pack.dep"_s;
        dependent.version = u"3.0.0"_s;
        dependent.dependencies.append(rangeOf(base.id, u"1.0.0"_s, u"2.0.0"_s));
        Shop shop;
        shop.add(base);
        shop.add(dependent);
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, dependent.id).ok);

        Pack base2 = base;
        base2.version = u"2.0.0"_s;
        Pack older = dependent;
        older.version = u"1.0.0"_s;
        older.dependencies = QJsonArray { rangeOf(base.id, u"2.0.0"_s, u"3.0.0"_s) };
        shop.items.clear();
        shop.add(base2);
        shop.add(older);
        shop.sequence = 2;
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);

        const Outcome outcome = installAndWait(store, older.id);
        QVERIFY(!outcome.ok);
        QVERIFY2(outcome.error.contains(u"newer"_s), qPrintable(outcome.error));
        QCOMPARE(store.installed().value(base.id).version, u"1.0.0"_s);
        QCOMPARE(store.installed().value(dependent.id).version, u"3.0.0"_s);
        QVERIFY(store.isIntact(base.id));
        QVERIFY(store.isIntact(dependent.id));
    }

    // What a crash leaves at every moment an install passes through, opened by a new engine: one state or the other,
    // whole.
    void aPlanSurvivesACrashAtEveryMoment_data()
    {
        QTest::addColumn<bool>("update");
        QTest::newRow("an update of two") << true;
        QTest::newRow("a first install of two") << false;
    }
    void aPlanSurvivesACrashAtEveryMoment()
    {
        QFETCH(bool, update);
        Pack base;
        base.id = u"org.example.pack.base"_s;
        base.files.insert(u"art/only-in-one.png"_s, kPng);
        Pack dependent;
        dependent.id = u"org.example.pack.dep"_s;
        dependent.dependencies.append(rangeOf(base.id, u"1.0.0"_s, u"2.0.0"_s));
        Pack base2 = base;
        base2.version = u"2.0.0"_s;
        base2.files.remove(u"art/only-in-one.png"_s);
        base2.files.insert(u"art/only-in-two.png"_s, kPng);
        Pack dependent2 = dependent;
        dependent2.version = u"2.0.0"_s;
        dependent2.dependencies = QJsonArray { rangeOf(base.id, u"2.0.0"_s, u"3.0.0"_s) };

        Shop shop;
        shop.add(base);
        shop.add(dependent);
        const QUrl oldCatalogue = shop.publish();
        QTemporaryDir data;
        const QByteArray key = shop.store.publicKey();
        PlantedEngine store(data.path(), key);
        store.setCreatorFeatures(true);
        if (update) {
            QCOMPARE(refreshed(store, oldCatalogue), StoreEngine::State::Ready);
            QVERIFY(installAndWait(store, dependent.id).ok);
        }
        shop.items.clear();
        shop.add(base2);
        shop.add(dependent2);
        shop.sequence = 2;
        const QUrl newCatalogue = shop.publish();
        QCOMPARE(refreshed(store, newCatalogue), StoreEngine::State::Ready);

        QTemporaryDir crashes;
        QStringList written; // whether each snapshot was taken before or after the record was written
        QList<PlantedEngine::Stage> stages;
        int taken = 0;
        bool recordWritten = false;
        store.onStage = [&](PlantedEngine::Stage stage) {
            if (stage == PlantedEngine::Stage::RecordWritten)
                recordWritten = true;
            copyFolder(data.path(), crashes.path() + u'/' + QString::number(taken++));
            written.append(recordWritten ? u"after"_s : u"before"_s);
            stages.append(stage);
            return true;
        };
        const Outcome outcome = installAndWait(store, dependent2.id);
        QVERIFY2(outcome.ok, qPrintable(outcome.error));
        store.onStage = nullptr;
        // The journal; set aside and swapped, for each of two; the record; the old ones cleared.
        QCOMPARE(taken, 8);
        QCOMPARE(written.count(u"before"_s), 5);

        // Opened by a new engine, a folder holds one state or the other, whole.
        const auto expectWhole = [&](const QString &folder, bool happened, const QString &label) {
            CreatorStore opened(folder, key);
            QCOMPARE(refreshed(opened, newCatalogue), StoreEngine::State::Ready);
            QVERIFY2(leftovers(folder).isEmpty(), qPrintable(label + u": "_s + leftovers(folder).join(u", "_s)));
            if (happened) {
                QCOMPARE(opened.installed().value(base.id).version, u"2.0.0"_s);
                QCOMPARE(opened.installed().value(dependent.id).version, u"2.0.0"_s);
                QVERIFY2(opened.isIntact(base.id), qPrintable(label));
                QVERIFY2(opened.isIntact(dependent.id), qPrintable(label));
                QVERIFY2(QFile::exists(opened.installDirectory(base.id) + u"/art/only-in-two.png"_s),
                         qPrintable(label));
            } else if (update) {
                QCOMPARE(opened.installed().value(base.id).version, u"1.0.0"_s);
                QCOMPARE(opened.installed().value(dependent.id).version, u"1.0.0"_s);
                QVERIFY2(opened.isIntact(base.id), qPrintable(label));
                QVERIFY2(opened.isIntact(dependent.id), qPrintable(label));
                QVERIFY2(QFile::exists(opened.installDirectory(base.id) + u"/art/only-in-one.png"_s),
                         qPrintable(label));
            } else {
                QVERIFY2(opened.installed().isEmpty(), qPrintable(label));
                QVERIFY2(!QFile::exists(opened.installDirectory(base.id)), qPrintable(label));
                QVERIFY2(!QFile::exists(opened.installDirectory(dependent.id)), qPrintable(label));
            }
        };

        QTemporaryDir work;
        int copies = 0;
        for (int i = 0; i < taken; ++i) {
            const QString snapshot = crashes.path() + u'/' + QString::number(i);
            const bool happened = written.at(i) == u"after"_s;
            const QString label = u"snapshot %1"_s.arg(i);

            // Once.
            const QString once = work.path() + u'/' + QString::number(copies++);
            copyFolder(snapshot, once);
            expectWhole(once, happened, label);
            if (QTest::currentTestFailed())
                return;

            // Twice over the same journal, as when it could not be removed, or the engine died before removing it.
            if (QFile::exists(snapshot + u"/plan-journal.json"_s)) {
                QVERIFY(QFile::copy(snapshot + u"/plan-journal.json"_s, once + u"/plan-journal.json"_s));
                expectWhole(once, happened, label + u", recovered twice"_s);
                if (QTest::currentTestFailed())
                    return;
            }

            // With the record lost as well: then what is on disk decides, and a plan whose every new version is in
            // place stands, rather than a first install that was recorded being deleted.
            const QString lost = work.path() + u'/' + QString::number(copies++);
            copyFolder(snapshot, lost);
            // Every snapshot has a record, a first install's too: one is written before any journal.
            QVERIFY(QFile::remove(lost + u"/installed.json"_s));
            const bool landed = i >= stages.lastIndexOf(PlantedEngine::Stage::ItemSwapped);
            expectWhole(lost, landed, label + u", its record lost"_s);
            if (QTest::currentTestFailed())
                return;

            // Cut short after each of its own steps, as a death would cut it, and finished by the next engine.
            for (int cut = 0;; ++cut) {
                const QString folder = work.path() + u'/' + QString::number(copies++);
                bool wasCut = false;
                {
                    PlantedEngine dying(folder, key); // made before the crash is copied in, so recovery waits for it
                    copyFolder(snapshot, folder);
                    int steps = 0;
                    dying.onStage = [&](PlantedEngine::Stage stage) {
                        if (stage != PlantedEngine::Stage::RecoveryStep)
                            return true;
                        wasCut = steps++ == cut;
                        return !wasCut;
                    };
                    (void)dying.remove(u"org.example.pack.none"_s); // takes the lock, and so recovers first
                }
                expectWhole(folder, happened, label + u", recovery cut after step %1"_s.arg(cut));
                if (QTest::currentTestFailed() || !wasCut)
                    break;
            }
            if (QTest::currentTestFailed())
                return;
        }
    }

    // A step failing at each moment before the record is written: everything goes back, in this process.
    void aFailureAtAnyMomentPutsEverythingBack_data()
    {
        QTest::addColumn<int>("failAt");
        for (int i = 0; i < 5; ++i)
            QTest::addRow("moment %d", i) << i;
    }
    void aFailureAtAnyMomentPutsEverythingBack()
    {
        QFETCH(int, failAt);
        Pack base;
        base.id = u"org.example.pack.base"_s;
        Pack dependent;
        dependent.id = u"org.example.pack.dep"_s;
        dependent.dependencies.append(rangeOf(base.id, u"1.0.0"_s, u"2.0.0"_s));
        Shop shop;
        shop.add(base);
        shop.add(dependent);
        QTemporaryDir data;
        PlantedEngine store(data.path(), shop.store.publicKey());
        store.setCreatorFeatures(true);
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, dependent.id).ok);

        Pack base2 = base;
        base2.version = u"2.0.0"_s;
        Pack dependent2 = dependent;
        dependent2.version = u"2.0.0"_s;
        dependent2.dependencies = QJsonArray { rangeOf(base.id, u"2.0.0"_s, u"3.0.0"_s) };
        shop.items.clear();
        shop.add(base2);
        shop.add(dependent2);
        shop.sequence = 2;
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);

        int moment = 0;
        store.onStage = [&](PlantedEngine::Stage) { return moment++ != failAt; };
        const Outcome outcome = installAndWait(store, dependent2.id);
        store.onStage = nullptr;
        QVERIFY(!outcome.ok);
        QVERIFY(!outcome.error.isEmpty());
        QCOMPARE(store.installed().value(base.id).version, u"1.0.0"_s);
        QCOMPARE(store.installed().value(dependent.id).version, u"1.0.0"_s);
        QVERIFY(store.isIntact(base.id));
        QVERIFY(store.isIntact(dependent.id));
        QVERIFY2(leftovers(store.dataDir()).isEmpty(), qPrintable(leftovers(store.dataDir()).join(u", "_s)));
    }

    void aRecordThatCannotBeWrittenPutsEverythingBack()
    {
        Pack v1;
        Pack v2 = v1;
        v2.version = u"1.1.0"_s;
        v2.files.insert(u"art/new.png"_s, kPng);
        Shop shop;
        shop.add(v1);
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, v1.id).ok);
        shop.items.clear();
        shop.add(v2);
        shop.sequence = 2;
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);

        // A folder where the record goes, which nothing can write over.
        const QString record = data.path() + u"/installed.json"_s;
        QVERIFY(QFile::remove(record));
        QVERIFY(QDir().mkpath(record));
        const Outcome outcome = installAndWait(store, v1.id);
        QVERIFY(!outcome.ok);
        QVERIFY2(outcome.error.contains(u"what is installed"_s), qPrintable(outcome.error));
        QVERIFY(!QFile::exists(store.installDirectory(v1.id) + u"/art/new.png"_s));
        QVERIFY(QFile::exists(store.installDirectory(v1.id) + u"/art/hero.png"_s));
        QVERIFY2(leftovers(store.dataDir()).isEmpty(), qPrintable(leftovers(store.dataDir()).join(u", "_s)));

        // With the record back, all of it is as it was.
        QVERIFY(QDir(record).removeRecursively());
        writeFile(record,
                  QJsonDocument(
                      QJsonObject {
                          { u"format"_s, 1 },
                          { u"items"_s, QJsonObject { { v1.id, QJsonObject { { u"kind"_s, v1.kind },
                                                                             { u"version"_s, v1.version },
                                                                             { u"developer"_s, v1.developer } } } } } })
                      .toJson());
        CreatorStore again(data.path(), shop.store.publicKey());
        QVERIFY(again.installed().contains(v1.id));
    }

    // A journal is a file anything running as this person can write, and is not to be followed out of the folder.
    void aHostileJournalReachesNothingOutside()
    {
        QTemporaryDir data;
        QTemporaryDir outside;
        writeFile(outside.path() + u"/mine.txt"_s, "mine");
        QDir().mkpath(data.path() + u"/items"_s);
        const QString climbing = QDir(data.path() + u"/items"_s).relativeFilePath(outside.path());
        const QJsonObject journal {
            { u"transaction"_s, u"abc"_s },
            { u"moves"_s,
              QJsonArray {
                  QJsonObject { { u"id"_s, climbing }, { u"staging"_s, climbing }, { u"previous"_s, climbing } },
                  QJsonObject { { u"id"_s, u"org.example.pack.art"_s },
                                { u"staging"_s, QString(climbing + u"/x"_s) },
                                { u"previous"_s, QString(u"org.example.pack.art.old-"_s + QString(32, u'a')) } } } }
        };
        writeFile(data.path() + u"/plan-journal.json"_s, QJsonDocument(journal).toJson());
        const StoreEngine store(data.path(), QByteArray(65, '\x04'));
        QCOMPARE(readFile(outside.path() + u"/mine.txt"_s), QByteArray("mine"));
        QVERIFY(!QFile::exists(data.path() + u"/plan-journal.json"_s));

        // And one that is not even JSON.
        writeFile(data.path() + u"/plan-journal.json"_s, "not json");
        const StoreEngine again(data.path(), QByteArray(65, '\x04'));
        QVERIFY(!QFile::exists(data.path() + u"/plan-journal.json"_s));
    }

    // A journal's move, each part of it on its own, named so that it would reach outside items/. Rolled back or
    // finished, nothing outside is moved or deleted, and what is installed under a good id is not touched for it:
    // neither an id that climbs, nor a good id with a name set aside after it that climbs, which only `<id>.old-<hex>`
    // exactly passes.
    void aJournalReachesNothingOutsideByAnyPartOfAMove_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<QString>("aside");
        QTest::addColumn<bool>("committed");
        const QString hex(32, u'a');
        for (const bool committed : { false, true }) {
            const char *how = committed ? "finished" : "put back";
            QTest::addRow("an id that climbs, %s", how)
                << u"../victim"_s << QString(u"../victim.old-"_s + hex) << committed;
            QTest::addRow("a good id, and a name set aside after it that climbs, %s", how)
                << u"org.example.pack.art"_s << QString(u"org.example.pack.art/../../victim.old-"_s + hex) << committed;
        }
    }
    void aJournalReachesNothingOutsideByAnyPartOfAMove()
    {
        QFETCH(QString, id);
        QFETCH(QString, aside);
        QFETCH(bool, committed);
        QTemporaryDir data;
        const QString victim = data.path() + u"/victim.old-"_s + QString(32, u'a');
        writeFile(victim + u"/mine.txt"_s, "mine");
        // Installed under the good id, as it would be, so that a path through it resolves.
        const QString installed = data.path() + u"/items/org.example.pack.art"_s;
        writeFile(installed + u"/kept.txt"_s, "kept");
        const QJsonObject journal {
            { u"transaction"_s, u"abc"_s },
            { u"moves"_s,
              QJsonArray { QJsonObject { { u"id"_s, id }, { u"staging"_s, u""_s }, { u"previous"_s, aside } } } }
        };
        writeFile(data.path() + u"/plan-journal.json"_s, QJsonDocument(journal).toJson());
        if (committed)
            writeFile(data.path() + u"/installed.json"_s, QJsonDocument(QJsonObject { { u"format"_s, 1 },
                                                                                      { u"transaction"_s, u"abc"_s },
                                                                                      { u"items"_s, QJsonObject {} } })
                                                              .toJson());
        const StoreEngine store(data.path(), QByteArray(65, '\x04'));
        QCOMPARE(readFile(victim + u"/mine.txt"_s), QByteArray("mine"));
        QVERIFY(!QFileInfo::exists(data.path() + u"/victim"_s));
        QCOMPARE(readFile(installed + u"/kept.txt"_s), QByteArray("kept"));
        QVERIFY(!QFile::exists(data.path() + u"/plan-journal.json"_s));
    }

#ifdef Q_OS_UNIX
    // Clearing up deletes no folder through a link to it: a link standing where an item, an old version or a staging
    // folder would be goes itself, and what it points to stays.
    void clearingUpNeverDeletesThroughALink_data()
    {
        QTest::addColumn<QString>("link");    // what in items/ is a link to a folder outside
        QTest::addColumn<QString>("staging"); // the journal's move, if there is a journal
        QTest::addColumn<QString>("previous");
        QTest::addColumn<bool>("asideIsReal"); // a real old version set aside, to come back
        QTest::addColumn<bool>("journal");
        QTest::addColumn<bool>("committed");
        const QString hex(32, u'b');
        const QString staging = u".staging-"_s + hex;
        const QString old = u"org.example.pack.art.old-"_s + hex;
        QTest::newRow("an item that is a link, its staging gone")
            << u"org.example.pack.art"_s << staging << QString() << false << true << false;
        QTest::newRow("an item that is a link, its old version set aside")
            << u"org.example.pack.art"_s << staging << old << true << true << false;
        QTest::newRow("an old version that is a link, finished") << old << QString() << old << false << true << true;
        QTest::newRow("a staging folder that is a link, with no journal")
            << staging << QString() << QString() << false << false << false;
    }
    void clearingUpNeverDeletesThroughALink()
    {
        QFETCH(QString, link);
        QFETCH(QString, staging);
        QFETCH(QString, previous);
        QFETCH(bool, asideIsReal);
        QFETCH(bool, journal);
        QFETCH(bool, committed);
        QTemporaryDir data;
        QTemporaryDir outside;
        writeFile(outside.path() + u"/mine.txt"_s, "mine");
        const QString items = data.path() + u"/items/"_s;
        QDir().mkpath(items);
        QVERIFY(QFile::link(outside.path(), items + link));
        if (asideIsReal)
            writeFile(items + previous + u"/old.txt"_s, "old");
        if (journal) {
            const QJsonObject move { { u"id"_s, u"org.example.pack.art"_s },
                                     { u"staging"_s, staging },
                                     { u"previous"_s, previous } };
            writeFile(data.path() + u"/plan-journal.json"_s,
                      QJsonDocument(QJsonObject { { u"transaction"_s, u"abc"_s }, { u"moves"_s, QJsonArray { move } } })
                          .toJson());
        }
        if (committed)
            writeFile(data.path() + u"/installed.json"_s, QJsonDocument(QJsonObject { { u"format"_s, 1 },
                                                                                      { u"transaction"_s, u"abc"_s },
                                                                                      { u"items"_s, QJsonObject {} } })
                                                              .toJson());
        const StoreEngine store(data.path(), QByteArray(65, '\x04'));
        QCOMPARE(readFile(outside.path() + u"/mine.txt"_s), QByteArray("mine"));
        if (asideIsReal)
            QCOMPARE(readFile(items + u"org.example.pack.art/old.txt"_s), QByteArray("old"));
        else
            QVERIFY(!QFileInfo(items + link).isSymLink());
        QVERIFY2(leftovers(data.path()).isEmpty(), qPrintable(leftovers(data.path()).join(u", "_s)));
    }
#endif

    void aRemovalWhoseRecordCannotBeWrittenPutsItBack()
    {
        const Pack pack;
        Shop shop;
        shop.add(pack);
        QTemporaryDir data;
        PlantedEngine store(data.path(), shop.store.publicKey());
        store.setCreatorFeatures(true);
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, pack.id).ok);

        store.refuseRecords = true;
        QSignalSpy finished(&store, &StoreEngine::finished);
        QVERIFY(!store.remove(pack.id));
        QCOMPARE(finished.count(), 1);
        QVERIFY2(finished.at(0).at(2).toString().contains(u"what is installed"_s),
                 qPrintable(finished.at(0).at(2).toString()));
        QVERIFY(store.installed().contains(pack.id));
        QVERIFY(store.isIntact(pack.id));
        QVERIFY2(leftovers(store.dataDir()).isEmpty(), qPrintable(leftovers(store.dataDir()).join(u", "_s)));

        store.refuseRecords = false;
        QVERIFY(store.remove(pack.id));
    }

    // A store that goes while installs are on their way ends every one of them, and says so for each. Ending one takes
    // its jobs out of the list being walked, which must not make the walk miss any.
    void aStoreThatGoesEndsEveryInstallOnItsWay()
    {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        constexpr int kInstalls = 160;
        TinyServer server;
        QList<CatalogItem> items;
        for (int i = 0; i < kInstalls; ++i) {
            CatalogItem item = plantedItem(u"org.example.pack.p%1"_s.arg(i), u"asset-pack"_s);
            item.packageFile = u"packages/p%1.qorpkg"_s.arg(i);
            server.held.insert(item.packageFile);
            items.append(item);
        }
        QTemporaryDir data;
        auto store = std::make_unique<PlantedEngine>(data.path(), QByteArray(65, '\x04'));
        store->setCreatorFeatures(true);
        store->setCatalogUrl(server.url(u"catalog.json"_s));
        store->adoptCatalog(plantedCatalog(items));
        QSignalSpy finished(store.get(), &StoreEngine::finished);
        for (const CatalogItem &item : std::as_const(items))
            QVERIFY(store->install(item.id));
        QTRY_VERIFY(server.waiting() > 0);
        QCOMPARE(finished.count(), 0);

        store.reset();
        QCOMPARE(finished.count(), kInstalls);
        QSet<QString> ended;
        for (const QList<QVariant> &arguments : std::as_const(finished)) {
            QVERIFY(!arguments.at(1).toBool());
            ended.insert(arguments.at(0).toString());
        }
        QCOMPARE(ended.size(), kInstalls);
    }

    // However long a chain a store-signed catalogue holds, walking it never goes deeper than the limit, so a small
    // stack is enough. Without that, the walk would go as deep as the chain before it found out.
    void aLongChainIsWalkedOnASmallStack()
    {
        constexpr int kLength = 30000;
        Catalog planted = plantedCatalog({});
        for (int i = 0; i < kLength; ++i) {
            CatalogItem item = plantedItem(u"org.example.pack.p%1"_s.arg(i), u"asset-pack"_s);
            if (i < kLength - 1)
                item.dependencies.append(VersionRange { u"org.example.pack.p%1"_s.arg(i + 1), u"1.0.0"_s, u"2.0.0"_s });
            planted.items.append(item);
        }
        InstallPlan plan;
        std::unique_ptr<QThread> thread(
            QThread::create([&] { plan = resolveDependencies(planted, u"org.example.pack.p0"_s, {}); }));
        thread->setStackSize(512 * 1024);
        thread->start();
        QVERIFY(thread->wait(60000));
        QVERIFY(!plan.ok);
        QVERIFY2(plan.error.contains(u"too deep"_s), qPrintable(plan.error));
    }

    // The record of what is installed lost or unreadable: what the folders hold stands in for it, so an id keeps its
    // version against a catalogue that would move it back.
    void aLostRecordStillKeepsEachIdMovingForward_data()
    {
        QTest::addColumn<bool>("unreadable");
        QTest::newRow("unreadable") << true;
        QTest::newRow("gone") << false;
    }
    void aLostRecordStillKeepsEachIdMovingForward()
    {
        QFETCH(bool, unreadable);
        Pack v2;
        v2.version = u"2.0.0"_s;
        const Pack v1;
        Shop shop;
        shop.add(v2);
        QTemporaryDir data;
        const QByteArray key = shop.store.publicKey();
        {
            CreatorStore store(data.path(), key);
            QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
            QVERIFY(installAndWait(store, v2.id).ok);
        }
        const QString record = data.path() + u"/installed.json"_s;
        if (unreadable)
            writeFile(record, "{ not a record");
        else
            QVERIFY(QFile::remove(record));

        shop.items.clear();
        shop.add(v1);
        shop.sequence = 2;
        CreatorStore store(data.path(), key);
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QCOMPARE(store.installed().value(v1.id).version, u"2.0.0"_s);
        QCOMPARE(store.installed().value(v1.id).developer, kDeveloper);
        const Outcome outcome = installAndWait(store, v1.id);
        QVERIFY(!outcome.ok);
        QVERIFY2(outcome.error.contains(u"newer"_s), qPrintable(outcome.error));
        QVERIFY(store.isIntact(v1.id));
        QCOMPARE(store.installed().value(v1.id).version, u"2.0.0"_s);
        QVERIFY(store.installed().value(v1.id).claimed);

        // The next install writes the record again, and what was taken from the manifest says so there.
        Pack other;
        other.id = u"org.example.pack.other"_s;
        shop.add(other);
        shop.sequence = 3;
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, other.id).ok);
        const QJsonObject items =
            QJsonDocument::fromJson(readFile(data.path() + u"/installed.json"_s)).object().value(u"items"_s).toObject();
        QVERIFY(items.value(v1.id).toObject().value(u"claimed"_s).toBool());
        QVERIFY(!items.value(other.id).toObject().contains(u"claimed"_s));
        QVERIFY(!store.installed().value(other.id).claimed);
    }

    // Plans started together wait together, so what they hold in memory is bounded together, and what each held is let
    // go when it ends.
    void plansStartedTogetherShareOneLimit()
    {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        Pack a1;
        a1.id = u"org.example.pack.a1"_s;
        Pack a;
        a.id = u"org.example.pack.a"_s;
        a.dependencies.append(rangeOf(a1.id, u"1.0.0"_s, u"2.0.0"_s));
        Pack b1;
        b1.id = u"org.example.pack.b1"_s;
        Pack b;
        b.id = u"org.example.pack.b"_s;
        b.dependencies.append(rangeOf(b1.id, u"1.0.0"_s, u"2.0.0"_s));
        Shop shop;
        TinyServer server;
        for (const Pack *pack : { &a1, &a, &b1, &b }) {
            const QByteArray bytes = pack->package(shop.developer);
            shop.addPackage(*pack, bytes);
            server.files.insert(u"packages/%1-1.0.0.qorpkg"_s.arg(pack->id), bytes);
        }
        const QByteArray json = shop.json();
        server.files.insert(u"catalog.json"_s, json);
        server.files.insert(u"catalog.json.sig"_s, shop.store.sign(json));
        QMap<QString, qint64> unpacked;
        QMap<QString, qint64> sent; // what each download holds as it arrives: the package itself, a little larger
        for (const QJsonObject &listing : std::as_const(shop.items)) {
            const QString id = listing.value(u"id"_s).toString();
            unpacked.insert(id, listing.value(u"package"_s).toObject().value(u"unpacked"_s).toInteger());
            sent.insert(id, server.files.value(u"packages/%1-1.0.0.qorpkg"_s.arg(id)).size());
        }

        QTemporaryDir data;
        PlantedEngine store(data.path(), shop.store.publicKey());
        store.setCreatorFeatures(true);
        // Room for either plan whole, each package counted as it arrives and then as it unpacks, and for both first
        // packages on their way at once, and not for both plans' first packages and a second on its way.
        const auto u = [&unpacked](const Pack &pack) { return unpacked.value(pack.id); };
        const auto s = [&sent](const Pack &pack) { return sent.value(pack.id); };
        store.heldLimit = std::max(
            { u(a1) + u(a), u(b1) + u(b), u(a1) + s(a), u(b1) + s(b), s(a1) + s(b1), u(a1) + s(b1), u(b1) + s(a1) });
        QVERIFY2(u(a1) + u(b1) + std::min(s(a), s(b)) > store.heldLimit,
                 "The packages are too small beside their headers for this test to say anything.");
        QCOMPARE(refreshed(store, server.url(u"catalog.json"_s)), StoreEngine::State::Ready);
        server.held = { u"packages/org.example.pack.a-1.0.0.qorpkg"_s, u"packages/org.example.pack.b-1.0.0.qorpkg"_s };
        QSignalSpy finished(&store, &StoreEngine::finished);
        QVERIFY(store.install(a.id));
        QVERIFY(store.install(b.id));
        // Each plan's first package is in memory, waiting for its second.
        QTRY_COMPARE(server.waiting(), 2);
        QCOMPARE(finished.count(), 0);
        server.release();
        QTRY_COMPARE(finished.count(), 4);

        QStringList failed;
        QStringList installed;
        for (const QList<QVariant> &emission : std::as_const(finished)) {
            const QString id = emission.at(0).toString();
            (emission.at(1).toBool() ? installed : failed).append(id);
            if (!emission.at(1).toBool())
                QVERIFY2(emission.at(2).toString().contains(u"Too much is on its way at once"_s),
                         qPrintable(emission.at(2).toString()));
        }
        failed.sort();
        installed.sort();
        const bool aFailed = failed.contains(a.id);
        QCOMPARE(failed, aFailed ? QStringList({ a.id, a1.id }) : QStringList({ b.id, b1.id }));
        QCOMPARE(installed, aFailed ? QStringList({ b.id, b1.id }) : QStringList({ a.id, a1.id }));

        // What both held is let go: the one that stopped installs now, on its own.
        const Outcome again = installAndWait(store, aFailed ? a.id : b.id);
        QVERIFY2(again.ok, qPrintable(again.error));
    }

    // A package on its way counts as it arrives, so a download that would take more than is left stops there, before
    // it is held whole beside everything else. What it held is let go when it stops.
    void aDownloadCountsAgainstTheLimitAsItArrives()
    {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        const Pack pack;
        Shop shop;
        TinyServer server;
        const QByteArray bytes = pack.package(shop.developer);
        shop.addPackage(pack, bytes);
        server.files.insert(u"packages/%1-1.0.0.qorpkg"_s.arg(pack.id), bytes);
        const QByteArray json = shop.json();
        server.files.insert(u"catalog.json"_s, json);
        server.files.insert(u"catalog.json.sig"_s, shop.store.sign(json));
        const qint64 unpacked = shop.items.constFirst().value(u"package"_s).toObject().value(u"unpacked"_s).toInteger();
        QVERIFY(bytes.size() > unpacked);

        QTemporaryDir data;
        PlantedEngine store(data.path(), shop.store.publicKey());
        store.setCreatorFeatures(true);
        QCOMPARE(refreshed(store, server.url(u"catalog.json"_s)), StoreEngine::State::Ready);
        // Room for what it unpacks to, and not for the download itself.
        store.heldLimit = unpacked;
        const Outcome refused = installAndWait(store, pack.id);
        QVERIFY(!refused.ok);
        QVERIFY2(refused.error.contains(u"Too much is on its way at once"_s), qPrintable(refused.error));
        QVERIFY(!store.installed().contains(pack.id));

        // Room for exactly the download: what the refused one held was let go.
        store.heldLimit = bytes.size();
        const Outcome installed = installAndWait(store, pack.id);
        QVERIFY2(installed.ok, qPrintable(installed.error));
        QVERIFY(store.isIntact(pack.id));
    }

    // A licence is only in its package, so fetching one brings the whole package, and it counts against what every plan
    // may hold together as an install's download does.
    void aLicenceOnItsWayCountsAgainstTheLimit()
    {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        const Pack pack;
        Shop shop;
        TinyServer server;
        const QByteArray bytes = pack.package(shop.developer);
        shop.addPackage(pack, bytes);
        server.files.insert(u"packages/%1-1.0.0.qorpkg"_s.arg(pack.id), bytes);
        const QByteArray json = shop.json();
        server.files.insert(u"catalog.json"_s, json);
        server.files.insert(u"catalog.json.sig"_s, shop.store.sign(json));

        QTemporaryDir data;
        PlantedEngine store(data.path(), shop.store.publicKey());
        store.setCreatorFeatures(true);
        QCOMPARE(refreshed(store, server.url(u"catalog.json"_s)), StoreEngine::State::Ready);
        QSignalSpy fetched(&store, &StoreEngine::licenceFetched);
        store.heldLimit = bytes.size() - 1;
        QVERIFY(store.fetchLicence(pack.id));
        QTRY_COMPARE(fetched.count(), 1);
        QVERIFY(!fetched.at(0).at(1).toBool());
        QVERIFY2(fetched.at(0).at(3).toString().contains(u"Too much is on its way at once"_s),
                 qPrintable(fetched.at(0).at(3).toString()));

        // Room for exactly the download: what the refused one held was let go.
        store.heldLimit = bytes.size();
        QVERIFY(store.fetchLicence(pack.id));
        QTRY_COMPARE(fetched.count(), 2);
        QVERIFY2(fetched.at(1).at(1).toBool(), qPrintable(fetched.at(1).at(3).toString()));
        QCOMPARE(fetched.at(1).at(2).toString(), QString::fromUtf8(kLicence));
        QVERIFY(store.installed().isEmpty());
    }

    // Read from a local file, as a demonstration's catalogue is, a licence's package counts the same, before it is
    // read.
    void aLicenceReadFromAFileCountsAgainstTheLimit()
    {
        const Pack pack;
        Shop shop;
        const QByteArray bytes = pack.package(shop.developer);
        shop.addPackage(pack, bytes);
        QTemporaryDir data;
        PlantedEngine store(data.path(), shop.store.publicKey());
        store.setCreatorFeatures(true);
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QSignalSpy fetched(&store, &StoreEngine::licenceFetched);
        store.heldLimit = bytes.size() - 1;
        QVERIFY(store.fetchLicence(pack.id));
        QTRY_COMPARE(fetched.count(), 1);
        QVERIFY(!fetched.at(0).at(1).toBool());
        QVERIFY2(fetched.at(0).at(3).toString().contains(u"Too much is on its way at once"_s),
                 qPrintable(fetched.at(0).at(3).toString()));

        store.heldLimit = bytes.size();
        QVERIFY(store.fetchLicence(pack.id));
        QTRY_COMPARE(fetched.count(), 2);
        QVERIFY2(fetched.at(1).at(1).toBool(), qPrintable(fetched.at(1).at(3).toString()));
        QCOMPARE(fetched.at(1).at(2).toString(), QString::fromUtf8(kLicence));
    }

    // What moves is made durable at each moment it must be, where the system can: what was staged before the journal
    // says it is there, the journal before anything moves, the swap before the record, and the record before the old
    // versions go. The same for a removal, and for the memory of a catalogue's sequence.
    void theDiskIsToldAtEachMoment()
    {
        const Pack v1;
        Pack v2 = v1;
        v2.version = u"1.1.0"_s;
        v2.files.insert(u"art/more/new.png"_s, kPng);
        Shop shop;
        shop.add(v1);
        QTemporaryDir data;
        PlantedEngine store(data.path(), shop.store.publicKey());
        store.setCreatorFeatures(true);
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QCOMPARE(store.events, QStringList { u"sync ."_s });
        QVERIFY(installAndWait(store, v1.id).ok);
        shop.items.clear();
        shop.add(v2);
        shop.sequence = 2;
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);

        store.events.clear();
        store.onStage = [&store](PlantedEngine::Stage stage) {
            store.events.append(u"stage %1"_s.arg(int(stage)));
            return true;
        };
        QVERIFY(installAndWait(store, v2.id).ok);
        store.onStage = nullptr;
        const QStringList events = store.events;
        const qsizetype journal = events.indexOf(u"stage %1"_s.arg(int(PlantedEngine::Stage::JournalWritten)));
        const qsizetype record = events.indexOf(u"stage %1"_s.arg(int(PlantedEngine::Stage::RecordWritten)));
        const qsizetype swapped = events.lastIndexOf(u"stage %1"_s.arg(int(PlantedEngine::Stage::ItemSwapped)));
        QVERIFY2(journal >= 3 && swapped > journal && record > swapped, qPrintable(events.join(u", "_s)));
        // Every staged folder, then items/, then the folder the journal is in once it is written.
        static const QRegularExpression staged(u"^sync items/\\.staging-[0-9a-f]{32}(/art(/more)?)?\\z"_s);
        QCOMPARE(events.mid(0, journal - 2).filter(staged).size(), 3);
        QCOMPARE(events.at(journal - 2), u"sync items"_s);
        QCOMPARE(events.at(journal - 1), u"sync ."_s);
        // The swap, then the record.
        QCOMPARE(events.mid(swapped + 1, record - swapped - 1), QStringList({ u"sync items"_s, u"sync ."_s }));

        // A removal: the journal, the move, the record.
        store.events.clear();
        QVERIFY(store.remove(v2.id));
        QCOMPARE(store.events, QStringList({ u"sync ."_s, u"sync items"_s, u"sync ."_s }));
    }

    // While another program holds the store, a catalogue is not taken, so its sequence is not written over another's.
    // A lock whose holder lives is never taken from it, however old: where the file system has no locks of its own,
    // Qt would take one older than 30 seconds, and this lock is such a file, with nothing of the system's on it.
    void aCatalogueIsNotTakenWhileAnotherProgramHoldsTheStore_data()
    {
        QTest::addColumn<bool>("old");
        QTest::newRow("a holder that is alive") << false;
        QTest::newRow("a holder that is alive, an hour on, and no lock of the system's") << true;
    }
    void aCatalogueIsNotTakenWhileAnotherProgramHoldsTheStore()
    {
        QFETCH(bool, old);
        const Pack pack;
        Shop shop;
        shop.add(pack);
        const QUrl url = shop.publish();
        QTemporaryDir data;
        const QString path = data.path() + u"/.lock"_s;
        QLockFile holder(path);
        QVERIFY(holder.tryLock(0));
        if (old) {
            const QByteArray contents = readFile(path);
            holder.unlock();
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(contents), contents.size());
            QVERIFY(file.flush()); // or the write lands after the time is set, and sets it again
            QVERIFY(
                file.setFileTime(QDateTime::currentDateTimeUtc().addSecs(-3600), QFileDevice::FileModificationTime));
            file.close();
            QVERIFY(QFileInfo(path).lastModified(QTimeZone::UTC).secsTo(QDateTime::currentDateTimeUtc()) > 3000);
        }
        StoreEngine store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, url), StoreEngine::State::Failed);
        QVERIFY2(store.error().contains(u"busy"_s), qPrintable(store.error()));
        QCOMPARE(store.lastSequence(), 0);
        QVERIFY(QFile::exists(path));

        if (old)
            QVERIFY(QFile::remove(path));
        else
            holder.unlock();
        QCOMPARE(refreshed(store, url), StoreEngine::State::Ready);
        QCOMPARE(store.lastSequence(), 1);
    }

#ifdef Q_OS_UNIX
    // Putting back an update, the new folder cannot all be deleted, as when a scanner holds a file just written. Undo
    // says so, the journal stays, and the old version set aside is kept, the only good copy, until a later try
    // succeeds.
    void aRollbackThatCannotDeleteKeepsTheOldVersion()
    {
        const Pack v1;
        Pack v2 = v1;
        v2.version = u"1.1.0"_s;
        v2.files.insert(u"art/more/new.png"_s, kPng);
        Shop shop;
        shop.add(v1);
        QTemporaryDir data;
        const QByteArray key = shop.store.publicKey();
        PlantedEngine store(data.path(), key);
        store.setCreatorFeatures(true);
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, v1.id).ok);
        shop.items.clear();
        shop.add(v2);
        shop.sequence = 2;
        const QUrl second = shop.publish();
        QCOMPARE(refreshed(store, second), StoreEngine::State::Ready);

        const QString held = store.installDirectory(v1.id) + u"/art"_s;
        store.onStage = [&](PlantedEngine::Stage stage) {
            if (stage != PlantedEngine::Stage::ItemSwapped)
                return true;
            // The new version is in place. A file in it cannot be deleted, and the install fails here.
            QFile::setPermissions(held, QFileDevice::ReadOwner | QFileDevice::ExeOwner);
            return false;
        };
        const Outcome outcome = installAndWait(store, v2.id);
        store.onStage = nullptr;
        QVERIFY(!outcome.ok);
        QVERIFY(QFile::exists(data.path() + u"/plan-journal.json"_s));
        const QStringList aside = QDir(data.path() + u"/items"_s).entryList({ u"*.old-*"_s }, QDir::Dirs);
        QCOMPARE(aside.size(), 1);
        QVERIFY(holdsWhatItsManifestLists(data.path() + u"/items/"_s + aside.constFirst()));

        // Still held: a new engine tries again and keeps it all.
        {
            const StoreEngine again(data.path(), key);
            QVERIFY(QFile::exists(data.path() + u"/plan-journal.json"_s));
            QVERIFY(holdsWhatItsManifestLists(data.path() + u"/items/"_s + aside.constFirst()));
        }

        // Let go: the next engine finishes putting it back.
        QVERIFY(QFile::setPermissions(held, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        CreatorStore after(data.path(), key);
        QCOMPARE(refreshed(after, second), StoreEngine::State::Ready);
        QCOMPARE(after.installed().value(v1.id).version, u"1.0.0"_s);
        QVERIFY(after.isIntact(v1.id));
        QVERIFY2(leftovers(data.path()).isEmpty(), qPrintable(leftovers(data.path()).join(u", "_s)));
    }

    // A removal that is recorded but cannot all be deleted yet keeps its journal, and the next engine finishes deleting
    // it, rather than taking what is left for an old version to put back.
    void aRemovalThatCannotDeleteIsFinishedLater()
    {
        const Pack pack;
        Shop shop;
        shop.add(pack);
        QTemporaryDir data;
        const QByteArray key = shop.store.publicKey();
        const QString items = data.path() + u"/items/"_s;
        QString held;
        {
            CreatorStore store(data.path(), key);
            QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
            QVERIFY(installAndWait(store, pack.id).ok);
            held = store.installDirectory(pack.id) + u"/art"_s;
            QVERIFY(QFile::setPermissions(held, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
            QVERIFY(store.remove(pack.id));
            QVERIFY(!store.installed().contains(pack.id));
        }
        QVERIFY(QFile::exists(data.path() + u"/plan-journal.json"_s));
        const QStringList aside = QDir(items).entryList({ u"*.old-*"_s }, QDir::Dirs);
        QCOMPARE(aside.size(), 1);
        held = items + aside.constFirst() + u"/art"_s;

        // Still held: a new engine leaves it set aside, and puts nothing back under the id.
        {
            const StoreEngine again(data.path(), key);
            QVERIFY(!QFileInfo::exists(items + pack.id));
            QVERIFY(QFile::exists(data.path() + u"/plan-journal.json"_s));
        }
        QVERIFY(QFile::setPermissions(held, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        const StoreEngine after(data.path(), key);
        QVERIFY(!QFileInfo::exists(items + pack.id));
        QVERIFY(!after.installed().contains(pack.id));
        QVERIFY2(leftovers(data.path()).isEmpty(), qPrintable(leftovers(data.path()).join(u", "_s)));
    }
#endif

    // Once an update is recorded, its old version goes only if the new one is whole. If something broke it at once, the
    // old version stays set aside, and goes once a reinstall has made the new one whole.
    void anUpdateBrokenAsItLandsKeepsTheOldVersion()
    {
        const Pack v1;
        Pack v2 = v1;
        v2.version = u"1.1.0"_s;
        Shop shop;
        shop.add(v1);
        QTemporaryDir data;
        const QByteArray key = shop.store.publicKey();
        PlantedEngine store(data.path(), key);
        store.setCreatorFeatures(true);
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, v1.id).ok);
        shop.items.clear();
        shop.add(v2);
        shop.sequence = 2;
        const QUrl second = shop.publish();
        QCOMPARE(refreshed(store, second), StoreEngine::State::Ready);

        store.onStage = [&](PlantedEngine::Stage stage) {
            if (stage == PlantedEngine::Stage::RecordWritten)
                QFile::remove(store.installDirectory(v1.id) + u"/art/hero.png"_s); // as a scanner might take it
            return true;
        };
        QVERIFY(installAndWait(store, v2.id).ok);
        store.onStage = nullptr;
        const QString items = data.path() + u"/items/"_s;
        QVERIFY(!holdsWhatItsManifestLists(items + v1.id));
        const QStringList aside = QDir(items).entryList({ u"*.old-*"_s }, QDir::Dirs);
        QCOMPARE(aside.size(), 1);
        QVERIFY(holdsWhatItsManifestLists(items + aside.constFirst()));
        QVERIFY(!QFile::exists(data.path() + u"/plan-journal.json"_s));

        // A new engine keeps it too, while what stands in its place is broken.
        {
            const StoreEngine again(data.path(), key);
            QVERIFY(holdsWhatItsManifestLists(items + aside.constFirst()));
        }
        // Installed again, the new version is whole, and the next engine lets the old one go.
        {
            CreatorStore after(data.path(), key);
            QCOMPARE(refreshed(after, second), StoreEngine::State::Ready);
            QVERIFY(!after.isIntact(v1.id));
            QVERIFY(installAndWait(after, v1.id).ok);
            QVERIFY(after.isIntact(v1.id));
        }
        const StoreEngine last(data.path(), key);
        QVERIFY2(leftovers(data.path()).isEmpty(), qPrintable(leftovers(data.path()).join(u", "_s)));
        QVERIFY(holdsWhatItsManifestLists(items + v1.id));
    }

    // An old version set aside is let go only when what stands under its id is whole, journal or none: otherwise it may
    // be the only good copy.
    void anOldVersionIsKeptWhileItsPlaceHoldsSomethingBroken_data()
    {
        QTest::addColumn<QString>("journal"); // none, finished, or not known to be, its record lost
        QTest::addColumn<bool>("broken");
        QTest::addColumn<bool>("kept");       // the old version still set aside
        QTest::addColumn<bool>("placeWhole"); // what stands under the id after
        QTest::newRow("no journal, its place whole") << u"none"_s << false << false << true;
        QTest::newRow("no journal, its place broken") << u"none"_s << true << true << false;
        QTest::newRow("a finished journal, its place whole") << u"finished"_s << false << false << true;
        QTest::newRow("a finished journal, its place broken") << u"finished"_s << true << true << false;
        // With the record lost, a broken place is a plan that did not land, so the old version goes back.
        QTest::newRow("a journal with its record lost, its place whole") << u"lost"_s << false << false << true;
        QTest::newRow("a journal with its record lost, its place broken") << u"lost"_s << true << false << true;
    }
    void anOldVersionIsKeptWhileItsPlaceHoldsSomethingBroken()
    {
        QFETCH(QString, journal);
        QFETCH(bool, broken);
        QFETCH(bool, kept);
        QFETCH(bool, placeWhole);
        const Pack pack;
        Shop shop;
        shop.add(pack);
        QTemporaryDir data;
        const QByteArray key = shop.store.publicKey();
        {
            CreatorStore store(data.path(), key);
            QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
            QVERIFY(installAndWait(store, pack.id).ok);
        }
        const QString items = data.path() + u"/items/"_s;
        const QString old = pack.id + u".old-"_s + QString(32, u'd');
        copyFolder(items + pack.id, items + old);
        if (broken)
            QVERIFY(QFile::remove(items + pack.id + u"/art/hero.png"_s));
        if (journal != u"none"_s) {
            const QJsonObject move { { u"id"_s, pack.id },
                                     { u"staging"_s, QString(u".staging-"_s + QString(32, u'd')) },
                                     { u"previous"_s, old } };
            writeFile(data.path() + u"/plan-journal.json"_s,
                      QJsonDocument(QJsonObject { { u"transaction"_s, u"abc"_s }, { u"moves"_s, QJsonArray { move } } })
                          .toJson());
            QJsonObject record = QJsonDocument::fromJson(readFile(data.path() + u"/installed.json"_s)).object();
            record.insert(u"transaction"_s, u"abc"_s);
            writeFile(data.path() + u"/installed.json"_s, QJsonDocument(record).toJson());
            if (journal == u"lost"_s)
                QVERIFY(QFile::remove(data.path() + u"/installed.json"_s));
        }
        const StoreEngine store(data.path(), key);
        QCOMPARE(QFileInfo::exists(items + old), kept);
        if (kept)
            QVERIFY(holdsWhatItsManifestLists(items + old));
        QCOMPARE(holdsWhatItsManifestLists(items + pack.id), placeWhole);
        QVERIFY(!QFile::exists(data.path() + u"/plan-journal.json"_s));
    }

#ifdef Q_OS_UNIX
    // A tree planted where clearing up looks, deeper than the handles a process may hold: it is cleared with one
    // listing open at a time.
    void aDeepTreeIsClearedWithFewHandles()
    {
        QTemporaryDir data;
        const QString top = data.path() + u"/items/.staging-"_s + QString(32, u'e');
        // Each level holds files beside the next level, written before it: a listing read to its end lets its handle
        // go, so a walk that holds one open while it goes deeper is seen only where more is left to read.
        QString deep = top;
        QVERIFY(QDir().mkpath(deep));
        for (int i = 0; i < 1000; ++i) {
            for (int k = 0; k < 3; ++k)
                writeFile(deep + u"/f%1-%2"_s.arg(i).arg(k), "x");
            deep += u"/a"_s;
            QVERIFY(QDir().mkdir(deep));
        }
        rlimit before {};
        QCOMPARE(getrlimit(RLIMIT_NOFILE, &before), 0);
        if (before.rlim_cur < 256)
            QSKIP("This system allows a process fewer than 256 open files already.");
        rlimit tight = before;
        tight.rlim_cur = 256;
        QCOMPARE(setrlimit(RLIMIT_NOFILE, &tight), 0);
        {
            const StoreEngine store(data.path(), QByteArray(65, '\x04'));
        }
        QCOMPARE(setrlimit(RLIMIT_NOFILE, &before), 0);
        QVERIFY(!QFileInfo::exists(top));
    }
#endif

    // Whatever is already wrong, a block is about what it affects, so nothing else is kept from installing.
    void aBrokenInstallBlocksOnlyWhatItAffects()
    {
        Pack base;
        base.id = u"org.example.pack.base"_s;
        Pack dependent;
        dependent.id = u"org.example.pack.dep"_s;
        dependent.dependencies.append(rangeOf(base.id, u"1.0.0"_s, u"2.0.0"_s));
        Pack unrelated;
        unrelated.id = u"org.example.pack.other"_s;
        Shop shop;
        shop.add(base);
        shop.add(dependent);
        shop.add(unrelated);
        const QUrl catalogue = shop.publish();
        QTemporaryDir data;
        const QByteArray key = shop.store.publicKey();
        {
            CreatorStore store(data.path(), key);
            QCOMPARE(refreshed(store, catalogue), StoreEngine::State::Ready);
            QVERIFY(installAndWait(store, dependent.id).ok);
        }
        // As an older version of this code could leave it, or a person by hand: base moved out from under what needs
        // it.
        const QString record = data.path() + u"/installed.json"_s;
        QByteArray bytes = readFile(record);
        QJsonObject root = QJsonDocument::fromJson(bytes).object();
        QJsonObject items = root.value(u"items"_s).toObject();
        QJsonObject entry = items.value(base.id).toObject();
        entry.insert(u"version"_s, u"2.5.0"_s);
        items.insert(base.id, entry);
        root.insert(u"items"_s, items);
        writeFile(record, QJsonDocument(root).toJson());

        CreatorStore store(data.path(), key);
        QCOMPARE(refreshed(store, catalogue), StoreEngine::State::Ready);
        // Unrelated: goes ahead.
        const Outcome other = installAndWait(store, unrelated.id);
        QVERIFY2(other.ok, qPrintable(other.error));
        // A range that does not hold is not kept holding by refusing this.
        QVERIFY(store.remove(base.id));
        QVERIFY(!store.installed().contains(base.id));
    }

    // Removed, or moved, while a plan was on its way. It is checked against what is there when it lands.
    void aDependencyRemovedInFlightStopsThePlan()
    {
        Pack base;
        base.id = u"org.example.pack.base"_s;
        Pack later;
        later.id = u"org.example.pack.later"_s;
        later.dependencies.append(rangeOf(base.id, u"1.0.0"_s, u"2.0.0"_s));
        Shop shop;
        shop.add(base);
        shop.add(later);
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, base.id).ok);

        QSignalSpy finished(&store, &StoreEngine::finished);
        QVERIFY(store.install(later.id));
        QVERIFY(store.remove(base.id));
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(!finished.at(0).at(1).toBool());
        QVERIFY2(finished.at(0).at(2).toString().contains(u"would not leave it"_s),
                 qPrintable(finished.at(0).at(2).toString()));
        QVERIFY(!store.installed().contains(later.id));
        QVERIFY(!QFile::exists(store.installDirectory(later.id)));
    }

    void aDependencyMovedInFlightStopsWhicheverLandsSecond()
    {
        Pack base;
        base.id = u"org.example.pack.base"_s;
        Pack base2 = base;
        base2.version = u"2.0.0"_s;
        Pack later;
        later.id = u"org.example.pack.later"_s;
        later.dependencies.append(rangeOf(base.id, u"1.0.0"_s, u"2.0.0"_s));
        Shop shop;
        shop.add(base);
        const QUrl first = shop.publish();
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, first), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, base.id).ok);
        shop.items.clear();
        shop.add(base2);
        shop.add(later);
        shop.sequence = 2;
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);

        QSignalSpy finished(&store, &StoreEngine::finished);
        QVERIFY(store.install(later.id));
        QVERIFY(store.install(base.id));
        QTRY_COMPARE(finished.count(), 2);
        int landed = 0;
        for (const QList<QVariant> &emission : std::as_const(finished))
            landed += emission.at(1).toBool() ? 1 : 0;
        QCOMPARE(landed, 1);
        // Whichever it was, what stands holds together.
        const auto installed = store.installed();
        if (installed.contains(later.id))
            QCOMPARE(installed.value(base.id).version, u"1.0.0"_s);
        else
            QCOMPARE(installed.value(base.id).version, u"2.0.0"_s);
    }

    void aCatalogueChangingStopsAPlan()
    {
        Pack base;
        base.id = u"org.example.pack.base"_s;
        Pack dependent;
        dependent.id = u"org.example.pack.dep"_s;
        dependent.dependencies.append(rangeOf(base.id, u"1.0.0"_s, u"2.0.0"_s));
        Shop shop;
        Catalog catalog =
            plantedCatalog({ plantedPackage(shop, base, { Placement { View::Creator, u"Things"_s } }),
                             plantedPackage(shop, dependent, { Placement { View::Creator, u"Things"_s } }) });
        listDeveloper(&catalog, shop);
        QTemporaryDir data;
        PlantedEngine store(data.path(), shop.store.publicKey());
        store.setCreatorFeatures(true);
        store.setCatalogUrl(QUrl::fromLocalFile(shop.dir.path() + u"/catalog.json"_s));
        store.adoptCatalog(catalog);

        QSignalSpy finished(&store, &StoreEngine::finished);
        QVERIFY(store.install(dependent.id));
        store.adoptCatalog(catalog); // another catalogue, as far as a plan can tell
        QTRY_COMPARE(finished.count(), 2);
        for (const QList<QVariant> &emission : std::as_const(finished)) {
            QVERIFY(!emission.at(1).toBool());
            QVERIFY2(emission.at(2).toString().contains(u"catalogue changed"_s), qPrintable(emission.at(2).toString()));
        }
        QVERIFY(store.installed().isEmpty());
        QVERIFY(!store.isBusy(base.id));
        QVERIFY(!store.isBusy(dependent.id));
    }

    // ── Another program in the same folder ───────────────────────────────────

    void anotherProgramsProgressIsRespected()
    {
        const Pack pack;
        Pack other;
        other.id = u"org.example.pack.other"_s;
        Shop shop;
        shop.add(pack);
        shop.add(other);
        QTemporaryDir data;
        const QByteArray key = shop.store.publicKey();
        // This one started first, and has not heard of anything since.
        CreatorStore stale(data.path(), key);
        shop.sequence = 7;
        const QUrl seven = shop.publish();
        {
            CreatorStore second(data.path(), key);
            QCOMPARE(refreshed(second, seven), StoreEngine::State::Ready);
            QVERIFY(installAndWait(second, pack.id).ok);
        }
        // Its memory is what is on disk, not what it loaded.
        shop.sequence = 6;
        QCOMPARE(refreshed(stale, shop.publish()), StoreEngine::State::Failed);
        QVERIFY2(stale.error().contains(u"older"_s), qPrintable(stale.error()));
        QCOMPARE(stale.lastSequence(), 7);

        // Nor does it write its stale record over the other's.
        shop.sequence = 7;
        QCOMPARE(refreshed(stale, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(stale, other.id).ok);
        CreatorStore after(data.path(), key);
        QVERIFY(after.installed().contains(pack.id));
        QVERIFY(after.installed().contains(other.id));
    }

    void aRemovalLeavesAnotherProgramsRecords()
    {
        const Pack pack;
        Pack other;
        other.id = u"org.example.pack.other"_s;
        Shop shop;
        shop.add(pack);
        shop.add(other);
        const QUrl catalogue = shop.publish();
        QTemporaryDir data;
        const QByteArray key = shop.store.publicKey();
        CreatorStore stale(data.path(), key);
        QCOMPARE(refreshed(stale, catalogue), StoreEngine::State::Ready);
        QVERIFY(installAndWait(stale, pack.id).ok);
        {
            CreatorStore second(data.path(), key);
            QCOMPARE(refreshed(second, catalogue), StoreEngine::State::Ready);
            QVERIFY(installAndWait(second, other.id).ok);
        }
        QVERIFY(!stale.installed().contains(other.id));
        QVERIFY(stale.remove(pack.id));
        QVERIFY(stale.installed().contains(other.id));

        CreatorStore after(data.path(), key);
        QVERIFY(!after.installed().contains(pack.id));
        QVERIFY(after.installed().contains(other.id));
        QVERIFY(QFileInfo::exists(after.installDirectory(other.id) + u"/manifest.json"_s));
    }

    void anotherProgramsNewerVersionIsNotMovedBack()
    {
        Pack v1;
        Pack v2 = v1;
        v2.version = u"2.0.0"_s;
        Shop shop;
        shop.add(v1);
        const QUrl older = shop.publish();
        QTemporaryDir data;
        const QByteArray key = shop.store.publicKey();
        CreatorStore stale(data.path(), key);
        QCOMPARE(refreshed(stale, older), StoreEngine::State::Ready);

        // Another program takes a newer catalogue and the newer version, in the same folder.
        shop.items.clear();
        shop.add(v2);
        shop.sequence = 2;
        const QUrl current = shop.publish();
        {
            CreatorStore second(data.path(), key);
            QCOMPARE(refreshed(second, current), StoreEngine::State::Ready);
            QVERIFY(installAndWait(second, v2.id).ok);
        }
        // The stale engine still holds the catalogue it read, and believes nothing is installed.
        const Outcome outcome = installAndWait(stale, v1.id);
        QVERIFY(!outcome.ok);
        QVERIFY2(outcome.error.contains(u"newer"_s), qPrintable(outcome.error));
        CreatorStore after(data.path(), key);
        QCOMPARE(after.installed().value(v1.id).version, u"2.0.0"_s);
    }

    void aRecordThatCannotBeReadIsSetAsideAndStopsNothing()
    {
        const Pack pack;
        Shop shop;
        shop.add(pack);
        QTemporaryDir data;
        writeFile(data.path() + u"/installed.json"_s, "not json");
        CreatorStore store(data.path(), shop.store.publicKey());
        QVERIFY(store.installed().isEmpty());
        QVERIFY(QFile::exists(data.path() + u"/installed.json.unreadable"_s));
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        const Outcome outcome = installAndWait(store, pack.id);
        QVERIFY2(outcome.ok, qPrintable(outcome.error));
    }

    // However old it looks. Qt will not take a lock whose holder is alive (QLockFilePrivate::removeStaleLock), so a
    // slow install of another program's is safe from a program starting up.
    void aLockIsLeftAloneWhileItsHolderLives()
    {
        QTemporaryDir base;
        const QString staging = base.path() + u"/items/.staging-"_s + QString(32, u'a');
        writeFile(staging + u"/art/hero.png"_s, "x");
        QLockFile other(base.path() + u"/.lock"_s);
        QVERIFY(other.tryLock(0));
#ifndef Q_OS_WIN // Windows keeps a lock file open to its holder, so its age cannot be set from here
        QFile file(base.path() + u"/.lock"_s);
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(QDateTime::currentDateTimeUtc().addDays(-1), QFileDevice::FileModificationTime));
        file.close();
#endif
        const StoreEngine store(base.path(), QByteArray(65, '\x04'));
        QVERIFY(QFileInfo::exists(staging));
    }

    // Left by a crash, its holder gone: it stops nothing, and what the crash left is cleared.
    void aLockLeftByACrashDoesNotStopRecovery()
    {
        QTemporaryDir base;
        const QString staging = base.path() + u"/items/.staging-"_s + QString(32, u'a');
        writeFile(staging + u"/art/hero.png"_s, "x");
        writeFile(base.path() + u"/.lock"_s, "2147483646\nsomeprogram\n\n");
        const StoreEngine store(base.path(), QByteArray(65, '\x04'));
        QVERIFY(!QFileInfo::exists(staging));
    }

    // ── Replay memory, for each store key ────────────────────────────────────

    void eachStoreKeyHasItsOwnMemory()
    {
        const Pack pack;
        Shop demonstration;
        demonstration.add(pack);
        demonstration.sequence = 5;
        QTemporaryDir data;
        {
            StoreEngine store(data.path(), demonstration.store.publicKey());
            QCOMPARE(refreshed(store, demonstration.publish()), StoreEngine::State::Ready);
            QCOMPARE(store.lastSequence(), 5);
        }
        // A build with another key, in the same folder: format 1 as it always was, and nothing locked out.
        {
            StoreEngine normal(data.path(), publicKey(kFixtures + u"/store.pub"_s));
            QCOMPARE(normal.lastSequence(), 0);
            QCOMPARE(refreshed(normal, QUrl::fromLocalFile(kFixtures + u"/v1/catalog.json"_s)),
                     StoreEngine::State::Ready);
            QVERIFY(installAndWait(normal, u"org.example.theme.test"_s).ok);
        }
        // Another store's format 2, at a lower sequence than the first has seen.
        {
            Shop other;
            other.add(pack);
            other.sequence = 1;
            StoreEngine store(data.path(), other.store.publicKey());
            QCOMPARE(refreshed(store, other.publish()), StoreEngine::State::Ready);
            QCOMPARE(store.lastSequence(), 1);
        }
        // And the first's memory stands through all of it.
        StoreEngine back(data.path(), demonstration.store.publicKey());
        QCOMPARE(back.lastSequence(), 5);
        demonstration.sequence = 4;
        QCOMPARE(refreshed(back, demonstration.publish()), StoreEngine::State::Failed);
        QVERIFY2(back.error().contains(u"older"_s), qPrintable(back.error()));
    }

    // Unreadable is treated as deleted, which it cannot be told from. Refusing would be an outage with no security in
    // it.
    void aMemoryThatCannotBeReadStartsAgain()
    {
        const Pack pack;
        Shop shop;
        shop.add(pack);
        QTemporaryDir data;
        writeFile(data.path() + u"/catalog-state.json"_s, "not json");
        StoreEngine store(data.path(), shop.store.publicKey());
        QCOMPARE(store.lastSequence(), 0);
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QCOMPARE(store.lastSequence(), 1);
    }

    void aSequenceThatCannotBeRememberedIsNotTaken()
    {
        const Pack pack;
        Shop shop;
        shop.add(pack);
        QTemporaryDir data;
        QVERIFY(QDir().mkpath(data.path() + u"/catalog-state.json"_s));
        StoreEngine store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Failed);
        QVERIFY2(store.error().contains(u"could not remember"_s), qPrintable(store.error()));
        QVERIFY(store.catalog().items.isEmpty());
    }

    // ── Platforms and engines ────────────────────────────────────────────────

    void aListingForAnotherSystemIsRefused()
    {
        Pack pack;
        pack.platforms = QJsonArray { u"some-other-system"_s };
        Shop shop;
        shop.add(pack);
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        const Outcome outcome = installAndWait(store, pack.id);
        QVERIFY(!outcome.started);
        QVERIFY2(outcome.error.contains(u"not for this system"_s), qPrintable(outcome.error));
        QVERIFY(store.installed().isEmpty());

        // Listed for this one among others, it is fine.
        Pack both;
        both.id = u"org.example.pack.both"_s;
        both.platforms = QJsonArray { u"some-other-system"_s, currentPlatform() };
        shop.items.clear();
        shop.add(both);
        shop.sequence = 2;
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        const Outcome taken = installAndWait(store, both.id);
        QVERIFY2(taken.ok, qPrintable(taken.error));
    }

    void anEngineIsCheckedWhereItCanBe_data()
    {
        QTest::addColumn<QVariantMap>("told");
        QTest::addColumn<bool>("taken");
        QTest::newRow("told nothing, so it cannot say") << QVariantMap {} << true;
        QTest::newRow("in range") << QVariantMap { { u"qq"_s, u"0.5.0"_s } } << true;
        QTest::newRow("at the bottom of the range") << QVariantMap { { u"qq"_s, u"0.1.0"_s } } << true;
        QTest::newRow("at the top, which is outside") << QVariantMap { { u"qq"_s, u"1.0.0"_s } } << false;
        QTest::newRow("too old") << QVariantMap { { u"qq"_s, u"0.0.9"_s } } << false;
        QTest::newRow("only another engine") << QVariantMap { { u"other"_s, u"0.5.0"_s } } << false;
        QTest::newRow("that engine and another")
            << QVariantMap { { u"qq"_s, u"0.5.0"_s }, { u"other"_s, u"9.0.0"_s } } << true;
    }
    void anEngineIsCheckedWhereItCanBe()
    {
        QFETCH(QVariantMap, told);
        QFETCH(bool, taken);
        const Pack pack;
        Shop shop;
        shop.add(pack);
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QMap<QString, QString> engines;
        for (auto it = told.cbegin(); it != told.cend(); ++it)
            engines.insert(it.key(), it.value().toString());
        store.setEngines(engines);
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        const Outcome outcome = installAndWait(store, pack.id);
        QCOMPARE(outcome.ok, taken);
        if (!taken)
            QVERIFY2(outcome.error.contains(u"engine this system does not have"_s), qPrintable(outcome.error));
    }

    // ── An id is one thing ───────────────────────────────────────────────────

    void anIdKeepsItsDeveloperAndItsKind()
    {
        const Pack pack;
        Shop shop;
        shop.add(pack);
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, pack.id).ok);

        // The same id, as a game.
        Pack game = gamePack();
        game.id = pack.id;
        game.version = u"2.0.0"_s;
        shop.items.clear();
        shop.add(game);
        shop.sequence = 2;
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        Outcome outcome = installAndWait(store, game.id);
        QVERIFY(!outcome.ok);
        QVERIFY2(outcome.error.contains(u"installed as a asset-pack"_s), qPrintable(outcome.error));
        QCOMPARE(store.installed().value(pack.id).kind, u"asset-pack"_s);

        // The same id, from someone else, whose signature is good.
        TestSigner someoneElse;
        Pack taken = pack;
        taken.developer = u"org.other"_s;
        taken.version = u"3.0.0"_s;
        shop.items.clear();
        shop.addPackage(taken, taken.package(someoneElse));
        shop.sequence = 3;
        const QJsonObject developers {
            { kDeveloper, QJsonObject { { u"name"_s, u"Example"_s },
                                        { u"verified"_s, true },
                                        { u"key"_s, QString::fromLatin1(shop.developer.publicKey().toBase64()) } } },
            { u"org.other"_s, QJsonObject { { u"name"_s, u"Someone else"_s },
                                            { u"verified"_s, false },
                                            { u"key"_s, QString::fromLatin1(someoneElse.publicKey().toBase64()) } } }
        };
        const QByteArray bytes = shop.json({ { u"developers"_s, developers } });
        writeFile(shop.dir.path() + u"/catalog.json"_s, bytes);
        writeFile(shop.dir.path() + u"/catalog.json.sig"_s, shop.store.sign(bytes));
        QCOMPARE(refreshed(store, QUrl::fromLocalFile(shop.dir.path() + u"/catalog.json"_s)),
                 StoreEngine::State::Ready);
        outcome = installAndWait(store, taken.id);
        QVERIFY(!outcome.ok);
        QVERIFY2(outcome.error.contains(u"is installed from"_s), qPrintable(outcome.error));
        QCOMPARE(store.installed().value(pack.id).developer, kDeveloper);

        // Removed first, it is a new thing, and that is allowed.
        QVERIFY(store.remove(pack.id));
        outcome = installAndWait(store, taken.id);
        QVERIFY2(outcome.ok, qPrintable(outcome.error));
    }

    // ── Nothing added to what is installed ───────────────────────────────────

    void anAddedFolderOrLinkIsNotIntact()
    {
        const Pack pack;
        Shop shop;
        shop.add(pack);
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, pack.id).ok);
        QVERIFY(store.isIntact(pack.id));
        const QString folder = store.installDirectory(pack.id);

        QVERIFY(QDir().mkpath(folder + u"/empty/inner"_s));
        QVERIFY(!store.isIntact(pack.id));
        QVERIFY(QDir(folder + u"/empty"_s).removeRecursively());
        QVERIFY(store.isIntact(pack.id));

#ifdef Q_OS_UNIX
        QTemporaryDir elsewhere;
        writeFile(elsewhere.path() + u"/evil.qml"_s, "x");
        QVERIFY(QFile::link(elsewhere.path(), folder + u"/linked"_s));
        QVERIFY(!store.isIntact(pack.id));
        QVERIFY(QFile::remove(folder + u"/linked"_s));
        QVERIFY(store.isIntact(pack.id));

        // A link standing in for a file it even matches.
        QTemporaryDir copy;
        writeFile(copy.path() + u"/hero.png"_s, kPng);
        QVERIFY(QFile::remove(folder + u"/art/hero.png"_s));
        QVERIFY(QFile::link(copy.path() + u"/hero.png"_s, folder + u"/art/hero.png"_s));
        QVERIFY(!store.isIntact(pack.id));
#endif
    }

    // ── Expiry after arrival ─────────────────────────────────────────────────

    // A catalogue that was good when it arrived and has run out since says nothing more.
    void aCatalogueThatHasExpiredAnswersNothing()
    {
        QTemporaryDir data;
        PlantedEngine store(data.path(), QByteArray(65, '\x04'));
        store.setCreatorFeatures(true);
        CatalogItem item = plantedItem(u"org.example.planted.one"_s, u"asset-pack"_s);
        item.featured = true;
        item.previews = { Preview { u"p.png"_s, QByteArray(32, 'x') } };
        Catalog catalog = plantedCatalog({ item });
        store.adoptCatalog(catalog);
        QCOMPARE(store.listings(View::Creator).size(), 1);
        QCOMPARE(store.featured(View::Creator).size(), 1);
        QCOMPARE(store.search(View::Creator, QString()).size(), 1);
        QCOMPARE(store.categoryCounts(View::Creator).size(), 1);

        catalog.expires = QDate(2020, 1, 1);
        store.adoptCatalog(catalog);
        QVERIFY(store.listings(View::Creator).isEmpty());
        QVERIFY(store.featured(View::Creator).isEmpty());
        QVERIFY(store.search(View::Creator, QString()).isEmpty());
        QVERIFY(store.categoryCounts(View::Creator).isEmpty());
        QVERIFY(!store.fetchPreview(item.id, 0));
    }

    // ── The shell's Store knows no view ──────────────────────────────────────

    void theShellsStoreHandsOutNoFormatTwoListing()
    {
        struct Reset
        {
            ~Reset() { Store::setEngineForTests(nullptr); }
        } reset;

        const Pack pack;
        Shop shop;
        shop.add(pack);
        QTemporaryDir data;
        CreatorStore engine(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(engine, shop.publish()), StoreEngine::State::Ready);
        Store::setEngineForTests(&engine);
        {
            Store store;
            QVERIFY(engine.catalog().item(pack.id));
            QVERIFY(store.item(pack.id).isEmpty());
            QVERIFY(store.items().isEmpty());
            QVERIFY(store.search(QString()).isEmpty());

            // Installed, and then no longer listed, it is still not this page's.
            QVERIFY(installAndWait(engine, pack.id).ok);
            QVERIFY(store.item(pack.id).isEmpty());
            shop.items.clear();
            shop.sequence = 2;
            QCOMPARE(refreshed(engine, shop.publish()), StoreEngine::State::Ready);
            QVERIFY(engine.installed().contains(pack.id));
            QVERIFY(!engine.catalog().item(pack.id));
            QVERIFY(store.item(pack.id).isEmpty());
        }
        Store::setEngineForTests(nullptr);

        // A theme and a widget it hands out as it always has.
        QTemporaryDir other;
        StoreEngine formatOne(other.path(), publicKey(kFixtures + u"/store.pub"_s));
        QCOMPARE(refreshed(formatOne, QUrl::fromLocalFile(kFixtures + u"/v1/catalog.json"_s)),
                 StoreEngine::State::Ready);
        Store::setEngineForTests(&formatOne);
        Store store;
        QCOMPARE(store.items().size(), 2);
        QCOMPARE(store.item(u"org.example.theme.test"_s).value(u"kind"_s).toString(), u"theme"_s);
        QVERIFY(installAndWait(formatOne, u"org.example.theme.test"_s).ok);
        QVERIFY(!store.item(u"org.example.theme.test"_s).isEmpty());
    }

    // ── Over the network ─────────────────────────────────────────────────────

    void theNetworkPathInstallsAndHoldsToItsLimits()
    {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        const auto serve = [](TinyServer &server, Shop &shop) {
            const QByteArray bytes = shop.json();
            server.files.insert(u"catalog.json"_s, bytes);
            server.files.insert(u"catalog.json.sig"_s, shop.store.sign(bytes));
        };
        Pack pack;
        Pack dependent;
        dependent.id = u"org.example.pack.dep"_s;
        dependent.dependencies.append(rangeOf(pack.id, u"1.0.0"_s, u"2.0.0"_s));
        Pack another;
        another.id = u"org.example.pack.another"_s;
        Shop shop;
        const QByteArray packBytes = pack.package(shop.developer);
        const QByteArray dependentBytes = dependent.package(shop.developer);
        const QByteArray anotherBytes = another.package(shop.developer);
        shop.addPackage(pack, packBytes);
        shop.addPackage(dependent, dependentBytes);
        shop.addPackage(another, anotherBytes);
        const QString packFile = u"packages/%1-1.0.0.qorpkg"_s.arg(pack.id);
        const QString dependentFile = u"packages/%1-1.0.0.qorpkg"_s.arg(dependent.id);
        const QString anotherFile = u"packages/%1-1.0.0.qorpkg"_s.arg(another.id);

        TinyServer server;
        serve(server, shop);
        server.files.insert(packFile, packBytes);
        server.files.insert(dependentFile, dependentBytes);
        server.files.insert(anotherFile, anotherBytes);
        const QByteArray key = shop.store.publicKey();

        // A plain install, and what it installed.
        {
            QTemporaryDir data;
            CreatorStore store(data.path(), key);
            QCOMPARE(refreshed(store, server.url(u"catalog.json"_s)), StoreEngine::State::Ready);
            const Outcome outcome = installAndWait(store, dependent.id);
            QVERIFY2(outcome.ok, qPrintable(outcome.error));
            QVERIFY(store.isIntact(pack.id));
            QVERIFY(store.isIntact(dependent.id));
        }
        // Larger than the catalogue says, and it is not read to its end.
        {
            QTemporaryDir data;
            CreatorStore store(data.path(), key);
            QCOMPARE(refreshed(store, server.url(u"catalog.json"_s)), StoreEngine::State::Ready);
            server.files.insert(packFile, packBytes + QByteArray(8 * 1024 * 1024, 'x'));
            const Outcome outcome = installAndWait(store, pack.id);
            server.files.insert(packFile, packBytes);
            QVERIFY(!outcome.ok);
            QVERIFY(store.installed().isEmpty());
        }
        // A catalogue that is the same bytes does not stop a plan on its way: this is "Check for Updates".
        {
            QTemporaryDir data;
            CreatorStore store(data.path(), key);
            QCOMPARE(refreshed(store, server.url(u"catalog.json"_s)), StoreEngine::State::Ready);
            QSignalSpy finished(&store, &StoreEngine::finished);
            server.held.insert(packFile);
            QVERIFY(store.install(pack.id));
            QTRY_COMPARE(server.waiting(), 1);
            QCOMPARE(refreshed(store, server.url(u"catalog.json"_s)), StoreEngine::State::Ready);
            server.release();
            QTRY_COMPARE(finished.count(), 1);
            QVERIFY2(finished.at(0).at(1).toBool(), qPrintable(finished.at(0).at(2).toString()));
        }
        // One that is not does.
        {
            QTemporaryDir data;
            CreatorStore store(data.path(), key);
            QCOMPARE(refreshed(store, server.url(u"catalog.json"_s)), StoreEngine::State::Ready);
            QSignalSpy finished(&store, &StoreEngine::finished);
            server.held.insert(packFile);
            QVERIFY(store.install(pack.id));
            QTRY_COMPARE(server.waiting(), 1);
            shop.sequence = 2;
            serve(server, shop);
            QCOMPARE(refreshed(store, server.url(u"catalog.json"_s)), StoreEngine::State::Ready);
            server.release();
            QTRY_COMPARE(finished.count(), 1);
            QVERIFY(!finished.at(0).at(1).toBool());
            QVERIFY2(finished.at(0).at(2).toString().contains(u"catalogue changed"_s),
                     qPrintable(finished.at(0).at(2).toString()));
            QVERIFY(store.installed().isEmpty());
            shop.sequence = 1;
            serve(server, shop);
        }
        // Closed with plans on their way, and two of them, so that ending one changes what the other is iterating.
        {
            QTemporaryDir data;
            CreatorStore store(data.path(), key);
            QCOMPARE(refreshed(store, server.url(u"catalog.json"_s)), StoreEngine::State::Ready);
            server.held.insert(packFile);
            server.held.insert(anotherFile);
            QVERIFY(store.install(pack.id));
            QVERIFY(store.install(another.id));
            QTRY_COMPARE(server.waiting(), 2);
        }
        server.release();
    }

    void aFailedDependencyStopsWhatNeedsIt()
    {
        Pack base;
        base.id = u"org.example.pack.base"_s;
        Pack pack;
        pack.dependencies.append(rangeOf(base.id, u"1.0.0"_s, u"2.0.0"_s));
        Shop shop;
        shop.add(base);
        shop.add(pack);
        // Damaged where it lies, after the catalogue was signed.
        writeFile(shop.dir.path() + u"/packages/%1-1.0.0.qorpkg"_s.arg(base.id), "damaged");

        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        const Outcome outcome = installAndWait(store, pack.id);
        QVERIFY(outcome.started);
        QVERIFY(!outcome.ok);
        QVERIFY2(outcome.error.contains(base.id), qPrintable(outcome.error));
        QVERIFY(store.installed().isEmpty());
        QVERIFY(!store.isBusy(pack.id));
        QVERIFY(!store.isBusy(base.id));
    }

    void anUpdateThatWouldBreakADependentIsRefused()
    {
        Pack base;
        base.id = u"org.example.pack.base"_s;
        Pack pack;
        pack.dependencies.append(rangeOf(base.id, u"1.0.0"_s, u"2.0.0"_s));
        Shop shop;
        shop.add(base);
        shop.add(pack);
        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, pack.id).ok);

        // A newer catalogue, whose base has moved out of what the installed pack accepts.
        Pack newer = base;
        newer.version = u"2.0.0"_s;
        shop.items.clear();
        shop.add(newer);
        shop.add(pack);
        shop.sequence = 2;
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        const Outcome outcome = installAndWait(store, base.id);
        QVERIFY(!outcome.started);
        QVERIFY2(outcome.error.contains(u"would not leave it"_s), qPrintable(outcome.error));
        QCOMPARE(store.installed().value(base.id).version, u"1.0.0"_s);
    }

    void aDependencyIsNotMovedBack()
    {
        Catalog catalog = plantedCatalog({});
        CatalogItem base = plantedItem(u"org.example.pack.base"_s, u"asset-pack"_s);
        base.version = u"1.0.0"_s;
        CatalogItem pack = plantedItem(u"org.example.pack.art"_s, u"asset-pack"_s);
        pack.dependencies.append(VersionRange { base.id, u"1.0.0"_s, u"3.0.0"_s });
        catalog.items = { base, pack };
        // Installed newer than the range, and newer than what the store lists.
        const InstallPlan plan =
            resolveDependencies(catalog, pack.id, QMap<QString, InstalledVersion> { { base.id, { u"4.0.0"_s, {} } } });
        QVERIFY(!plan.ok);
        QVERIFY2(plan.error.contains(u"nothing is moved back"_s), qPrintable(plan.error));
    }

    void aPackOthersNeedStays()
    {
        Pack base;
        base.id = u"org.example.pack.base"_s;
        Pack pack;
        pack.dependencies.append(rangeOf(base.id, u"1.0.0"_s, u"2.0.0"_s));
        Shop shop;
        shop.add(base);
        shop.add(pack);
        QTemporaryDir data;
        const QByteArray key = shop.store.publicKey();
        {
            CreatorStore store(data.path(), key);
            QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
            QVERIFY(installAndWait(store, pack.id).ok);
        }
        // Remembered across a restart, or the first remove afterwards would break it.
        CreatorStore store(data.path(), key);
        QSignalSpy finished(&store, &StoreEngine::finished);
        QVERIFY(!store.remove(base.id));
        QCOMPARE(finished.count(), 1);
        QVERIFY2(finished.at(0).at(2).toString().contains(pack.id), qPrintable(finished.at(0).at(2).toString()));
        QVERIFY(QFileInfo::exists(store.installDirectory(base.id)));
        QVERIFY(store.remove(pack.id));
        QVERIFY(store.remove(base.id));
    }

    // ── Updates touch only their own folder ──────────────────────────────────

    void updatingAPackTouchesNothingOutsideItsFolder()
    {
        Pack v1;
        v1.files.insert(u"art/old.png"_s, kPng);
        Pack v2 = v1;
        v2.version = u"1.1.0"_s;
        v2.files.remove(u"art/old.png"_s);
        v2.files.insert(u"art/new.png"_s, kPng);
        // One whose id begins with the pack's, the nearest neighbour an update could mistake.
        Pack neighbour;
        neighbour.id = v1.id + u".more"_s;

        Shop shop;
        shop.add(v1);
        shop.add(neighbour);
        QTemporaryDir base;
        CreatorStore store(base.path() + u"/store"_s, shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(store, v1.id).ok);
        QVERIFY(installAndWait(store, neighbour.id).ok);

        // What a person, or a project of theirs, keeps around it.
        const QStringList around { base.path() + u"/my-project/assets/art/hero.png"_s, base.path() + u"/note.txt"_s,
                                   store.dataDir() + u"/note.txt"_s, store.dataDir() + u"/items/note.txt"_s,
                                   store.installDirectory(neighbour.id) + u"/art/hero.png"_s };
        QMap<QString, QByteArray> before;
        for (const QString &path : around) {
            if (!QFileInfo::exists(path))
                writeFile(path, "mine");
            before.insert(path, readFile(path));
        }
        const QString neighbourNames = folderOfNames(store.installDirectory(neighbour.id));

        shop.items.clear();
        shop.add(v2);
        shop.add(neighbour);
        shop.sequence = 2;
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        const Outcome outcome = installAndWait(store, v2.id);
        QVERIFY2(outcome.ok, qPrintable(outcome.error));
        QCOMPARE(store.installed().value(v2.id).version, u"1.1.0"_s);

        for (const QString &path : around)
            QCOMPARE(readFile(path), before.value(path));
        QCOMPARE(folderOfNames(store.installDirectory(neighbour.id)), neighbourNames);
        QVERIFY(store.isIntact(neighbour.id));

        // Inside its own folder it is v2 exactly, and what v2 dropped has gone.
        QVERIFY(store.isIntact(v2.id));
        QVERIFY(!QFileInfo::exists(store.installDirectory(v2.id) + u"/art/old.png"_s));
        QVERIFY(QFileInfo::exists(store.installDirectory(v2.id) + u"/art/new.png"_s));
        // Nothing beside it but its neighbour and the note.
        QCOMPARE(QDir(store.dataDir() + u"/items"_s).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden).size(),
                 2);
    }

    // What a crash during an install leaves behind (StoreEngine::complete).
    void anInterruptedInstallIsPutRight()
    {
        const Pack pack;
        Pack other;
        other.id = u"org.example.pack.other"_s;
        Shop shop;
        shop.add(pack);
        shop.add(other);
        QTemporaryDir base;
        const QByteArray key = shop.store.publicKey();
        QString items;
        {
            CreatorStore store(base.path(), key);
            QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
            QVERIFY(installAndWait(store, pack.id).ok);
            QVERIFY(installAndWait(store, other.id).ok);
            items = store.dataDir() + u"/items"_s;
        }
        const QString hex(32, u'a');
        // Crashed mid-swap: the old version is aside and the new one never landed.
        QVERIFY(QDir().rename(items + u'/' + pack.id, items + u'/' + pack.id + u".old-"_s + hex));
        // Crashed just after the swap: the old one is still beside the new one.
        writeFile(items + u'/' + other.id + u".old-"_s + hex + u"/stale.qml"_s, "x");
        // Crashed while writing.
        writeFile(items + u"/.staging-"_s + hex + u"/art/hero.png"_s, "x");
        // Not ours: it does not have the form.
        writeFile(items + u"/org.example.pack.old-notauuid/keep.png"_s, "x");

        CreatorStore store(base.path(), key);
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(store.isIntact(pack.id));
        QVERIFY(store.isIntact(other.id));
        QVERIFY(!QFileInfo::exists(items + u'/' + pack.id + u".old-"_s + hex));
        QVERIFY(!QFileInfo::exists(items + u'/' + other.id + u".old-"_s + hex));
        QVERIFY(!QFileInfo::exists(items + u"/.staging-"_s + hex));
        QVERIFY(QFileInfo::exists(items + u"/org.example.pack.old-notauuid/keep.png"_s));
    }

    // Another program of this person's, mid-install, is not a crash.
    void aLockedStoreIsLeftAlone()
    {
        QTemporaryDir base;
        const QString folder = base.path();
        const QString staging = folder + u"/items/.staging-"_s + QString(32, u'a');
        writeFile(staging + u"/art/hero.png"_s, "x");
        QLockFile other(folder + u"/.lock"_s);
        QVERIFY(other.tryLock(0));
        {
            StoreEngine store(base.path(), QByteArray(65, '\x04'));
            QVERIFY(QFileInfo::exists(staging));
        }
        other.unlock();
        StoreEngine store(base.path(), QByteArray(65, '\x04'));
        QVERIFY(!QFileInfo::exists(staging));
    }

    void anIdCannotPoseAsWhatAnUpdateLeaves()
    {
        QVERIFY(!isValidItemId(u"org.example.pack.old-"_s + QString(32, u'a')));
        QVERIFY(isValidItemId(u"org.example.pack.old-school"_s));
    }

    // ── Previews ─────────────────────────────────────────────────────────────

    void aPreviewIsCheckedBeforeItIsHandedOver()
    {
        const Pack pack;
        Shop shop;
        QJsonObject &listing = shop.add(pack);
        const auto preview = [](const char *file, const QByteArray &digestOf) {
            return QJsonObject { { u"file"_s, QString::fromLatin1(file) }, { u"sha256"_s, hexDigest(digestOf) } };
        };
        listing.insert(u"previews"_s,
                       QJsonArray { preview("previews/good.png", kPng), preview("previews/altered.png", kPng),
                                    preview("previews/script.png", "#!/bin/sh\n"),
                                    preview("previews/missing.png", kPng) });
        writeFile(shop.dir.path() + u"/previews/good.png"_s, kPng);
        writeFile(shop.dir.path() + u"/previews/altered.png"_s, kPng + "altered");
        writeFile(shop.dir.path() + u"/previews/script.png"_s, "#!/bin/sh\n");

        QTemporaryDir data;
        CreatorStore store(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(store, shop.publish()), StoreEngine::State::Ready);
        QSignalSpy fetched(&store, &StoreEngine::previewFetched);

        const auto fetch = [&](int index) -> QList<QVariant> {
            fetched.clear();
            if (!store.fetchPreview(pack.id, index)
                || !QTest::qWaitFor([&fetched] { return fetched.count() > 0; }, 10000))
                return {};
            return fetched.at(0);
        };
        QList<QVariant> good = fetch(0);
        QVERIFY(good.size() == 5 && good.at(2).toBool());
        QCOMPARE(good.at(3).toByteArray(), kPng);
        QList<QVariant> altered = fetch(1);
        QVERIFY(altered.size() == 5 && !altered.at(2).toBool());
        QVERIFY(altered.at(3).toByteArray().isEmpty());
        QVERIFY2(altered.at(4).toString().contains(u"does not match"_s), qPrintable(altered.at(4).toString()));
        QList<QVariant> script = fetch(2);
        QVERIFY(script.size() == 5 && !script.at(2).toBool());
        QVERIFY2(script.at(4).toString().contains(u"not a picture"_s), qPrintable(script.at(4).toString()));
        QList<QVariant> missing = fetch(3);
        QVERIFY(missing.size() == 5 && !missing.at(2).toBool());

        QVERIFY(!store.fetchPreview(pack.id, 4));
        QVERIFY(!store.fetchPreview(pack.id, -1));
        QVERIFY(!store.fetchPreview(u"org.example.pack.none"_s, 0));
    }

    // ── Format 1's refusals, each at its own door ────────────────────────────

    // tst_store.cpp's refusesBadPackages asks only that each is refused, with one reason. This asks each for its own.
    void formatOnesRefusalsEachReachTheirOwnCheck()
    {
        const CatalogResult catalog = parseCatalog(readFile(kCatalog + u"/catalog.json"_s),
                                                   readFile(kCatalog + u"/catalog.json.sig"_s), publicKey(kStorePub));
        QVERIFY(catalog.ok);
        const CatalogItem *clock = catalog.catalog.item(u"dev.qorsync.widget.clock"_s);
        QVERIFY(clock);
        const QByteArray key = catalog.catalog.developers.value(u"dev.qorsync"_s).key;
        const QByteArray package = readFile(kCatalog + u'/' + clock->packageFile);
        QVERIFY(openPackage(package, key).ok);
        QVERIFY(openPackage(rezipStored(package, [](auto &) { }), key).ok);

        const auto reason = [&key, &package](const std::function<void(QMap<QString, QByteArray> &)> &edit) {
            const PackageResult opened = openPackage(rezipStored(package, edit), key);
            return opened.ok ? u"it opened"_s : opened.error;
        };
        QString why = reason([](auto &entries) { entries[u"Clock.qml"_s].append("\nRectangle {}"); });
        QVERIFY2(why.contains(u"Clock.qml does not match"_s), qPrintable(why));
        why = reason([](auto &entries) { entries.insert(u"extra.qml"_s, "x"); });
        QVERIFY2(why.contains(u"do not match its manifest"_s), qPrintable(why));
        why = reason([](auto &entries) { entries.remove(u"widget.json"_s); });
        QVERIFY2(why.contains(u"do not match its manifest"_s), qPrintable(why));
        why = reason([](auto &entries) { entries.insert(u"../evil.qml"_s, "x"); });
        QVERIFY2(why.contains(u"unsafe"_s), qPrintable(why));
        why = reason([](auto &entries) { entries.remove(u"manifest.sig"_s); });
        QVERIFY2(why.contains(u"no signed manifest"_s), qPrintable(why));
        const PackageResult wrongKey = openPackage(package, publicKey(kFixtures + u"/store.pub"_s));
        QVERIFY2(wrongKey.error.contains(u"developer signature"_s), qPrintable(wrongKey.error));
    }

    // ── The demonstration ────────────────────────────────────────────────────

    void theDemonstrationInstallsAPackAndUpdatesIt()
    {
        const QByteArray key = publicKey(kDemo + u"/store.pub"_s);
        const QString id = u"org.example.demo.pack.ember-climb-art"_s;
        const QString assets = kSource + u"/products/qq/samples/ember-climb/assets/"_s;
        QTemporaryDir base;
        CreatorStore store(base.path(), key);

        QCOMPARE(refreshed(store, QUrl::fromLocalFile(kDemo + u"/v1/catalog.json"_s)), StoreEngine::State::Ready);
        QCOMPARE(store.lastSequence(), 1);
        const CatalogItem *item = store.catalog().item(id);
        QVERIFY(item);
        QCOMPARE(item->kind, u"asset-pack"_s);
        QCOMPARE(item->entitlement, u"free"_s);
        QVERIFY(item->platforms.contains(u"linux-x86_64"_s));
        QVERIFY(item->platforms.contains(currentPlatform()));
        QCOMPARE(item->licence.spdx, u"Apache-2.0"_s);
        QVERIFY(!item->provenance.origin.isEmpty());
        QCOMPARE(item->engines.constFirst().id, u"qq"_s);
        QVERIFY(!item->executable);
        QCOMPARE(item->previews.size(), 1);
        QCOMPARE(item->storeNames(), QStringList { u"creator"_s });
        const QVariantMap described = item->toVariant();
        QCOMPARE(described.value(u"licence"_s).toMap().value(u"spdx"_s).toString(), u"Apache-2.0"_s);
        QCOMPARE(described.value(u"entitlement"_s).toMap().value(u"type"_s).toString(), u"free"_s);
        QCOMPARE(described.value(u"placements"_s).toList().size(), 1);
        QCOMPARE(idsOf(store.listings(View::Creator)), QStringList { id });

        Outcome outcome = installAndWait(store, id);
        QVERIFY2(outcome.ok, qPrintable(outcome.error));
        QVERIFY(store.isIntact(id));
        const QString folder = store.installDirectory(id);
        QCOMPARE(readFile(folder + u"/art/hero.png"_s), readFile(assets + u"hero.png"_s));
        QVERIFY(!QFileInfo::exists(folder + u"/art/torch.png"_s));
        // Its licence and its notice travel with it.
        QCOMPARE(readFile(folder + u"/LICENSE.txt"_s), readFile(kSource + u"/LICENSES/Apache-2.0.txt"_s));
        QVERIFY(readFile(folder + u"/NOTICE.txt"_s).contains("Copyright 2026 Demiurge"));

        QCOMPARE(refreshed(store, QUrl::fromLocalFile(kDemo + u"/v2/catalog.json"_s)), StoreEngine::State::Ready);
        QCOMPARE(store.lastSequence(), 2);
        outcome = installAndWait(store, id);
        QVERIFY2(outcome.ok, qPrintable(outcome.error));
        QCOMPARE(store.installed().value(id).version, u"1.1.0"_s);
        QVERIFY(store.isIntact(id));
        QCOMPARE(readFile(folder + u"/art/torch.png"_s), readFile(assets + u"torch.png"_s));

        // v1's catalogue, replayed.
        QCOMPARE(refreshed(store, QUrl::fromLocalFile(kDemo + u"/v1/catalog.json"_s)), StoreEngine::State::Failed);
        QVERIFY2(store.error().contains(u"older"_s), qPrintable(store.error()));

        // Its preview.
        QCOMPARE(refreshed(store, QUrl::fromLocalFile(kDemo + u"/v2/catalog.json"_s)), StoreEngine::State::Ready);
        QSignalSpy fetched(&store, &StoreEngine::previewFetched);
        QVERIFY(store.fetchPreview(id, 0));
        QVERIFY(QTest::qWaitFor([&fetched] { return fetched.count() > 0; }, 10000));
        QVERIFY2(fetched.at(0).at(2).toBool(), qPrintable(fetched.at(0).at(4).toString()));
        QCOMPARE(fetched.at(0).at(3).toByteArray(), readFile(assets + u"hero.png"_s));
    }

    void theDemonstrationInstallsAGameForAPlayer()
    {
        const QByteArray key = publicKey(kDemo + u"/store.pub"_s);
        const QString id = u"org.example.demo.game.ember-climb"_s;
        const QString pack = u"org.example.demo.pack.ember-climb-art"_s;
        const QString project = u"org.example.demo.project.ember-climb"_s;
        QTemporaryDir base;
        StoreEngine store(base.path(), key); // creator features off, as a player's are
        QCOMPARE(refreshed(store, QUrl::fromLocalFile(kDemo + u"/v1/catalog.json"_s)), StoreEngine::State::Ready);

        // A player sees the game, and neither the pack nor the project.
        QCOMPARE(idsOf(store.listings(View::Player)), QStringList { id });
        QVERIFY(store.listings(View::Creator).isEmpty());
        QCOMPARE(store.categoryCounts(View::Player), (QMap<QString, int> { { u"Platformers"_s, 1 } }));
        QVERIFY(store.search(View::Player, u"pixel"_s).isEmpty());
        const Outcome refused = installAndWait(store, pack);
        QVERIFY(!refused.started);
        QVERIFY2(refused.error.contains(u"creator features are off"_s), qPrintable(refused.error));

        const Outcome outcome = installAndWait(store, id);
        QVERIFY2(outcome.ok, qPrintable(outcome.error));
        QVERIFY(store.isIntact(id));
        const QString folder = store.installDirectory(id);
        const QString sample = kSource + u"/products/qq/samples/ember-climb/"_s;
        for (const QString &path : { u"scenes/ember-climb.qml"_s, u"logic/rules.qml"_s, u"assets/level.tmj"_s,
                                     u"assets/tiles.tsj"_s, u"assets/hero.png"_s })
            QCOMPARE(readFile(folder + u'/' + path), readFile(sample + path));
        QVERIFY(!QFileInfo::exists(folder + u"/make_art.py"_s));
        QCOMPARE(readFile(folder + u"/LICENSE.txt"_s), readFile(kSource + u"/LICENSES/Apache-2.0.txt"_s));

        // Holding the game gives nothing of its editable project, which is a draft here.
        const Draft *draft = store.catalog().draft(project);
        QVERIFY(draft);
        QCOMPARE(draft->code, u"kind-not-taken-yet"_s);
        QCOMPARE(store.catalog().item(id)->edition.role, u"game"_s);
        QVERIFY(!installAndWait(store, project).ok);
        QCOMPARE(store.installed().size(), 1);
    }

    // Its key opens nothing else, nothing else's key opens it, and the build holds none of it.
    void theDemonstrationIsNotTheRealStore()
    {
        const QByteArray demoKey = publicKey(kDemo + u"/store.pub"_s);
        const QByteArray realKey = publicKey(kStorePub);
        const QByteArray fixtureKey = publicKey(kFixtures + u"/store.pub"_s);
        QCOMPARE(demoKey.size(), 65);
        QVERIFY(demoKey != realKey);
        QVERIFY(demoKey != fixtureKey);

        const QByteArray json = readFile(kDemo + u"/v1/catalog.json"_s);
        const QByteArray signature = readFile(kDemo + u"/v1/catalog.json.sig"_s);
        QVERIFY(verifySignature(demoKey, json, signature));
        for (const QByteArray &other : { realKey, fixtureKey }) {
            const CatalogResult result = parseCatalog(json, signature, other);
            QVERIFY(!result.ok);
            QVERIFY(result.error.contains(u"signature"_s));
        }
        QVERIFY(
            !parseCatalog(readFile(kCatalog + u"/catalog.json"_s), readFile(kCatalog + u"/catalog.json.sig"_s), demoKey)
                 .ok);
        QVERIFY(!json.contains("\"price\""));
        QVERIFY(!json.contains("\"audience\""));

        // Nothing of it is among the resources this program was linked with, which hold the Store's own catalogue.
        QVERIFY(QFile::exists(u":/qt/qml/QOR/Store/catalog/catalog.json"_s));
        QDirIterator resources(u":/qt/qml/QOR/Store"_s, QDirIterator::Subdirectories);
        while (resources.hasNext()) {
            const QString path = resources.next();
            QVERIFY2(!path.contains(u"demo"_s), qPrintable(path));
        }
    }
};

QTEST_GUILESS_MAIN(TestStoreFormat2)
#include "tst_store_format2.moc"
