// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// The two storefronts as the shell's pages have them (QOR-0021, docs/STORE.md): the build choice that holds them back
// from everything but a demonstration, the explicit views, and what a catalogue hands over, from the hostile side.
// Their pages are in tst_storefronts_qml, and a person's walk through them is in store-demonstration.e2e.json.
//
// Most of this needs no demonstration build: an engine is handed in, with a key read from a file or made for the test.
// What depends on the build says so, and its other half runs in the other build.
#include "services.h"
#include "simservices.h"
#include "simsession.h"
#include "store.h"
#include "storeengine.h"
#include "storefronts.h"
#include "testshop.h"

#include <QtCore/QBuffer>
#include <QtCore/QDateTime>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QRegularExpression>
#include <QtCore/QScopeGuard>
#include <QtCore/QSettings>
#include <QtCore/QTemporaryDir>
#include <QtGui/QImage>
#include <QtNetwork/QNetworkProxy>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <memory>

#include "serviceendpoint.h"

using namespace qor::store;
using namespace teststore;
using namespace Qt::StringLiterals;

namespace {

const QString kSource = QStringLiteral(QOR_SOURCE_DIR);
const QString kDemo = QStringLiteral(QOR_STORE_DEMO);
const QString kOption = QStringLiteral("QOR_STORE_DEMONSTRATION");

quint32 crc32Of(const QByteArray &data)
{
    quint32 crc = 0xffffffffU;
    for (const char byte : data) {
        crc ^= quint8(byte);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 1U) != 0 ? (crc >> 1) ^ 0xedb88320U : crc >> 1;
    }
    return ~crc;
}

void putBigEndian(QByteArray &out, quint32 value)
{
    for (int shift = 24; shift >= 0; shift -= 8)
        out.append(char((value >> shift) & 0xff));
}

QByteArray pngChunk(const QByteArray &type, const QByteArray &body)
{
    QByteArray chunk;
    putBigEndian(chunk, quint32(body.size()));
    chunk.append(type);
    chunk.append(body);
    putBigEndian(chunk, crc32Of(type + body));
    return chunk;
}

// A short file saying it is a very large picture, or a large file saying it is a small one: what a decoder would
// believe.
QByteArray pngClaiming(quint32 width, quint32 height, qsizetype padding = 16)
{
    QByteArray header;
    putBigEndian(header, width);
    putBigEndian(header, height);
    header.append(QByteArray::fromHex("0802000000")); // 8 bits, RGB, the rest ordinary
    return QByteArray::fromHex("89504e470d0a1a0a") + pngChunk("IHDR", header)
           + pngChunk("IDAT", QByteArray(padding, 'x')) + pngChunk("IEND", {});
}

QByteArray realPng()
{
    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(Qt::red);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

// A catalogue with a game for players and a pack for creators, the pack featured and worded so that anything merging
// the two views would show it.
struct TwoStores
{
    Shop shop;
    Pack game = gamePack();
    Pack pack;

    TwoStores()
    {
        game.category = u"Platformers"_s;
        pack.category = u"2D art"_s;
        QJsonObject &forGames = shop.add(game);
        forGames.insert(u"name"_s, u"One climb"_s);
        forGames.insert(u"featured"_s, true);
        QJsonObject &forCreators = shop.add(pack);
        forCreators.insert(u"name"_s, u"One creator pack"_s);
        forCreators.insert(u"featured"_s, true);
        forCreators.insert(u"tags"_s, QJsonArray { u"one"_s, u"pixel"_s });
    }
};

// A storefront over an engine of the test's own.
struct Fronts
{
    QTemporaryDir data;
    StoreEngine engine;
    Storefronts *fronts = nullptr;
    QSignalSpy *finished = nullptr;

    Fronts(const QByteArray &key, const QList<QUrl> &catalogues) : engine(data.path(), key)
    {
        Storefronts::useEngine(&engine, catalogues);
        fronts = new Storefronts(&engine);
        finished = new QSignalSpy(fronts, &Storefronts::finished);
    }
    ~Fronts()
    {
        delete finished;
        Storefronts::useEngine(nullptr, {});
    }
    Fronts(const Fronts &) = delete;
    Fronts &operator=(const Fronts &) = delete;
};

// The shop's catalogue and every file beside it, over the loopback. Returns the catalogue's URL there.
QUrl serve(const Shop &shop, TinyServer &server)
{
    shop.publish();
    const QDir folder(shop.dir.path());
    QDirIterator it(shop.dir.path(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        server.files.insert(folder.relativeFilePath(path), readFile(path));
    }
    return server.url(u"catalog.json"_s);
}

QStringList idsOf(const QVariantList &items)
{
    QStringList ids;
    for (const QVariant &item : items)
        ids.append(item.toMap().value(u"id"_s).toString());
    return ids;
}

bool holdsItsBytes(const QString &binary, const QByteArray &needle)
{
    QFile file(binary);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("%s cannot be read", qPrintable(binary));
        return false;
    }
    const uchar *bytes = file.map(0, file.size());
    if (!bytes) {
        qWarning("%s cannot be mapped", qPrintable(binary));
        return false;
    }
    return QByteArrayView(bytes, file.size()).contains(needle);
}

// ── Presets ─────────────────────────────────────────────────────────────────

bool isOn(const QJsonValue &value)
{
    const QJsonValue plain = value.isObject() ? value.toObject().value(u"value"_s) : value;
    if (plain.isBool())
        return plain.toBool();
    const QString text = plain.toString().trimmed().toUpper();
    static const QStringList off {
        QString(), u"OFF"_s, u"0"_s, u"FALSE"_s, u"NO"_s, u"N"_s, u"IGNORE"_s, u"NOTFOUND"_s
    };
    return !off.contains(text) && !text.endsWith(u"-NOTFOUND"_s);
}

// What a preset's cache says, as CMake reads it: a preset's own values first, then its parents', the earlier one first.
QJsonObject cacheOf(const QHash<QString, QJsonObject> &presets, const QString &name)
{
    const QJsonObject preset = presets.value(name);
    QJsonObject cache;
    const QJsonValue inherits = preset.value(u"inherits"_s);
    QStringList parents;
    if (inherits.isString())
        parents.append(inherits.toString());
    for (const QJsonValue &parent : inherits.toArray())
        parents.append(parent.toString());
    for (qsizetype i = parents.size() - 1; i >= 0; --i) {
        const QJsonObject inherited = cacheOf(presets, parents.at(i));
        for (auto it = inherited.begin(); it != inherited.end(); ++it)
            cache.insert(it.key(), it.value());
    }
    const QJsonObject own = preset.value(u"cacheVariables"_s).toObject();
    for (auto it = own.begin(); it != own.end(); ++it)
        cache.insert(it.key(), it.value());
    return cache;
}

QStringList presetsHoldingIt(const QByteArray &json)
{
    QHash<QString, QJsonObject> presets;
    const QJsonArray list = QJsonDocument::fromJson(json).object().value(u"configurePresets"_s).toArray();
    for (const QJsonValue &preset : list)
        presets.insert(preset.toObject().value(u"name"_s).toString(), preset.toObject());
    QStringList holding;
    for (auto it = presets.cbegin(); it != presets.cend(); ++it)
        if (isOn(cacheOf(presets, it.key()).value(kOption)))
            holding.append(it.key());
    holding.sort();
    return holding;
}

// The presets that leave it to whatever a build folder's cache already holds, as one configured by hand before would.
QStringList presetsLeavingItToTheCache(const QByteArray &json)
{
    QHash<QString, QJsonObject> presets;
    const QJsonArray list = QJsonDocument::fromJson(json).object().value(u"configurePresets"_s).toArray();
    for (const QJsonValue &preset : list)
        presets.insert(preset.toObject().value(u"name"_s).toString(), preset.toObject());
    QStringList leaving;
    for (auto it = presets.cbegin(); it != presets.cend(); ++it)
        if (!cacheOf(presets, it.key()).contains(kOption))
            leaving.append(it.key());
    leaving.sort();
    return leaving;
}

// Everything under a folder of the repository that decides what CI and a release build.
QStringList filesDecidingABuild()
{
    QStringList files;
    for (const QString &folder : { u"/.github/workflows"_s, u"/tools/ci"_s, u"/image"_s }) {
        QDirIterator it(kSource + folder, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            // What Python leaves beside a script it ran is no part of the repository, and holds the script's words.
            const QString file = it.next();
            if (!file.contains(u"/__pycache__/"_s))
                files.append(file);
        }
    }
    return files;
}

} // namespace

