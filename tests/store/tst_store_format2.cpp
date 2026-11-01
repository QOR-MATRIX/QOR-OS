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
#include "storeengine.h"
#include "testsigner.h"

#include <QtCore/QBuffer>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QLockFile>
#include <QtCore/QRandomGenerator>
#include <QtCore/QSet>
#include <QtCore/QTemporaryDir>
#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace qor::store;
using namespace Qt::StringLiterals;

namespace {

const QString kSource = QStringLiteral(QOR_SOURCE_DIR);
const QString kCatalog = QStringLiteral(QOR_STORE_CATALOG);
const QString kStorePub = QStringLiteral(QOR_STORE_PUB);
const QString kFixtures = QStringLiteral(QOR_STORE_FIXTURES);
const QString kDemo = QStringLiteral(QOR_STORE_DEMO);
const QString kDeveloper = QStringLiteral("org.example");

const QByteArray kPng = QByteArray::fromHex("89504e470d0a1a0a") + "a picture";
const QByteArray kJpg = QByteArray::fromHex("ffd8ffe0") + "a picture";
const QByteArray kWebp = QByteArrayLiteral("RIFF....WEBPa picture");
const QByteArray kWav = QByteArrayLiteral("RIFF....WAVEa sound");
const QByteArray kOgg = QByteArrayLiteral("OggSa sound");
const QByteArray kFlac = QByteArrayLiteral("fLaCa sound");
const QByteArray kMp3 = QByteArrayLiteral("ID3a sound");
const QByteArray kHdr = QByteArrayLiteral("#?RADIANCE\n");
const QByteArray kLicence = QByteArrayLiteral("Free to use.\n");

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(data), data.size());
}

QByteArray publicKey(const QString &path) { return QByteArray::fromBase64(readFile(path).trimmed()); }

