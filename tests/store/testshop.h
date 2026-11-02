// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// What the Store's suites sign and drive: packages and format 2 catalogues under a test's control, signed with keys
// made for it and dropped with it (testsigner.h).
#pragma once

#include "catalog.h"
#include "storeengine.h"
#include "testsigner.h"

#include <QtCore/QByteArray>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTemporaryDir>
#include <QtCore/QUrl>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

namespace teststore {

using namespace qor::store;
using namespace Qt::StringLiterals;

inline const QString kDeveloper = QStringLiteral("org.example");
inline const QByteArray kPng = QByteArray::fromHex("89504e470d0a1a0a") + "a picture";
inline const QByteArray kLicence = QByteArrayLiteral("Free to use.\n");

inline QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

inline void writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(data), data.size());
}

inline QByteArray publicKey(const QString &path) { return QByteArray::fromBase64(readFile(path).trimmed()); }

inline QString hexDigest(const QByteArray &data)
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

inline void put16(QByteArray &out, quint16 value)
{
    out.append(char(value & 0xff));
    out.append(char(value >> 8));
}

inline void put32(QByteArray &out, quint32 value)
{
    put16(out, quint16(value & 0xffff));
    put16(out, quint16(value >> 16));
}

inline QByteArray forgeArchive(const QList<RawEntry> &entries, const QByteArray &leader = {},
                               const QByteArray &trailer = {}, int claimedCount = -1)
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

inline QJsonObject rangeOf(const QString &id, const QString &min, const QString &below)
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

inline Pack gamePack()
{
    Pack game;
    game.id = u"org.example.game.one"_s;
    game.kind = u"game"_s;
    game.stores = { u"player"_s };
    return game;
}

inline QJsonObject placementOf(const QString &store, const QString &category)
{
    return { { u"store"_s, store }, { u"category"_s, category } };
}

// All a listing needs to say what it is. A draft needs nothing more, and has nothing more believed.
inline QJsonObject classifiedListing(const QString &id, const QString &kind, const QJsonArray &placements)
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

// ── An engine, driven ───────────────────────────────────────────────────────

inline StoreEngine::State refreshed(StoreEngine &store, const QUrl &url)
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

inline Outcome installAndWait(StoreEngine &store, const QString &id)
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

} // namespace teststore