class TestStorefronts : public QObject
{
    Q_OBJECT

    QTemporaryDir m_storeDir;
    std::unique_ptr<qor::sim::SimServices> m_services;

private Q_SLOTS:
    void initTestCase()
    {
        // Set before anything can ask, in both builds: without the option it must be read by nothing.
        qputenv("QOR_STORE_DIR", m_storeDir.path().toUtf8());
        qputenv("QOR_STORE_DEMONSTRATION_URL",
                (kDemo + u"/v1/catalog.json|"_s + kDemo + u"/v2/catalog.json"_s).toUtf8());
    }

    // ── The build choice ─────────────────────────────────────────────────────

#ifndef QOR_STORE_DEMONSTRATION
    void anOrdinaryBuildHasNoSecondEngine()
    {
        QCOMPARE(Storefronts::engine(), nullptr);
        Storefronts fronts;
        QVERIFY(!fronts.isAvailable());
        QVERIFY(!fronts.isReady());
        QVERIFY(!fronts.isLoading());
        QVERIFY(fronts.error().isEmpty());
        QVERIFY(!fronts.creatorFeatures());
        QVERIFY(!fronts.hasNextCatalogue());
        QVERIFY(fronts.library().isEmpty());
        QCOMPARE(fronts.updateCount(), 0);
        for (const QString &view : { u"player"_s, u"creator"_s }) {
            QVERIFY(fronts.listings(view).isEmpty());
            QVERIFY(fronts.featured(view).isEmpty());
            QVERIFY(fronts.search(view, QString()).isEmpty());
            QVERIFY(fronts.categoryCounts(view).isEmpty());
        }
        const QString id = u"org.example.demo.game.ember-climb"_s;
        QVERIFY(fronts.item(u"player"_s, id).isEmpty());
        QVERIFY(fronts.status(id).isEmpty());
        QVERIFY(!fronts.install(id));
        QVERIFY(!fronts.remove(id));
        QVERIFY(!fronts.fetchPreview(id, 0));
        QVERIFY(!fronts.fetchLicence(id));
        // Nothing can set it going: the URL is set, and is read by nothing, and no folder was made for it.
        fronts.refresh();
        fronts.nextCatalogue();
        QCOMPARE(Storefronts::engine(), nullptr);
        QVERIFY(!QFileInfo::exists(Store::dataLocation() + u"/demonstration"_s));
    }
#else
    void aDemonstrationBuildHasASecondEngineOfItsOwn()
    {
        StoreEngine *second = Storefronts::engine();
        QVERIFY(second);
        QCOMPARE(Storefronts::engine(), second);
        StoreEngine *first = Store::engine();
        QVERIFY(first != second);
        QVERIFY(first->dataDir() != second->dataDir());
        QCOMPARE(second->dataDir(), Store::dataLocation() + u"/demonstration"_s);
        QTRY_COMPARE(second->state(), StoreEngine::State::Ready);
        QTRY_COMPARE(first->state(), StoreEngine::State::Ready);

        // It took the demonstration with the key compiled in, and the first engine still has its own catalogue.
        QCOMPARE(second->catalog().format, 2);
        QCOMPARE(second->lastSequence(), 1);
        QVERIFY(second->catalog().item(u"org.example.demo.game.ember-climb"_s));
        QCOMPARE(first->catalog().format, 1);
        QCOMPARE(first->lastSequence(), 0);
        QVERIFY(!first->catalog().item(u"org.example.demo.game.ember-climb"_s));

        Storefronts fronts;
        QVERIFY(fronts.isAvailable());
        QVERIFY(fronts.hasNextCatalogue());
        Store store;
        QTRY_VERIFY(store.isReady());
        QCOMPARE(store.items().size(), 14);
    }
#endif

    // Whichever way the build went, that is what its programs hold.
    void aProgramHoldsTheKeyOnlyIfItsBuildAskedForIt()
    {
        const QByteArray encoded = readFile(kDemo + u"/store.pub"_s).trimmed();
        const QByteArray raw = QByteArray::fromBase64(encoded);
        QCOMPARE(raw.size(), 65);
        QStringList programs { QCoreApplication::applicationFilePath(), QStringLiteral(QOR_SHELL_PROGRAM) };
#ifdef QOR_PRISM_PROGRAM
        programs.append(QStringLiteral(QOR_PRISM_PROGRAM));
#endif
        for (const QString &program : std::as_const(programs)) {
            QVERIFY2(QFileInfo::exists(program), qPrintable(program));
#ifdef QOR_STORE_DEMONSTRATION
            // Which also shows this scan can find it.
            QVERIFY2(holdsItsBytes(program, encoded), qPrintable(program));
#else
            QVERIFY2(!holdsItsBytes(program, encoded), qPrintable(program));
            QVERIFY2(!holdsItsBytes(program, raw), qPrintable(program));
#endif
        }
    }