QString hexDigest(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

// ── An archive, bytes under the test's control ──────────────────────────────

struct RawEntry
{
    QByteArray name;
    QByteArray data;
    quint16 method = 0;
    quint16 flags = 0;
    qint64 packed = -1;          // what both headers call its stored size, when not the bytes' own
    qint64 unpacked = -1;        // what both headers call its unpacked size, when not the bytes' own
    qint64 centralUnpacked = -1; // the same in the directory alone
    int host = 3;
    quint32 mode = 0100644;
    QByteArray extra;
    qint64 centralOffset = -1; // where the directory says it starts, when not where it does
    QByteArray centralName;    // what the directory calls it, when not what its own header does
};

void put16(QByteArray &out, quint16 value)
{
    out.append(char(value & 0xff));
    out.append(char(value >> 8));
}

void put32(QByteArray &out, quint32 value)
{
    put16(out, quint16(value & 0xffff));
    put16(out, quint16(value >> 16));
}

QByteArray forgeArchive(const QList<RawEntry> &entries, const QByteArray &leader = {}, const QByteArray &trailer = {},
                        int claimedCount = -1)
{
    QByteArray archive = leader;
    QByteArray central;
    for (const RawEntry &entry : entries) {
        const auto offset = quint32(archive.size());
        const auto packed = quint32(entry.packed >= 0 ? entry.packed : entry.data.size());
        const auto unpacked = quint32(entry.unpacked >= 0 ? entry.unpacked : entry.data.size());
        put32(archive, 0x04034b50);
        put16(archive, 20);
        put16(archive, entry.flags);
        put16(archive, entry.method);
        put16(archive, 0);
        put16(archive, 0x21);
        put32(archive, 0);
        put32(archive, packed);
        put32(archive, unpacked);
        put16(archive, quint16(entry.name.size()));
        put16(archive, quint16(entry.extra.size()));
        archive.append(entry.name);
        archive.append(entry.extra);
        archive.append(entry.data);

        const QByteArray name = entry.centralName.isNull() ? entry.name : entry.centralName;
        put32(central, 0x02014b50);
        put16(central, quint16((entry.host << 8) | 20));
        put16(central, 20);
        put16(central, entry.flags);
        put16(central, entry.method);
        put16(central, 0);
        put16(central, 0x21);
        put32(central, 0);
        put32(central, packed);
        put32(central, entry.centralUnpacked >= 0 ? quint32(entry.centralUnpacked) : unpacked);
        put16(central, quint16(name.size()));
        put16(central, quint16(entry.extra.size()));
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put32(central, entry.mode << 16);
        put32(central, entry.centralOffset >= 0 ? quint32(entry.centralOffset) : offset);
        central.append(name);
        central.append(entry.extra);
    }
    const auto centralStart = quint32(archive.size());
    archive.append(central);
    const quint16 count = claimedCount >= 0 ? quint16(claimedCount) : quint16(entries.size());
    put32(archive, 0x06054b50);
    put16(archive, 0);
    put16(archive, 0);
    put16(archive, count);
    put16(archive, count);
    put32(archive, quint32(central.size()));
    put32(archive, centralStart);
    put16(archive, 0);
    archive.append(trailer);
    return archive;
}

// ── A package, signed ───────────────────────────────────────────────────────

QJsonObject rangeOf(const QString &id, const QString &min, const QString &below)
{
    return { { u"id"_s, id }, { u"min"_s, min }, { u"below"_s, below } };
}

struct Pack
{
    QString id = u"org.example.pack.art"_s;
    QString kind = u"asset-pack"_s;
    QStringList stores { u"creator"_s };
    QString category = u"Things"_s;
    QJsonObject edition;
    QString version = u"1.0.0"_s;
    QString developer = kDeveloper;
    QString spdx = u"Apache-2.0"_s;
    QString licenceFile = u"LICENSE.txt"_s;
    QJsonArray dependencies;
    QMap<QString, QByteArray> files { { u"LICENSE.txt"_s, kLicence }, { u"art/hero.png"_s, kPng } };
    int format = 2;

    QJsonObject licence() const { return { { u"spdx"_s, spdx }, { u"file"_s, licenceFile } }; }

    QByteArray manifest() const
    {
        QJsonObject digests;
        for (auto it = files.cbegin(); it != files.cend(); ++it)
            digests.insert(it.key(), hexDigest(it.value()));
        QJsonObject object { { u"format"_s, format },    { u"id"_s, id },
                             { u"kind"_s, kind },        { u"version"_s, version },
                             { u"name"_s, u"A pack"_s }, { u"developer"_s, developer },
                             { u"files"_s, digests } };
        if (format == 2) {
            object.insert(u"stores"_s, QJsonArray::fromStringList(stores));
            object.insert(u"licence"_s, licence());
            object.insert(u"dependencies"_s, dependencies);
            if (!edition.isEmpty())
                object.insert(u"edition"_s, edition);
        }
        return QJsonDocument(object).toJson(QJsonDocument::Compact);
    }

    QList<RawEntry> entries(const TestSigner &signer) const
    {
        const QByteArray signedManifest = manifest();
        QList<RawEntry> list;
        RawEntry first;
        first.name = "manifest.json";
        first.data = signedManifest;
        list.append(first);
        RawEntry second;
        second.name = "manifest.sig";
        second.data = signer.sign(signedManifest);
        list.append(second);
        for (auto it = files.cbegin(); it != files.cend(); ++it) {
            RawEntry entry;
            entry.name = it.key().toLatin1();
            entry.data = it.value();
            list.append(entry);
        }
        return list;
    }

    QByteArray package(const TestSigner &signer) const { return forgeArchive(entries(signer)); }
};

Pack gamePack()
{
    Pack game;
    game.id = u"org.example.game.one"_s;
    game.kind = u"game"_s;
    game.stores = { u"player"_s };
    return game;
}

QJsonObject placementOf(const QString &store, const QString &category)
{
    return { { u"store"_s, store }, { u"category"_s, category } };
}

// All a listing needs to say what it is. A draft needs nothing more, and has nothing more believed.
QJsonObject classifiedListing(const QString &id, const QString &kind, const QJsonArray &placements)
{
    return { { u"id"_s, id },
             { u"kind"_s, kind },
             { u"placements"_s, placements },
             { u"entitlement"_s, QJsonObject { { u"type"_s, u"free"_s } } },
             { u"platforms"_s, QJsonArray { u"linux-x86_64"_s } },
             { u"executable"_s, false } };
}

// A format 2 catalogue, its packages and its signature, in a folder of its own.
class Shop
{
public:
    TestSigner store;
    TestSigner developer;
    QTemporaryDir dir;
    qint64 sequence = 1;
    QString expires = u"2099-12-31"_s;
    QList<QJsonObject> items;

    QJsonObject listing(const Pack &pack, const QByteArray &package) const
    {
        qint64 unpacked = 0;
        const QList<RawEntry> entries = pack.entries(developer);
        for (const RawEntry &entry : entries)
            unpacked += entry.data.size();
        const QString file = u"packages/%1-%2.qorpkg"_s.arg(pack.id, pack.version);
        QJsonArray placements;
        for (const QString &where : pack.stores)
            placements.append(placementOf(where, pack.category));
        QJsonObject result = classifiedListing(pack.id, pack.kind, placements);
        if (!pack.edition.isEmpty())
            result.insert(u"edition"_s, pack.edition);
        const QJsonObject rest { { u"name"_s, u"A pack"_s },
                                 { u"developer"_s, pack.developer },
                                 { u"version"_s, pack.version },
                                 { u"summary"_s, u"A summary."_s },
                                 { u"description"_s, u"A description."_s },
                                 { u"released"_s, u"2026-11-01"_s },
                                 { u"licence"_s, pack.licence() },
                                 { u"provenance"_s, QJsonObject { { u"origin"_s, u"here"_s },
                                                                  { u"author"_s, u"someone"_s },
                                                                  { u"source"_s, u"somewhere"_s },
                                                                  { u"permission"_s, u"their own"_s } } },
                                 { u"engines"_s, QJsonArray { rangeOf(u"qq"_s, u"0.1.0"_s, u"1.0.0"_s) } },
                                 { u"dependencies"_s, pack.dependencies },
                                 { u"previews"_s, QJsonArray {} },
                                 { u"package"_s, QJsonObject { { u"file"_s, file },
                                                               { u"size"_s, package.size() },
                                                               { u"sha256"_s, hexDigest(package) },
                                                               { u"unpacked"_s, unpacked } } } };
        for (auto it = rest.begin(); it != rest.end(); ++it)
            result.insert(it.key(), it.value());
        return result;
    }

    // Writes a package and lists it, and returns the listing to change if a test wants it changed.
    QJsonObject &add(const Pack &pack) { return addPackage(pack, pack.package(developer)); }

    QJsonObject &addPackage(const Pack &pack, const QByteArray &package)
    {
        writeFile(dir.path() + u"/packages/%1-%2.qorpkg"_s.arg(pack.id, pack.version), package);
        items.append(listing(pack, package));
        return items.last();
    }

    QByteArray json(const QJsonObject &extraRoot = {}) const
    {
        QJsonArray array;
        for (const QJsonObject &item : items)
            array.append(item);
        QJsonObject root {
            { u"format"_s, 2 },
            { u"store"_s, u"A test store"_s },
            { u"sequence"_s, sequence },
            { u"expires"_s, expires },
            { u"developers"_s,
              QJsonObject { { kDeveloper,
                              QJsonObject { { u"name"_s, u"Example"_s },
                                            { u"verified"_s, true },
                                            { u"key"_s, QString::fromLatin1(developer.publicKey().toBase64()) } } } } },
            { u"items"_s, array }
        };
        for (auto it = extraRoot.begin(); it != extraRoot.end(); ++it)
            root.insert(it.key(), it.value());
        return QJsonDocument(root).toJson(QJsonDocument::Indented);
    }

    QUrl publish() const
    {
        const QByteArray bytes = json();
        writeFile(dir.path() + u"/catalog.json"_s, bytes);
        writeFile(dir.path() + u"/catalog.json.sig"_s, store.sign(bytes));
        return QUrl::fromLocalFile(dir.path() + u"/catalog.json"_s);
    }

    CatalogResult parse(const QJsonObject &extraRoot = {}, QDate today = {}) const
    {
        const QByteArray bytes = json(extraRoot);
        return parseCatalog(bytes, store.sign(bytes), store.publicKey(), today);
    }
};

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

// ── An engine, driven ───────────────────────────────────────────────────────

StoreEngine::State refreshed(StoreEngine &store, const QUrl &url)
{
    store.setCatalogUrl(url);
    store.refresh();
    (void)QTest::qWaitFor([&store] { return store.state() != StoreEngine::State::Loading; }, 10000);
    return store.state();
}

struct Outcome
{
    bool started = false;
    bool ok = false;
    QString error;
    QStringList order; // every item that finished, in the order it did
};

Outcome installAndWait(StoreEngine &store, const QString &id)
{
    QSignalSpy finished(&store, &StoreEngine::finished);
    Outcome outcome;
    outcome.started = store.install(id);
    const auto report = [&finished, &id]() -> const QList<QVariant> * {
        for (const QList<QVariant> &emission : std::as_const(finished))
            if (emission.at(0).toString() == id)
                return &emission;
        return nullptr;
    };
    if (outcome.started || report())
        (void)QTest::qWaitFor([&report] { return report() != nullptr; }, 10000);
    for (const QList<QVariant> &emission : std::as_const(finished))
        outcome.order.append(emission.at(0).toString());
    if (const QList<QVariant> *found = report()) {
        outcome.ok = found->at(1).toBool();
        outcome.error = found->at(2).toString();
    }
    return outcome;
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
    QSet<QString> laterKinds;

protected:
    bool takesKind(const QString &kind) const override
    {
        return laterKinds.contains(kind) || StoreEngine::takesKind(kind);
    }
};

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
        QTest::newRow("headers both small, bytes large") << forgeArchive({ smaller }) << u"gap"_s;

        RawEntry directory;
        directory.name = "a.png";
        directory.data = big;
        directory.centralUnpacked = 10;
        QTest::newRow("the directory differs from the entry") << forgeArchive({ directory }) << u"claims"_s;

        RawEntry differentName;
        differentName.name = "evil.dll";
        differentName.centralName = "good.png";
        differentName.data = big;
        QTest::newRow("the directory names it differently") << forgeArchive({ differentName }) << u"claims"_s;

        RawEntry beyond;
        beyond.name = "a.png";
        beyond.data = big;
        beyond.packed = 0x7fffffff;
        beyond.unpacked = 0x7fffffff;
        QTest::newRow("runs off the end of the archive") << forgeArchive({ beyond }) << u"readable"_s;

        RawEntry zip64;
        zip64.name = "a.png";
        zip64.data = big;
        zip64.packed = 0xffffffff;
        zip64.unpacked = 0xffffffff;
        QTest::newRow("a zip64 marker") << forgeArchive({ zip64 }) << u"readable"_s;
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
        QTest::newRow("a file where a folder goes") << QStringList { u"art"_s, u"art/a.png"_s };
        QTest::newRow("a folder where a file goes") << QStringList { u"art/a.png"_s, u"art"_s };
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

        RawEntry link = plain;
        link.mode = 0120777;
        QTest::newRow("a symbolic link") << forgeArchive({ link }) << u"unsafe"_s;
        RawEntry linkFromDos = link;
        linkFromDos.host = 0;
        QTest::newRow("a symbolic link that says it came from DOS") << forgeArchive({ linkFromDos }) << u"unsafe"_s;
        RawEntry folder = plain;
        folder.mode = 040755;
        QTest::newRow("a folder by its attributes") << forgeArchive({ folder }) << u"unsafe"_s;
        RawEntry unknownSystem = plain;
        unknownSystem.host = 19;
        QTest::newRow("from a system not known") << forgeArchive({ unknownSystem }) << u"unsafe"_s;
        RawEntry encrypted = plain;
        encrypted.flags = 1;
        QTest::newRow("encrypted") << forgeArchive({ encrypted }) << u"may not"_s;
        RawEntry streamed = plain;
        streamed.flags = 8;
        QTest::newRow("sizes given after the data") << forgeArchive({ streamed }) << u"may not"_s;
        RawEntry extra = plain;
        extra.extra = "xx\0\0";
        QTest::newRow("an extra field") << forgeArchive({ extra }) << u"may not"_s;
        RawEntry unknownMethod = plain;
        unknownMethod.method = 99;
        QTest::newRow("a method not known") << forgeArchive({ unknownMethod }) << u"compressed"_s;

        QTest::newRow("bytes before the first entry") << forgeArchive({ plain }, "leading") << u"overlap"_s;
        QTest::newRow("bytes after the end") << forgeArchive({ plain }, {}, "trailing") << u"readable"_s;
        QTest::newRow("a count that is too high") << forgeArchive({ plain }, {}, {}, 2) << u"readable"_s;
        QTest::newRow("a count that is too low") << forgeArchive({ plain, plain }, {}, {}, 1) << u"readable"_s;
        QTest::newRow("no entries, no manifest") << forgeArchive({}) << u""_s;
        QTest::newRow("not an archive") << QByteArray("not a zip") << u"readable"_s;
        QTest::newRow("nothing") << QByteArray() << u"readable"_s;
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
        QTest::newRow("data of another kind") << u"m/a.gltf"_s << gltf("data:text/html,x") << u"outside the package"_s;
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
    }
    void aLicenceHoldsNoMarkup()
    {
        QFETCH(QByteArray, text);
        const TestSigner developer;
        Pack pack;
        pack.files.insert(u"LICENSE.txt"_s, text);
        const PackageResult opened = openPackage(pack.package(developer), developer.publicKey());
        QVERIFY(!opened.ok);
        QVERIFY2(opened.error.contains(u"markup"_s), qPrintable(opened.error));
    }

    void aLicenceMayHoldAngleBracketsAsWords()
    {
        const TestSigner developer;
        Pack pack;
        pack.files.insert(u"LICENSE.txt"_s,
                          "<one line to give the program's name and a brief idea of what it does.>\n"
                          "Copyright (C) <year> <name of author>, <https://www.gnu.org/licenses/>.\n\f\n1 < 2 > 1\n");
        const PackageResult opened = openPackage(pack.package(developer), developer.publicKey());
        QVERIFY2(opened.ok, qPrintable(opened.error));
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
        QVERIFY2(result.error.contains(u"price"_s), qPrintable(result.error));
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
        QVERIFY2(result.error.contains(u"price"_s), qPrintable(result.error));
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
        QTest::newRow("past what JSON holds exactly") << QJsonValue(1e300);
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
        for (const QString &what : { u"id"_s, u"version"_s, u"kind and stores"_s, u"edition"_s, u"licence's SPDX"_s,
                                     u"licence's file"_s, u"dependencies"_s, u"developer"_s })
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
        shop.add(pack).insert(u"featured"_s, true);
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

    void aDependencyChainHasALimit()
    {
        Shop shop;
        for (int i = 0; i < 40; ++i) {
            Pack pack;
            pack.id = u"org.example.pack.p%1"_s.arg(i);
            if (i < 39)
                pack.dependencies.append(rangeOf(u"org.example.pack.p%1"_s.arg(i + 1), u"1.0.0"_s, u"2.0.0"_s));
            shop.add(pack);
        }
        const CatalogResult result = shop.parse();
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(u"too deep"_s), qPrintable(result.error));
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

    // tst_store.cpp's refusesBadPackages rewrites packages with Qt's writer, which deflates, so since compressed
    // entries were refused all of its cases stop at that one door. They still pass, and prove less. Here each is
    // stored, and must be refused for the reason it is about.
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
        QCOMPARE(item->platforms, QStringList { u"linux-x86_64"_s });
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
