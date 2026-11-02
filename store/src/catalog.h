// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QDate>
#include <QtCore/QHash>
#include <QtCore/QJsonValue>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>

namespace qor::store {

// The two storefronts (QOR-0021). One catalogue feeds both, and a listing's `placements` say where it appears. Nothing
// defaults to either: every query names its view, and what it does not find there is not looked for in the other.
enum class View { Player, Creator };

QString viewName(View view);

// Every kind format 2 knows, taken or not: game, demo, expansion, soundtrack, mod, utility; asset-pack, template,
// project, plugin, tool, creator-app.
bool isKnownKind(const QString &kind);

// What this stage installs: game and asset-pack. Every other known kind parses and stays a draft until it has a stage
// of its own, with rules for what its packages may hold.
bool isInstallableKind(const QString &kind);

// The name of the system this is running on, as a listing's `platforms` names it: linux-x86_64, windows-x86_64.
QString currentPlatform();

// Whether a kind may be placed in a store at all. A utility or a creator-app may be in either or both. Every other
// player kind is for the player store alone, and every other creator kind for the creator store alone.
bool kindMayBePlacedIn(const QString &kind, View store);

// A publisher, as the store vouches for it. `key` is the developer's ECDSA
// P-256 public key (65-byte uncompressed point): every package from this
// developer carries a manifest signed with it.
struct Developer
{
    QString id;
    QString name;
    bool verified = false;
    QByteArray key;
};

// A range of versions something works with, min <= version < below. `id` is an engine's or another item's.
struct VersionRange
{
    QString id;
    QString min;
    QString below;

    bool operator==(const VersionRange &) const = default;
};

struct Licence
{
    QString spdx; // an SPDX expression
    QString file; // the licence's text, a .txt file in the package

    bool operator==(const Licence &) const = default;
};

// Where an item's content came from and why the store may carry it (QOR-0021, decision 6).
struct Provenance
{
    QString origin;
    QString author;
    QString source;
    QString permission;
};

// A picture shown before download: a file beside the catalogue, with its digest.
struct Preview
{
    QString file;
    QByteArray sha256; // raw digest
};

// Where a listing appears, and under what.
struct Placement
{
    View store = View::Player;
    QString category;
};

// A finished game and its editable project share a group and are separate packages with separate entitlements. Holding
// one gives nothing of the other, and neither may depend on the other, since installing it would bring the other along.
struct Edition
{
    QString group;
    QString role; // "game" | "editable-project"

    bool operator==(const Edition &) const = default;
};

// One listing in the catalogue. `id` is the package's identity: one id, one install, whichever store it was found in.
struct CatalogItem
{
    int format = 1;
    QString id;   // reverse-DNS, e.g. dev.qorsync.theme.ember
    QString kind; // "theme" | "widget", or in format 2 a kind isKnownKind names
    QString name;
    QString developer; // a Developer id
    QString version;   // major.minor.patch
    QString summary;
    QString description;
    QString whatsNew;
    QString released; // ISO date
    QStringList tags;
    bool featured = false;
    QVariantMap preview; // shown before download (a theme's colours, a widget's size and glyph)
    QString packageFile; // relative to the catalogue URL
    qint64 packageSize = 0;
    QByteArray packageSha256; // raw digest

    // Format 2 only.
    QList<Placement> placements;
    QString entitlement; // "free", the only value this stage holds, which replaces any notion of price
    QStringList platforms;
    Edition edition; // empty when it has none
    bool executable = false;
    Licence licence;
    Provenance provenance;
    QList<VersionRange> engines;
    QList<VersionRange> dependencies; // an expansion's or a mod's base game among them
    QList<Preview> previews;
    qint64 packageUnpacked = 0; // every byte the package unpacks to, its manifest and signature included

    bool isPlacedIn(View view) const;
    QStringList categoriesIn(View view) const;
    QStringList storeNames() const; // the stores it is placed in, sorted, each once: what its manifest mirrors
    QVariantMap toVariant() const;
};

// A listing that is unclassified or contradictory, or of a kind this stage does not take. It is in no view and nothing
// installs it. `code` is for a tool to read, and is one of: unknown-kind, no-placements, bad-placement, unknown-store,
// empty-category, placement-contradicts-kind, duplicate-placement, entitlement-not-free, no-platforms,
// executable-not-said, executable-not-taken, edition-contradicts-kind, edition-dependency, kind-not-taken-yet.
struct Draft
{
    QString id;
    QString kind;
    QString code;
    QString reason;
};

struct Catalog
{
    int format = 1;
    QString name;
    QHash<QString, Developer> developers;
    QList<CatalogItem> items;