    void noCiOrReleasePresetHoldsTheDemonstration()
    {
        // The check first, on presets made to fail it: by its own value, through a parent, and as CMake's other
        // spellings.
        const QByteArray planted = R"({ "configurePresets": [
            { "name": "base", "hidden": true, "cacheVariables": { "QOR_STORE_DEMONSTRATION": "ON" } },
            { "name": "ci-linux", "inherits": ["base"] },
            { "name": "ci-windows", "inherits": "base" },
            { "name": "linux-release", "cacheVariables": { "QOR_STORE_DEMONSTRATION": { "type": "BOOL", "value": "TRUE" } } },
            { "name": "windows-release", "cacheVariables": { "QOR_STORE_DEMONSTRATION": true } },
            { "name": "turned-back-off", "inherits": "base", "cacheVariables": { "QOR_STORE_DEMONSTRATION": "OFF" } },
            { "name": "linux-debug" }
        ] })";
        QCOMPARE(presetsHoldingIt(planted),
                 (QStringList { u"base"_s, u"ci-linux"_s, u"ci-windows"_s, u"linux-release"_s, u"windows-release"_s }));
        QCOMPARE(presetsLeavingItToTheCache(planted), QStringList { u"linux-debug"_s });

        // Every preset says, so that a folder once configured with it on by hand is turned back off by its preset.
        const QByteArray presets = readFile(kSource + u"/CMakePresets.json"_s);
        QCOMPARE(presetsLeavingItToTheCache(presets), QStringList());

        // Only a preset made for it, whose name says so, and none whose name says CI or a release.
        const QStringList holding = presetsHoldingIt(presets);
        QVERIFY(!holding.isEmpty());
        for (const QString &name : holding) {
            QVERIFY2(name.endsWith(u"-demonstration"_s), qPrintable(name));
            QVERIFY2(!name.startsWith(u"ci-"_s), qPrintable(name));
            QVERIFY2(!name.contains(u"release"_s), qPrintable(name));
        }

        // What CI, a release and the image build say nothing of it, but for the check that refuses a build folder
        // with it on, and name no such preset.
        const QStringList files = filesDecidingABuild();
        QVERIFY(files.size() > 3);
        const QString refusing = kSource + u"/tools/ci/check_hardening.py"_s;
        QVERIFY2(files.contains(refusing), qPrintable(refusing));
        QVERIFY(readFile(refusing).contains(kOption.toUtf8()));
        static const QRegularExpression preset(u"--preset[= ]+([A-Za-z0-9_-]+)"_s);
        QStringList named;
        for (const QString &file : files) {
            const QByteArray bytes = readFile(file);
            QVERIFY2(file == refusing || !bytes.contains(kOption.toUtf8()), qPrintable(file));
            auto matches = preset.globalMatch(QString::fromUtf8(bytes));
            while (matches.hasNext())
                named.append(matches.next().captured(1));
        }
        QVERIFY2(named.contains(u"ci-linux"_s), "the search for presets found none it should");
        for (const QString &name : std::as_const(named))
            QVERIFY2(!holding.contains(name), qPrintable(name));
    }

    // ── Views ────────────────────────────────────────────────────────────────

    void aPlayerPageHoldsNoCreatorListing_data()
    {
        QTest::addColumn<bool>("creatorFeatures");
        QTest::newRow("with creator features off") << false;
        QTest::newRow("and on, as a person who makes games still plays them") << true;
    }

    void aPlayerPageHoldsNoCreatorListing()
    {
        QFETCH(bool, creatorFeatures);
        TwoStores stores;
        Fronts all(stores.shop.store.publicKey(), { stores.shop.publish() });
        all.engine.setCreatorFeatures(creatorFeatures);
        QTRY_VERIFY(all.fronts->isReady());
        const Storefronts &fronts = *all.fronts;

        const QStringList justTheGame { stores.game.id };
        QCOMPARE(idsOf(fronts.listings(u"player"_s)), justTheGame);
        QCOMPARE(idsOf(fronts.featured(u"player"_s)), justTheGame);
        QCOMPARE(idsOf(fronts.search(u"player"_s, u"one"_s)), justTheGame);
        // What only the pack is called, and what its category is called, find nothing here.
        QVERIFY(fronts.search(u"player"_s, u"creator"_s).isEmpty());
        QVERIFY(fronts.search(u"player"_s, u"pixel"_s).isEmpty());
        QVERIFY(fronts.search(u"player"_s, u"2D art"_s).isEmpty());
        QCOMPARE(fronts.categoryCounts(u"player"_s), (QVariantMap { { u"Platformers"_s, 1 } }));
        QVERIFY(fronts.item(u"player"_s, stores.pack.id).isEmpty());
    }

    void theCreatorViewAnswersOnlyWithCreatorFeaturesOn()
    {
        TwoStores stores;
        Fronts all(stores.shop.store.publicKey(), { stores.shop.publish() });
        QTRY_VERIFY(all.fronts->isReady());
        const Storefronts &fronts = *all.fronts;

        QVERIFY(fronts.listings(u"creator"_s).isEmpty());
        QVERIFY(fronts.featured(u"creator"_s).isEmpty());
        QVERIFY(fronts.search(u"creator"_s, u"one"_s).isEmpty());
        QVERIFY(fronts.categoryCounts(u"creator"_s).isEmpty());
        QVERIFY(fronts.item(u"creator"_s, stores.pack.id).isEmpty());

        QSignalSpy changed(all.fronts, &Storefronts::creatorFeaturesChanged);
        QSignalSpy listings(all.fronts, &Storefronts::listingsChanged);
        all.engine.setCreatorFeatures(true);
        QVERIFY(fronts.creatorFeatures());
        QCOMPARE(changed.count(), 1);
        QCOMPARE(listings.count(), 1);

        const QStringList justThePack { stores.pack.id };
        QCOMPARE(idsOf(fronts.listings(u"creator"_s)), justThePack);
        QCOMPARE(idsOf(fronts.featured(u"creator"_s)), justThePack);
        QCOMPARE(idsOf(fronts.search(u"creator"_s, u"one"_s)), justThePack);
        QCOMPARE(fronts.categoryCounts(u"creator"_s), (QVariantMap { { u"2D art"_s, 1 } }));
        // A game is not found here because a person can also see the player store.
        QVERIFY(fronts.item(u"creator"_s, stores.game.id).isEmpty());
    }

    void aViewNeitherStoreIsCalledAnswersNothing_data()
    {
        QTest::addColumn<QString>("view");
        QTest::newRow("empty") << QString();
        QTest::newRow("both") << u"both"_s;
        QTest::newRow("capitalised") << u"Player"_s;
        QTest::newRow("all") << u"all"_s;
    }

    void aViewNeitherStoreIsCalledAnswersNothing()
    {
        QFETCH(QString, view);
        TwoStores stores;
        Fronts all(stores.shop.store.publicKey(), { stores.shop.publish() });
        all.engine.setCreatorFeatures(true);
        QTRY_VERIFY(all.fronts->isReady());
        const Storefronts &fronts = *all.fronts;
        QVERIFY(fronts.listings(view).isEmpty());
        QVERIFY(fronts.featured(view).isEmpty());
        QVERIFY(fronts.search(view, u"one"_s).isEmpty());
        QVERIFY(fronts.categoryCounts(view).isEmpty());
        QVERIFY(fronts.item(view, stores.game.id).isEmpty());
    }

    void aListingHoldsNoPathACatalogueChose()
    {
        TwoStores stores;
        stores.shop.items.last().insert(
            u"previews"_s,
            QJsonArray { QJsonObject { { u"file"_s, u"previews/one.png"_s }, { u"sha256"_s, hexDigest(realPng()) } } });
        Fronts all(stores.shop.store.publicKey(), { stores.shop.publish() });
        all.engine.setCreatorFeatures(true);
        QTRY_VERIFY(all.fronts->isReady());
        const QVariantMap listing = all.fronts->item(u"creator"_s, stores.pack.id);
        QCOMPARE(listing.value(u"previewCount"_s).toInt(), 1);
        QVERIFY(!listing.contains(u"previews"_s));
        QVERIFY(!listing.contains(u"package"_s));
        // What it is called in another store says nothing here either.
        QVERIFY(!listing.contains(u"placements"_s));
        QCOMPARE(listing.value(u"categories"_s).toStringList(), QStringList { u"2D art"_s });
        const QByteArray whole =
            QJsonDocument::fromVariant(listing).toJson()
            + QJsonDocument::fromVariant(all.fronts->status(listing.value(u"id"_s).toString())).toJson();
        QVERIFY2(!whole.contains("previews/one.png"), whole.constData());
        QVERIFY2(!whole.contains(".qorpkg"), whole.constData());
    }

    // ── What is off is refused, from here, with no page involved ─────────────

    void installRefusesWhatCreatorFeaturesKeepOutAndSaysWhy()
    {
        TwoStores stores;
        Fronts all(stores.shop.store.publicKey(), { stores.shop.publish() });
        QTRY_VERIFY(all.fronts->isReady());

        QVERIFY(!all.fronts->install(stores.pack.id));
        QTRY_COMPARE(all.finished->count(), 1);
        QVERIFY(!all.finished->at(0).at(1).toBool());
        QVERIFY2(all.finished->at(0).at(2).toString().contains(u"creator features are off"_s),
                 qPrintable(all.finished->at(0).at(2).toString()));
        QVERIFY(all.engine.installed().isEmpty());
        QVERIFY(!all.fronts->status(stores.pack.id).value(u"installed"_s).toBool());

        // Neither is the engine reached any other way, nor anything the catalogue never listed taken on trust.
        all.finished->clear();
        QVERIFY(!all.engine.install(stores.pack.id));
        QCOMPARE(all.finished->count(), 1);
        all.finished->clear();
        QVERIFY(!all.fronts->install(u"org.example.pack.none"_s));
        QCOMPARE(all.finished->count(), 1);
        QVERIFY2(all.finished->at(0).at(2).toString().contains(u"not in this catalogue"_s),
                 qPrintable(all.finished->at(0).at(2).toString()));
        QVERIFY(all.engine.installed().isEmpty());
    }

    void aDraftIsRefusedWithItsReason()
    {
        Fronts all(publicKey(kDemo + u"/store.pub"_s), { QUrl::fromLocalFile(kDemo + u"/v1/catalog.json"_s) });
        all.engine.setCreatorFeatures(true);
        QTRY_VERIFY(all.fronts->isReady());
        const QString project = u"org.example.demo.project.ember-climb"_s;
        QVERIFY(all.engine.catalog().draft(project));
        QVERIFY(!all.fronts->install(project));
        QCOMPARE(all.finished->count(), 1);
        QVERIFY(!all.finished->at(0).at(1).toBool());
        QVERIFY(!all.finished->at(0).at(2).toString().isEmpty());
        for (const QString &view : { u"player"_s, u"creator"_s })
            QVERIFY(!idsOf(all.fronts->listings(view)).contains(project));
    }

    // ── A person's walk, on the demonstration's own catalogues ───────────────

    void theDemonstrationCanBeWalked()
    {
        const QUrl v1 = QUrl::fromLocalFile(kDemo + u"/v1/catalog.json"_s);
        const QUrl v2 = QUrl::fromLocalFile(kDemo + u"/v2/catalog.json"_s);
        const QString game = u"org.example.demo.game.ember-climb"_s;
        const QString pack = u"org.example.demo.pack.ember-climb-art"_s;
        Fronts all(publicKey(kDemo + u"/store.pub"_s), { v1, v2 });
        Storefronts &fronts = *all.fronts;
        QTRY_VERIFY(fronts.isReady());
        QVERIFY(fronts.hasNextCatalogue());
        QVERIFY(fronts.error().isEmpty());

        // A player finds the game, with its licence and where it came from, and gets it.
        QCOMPARE(idsOf(fronts.listings(u"player"_s)), QStringList { game });
        const QVariantMap listing = fronts.item(u"player"_s, game);
        QCOMPARE(listing.value(u"licence"_s).toMap().value(u"spdx"_s).toString(), u"Apache-2.0"_s);
        QVERIFY(!listing.value(u"provenance"_s).toMap().value(u"origin"_s).toString().isEmpty());
        QVERIFY(!listing.value(u"provenance"_s).toMap().value(u"permission"_s).toString().isEmpty());
        QVERIFY(fronts.install(game));
        QTRY_COMPARE(all.finished->count(), 1);
        QVERIFY2(all.finished->at(0).at(1).toBool(), qPrintable(all.finished->at(0).at(2).toString()));
        QCOMPARE(fronts.status(game).value(u"installedVersion"_s).toString(), u"1.0.0"_s);
        QCOMPARE(fronts.status(game).value(u"name"_s).toString(), listing.value(u"name"_s).toString());

        // A maker turns creator features on, and gets the pack.
        all.engine.setCreatorFeatures(true);
        all.finished->clear();
        QCOMPARE(idsOf(fronts.listings(u"creator"_s)), QStringList { pack });
        QVERIFY(fronts.install(pack));
        QTRY_COMPARE(all.finished->count(), 1);
        QVERIFY2(all.finished->at(0).at(1).toBool(), qPrintable(all.finished->at(0).at(2).toString()));
        QCOMPARE(fronts.status(pack).value(u"installedVersion"_s).toString(), u"1.0.0"_s);
        QVERIFY(!fronts.status(pack).value(u"updateAvailable"_s).toBool());
        QCOMPARE(fronts.library().size(), 2);
        QCOMPARE(fronts.updateCount(), 0);

        // The catalogue moves on, and the pack can be updated.
        QSignalSpy state(&fronts, &Storefronts::stateChanged);
        fronts.nextCatalogue();
        QVERIFY(!fronts.hasNextCatalogue());
        QTRY_VERIFY(fronts.isReady());
        QVERIFY(state.count() > 0);
        QCOMPARE(all.engine.lastSequence(), 2);
        QVERIFY(fronts.status(pack).value(u"updateAvailable"_s).toBool());
        QCOMPARE(fronts.status(pack).value(u"version"_s).toString(), u"1.1.0"_s);
        QCOMPARE(fronts.updateCount(), 1);
        // Nothing more to move on to.
        fronts.nextCatalogue();
        QCOMPARE(all.engine.state(), StoreEngine::State::Ready);

        // With creator features off the Creator Store is gone, the pack stays, and only its update waits.
        all.engine.setCreatorFeatures(false);
        QVERIFY(fronts.listings(u"creator"_s).isEmpty());
        QVERIFY(fronts.status(pack).value(u"installed"_s).toBool());
        QVERIFY(!fronts.status(pack).value(u"updateAvailable"_s).toBool());
        QVERIFY(fronts.status(pack).value(u"updateWaits"_s).toBool());
        QCOMPARE(fronts.updateCount(), 0);
        QCOMPARE(fronts.waitingUpdateCount(), 1);
        QVERIFY(idsOf(fronts.library()).contains(pack));
        QVERIFY(all.engine.isIntact(pack));
        all.finished->clear();
        QVERIFY(!fronts.install(pack));
        QCOMPARE(all.finished->count(), 1);
        QVERIFY(all.finished->at(0).at(2).toString().contains(u"creator features are off"_s));
        QCOMPARE(fronts.status(pack).value(u"installedVersion"_s).toString(), u"1.0.0"_s);

        // On again, it updates.
        all.engine.setCreatorFeatures(true);
        all.finished->clear();
        QCOMPARE(fronts.updateCount(), 1);
        QVERIFY(fronts.install(pack));
        QTRY_COMPARE(all.finished->count(), 1);
        QVERIFY2(all.finished->at(0).at(1).toBool(), qPrintable(all.finished->at(0).at(2).toString()));
        QCOMPARE(fronts.status(pack).value(u"installedVersion"_s).toString(), u"1.1.0"_s);
        QCOMPARE(fronts.updateCount(), 0);

        QCOMPARE(fronts.waitingUpdateCount(), 0);

        // Off for good: removing what was installed needs nothing of the store it came from.
        all.engine.setCreatorFeatures(false);
        QVERIFY(fronts.remove(pack));
        QVERIFY(!fronts.status(pack).value(u"installed"_s).toBool());
        QCOMPARE(idsOf(fronts.library()), QStringList { game });
    }

    // One answer for every page and count: an update of what the switch keeps out waits, and nothing else's does.
    void anUpdateOfWhatOnlyTheCreatorStoreCarriesWaitsForTheSwitch()
    {
        Shop shop;
        Pack game = gamePack();
        Pack pack;
        shop.add(game);
        shop.add(pack);
        QTemporaryDir data;
        StoreEngine engine(data.path(), shop.store.publicKey());
        engine.setCreatorFeatures(true);
        QCOMPARE(refreshed(engine, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(engine, game.id).ok);
        QVERIFY(installAndWait(engine, pack.id).ok);
        QVERIFY(!engine.hasUpdate(game.id));
        QVERIFY(!engine.hasUpdate(pack.id));
        QCOMPARE(engine.updateCount(), 0);

        shop.items.clear();
        game.version = u"1.1.0"_s;
        pack.version = u"1.1.0"_s;
        shop.add(game);
        shop.add(pack);
        shop.sequence = 2;
        QCOMPARE(refreshed(engine, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(engine.hasUpdate(game.id));
        QVERIFY(engine.hasUpdate(pack.id));
        QCOMPARE(engine.updateCount(), 2);

        // The count Store gives its pages is this one, whatever the engine under it holds.
        Store::setEngineForTests(&engine);
        const auto restore = qScopeGuard([] { Store::setEngineForTests(nullptr); });
        const Store store;
        QCOMPARE(store.updateCount(), 2);

        engine.setCreatorFeatures(false);
        QVERIFY(engine.hasUpdate(game.id));
        QVERIFY(!engine.hasUpdate(pack.id));
        QCOMPARE(engine.updateCount(), 1);
        QCOMPARE(store.updateCount(), 1);
        QVERIFY(!engine.hasUpdate(u"org.example.pack.none"_s));

        engine.setCreatorFeatures(true);
        QCOMPARE(engine.updateCount(), 2);
        QCOMPARE(store.updateCount(), 2);

        // Nor is an update offered that install would refuse for another reason.
        engine.setEngines({ { u"qq"_s, u"9.0.0"_s } });
        QVERIFY(!engine.hasUpdate(game.id));
        QCOMPARE(store.updateCount(), 0);
        engine.setEngines({ { u"qq"_s, u"0.5.0"_s } });
        QCOMPARE(store.updateCount(), 2);

        // An update is said to wait for the switch only when nothing else would refuse it, and the page hears so.
        Storefronts::useEngine(&engine, {});
        const auto unhand = qScopeGuard([] { Storefronts::useEngine(nullptr, {}); });
        const Storefronts fronts;
        QVERIFY(!engine.updateWaitsForCreatorFeatures(pack.id));
        engine.setCreatorFeatures(false);
        QVERIFY(engine.updateWaitsForCreatorFeatures(pack.id));
        QVERIFY(!engine.updateWaitsForCreatorFeatures(game.id));
        QCOMPARE(fronts.waitingUpdateCount(), 1);
        QCOMPARE(fronts.status(pack.id).value(u"updateWaits"_s).toBool(), true);
        engine.setEngines({ { u"qq"_s, u"9.0.0"_s } });
        QVERIFY(!engine.updateWaitsForCreatorFeatures(pack.id));
        QCOMPARE(fronts.waitingUpdateCount(), 0);
        QCOMPARE(fronts.status(pack.id).value(u"updateWaits"_s).toBool(), false);
        engine.setEngines({ { u"qq"_s, u"0.5.0"_s } });
        QCOMPARE(fronts.waitingUpdateCount(), 1);
    }

    // With the switch off, a listing only the creator store carries is not described to anything that asks by its id:
    // not by name, nor by version. What is installed keeps its own name, and its version.
    void aCreatorOnlyListingTellsNothingOfItselfWithTheSwitchOff()
    {
        TwoStores stores;
        Fronts all(stores.shop.store.publicKey(), { stores.shop.publish() });
        Storefronts &fronts = *all.fronts;
        QTRY_VERIFY(fronts.isReady());
        const QString id = stores.pack.id;
        QVariantMap status = fronts.status(id);
        QCOMPARE(status.value(u"name"_s).toString(), id);
        QVERIFY(status.value(u"version"_s).toString().isEmpty());
        QCOMPARE(status.value(u"installed"_s).toBool(), false);

        all.engine.setCreatorFeatures(true);
        status = fronts.status(id);
        QCOMPARE(status.value(u"name"_s).toString(), u"One creator pack"_s);
        QCOMPARE(status.value(u"version"_s).toString(), stores.pack.version);
        QVERIFY(installAndWait(all.engine, id).ok);

        all.engine.setCreatorFeatures(false);
        status = fronts.status(id);
        QCOMPARE(status.value(u"installed"_s).toBool(), true);
        QCOMPARE(status.value(u"installedVersion"_s).toString(), stores.pack.version);
        QCOMPARE(status.value(u"name"_s).toString(), all.engine.installed().value(id).name);
        QVERIFY(status.value(u"version"_s).toString().isEmpty());
        const QVariantList library = fronts.library();
        QCOMPARE(library.size(), 1);
        QCOMPARE(library.constFirst().toMap().value(u"previewCount"_s).toInt(), 0);
    }

    // Every catalogue moved past is still signed, so it is shown refused rather than taken, in plain words.
    void anOlderCatalogueIsRefusedInPlainWordsAndTheNextStillOpens()
    {
        const QUrl v1 = QUrl::fromLocalFile(kDemo + u"/v1/catalog.json"_s);
        const QUrl v2 = QUrl::fromLocalFile(kDemo + u"/v2/catalog.json"_s);
        QTemporaryDir data;
        StoreEngine engine(data.path(), publicKey(kDemo + u"/store.pub"_s));
        QCOMPARE(refreshed(engine, v2), StoreEngine::State::Ready);

        // As it is when the demonstration starts again over a folder that has already seen the second.
        Storefronts::useEngine(&engine, { v1, v2 });
        Storefronts fronts;
        QTRY_VERIFY(!fronts.isLoading());
        QVERIFY(!fronts.isReady());
        QVERIFY2(fronts.error().contains(u"older"_s), qPrintable(fronts.error()));
        QVERIFY(fronts.hasNextCatalogue());
        fronts.nextCatalogue();
        QTRY_VERIFY(fronts.isReady());
        Storefronts::useEngine(nullptr, {});
    }

    void aCatalogueNotConfiguredIsSaidPlainly()
    {
        QTemporaryDir data;
        StoreEngine engine(data.path(), publicKey(kDemo + u"/store.pub"_s));
        Storefronts::useEngine(&engine, {});
        Storefronts fronts;
        fronts.refresh();
        QTRY_VERIFY(!fronts.isLoading());
        QVERIFY(!fronts.isReady());
        QVERIFY2(fronts.error().contains(u"QOR_STORE_DEMONSTRATION_URL"_s), qPrintable(fronts.error()));
        QVERIFY(!fronts.hasNextCatalogue());
        Storefronts::useEngine(nullptr, {});
    }

    void theFormatOneEngineAndTheSecondKeepToTheirOwnFolders()
    {
        QTemporaryDir data;
        const QString second = data.path() + u"/demonstration"_s;
        const QString pack = u"org.example.demo.pack.ember-climb-art"_s;
        {
            StoreEngine demonstration(second, publicKey(kDemo + u"/store.pub"_s));
            demonstration.setCreatorFeatures(true);
            QCOMPARE(refreshed(demonstration, QUrl::fromLocalFile(kDemo + u"/v1/catalog.json"_s)),
                     StoreEngine::State::Ready);
            QVERIFY(installAndWait(demonstration, pack).ok);
        }
        // The first engine starting over the folder that holds it finds none of it, and leaves it alone.
        StoreEngine first(data.path(), publicKey(QStringLiteral(QOR_STORE_PUB)));
        QVERIFY(first.installed().isEmpty());
        QCOMPARE(refreshed(first, QUrl::fromLocalFile(QStringLiteral(QOR_STORE_CATALOG) + u"/catalog.json"_s)),
                 StoreEngine::State::Ready);
        QCOMPARE(first.catalog().format, 1);
        QCOMPARE(first.lastSequence(), 0);
        QVERIFY(first.listings(View::Player).isEmpty());
        QVERIFY(first.installed().isEmpty());

        // And the second, starting again, has what it had, and takes neither the first's catalogue nor format 1 at all.
        StoreEngine demonstration(second, publicKey(kDemo + u"/store.pub"_s));
        QVERIFY(demonstration.installed().contains(pack));
        QCOMPARE(refreshed(demonstration, QUrl::fromLocalFile(QStringLiteral(QOR_STORE_CATALOG) + u"/catalog.json"_s)),
                 StoreEngine::State::Failed);
        QVERIFY2(demonstration.error().contains(u"signature"_s), qPrintable(demonstration.error()));
    }

    // ── Previews, from the hostile side ──────────────────────────────────────

    void aPreviewIsHandedOverOnlyAsCheckedBytes()
    {
        const QByteArray picture = realPng();
        TwoStores stores;
        const auto preview = [](const QString &file, const QByteArray &digestOf) {
            return QJsonObject { { u"file"_s, file }, { u"sha256"_s, hexDigest(digestOf) } };
        };
        const QByteArray huge = pngClaiming(60000, 60000);
        const QByteArray wide = pngClaiming(100000, 1);
        const QByteArray truncated = picture.left(12);
        const QString folder = stores.shop.dir.path() + u"/previews/"_s;
        writeFile(folder + u"good.png"_s, picture);
        writeFile(folder + u"altered.png"_s, picture + "altered");
        writeFile(folder + u"huge.png"_s, huge);
        writeFile(folder + u"wide.png"_s, wide);
        writeFile(folder + u"truncated.png"_s, truncated);
        writeFile(folder + u"fake.png"_s, kPng);
        QJsonObject &listing = stores.shop.items.first();
        listing.insert(
            u"previews"_s,
            QJsonArray { preview(u"previews/good.png"_s, picture), preview(u"previews/altered.png"_s, picture),
                         preview(u"previews/huge.png"_s, huge), preview(u"previews/wide.png"_s, wide),
                         preview(u"previews/truncated.png"_s, truncated), preview(u"previews/fake.png"_s, kPng) });
        Fronts all(stores.shop.store.publicKey(), { stores.shop.publish() });
        Storefronts &fronts = *all.fronts;
        QTRY_VERIFY(fronts.isReady());
        const QString id = stores.game.id;
        QSignalSpy refused(&fronts, &Storefronts::previewRefused);
        QSignalSpy changed(&fronts, &Storefronts::previewsChanged);

        QVERIFY(fronts.preview(id, 0).isEmpty());
        QVERIFY(fronts.fetchPreview(id, 0));
        QVERIFY(fronts.preview(id, 0).isEmpty()); // not until it has been checked
        QTRY_VERIFY(!fronts.preview(id, 0).isEmpty());
        QCOMPARE(changed.count(), 1);
        const QString url = fronts.preview(id, 0).toString();
        QVERIFY2(url.startsWith(u"data:image/png;base64,"_s), qPrintable(url.left(40)));
        QCOMPARE(QByteArray::fromBase64(url.mid(url.indexOf(u',') + 1).toLatin1()), picture);
        QVERIFY(!url.contains(u"good.png"_s));
        // Asked again, it is not fetched again.
        QVERIFY(fronts.fetchPreview(id, 0));
        QTest::qWait(30);
        QCOMPARE(changed.count(), 1);

        const auto refusal = [&](int index) {
            refused.clear();
            if (!fronts.fetchPreview(id, index) || !QTest::qWaitFor([&refused] { return refused.count() > 0; }, 10000))
                return u"not refused"_s;
            return refused.at(0).at(2).toString();
        };
        {
            const QString why = refusal(1);
            QVERIFY2(why.contains(u"does not match"_s), qPrintable(why));
        }
        {
            const QString why = refusal(2);
            QVERIFY2(why.contains(u"larger"_s), qPrintable(why));
        }
        {
            const QString why = refusal(3);
            QVERIFY2(why.contains(u"larger"_s), qPrintable(why));
        }
        {
            const QString why = refusal(4);
            QVERIFY2(why.contains(u"not a picture"_s), qPrintable(why));
        }
        {
            const QString why = refusal(5);
            QVERIFY2(why.contains(u"not a picture"_s), qPrintable(why));
        }
        for (int index = 1; index <= 5; ++index)
            QVERIFY(fronts.preview(id, index).isEmpty());

        QVERIFY(!fronts.fetchPreview(id, 6));
        QVERIFY(!fronts.fetchPreview(id, -1));
        QVERIFY(!fronts.fetchPreview(u"org.example.pack.none"_s, 0));
    }

    void whatOnlyTheCreatorStoreCarriesIsNotHandedOverWithItsSwitchOff()
    {
        const QByteArray picture = realPng();
        TwoStores stores;
        writeFile(stores.shop.dir.path() + u"/previews/one.png"_s, picture);
        stores.shop.items.last().insert(
            u"previews"_s,
            QJsonArray { QJsonObject { { u"file"_s, u"previews/one.png"_s }, { u"sha256"_s, hexDigest(picture) } } });
        Fronts all(stores.shop.store.publicKey(), { stores.shop.publish() });
        Storefronts &fronts = *all.fronts;
        QTRY_VERIFY(fronts.isReady());
        const QString id = stores.pack.id;
        QVERIFY(!fronts.fetchPreview(id, 0));
        QVERIFY(!fronts.fetchLicence(id));

        all.engine.setCreatorFeatures(true);
        QVERIFY(fronts.fetchPreview(id, 0));
        QVERIFY(fronts.fetchLicence(id));
        QTRY_VERIFY(!fronts.preview(id, 0).isEmpty());
        QTRY_VERIFY(fronts.licence(id).contains(u"text"_s));

        // Turned off, what was fetched while it was on is no longer there to be had.
        all.engine.setCreatorFeatures(false);
        QVERIFY(fronts.preview(id, 0).isEmpty());
        QVERIFY(fronts.licence(id).isEmpty());

        // Nor what was on its way when it went off. Each arrives after this, on the next turn of the event loop.
        all.engine.setCreatorFeatures(true);
        QVERIFY(fronts.fetchPreview(id, 0));
        QVERIFY(fronts.fetchLicence(id));
        all.engine.setCreatorFeatures(false);
        QTest::qWait(100);
        QVERIFY(fronts.preview(id, 0).isEmpty());
        QVERIFY(fronts.licence(id).isEmpty());

        // The game beside it is another matter.
        all.engine.setCreatorFeatures(true);
    }

    void aPreviewOutlivesNoCatalogueAndHoldsABoundedAmountOfMemory()
    {
        // More than can be held together: each says it is small, and is padded to nearly the most a preview may be.
        // Held as base64 text in a data: URL, one takes a third of the bound and a little more, so three fit.
        const QByteArray large = pngClaiming(16, 16, 3900 * 1024);
        const int each = 5;
        TwoStores stores;
        QJsonArray previews;
        for (int i = 0; i < each; ++i) {
            const QString file = u"previews/%1.png"_s.arg(i);
            writeFile(stores.shop.dir.path() + u'/' + file, large);
            previews.append(QJsonObject { { u"file"_s, file }, { u"sha256"_s, hexDigest(large) } });
        }
        stores.shop.items.first().insert(u"previews"_s, previews);
        stores.shop.items.last().insert(u"previews"_s, previews);
        Fronts all(stores.shop.store.publicKey(), { stores.shop.publish() });
        all.engine.setCreatorFeatures(true);
        Storefronts &fronts = *all.fronts;
        QTRY_VERIFY(fronts.isReady());
        const QStringList ids { stores.game.id, stores.pack.id };
        QSignalSpy changed(&fronts, &Storefronts::previewsChanged);
        for (const QString &id : ids)
            for (int i = 0; i < each; ++i)
                QVERIFY(fronts.fetchPreview(id, i));
        QTRY_COMPARE(changed.count(), 2 * each);
        const auto held = [&] {
            QStringList keys;
            for (const QString &id : ids)
                for (int i = 0; i < each; ++i)
                    if (!fronts.preview(id, i).isEmpty())
                        keys.append(u"%1#%2"_s.arg(id).arg(i));
            return keys;
        };
        // The three that came last: the oldest went first, to make room for each.
        const QString &pack = stores.pack.id;
        QCOMPARE(held(), (QStringList { pack + u"#2"_s, pack + u"#3"_s, pack + u"#4"_s }));

        // One let go to make room may be asked for again, as a page built again does, and the oldest then goes.
        QVERIFY(fronts.fetchPreview(stores.game.id, 0));
        QTRY_COMPARE(changed.count(), 2 * each + 1);
        QCOMPARE(held(), (QStringList { stores.game.id + u"#0"_s, pack + u"#3"_s, pack + u"#4"_s }));

        // A new catalogue forgets them all.
        stores.shop.sequence = 2;
        stores.shop.publish();
        fronts.refresh();
        QTRY_VERIFY(fronts.isReady());
        QVERIFY(held().isEmpty());
    }

    // "Check for Updates" finds the same catalogue: what was fetched of it stays, and nothing is fetched again. One
    // that differs forgets it.
    void whatWasFetchedIsKeptThroughTheSameCatalogueAndForgottenWithAnother()
    {
        const QByteArray picture = realPng();
        TwoStores stores;
        writeFile(stores.shop.dir.path() + u"/previews/one.png"_s, picture);
        stores.shop.items.first().insert(
            u"previews"_s,
            QJsonArray { QJsonObject { { u"file"_s, u"previews/one.png"_s }, { u"sha256"_s, hexDigest(picture) } } });
        Fronts all(stores.shop.store.publicKey(), { stores.shop.publish() });
        Storefronts &fronts = *all.fronts;
        QTRY_VERIFY(fronts.isReady());
        const QString id = stores.game.id;
        QVERIFY(fronts.fetchPreview(id, 0));
        QVERIFY(fronts.fetchLicence(id));
        QTRY_VERIFY(!fronts.preview(id, 0).isEmpty());
        QTRY_VERIFY(fronts.licence(id).contains(u"text"_s));
        QSignalSpy forgotten(&fronts, &Storefronts::previewsForgotten);
        QSignalSpy previews(&all.engine, &StoreEngine::previewFetched);

        fronts.refresh();
        QVERIFY(fronts.isLoading());
        QTRY_VERIFY(fronts.isReady());
        QCOMPARE(forgotten.count(), 0);
        QVERIFY(!fronts.preview(id, 0).isEmpty());
        QVERIFY(fronts.licence(id).contains(u"text"_s));
        QVERIFY(fronts.fetchPreview(id, 0));
        QTest::qWait(50);
        QCOMPARE(previews.count(), 0);

        stores.shop.sequence = 2;
        stores.shop.publish();
        fronts.refresh();
        QTRY_VERIFY(fronts.isReady());
        QCOMPARE(forgotten.count(), 1);
        QVERIFY(fronts.preview(id, 0).isEmpty());
        QVERIFY(fronts.licence(id).isEmpty());
    }

    // A page of many listings asks for every preview at once. A few are on their way at a time, and the rest wait their
    // turn, so they are not all in memory at once as they arrive.
    void previewsComeAFewAtATime()
    {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        const QByteArray picture = realPng();
        TwoStores stores;
        QJsonArray previews;
        for (int i = 0; i < 8; ++i) {
            const QString file = u"previews/%1.png"_s.arg(i);
            writeFile(stores.shop.dir.path() + u'/' + file, picture);
            previews.append(QJsonObject { { u"file"_s, file }, { u"sha256"_s, hexDigest(picture) } });
        }
        stores.shop.items.first().insert(u"previews"_s, previews);
        TinyServer server;
        const QUrl url = serve(stores.shop, server);
        for (int i = 0; i < 8; ++i)
            server.held.insert(u"previews/%1.png"_s.arg(i));
        Fronts all(stores.shop.store.publicKey(), { url });
        Storefronts &fronts = *all.fronts;
        QTRY_VERIFY(fronts.isReady());
        QSignalSpy changed(&fronts, &Storefronts::previewsChanged);
        for (int i = 0; i < 8; ++i)
            QVERIFY(fronts.fetchPreview(stores.game.id, i));
        QTRY_COMPARE(server.waiting(), 4);
        QTest::qWait(300);
        QCOMPARE(server.waiting(), 4);
        QCOMPARE(changed.count(), 0);

        server.release();
        QTRY_COMPARE(changed.count(), 8);
        for (int i = 0; i < 8; ++i) {
            QVERIFY(!fronts.preview(stores.game.id, i).isEmpty());
            QCOMPARE(server.asked.count(u"previews/%1.png"_s.arg(i)), 1);
        }
    }

    // ── Licences, from the hostile side ──────────────────────────────────────

    void aLicenceIsReadFromTheCheckedPackageAndInstallsNothing()
    {
        const QByteArray text = QByteArrayLiteral("Copyright Someone <someone@example.org>.\nFree to use.\n");
        Shop shop;
        Pack game = gamePack();
        game.files.insert(u"LICENSE.txt"_s, text);
        shop.add(game);
        Fronts all(shop.store.publicKey(), { shop.publish() });
        Storefronts &fronts = *all.fronts;
        QTRY_VERIFY(fronts.isReady());

        QVERIFY(fronts.licence(game.id).isEmpty());
        QVERIFY(fronts.fetchLicence(game.id));
        QCOMPARE(fronts.licence(game.id).value(u"asked"_s).toBool(), true);
        QVERIFY(!fronts.licence(game.id).contains(u"text"_s));
        QTRY_VERIFY(fronts.licence(game.id).contains(u"text"_s));
        QCOMPARE(fronts.licence(game.id).value(u"text"_s).toString().toUtf8(), text);
        QVERIFY(!fronts.licence(game.id).contains(u"error"_s));

        QVERIFY(all.engine.installed().isEmpty());
        QVERIFY(QDir(all.data.path() + u"/items"_s).isEmpty());
    }

    void aLicenceIsNotBelievedFromAPackageThatFailsItsChecks_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("expected");
        QTest::newRow("altered after signing") << u"altered"_s << u"download does not match the catalogue"_s;
        QTest::newRow("another of its developer's packages in its place")
            << u"swapped"_s << u"download does not match the catalogue"_s;
        QTest::newRow("signed by someone else") << u"forged"_s << u"signature"_s;
        QTest::newRow("markup in the text") << u"markup"_s << u"LICENSE.txt"_s;
        QTest::newRow("too long to lay out") << u"long"_s << u"too long"_s;
        QTest::newRow("missing altogether") << u"missing"_s << u"cannot be read"_s;
    }

    void aLicenceIsNotBelievedFromAPackageThatFailsItsChecks()
    {
        QFETCH(QString, name);
        QFETCH(QString, expected);
        Shop shop;
        Pack game = gamePack();
        if (name == u"markup"_s)
            game.files.insert(u"LICENSE.txt"_s, "<img src=\"https://example.org/x.png\">");
        if (name == u"long"_s)
            game.files.insert(u"LICENSE.txt"_s, QByteArray(300 * 1024, 'x'));
        const TestSigner someoneElse;
        const QByteArray package = game.package(name == u"forged"_s ? someoneElse : shop.developer);
        QJsonObject &listing = shop.addPackage(game, package);
        const QString path = shop.dir.path() + u"/packages/%1-%2.qorpkg"_s.arg(game.id, game.version);
        if (name == u"altered"_s) {
            QByteArray altered = package;
            altered[altered.size() / 2] = char(altered.at(altered.size() / 2) ^ 1);
            writeFile(path, altered);
        }
        if (name == u"missing"_s)
            QVERIFY(QFile::remove(path));
        if (name == u"swapped"_s) {
            // Signed by the right developer and whole, with a licence that says something else in as many bytes, so
            // that it is the same size the catalogue lists.
            Pack other = game;
            other.files.insert(u"LICENSE.txt"_s, QByteArrayLiteral("Use it as is\n"));
            QCOMPARE(other.files.value(u"LICENSE.txt"_s).size(), kLicence.size());
            writeFile(path, other.package(shop.developer));
        }
        Q_UNUSED(listing)

        Fronts all(shop.store.publicKey(), { shop.publish() });
        Storefronts &fronts = *all.fronts;
        QTRY_VERIFY(fronts.isReady());
        QVERIFY(fronts.fetchLicence(game.id));
        QTRY_VERIFY(fronts.licence(game.id).contains(u"error"_s));
        QVERIFY(!fronts.licence(game.id).contains(u"text"_s));
        const QString error = fronts.licence(game.id).value(u"error"_s).toString();
        QVERIFY2(error.contains(expected), qPrintable(error));
        QVERIFY(all.engine.installed().isEmpty());
    }

    void aCatalogueThatHasExpiredHandsOverNothing()
    {
        Shop shop;
        Pack game = gamePack();
        shop.add(game);
        const QDate today = QDateTime::currentDateTimeUtc().date();
        shop.expires = today.toString(Qt::ISODate);
        // In date as it arrives, to be run out from under it below: an engine that can be handed another.
        class Expiring : public StoreEngine
        {
        public:
            using StoreEngine::adoptCatalog;
            using StoreEngine::StoreEngine;
        };
        QTemporaryDir data;
        Expiring engine(data.path(), shop.store.publicKey());
        QCOMPARE(refreshed(engine, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(engine.fetchLicence(game.id));
        Catalog expired = engine.catalog();
        expired.expires = today.addDays(-1);
        engine.adoptCatalog(expired);
        QVERIFY(!engine.fetchLicence(game.id));
        QVERIFY(!engine.fetchPreview(game.id, 0));
        QVERIFY(engine.listings(View::Player).isEmpty());
    }

    // The engine's own check, not the page's: a licence read from a file is not stopped as the switch goes off, and
    // arriving after, it is refused.
    void aLicenceThatArrivesAfterTheSwitchWentOffIsRefusedByTheEngine()
    {
        TwoStores stores;
        QTemporaryDir data;
        StoreEngine engine(data.path(), stores.shop.store.publicKey());
        engine.setCreatorFeatures(true);
        QCOMPARE(refreshed(engine, stores.shop.publish()), StoreEngine::State::Ready);
        QSignalSpy fetched(&engine, &StoreEngine::licenceFetched);
        QVERIFY(engine.fetchLicence(stores.pack.id));
        engine.setCreatorFeatures(false);
        QCOMPARE(fetched.count(), 0); // read on the next turn of the event loop
        QTRY_COMPARE(fetched.count(), 1);
        QCOMPARE(fetched.at(0).at(0).toString(), stores.pack.id);
        QCOMPARE(fetched.at(0).at(1).toBool(), false);
        QVERIFY(fetched.at(0).at(2).toString().isEmpty());
        const QString why = fetched.at(0).at(3).toString();
        QVERIFY2(why.contains(u"creator features are off"_s), qPrintable(why));

        // What players have is read with it off.
        QVERIFY(engine.fetchLicence(stores.game.id));
        QTRY_COMPARE(fetched.count(), 2);
        QVERIFY2(fetched.at(1).at(1).toBool(), qPrintable(fetched.at(1).at(3).toString()));
    }

    // A licence is a whole package on its way, so it is fetched once however often it is asked for, and it stops,
    // saying why, as soon as it is no longer wanted: when the switch goes off, or a catalogue that differs arrives.
    void aLicenceIsOnItsWayOnceAndStopsWhenNoLongerWanted()
    {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        TwoStores stores;
        TinyServer server;
        const QUrl url = serve(stores.shop, server);
        const QString packFile = u"packages/%1-%2.qorpkg"_s.arg(stores.pack.id, stores.pack.version);
        const QString gameFile = u"packages/%1-%2.qorpkg"_s.arg(stores.game.id, stores.game.version);
        QVERIFY(server.files.contains(packFile) && server.files.contains(gameFile));
        server.held = { packFile, gameFile };
        QTemporaryDir data;
        StoreEngine engine(data.path(), stores.shop.store.publicKey());
        engine.setCreatorFeatures(true);
        QCOMPARE(refreshed(engine, url), StoreEngine::State::Ready);
        QSignalSpy fetched(&engine, &StoreEngine::licenceFetched);

        QVERIFY(engine.fetchLicence(stores.pack.id));
        QVERIFY(engine.fetchLicence(stores.pack.id));
        QVERIFY(engine.fetchLicence(stores.game.id));
        QTRY_COMPARE(server.waiting(), 2);
        QTest::qWait(200);
        QCOMPARE(server.asked.count(packFile), 1);
        QCOMPARE(fetched.count(), 0);

        // The switch goes off: what only the creator store carries stops there and then, and says why.
        engine.setCreatorFeatures(false);
        QCOMPARE(fetched.count(), 1);
        QCOMPARE(fetched.at(0).at(0).toString(), stores.pack.id);
        QCOMPARE(fetched.at(0).at(1).toBool(), false);
        QVERIFY2(fetched.at(0).at(3).toString().contains(u"creator features are off"_s),
                 qPrintable(fetched.at(0).at(3).toString()));

        // A catalogue that differs stops the rest.
        stores.shop.sequence = 2;
        const QByteArray json = stores.shop.json();
        server.files.insert(u"catalog.json"_s, json);
        server.files.insert(u"catalog.json.sig"_s, stores.shop.store.sign(json));
        engine.refresh();
        QTRY_COMPARE(fetched.count(), 2);
        QCOMPARE(fetched.at(1).at(0).toString(), stores.game.id);
        QVERIFY2(fetched.at(1).at(3).toString().contains(u"catalogue changed"_s),
                 qPrintable(fetched.at(1).at(3).toString()));
        QTRY_COMPARE(engine.state(), StoreEngine::State::Ready);

        // Asked for again, it is fetched afresh, and what was stopped says nothing more.
        server.release();
        QVERIFY(engine.fetchLicence(stores.game.id));
        QTRY_COMPARE(fetched.count(), 3);
        QVERIFY2(fetched.at(2).at(1).toBool(), qPrintable(fetched.at(2).at(3).toString()));
        QCOMPARE(server.asked.count(gameFile), 2);
        QTest::qWait(200);
        QCOMPARE(fetched.count(), 3);
    }

    void aFormatOneListingHasNoLicenceToFetch()
    {
        QTemporaryDir data;
        StoreEngine first(data.path(), publicKey(QStringLiteral(QOR_STORE_PUB)));
        QCOMPARE(refreshed(first, QUrl::fromLocalFile(QStringLiteral(QOR_STORE_CATALOG) + u"/catalog.json"_s)),
                 StoreEngine::State::Ready);
        QVERIFY(!first.fetchLicence(first.catalog().items.constFirst().id));
    }

    // ── The preference ───────────────────────────────────────────────────────

    void thePreferenceIsRememberedAndOffUntilSet()
    {
        QTemporaryDir config;
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, config.path());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
        QCoreApplication::setOrganizationName(u"QOR-MATRIX-test"_s);
        QCoreApplication::setApplicationName(u"tst_storefronts"_s);

        qor::sim::SimSettings fresh(true);
        fresh.load({});
        QVERIFY(!fresh.creatorFeatures());
        fresh.setCreatorFeatures(true);

        qor::sim::SimSettings later(true);
        later.load({});
        QVERIFY(later.creatorFeatures());

        qor::sim::SimSettings ephemeral(false);
        ephemeral.load({});
        QVERIFY(!ephemeral.creatorFeatures());
    }

    // Last: once connected, the shared services stay connected, and every storefront made after this follows them.
    void thePreferenceIsHandedToTheEngine()
    {
        // Kept to the end of the run, since the shared services stay connected to it.
        m_services = std::make_unique<qor::sim::SimServices>();
        qor::sim::SimOptions options;
        options.url = qor::services::endpoint(u"qor-test-"_s + QString::number(QCoreApplication::applicationPid()));
        options.persistent = false;
        options.silent = true;
        QVERIFY(m_services->start(options));

        SettingsReplica *settings = nullptr;
        {
            TwoStores stores;
            Fronts all(stores.shop.store.publicKey(), { stores.shop.publish() });
            Storefronts &fronts = *all.fronts;
            QTRY_VERIFY(fronts.isReady());
            QVERIFY(!all.engine.creatorFeatures());

            // The storefront was there before the services, as on a slow start.
            QVERIFY(qor::Services::instance()->connectTo(m_services->url(), 5000));
            settings = qor::Services::instance()->settings();
            QVERIFY(settings);
            QVERIFY(!settings->creatorFeatures());
            QVERIFY(!all.engine.creatorFeatures());

            settings->setCreatorFeatures(true);
            QTRY_VERIFY(all.engine.creatorFeatures());
            QCOMPARE(idsOf(fronts.listings(u"creator"_s)), QStringList { stores.pack.id });
            settings->setCreatorFeatures(false);
            QTRY_VERIFY(!all.engine.creatorFeatures());
            QVERIFY(fronts.listings(u"creator"_s).isEmpty());
            QVERIFY(!fronts.install(stores.pack.id));

            settings->setCreatorFeatures(true);
            QTRY_VERIFY(all.engine.creatorFeatures());
        }
        // A storefront that starts with it already on, as after a restart, is on from its first moment.
        TwoStores stores;
        Fronts again(stores.shop.store.publicKey(), { stores.shop.publish() });
        QVERIFY(again.engine.creatorFeatures());
    }
};

QTEST_GUILESS_MAIN(TestStorefronts)
#include "tst_storefronts.moc"