    // Format 2 only.
    qint64 sequence = 0;
    QDate expires; // the last day it is good for, in UTC
    QList<Draft> drafts;

    const CatalogItem *item(const QString &id) const;
    const Draft *draft(const QString &id) const;

    // Each asks of one view and answers from that view alone: an empty answer stays empty. These know nothing of
    // creator features: a page asks StoreEngine's, which do.
    QList<CatalogItem> listings(View view) const;
    QList<CatalogItem> featured(View view) const;
    QList<CatalogItem> search(View view, const QString &query) const;
    QMap<QString, int> categoryCounts(View view) const;

    // What format 1 lists, the themes and widgets that are for everyone. Format 2's listings are not among them, so a
    // page that knows no view cannot show one.
    QList<CatalogItem> formatOneItems() const;
};

struct CatalogResult
{
    bool ok = false;
    QString error;
    Catalog catalog;
};

// Checks the store's signature over the catalogue's exact bytes, then
// parses and validates it. Nothing in an unsigned or altered catalogue is
// trusted, not even its developer keys.
//
// A failure of the catalogue refuses all of it: a bad signature, an unknown format, an expiry passed or not given, or
// a sequence that is not a positive integer. So does anything malformed or hostile in a listing that says clearly what
// it is, since a store that signed that is not to be half believed: a price anywhere, even zero; a field the format
// does not know; an unsafe package file; a dependency cycle.
//
// A listing that is unclassified or contradictory is a Draft instead, with a reason a tool can read, and the rest of
// the catalogue loads. Nothing else of a draft is checked, believed or kept.
//
// `today` is the date expiry is measured against, by default the date in UTC.
CatalogResult parseCatalog(const QByteArray &json, const QByteArray &signature, const QByteArray &storeKey,
                           QDate today = {});

// Compares dotted versions numerically: <0, 0 or >0.
int compareVersions(const QString &a, const QString &b);

bool isValidItemId(const QString &id);
bool isValidVersion(const QString &version);

// Whether min <= version < below, each a valid version.
bool isVersionInRange(const QString &version, const QString &min, const QString &below);

// Read from a catalogue's entry and from a signed manifest alike. Each returns why it could not, or an empty string.
QString parseLicence(const QJsonValue &value, Licence *licence);
QString parseDependencies(const QJsonValue &value, QList<VersionRange> *dependencies);
QString parseEngines(const QJsonValue &value, QList<VersionRange> *engines);
QString parsePlatforms(const QJsonValue &value, QStringList *platforms);
QString parseEdition(const QJsonValue &value, const QString &kind, Edition *edition);

// What is already installed, as dependencies see it.
struct InstalledVersion
{
    QString version;
    QList<VersionRange> dependencies;
};

// What installing an item takes: `order` holds the items to fetch, dependencies first and the item itself last.
struct InstallPlan
{
    bool ok = false;
    QString error;
    QStringList order;
};

// Works out what installing `id` takes, refusing rather than guessing: a cycle, a chain too deep, a dependency this
// catalogue does not list, a version outside the range asked for, a downgrade of a dependency, an update that would
// leave something installed outside the range it needs, and one edition of a group reaching another, however
// indirectly.
InstallPlan resolveDependencies(const Catalog &catalog, const QString &id,
                                const QMap<QString, InstalledVersion> &installed);

// Whether `incoming` landing on `installed` leaves things as they should be. Everything incoming must have what it
// needs. Something installed must not lose a range that holds now. What is already broken and is not touched is no
// reason to refuse: a block is about what this would affect, so one broken item does not stop every other install.
// Returns why not, or an empty string.
QString refusalOfOutcome(const QMap<QString, InstalledVersion> &installed,
                         const QMap<QString, InstalledVersion> &incoming);

// Bytes from a catalogue or an archive, cut down to something safe to put in a message or a log.
QString printable(QStringView text);

} // namespace qor::store
