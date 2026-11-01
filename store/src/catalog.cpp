// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "catalog.h"

#include "filerules.h"
#include "package.h"
#include "signature.h"

#include <QtCore/QDateTime>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>

#include <algorithm>
#include <functional>

using namespace Qt::StringLiterals;

namespace qor::store {

namespace {

constexpr qsizetype kMaximumItems = 4096;
constexpr qsizetype kMaximumText = 8192;
constexpr qsizetype kMaximumTags = 32;
constexpr qsizetype kMaximumRanges = 32;
constexpr qsizetype kMaximumPreviews = 8;
constexpr qsizetype kMaximumPlacements = 16;
constexpr qsizetype kMaximumPlatforms = 16;
constexpr qint64 kMaximumSequence = qint64(1) << 53; // what a JSON number holds exactly
constexpr qsizetype kMaximumDependencyDepth = 32;

const auto kInvalidEntry = QStringLiteral("Catalogue entry %1 is not valid.");

// The first key `object` has that `known` does not.
QString unknownKey(const QJsonObject &object, const QSet<QString> &known)
{
    for (auto it = object.begin(); it != object.end(); ++it)
        if (!known.contains(it.key()))
            return it.key();
    return {};
}

// Whether a price is anywhere in a listing, under any field. Parsers cap how deep JSON nests.
bool holdsPrice(const QJsonValue &value)
{
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it)
            if (it.key() == u"price"_s || holdsPrice(it.value()))
                return true;
    } else if (value.isArray()) {
        for (const QJsonValue &element : value.toArray())
            if (holdsPrice(element))
                return true;
    }
    return false;
}

bool isSha256(const QJsonValue &value)
{
    static const QRegularExpression pattern(u"^[0-9a-f]{64}$"_s);
    return value.isString() && pattern.match(value.toString()).hasMatch();
}

bool isText(const QJsonValue &value) { return value.isString() && value.toString().size() <= kMaximumText; }
bool isFilledText(const QJsonValue &value) { return isText(value) && !value.toString().trimmed().isEmpty(); }

bool isIsoDate(const QString &text)
{
    static const QRegularExpression pattern(u"^\\d{4}-\\d{2}-\\d{2}$"_s);
    return pattern.match(text).hasMatch() && QDate::fromString(text, Qt::ISODate).isValid();
}

CatalogResult refusal(const QString &error)
{
    CatalogResult result;
    result.error = error;
    return result;
}

QString parseDevelopers(const QJsonObject &root, Catalog *catalog)
{
    const QJsonObject developers = root.value(u"developers"_s).toObject();
    for (auto it = developers.begin(); it != developers.end(); ++it) {
        const QJsonObject entry = it.value().toObject();
        Developer developer { it.key(), entry.value(u"name"_s).toString(), entry.value(u"verified"_s).toBool(),
                              QByteArray::fromBase64(entry.value(u"key"_s).toString().toLatin1()) };
        if (developer.key.size() != 65 || developer.name.isEmpty())
            return u"Developer %1 has no valid key."_s.arg(printable(developer.id));
        catalog->developers.insert(developer.id, developer);
    }
    return {};
}

// A list of {id, min, below}, each id once, each range holding at least one version.
QString parseRanges(const QJsonValue &value, const std::function<bool(const QString &)> &isValidId,
                    QList<VersionRange> *ranges)
{
    if (!value.isArray())
        return u"it is not a list"_s;
    const QJsonArray array = value.toArray();
    if (array.size() > kMaximumRanges)
        return u"it is too long"_s;
    static const QSet<QString> known { u"id"_s, u"min"_s, u"below"_s };
    QSet<QString> seen;
    for (const QJsonValue &element : array) {
        const QJsonObject entry = element.toObject();
        const VersionRange range { entry.value(u"id"_s).toString(), entry.value(u"min"_s).toString(),
                                   entry.value(u"below"_s).toString() };
        if (!element.isObject() || !unknownKey(entry, known).isEmpty() || !isValidId(range.id)
            || !isValidVersion(range.min) || !isValidVersion(range.below)
            || compareVersions(range.min, range.below) >= 0)
            return u"%1's range is not valid"_s.arg(printable(range.id));
        if (seen.contains(range.id))
            return u"it names %1 twice"_s.arg(printable(range.id));
        seen.insert(range.id);
        ranges->append(range);
    }
    return {};
}

bool isValidEngineId(const QString &id)
{
    static const QRegularExpression pattern(u"^[a-z][a-z0-9-]{0,31}$"_s);
    return pattern.match(id).hasMatch();
}

// Whether a listing says what it is and does not contradict itself. Fills in what classifies it, and returns the draft
// it stays as, or one with no code. What it says is read and nothing beyond it is, so a draft needs no other field.
Draft classify(const QJsonObject &entry, CatalogItem *item)
{
    const auto draft = [item](const QString &code, const QString &reason) {
        return Draft { item->id, printable(item->kind), code, u"%1: %2"_s.arg(item->id, reason) };
    };

    item->kind = entry.value(u"kind"_s).toString();
    if (!isKnownKind(item->kind))
        return draft(u"unknown-kind"_s, u"it names no kind a store knows."_s);

    const QJsonValue placements = entry.value(u"placements"_s);
    if (!placements.isArray() || placements.toArray().isEmpty())
        return draft(u"no-placements"_s, u"it is placed in no store."_s);
    if (placements.toArray().size() > kMaximumPlacements)
        return draft(u"bad-placement"_s, u"it has too many placements."_s);
    static const QSet<QString> placementKeys { u"store"_s, u"category"_s };
    QSet<QString> seen;
    for (const QJsonValue &value : placements.toArray()) {
        const QJsonObject placement = value.toObject();
        if (!value.isObject() || !unknownKey(placement, placementKeys).isEmpty())
            return draft(u"bad-placement"_s, u"a placement is not a store and a category."_s);
        const QString store = placement.value(u"store"_s).toString();
        if (store != u"player"_s && store != u"creator"_s)
            return draft(u"unknown-store"_s, u"it is placed in a store that does not exist."_s);
        if (!isFilledText(placement.value(u"category"_s)))
            return draft(u"empty-category"_s, u"a placement has no category."_s);
        const View view = store == u"player"_s ? View::Player : View::Creator;
        if (!kindMayBePlacedIn(item->kind, view))
            return draft(u"placement-contradicts-kind"_s,
                         u"a %1 does not belong in the %2 store."_s.arg(item->kind, store));
        const QString category = placement.value(u"category"_s).toString();
        if (seen.contains(store + u'/' + category))
            return draft(u"duplicate-placement"_s, u"it is placed in one category twice."_s);
        seen.insert(store + u'/' + category);
        item->placements.append(Placement { view, category });
    }

    const QJsonValue entitlement = entry.value(u"entitlement"_s);
    if (!entitlement.isObject() || entitlement.toObject().size() != 1
        || entitlement.toObject().value(u"type"_s).toString() != u"free"_s)
        return draft(u"entitlement-not-free"_s, u"its entitlement is not free, the only kind this stage has."_s);
    item->entitlement = u"free"_s;

    static const QRegularExpression platformName(u"^[a-z0-9][a-z0-9_-]{0,31}$"_s);
    const QJsonValue platforms = entry.value(u"platforms"_s);
    if (!platforms.isArray() || platforms.toArray().isEmpty() || platforms.toArray().size() > kMaximumPlatforms)
        return draft(u"no-platforms"_s, u"it names no platform it runs on."_s);
    for (const QJsonValue &value : platforms.toArray()) {
        if (!platformName.match(value.toString()).hasMatch())
            return draft(u"no-platforms"_s, u"a platform it names is not the name of one."_s);
        item->platforms.append(value.toString());
    }

    // Nothing that starts by itself reaches this stage (docs/STORE.md says what the flag means for a game).
    const QJsonValue executable = entry.value(u"executable"_s);
    if (!executable.isBool())
        return draft(u"executable-not-said"_s, u"it does not say whether it is executable."_s);
    item->executable = executable.toBool();
    if (item->executable)
        return draft(u"executable-not-taken"_s, u"it is executable, and this stage takes nothing executable."_s);

    if (entry.contains(u"edition"_s)) {
        if (const QString why = parseEdition(entry.value(u"edition"_s), item->kind, &item->edition); !why.isEmpty())
            return draft(u"edition-contradicts-kind"_s, why + u'.');
    }

    if (!isInstallableKind(item->kind))
        return draft(u"kind-not-taken-yet"_s, u"this stage does not take a %1."_s.arg(item->kind));
    return {};
}

// Everything else in a listing that is classified, and is to be installable. Returns why it is not valid.
QString parseListing(const QJsonObject &entry, const Catalog &catalog, CatalogItem *item)
{
    for (const QString &field : { u"name"_s, u"summary"_s, u"description"_s })
        if (!isFilledText(entry.value(field)))
            return u"it has no %1"_s.arg(field);
    item->name = entry.value(u"name"_s).toString();
    item->summary = entry.value(u"summary"_s).toString();
    item->description = entry.value(u"description"_s).toString();

    item->version = entry.value(u"version"_s).toString();
    if (!isValidVersion(item->version))
        return u"its version is not major.minor.patch"_s;
    item->developer = entry.value(u"developer"_s).toString();
    if (!catalog.developers.contains(item->developer))
        return u"its developer is not listed"_s;
    item->released = entry.value(u"released"_s).toString();
    if (!isIsoDate(item->released))
        return u"its released date is not a date"_s;

    if (entry.contains(u"whatsNew"_s) && !isText(entry.value(u"whatsNew"_s)))
        return u"its whatsNew is not text"_s;
    item->whatsNew = entry.value(u"whatsNew"_s).toString();
    if (entry.contains(u"featured"_s) && !entry.value(u"featured"_s).isBool())
        return u"its featured is not true or false"_s;
    item->featured = entry.value(u"featured"_s).toBool();
    if (entry.contains(u"tags"_s)) {
        if (!entry.value(u"tags"_s).isArray() || entry.value(u"tags"_s).toArray().size() > kMaximumTags)
            return u"its tags are not a short list"_s;
        for (const QJsonValue &tag : entry.value(u"tags"_s).toArray()) {
            if (!isFilledText(tag))
                return u"a tag is not text"_s;
            item->tags.append(tag.toString());
        }
    }

    QString why = parseLicence(entry.value(u"licence"_s), &item->licence);
    if (!why.isEmpty())
        return why;

    static const QSet<QString> provenanceKeys { u"origin"_s, u"author"_s, u"source"_s, u"permission"_s };
    const QJsonObject provenance = entry.value(u"provenance"_s).toObject();
    if (!unknownKey(provenance, provenanceKeys).isEmpty())
        return u"its provenance has a field this format does not know"_s;
    for (const QString &field : provenanceKeys)
        if (!isFilledText(provenance.value(field)))
            return u"its provenance has no %1"_s.arg(field);
    item->provenance = { provenance.value(u"origin"_s).toString(), provenance.value(u"author"_s).toString(),
                         provenance.value(u"source"_s).toString(), provenance.value(u"permission"_s).toString() };

    why = parseRanges(entry.value(u"engines"_s), isValidEngineId, &item->engines);
    if (!why.isEmpty())
        return u"its engines: %1"_s.arg(why);
    if (item->engines.isEmpty())
        return u"it names no engine it works with"_s;
    why = parseDependencies(entry.value(u"dependencies"_s), &item->dependencies);
    if (!why.isEmpty())
        return why;

    static const QSet<QString> previewKeys { u"file"_s, u"sha256"_s };
    const QJsonValue previews = entry.value(u"previews"_s);
    if (!previews.isArray() || previews.toArray().size() > kMaximumPreviews)
        return u"its previews are not a short list"_s;
    for (const QJsonValue &value : previews.toArray()) {
        const QJsonObject preview = value.toObject();
        const QString file = preview.value(u"file"_s).toString();
        if (!value.isObject() || !unknownKey(preview, previewKeys).isEmpty() || !isSafePackagePath(file)
            || !isPreviewName(file) || !isSha256(preview.value(u"sha256"_s)))
            return u"a preview is not valid"_s;
        item->previews.append(Preview { file, QByteArray::fromHex(preview.value(u"sha256"_s).toString().toLatin1()) });
    }

    static const QSet<QString> packageKeys { u"file"_s, u"size"_s, u"sha256"_s, u"unpacked"_s };
    const QJsonObject package = entry.value(u"package"_s).toObject();
    if (!unknownKey(package, packageKeys).isEmpty())
        return u"its package has a field this format does not know"_s;
    item->packageFile = package.value(u"file"_s).toString();
    if (!isSafePackagePath(item->packageFile))
        return u"its package file is not a plain relative path"_s;
    if (!isSha256(package.value(u"sha256"_s)))
        return u"its package has no SHA-256"_s;
    item->packageSha256 = QByteArray::fromHex(package.value(u"sha256"_s).toString().toLatin1());
    item->packageSize = package.value(u"size"_s).toInteger(0);
    if (item->packageSize < 1 || item->packageSize > kMaximumPackageArchive)
        return u"its package's size is out of bounds"_s;
    item->packageUnpacked = package.value(u"unpacked"_s).toInteger(0);
    if (item->packageUnpacked < 1 || item->packageUnpacked > kMaximumPackageContent)
        return u"its package's unpacked size is out of bounds"_s;
    return {};
}

// Walks dependencies depth first, finding cycles and writing `order` with what each item needs before it.
struct DependencyWalk
{
    QHash<QString, const CatalogItem *> items;
    const QMap<QString, InstalledVersion> *installed = nullptr; // when null, every edge in the catalogue is walked
    QHash<QString, bool> finished;                              // false while the item is on `path`
    QStringList path;
    QStringList order;
    QString error;

    bool visit(const CatalogItem &item)
    {
        if (const auto mark = finished.constFind(item.id); mark != finished.cend()) {
            if (*mark)
                return true;
            error = u"These depend on each other: %1."_s.arg((path + QStringList { item.id }).join(u" -> "_s));
            return false;
        }
        if (path.size() >= kMaximumDependencyDepth) {
            error = u"%1's dependencies go too deep."_s.arg(path.constFirst());
            return false;
        }
        finished.insert(item.id, false);
        path.append(item.id);
        for (const VersionRange &range : item.dependencies)
            if (!follow(item, range))
                return false;
        path.removeLast();
        finished.insert(item.id, true);
        order.append(item.id);
        return true;
    }

    bool follow(const CatalogItem &item, const VersionRange &range)
    {
        const CatalogItem *target = items.value(range.id);
        if (target && !item.edition.group.isEmpty() && target->edition.group == item.edition.group) {
            error = u"%1 and %2 are editions of one thing, and neither brings the other."_s.arg(item.id, range.id);
            return false;
        }
        if (!installed)
            return !target || visit(*target);

        const auto present = installed->constFind(range.id);
        const bool isPresent = present != installed->cend();
        if (isPresent && isVersionInRange(present->version, range.min, range.below))
            return true;
        if (!target) {
            error = u"%1 needs %2, which this store does not list."_s.arg(item.id, range.id);
            return false;
        }
        if (!isVersionInRange(target->version, range.min, range.below)) {
            error = u"%1 needs %2 from %3 and below %4, and this store lists %5."_s.arg(item.id, range.id, range.min,
                                                                                        range.below, target->version);
            return false;
        }
        if (isPresent && compareVersions(target->version, present->version) < 0) {
            error = u"%1 needs %2 older than the one installed, and nothing is moved back."_s.arg(item.id, range.id);
            return false;
        }
        return visit(*target);
    }
};

CatalogResult parseFormat2(const QJsonObject &root, const QDate &today)
{
    static const QSet<QString> rootKeys { u"format"_s,  u"store"_s,      u"sequence"_s,
                                          u"expires"_s, u"developers"_s, u"items"_s };
    if (const QString unknown = unknownKey(root, rootKeys); !unknown.isEmpty())
        return refusal(u"The catalogue has a field format 2 does not know: %1."_s.arg(printable(unknown)));

    CatalogResult result;
    Catalog &catalog = result.catalog;
    catalog.format = 2;
    if (!isFilledText(root.value(u"store"_s)))
        return refusal(u"The catalogue does not name its store."_s);
    catalog.name = root.value(u"store"_s).toString();

    catalog.sequence = root.value(u"sequence"_s).toInteger(0);
    if (catalog.sequence < 1 || catalog.sequence > kMaximumSequence)
        return refusal(u"The catalogue's sequence is not a positive integer."_s);

    const QString expires = root.value(u"expires"_s).toString();
    if (!isIsoDate(expires))
        return refusal(u"The catalogue's expiry is not a date."_s);
    catalog.expires = QDate::fromString(expires, Qt::ISODate);
    if (today > catalog.expires)
        return refusal(u"The catalogue expired on %1."_s.arg(expires));

    if (const QString why = parseDevelopers(root, &catalog); !why.isEmpty())
        return refusal(why);

    const QJsonArray items = root.value(u"items"_s).toArray();
    if (items.size() > kMaximumItems)
        return refusal(u"The catalogue lists too many items."_s);

    static const QSet<QString> itemKeys { u"id"_s,        u"kind"_s,       u"placements"_s,  u"entitlement"_s,
                                          u"platforms"_s, u"edition"_s,    u"name"_s,        u"developer"_s,
                                          u"version"_s,   u"summary"_s,    u"description"_s, u"whatsNew"_s,
                                          u"released"_s,  u"tags"_s,       u"featured"_s,    u"executable"_s,
                                          u"licence"_s,   u"provenance"_s, u"engines"_s,     u"dependencies"_s,
                                          u"previews"_s,  u"package"_s };
    QSet<QString> seen;
    QHash<QString, QString> editionGroups; // every listing that says its group, drafts among them
    for (const QJsonValue &value : items) {
        const QJsonObject entry = value.toObject();
        const QString id = entry.value(u"id"_s).toString();
        // One that cannot be named cannot be a draft: no tool could say which listing it was.
        if (!value.isObject() || !isValidItemId(id) || seen.contains(id))
            return refusal(kInvalidEntry.arg(id.isEmpty() ? u"?"_s : printable(id)));
        seen.insert(id);
        // Under any field, on any kind, free or not (QOR-0006): whatever a later format does about money, this one
        // holds none.
        if (holdsPrice(entry))
            return refusal(u"Catalogue entry %1 has a price, and format 2 refuses any price, even zero."_s.arg(id));
        if (const QString unknown = unknownKey(entry, itemKeys); !unknown.isEmpty())
            return refusal(u"Catalogue entry %1 has a field format 2 does not know: %2."_s.arg(id, printable(unknown)));

        CatalogItem item;
        item.format = 2;
        item.id = id;
        const Draft draft = classify(entry, &item);
        if (!item.edition.group.isEmpty())
            editionGroups.insert(id, item.edition.group);
        if (!draft.code.isEmpty()) {
            catalog.drafts.append(draft);
            continue;
        }
        if (const QString why = parseListing(entry, catalog, &item); !why.isEmpty())
            return refusal(u"Catalogue entry %1 is not valid: %2."_s.arg(id, why));
        catalog.items.append(item);
    }

    // One edition depending on another of its group would bring it along, and an edition is held on its own.
    for (qsizetype i = catalog.items.size() - 1; i >= 0; --i) {
        const CatalogItem &item = catalog.items.at(i);
        const bool crosses =
            !item.edition.group.isEmpty()
            && std::any_of(item.dependencies.cbegin(), item.dependencies.cend(), [&](const VersionRange &range) {
                   return editionGroups.value(range.id) == item.edition.group;
               });
        if (crosses) {
            catalog.drafts.append(Draft { item.id, item.kind, u"edition-dependency"_s,
                                          u"%1: it depends on another edition of itself."_s.arg(item.id) });
            catalog.items.removeAt(i);
        }
    }

    DependencyWalk walk;
    for (const CatalogItem &item : std::as_const(catalog.items))
        walk.items.insert(item.id, &item);
    for (const CatalogItem &item : std::as_const(catalog.items))
        if (!walk.visit(item))
            return refusal(walk.error);

    result.ok = true;
    return result;
}

} // namespace

QString viewName(View view) { return view == View::Player ? u"player"_s : u"creator"_s; }

namespace {

const QSet<QString> &playerKinds()
{
    static const QSet<QString> kinds { u"game"_s, u"demo"_s, u"expansion"_s, u"soundtrack"_s, u"mod"_s, u"utility"_s };
    return kinds;
}

const QSet<QString> &creatorKinds()
{
    static const QSet<QString> kinds { u"asset-pack"_s, u"template"_s, u"project"_s,
                                       u"plugin"_s,     u"tool"_s,     u"creator-app"_s };
    return kinds;
}

} // namespace

bool isKnownKind(const QString &kind) { return playerKinds().contains(kind) || creatorKinds().contains(kind); }

bool isInstallableKind(const QString &kind) { return kind == u"game"_s || kind == u"asset-pack"_s; }

bool kindMayBePlacedIn(const QString &kind, View store)
{
    if (kind == u"utility"_s || kind == u"creator-app"_s)
        return true;
    return store == View::Player ? playerKinds().contains(kind) : creatorKinds().contains(kind);
}

bool CatalogItem::isPlacedIn(View view) const
{
    return std::any_of(placements.cbegin(), placements.cend(),
                       [view](const Placement &placement) { return placement.store == view; });
}

QStringList CatalogItem::categoriesIn(View view) const
{
    QStringList categories;
    for (const Placement &placement : placements)
        if (placement.store == view)
            categories.append(placement.category);
    return categories;
}

QStringList CatalogItem::storeNames() const
{
    QStringList names;
    if (isPlacedIn(View::Creator))
        names.append(viewName(View::Creator));
    if (isPlacedIn(View::Player))
        names.append(viewName(View::Player));
    return names;
}

QVariantMap CatalogItem::toVariant() const
{
    QVariantMap map {
        { u"id"_s, id },
        { u"kind"_s, kind },
        { u"name"_s, name },
        { u"developer"_s, developer },
        { u"version"_s, version },
        { u"summary"_s, summary },
        { u"description"_s, description },
        { u"whatsNew"_s, whatsNew },
        { u"released"_s, released },
        { u"tags"_s, tags },
        { u"featured"_s, featured },
        // Nothing with any other price gets past parseCatalog (QOR-0006).
        { u"price"_s, 0 },
        { u"preview"_s, preview },
        { u"size"_s, packageSize },
    };
    if (format != 2)
        return map;

    const auto ranges = [](const QList<VersionRange> &list) {
        QVariantList values;
        for (const VersionRange &range : list)
            values.append(QVariantMap { { u"id"_s, range.id }, { u"min"_s, range.min }, { u"below"_s, range.below } });
        return values;
    };
    QVariantList files;
    for (const Preview &picture : previews)
        files.append(picture.file);
    QVariantList where;
    for (const Placement &placement : placements)
        where.append(QVariantMap { { u"store"_s, viewName(placement.store) }, { u"category"_s, placement.category } });
    map.insert(u"placements"_s, where);
    map.insert(u"entitlement"_s, QVariantMap { { u"type"_s, entitlement } });
    map.insert(u"platforms"_s, platforms);
    if (!edition.group.isEmpty())
        map.insert(u"edition"_s, QVariantMap { { u"group"_s, edition.group }, { u"role"_s, edition.role } });
    map.insert(u"executable"_s, executable);
    map.insert(u"licence"_s, QVariantMap { { u"spdx"_s, licence.spdx }, { u"file"_s, licence.file } });
    map.insert(u"provenance"_s, QVariantMap { { u"origin"_s, provenance.origin },
                                              { u"author"_s, provenance.author },
                                              { u"source"_s, provenance.source },
                                              { u"permission"_s, provenance.permission } });
    map.insert(u"engines"_s, ranges(engines));
    map.insert(u"dependencies"_s, ranges(dependencies));
    map.insert(u"previews"_s, files);
    map.insert(u"unpacked"_s, packageUnpacked);
    return map;
}

const CatalogItem *Catalog::item(const QString &id) const
{
    for (const CatalogItem &candidate : items)
        if (candidate.id == id)
            return &candidate;
    return nullptr;
}

const Draft *Catalog::draft(const QString &id) const
{
    for (const Draft &candidate : drafts)
        if (candidate.id == id)
            return &candidate;
    return nullptr;
}

QList<CatalogItem> Catalog::listings(View view) const
{
    QList<CatalogItem> found;
    for (const CatalogItem &candidate : items)
        if (candidate.isPlacedIn(view))
            found.append(candidate);
    return found;
}

QList<CatalogItem> Catalog::featured(View view) const
{
    QList<CatalogItem> found;
    for (const CatalogItem &candidate : items)
        if (candidate.featured && candidate.isPlacedIn(view))
            found.append(candidate);
    return found;
}

QList<CatalogItem> Catalog::search(View view, const QString &query) const
{
    const QStringList words = query.simplified().toLower().split(u' ', Qt::SkipEmptyParts);
    QList<CatalogItem> found;
    for (const CatalogItem &candidate : items) {
        if (!candidate.isPlacedIn(view))
            continue;
        // Only its categories in this view: what it is called in the other store says nothing here.
        const QString haystack = (candidate.name + u' ' + candidate.summary + u' ' + candidate.kind + u' '
                                  + candidate.tags.join(u' ') + u' ' + candidate.categoriesIn(view).join(u' '))
                                     .toLower();
        if (std::all_of(words.cbegin(), words.cend(),
                        [&haystack](const QString &word) { return haystack.contains(word); }))
            found.append(candidate);
    }
    return found;
}

QMap<QString, int> Catalog::categoryCounts(View view) const
{
    QMap<QString, int> counts;
    for (const CatalogItem &candidate : items)
        for (const QString &category : candidate.categoriesIn(view))
            ++counts[category];
    return counts;
}

QList<CatalogItem> Catalog::formatOneItems() const
{
    QList<CatalogItem> found;
    for (const CatalogItem &candidate : items)
        if (candidate.format == 1)
            found.append(candidate);
    return found;
}

bool isValidItemId(const QString &id)
{
    static const QRegularExpression pattern(u"^[a-z][a-z0-9]*(\\.[a-z0-9][a-z0-9-]*){2,}$"_s);
    // Named like what an update leaves beside an item for a moment, it could be taken for one and cleared away.
    static const QRegularExpression leftover(u"\\.old-[0-9a-f]{32}$"_s);
    return id.size() <= 128 && pattern.match(id).hasMatch() && !leftover.match(id).hasMatch();
}

bool isValidVersion(const QString &version)
{
    static const QRegularExpression pattern(u"^\\d{1,5}\\.\\d{1,5}\\.\\d{1,5}$"_s);
    return pattern.match(version).hasMatch();
}

int compareVersions(const QString &a, const QString &b)
{
    const QStringList left = a.split(u'.');
    const QStringList right = b.split(u'.');
    for (qsizetype i = 0; i < std::max(left.size(), right.size()); ++i) {
        const int x = i < left.size() ? left.at(i).toInt() : 0;
        const int y = i < right.size() ? right.at(i).toInt() : 0;
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

bool isVersionInRange(const QString &version, const QString &min, const QString &below)
{
    return isValidVersion(version) && isValidVersion(min) && isValidVersion(below) && compareVersions(version, min) >= 0
           && compareVersions(version, below) < 0;
}

QString parseLicence(const QJsonValue &value, Licence *licence)
{
    static const QSet<QString> keys { u"spdx"_s, u"file"_s };
    static const QRegularExpression spdx(u"^[A-Za-z0-9.+()\\- ]{1,128}$"_s);
    const QJsonObject object = value.toObject();
    licence->spdx = object.value(u"spdx"_s).toString();
    licence->file = object.value(u"file"_s).toString();
    if (!value.isObject() || !unknownKey(object, keys).isEmpty())
        return u"its licence is not valid"_s;
    if (!spdx.match(licence->spdx).hasMatch() || licence->spdx == u"NOASSERTION"_s || licence->spdx == u"NONE"_s)
        return u"its licence names no SPDX expression"_s;
    if (!isSafePackagePath(licence->file) || !licence->file.endsWith(u".txt"_s))
        return u"its licence's file is not a .txt file in the package"_s;
    return {};
}

QString parseDependencies(const QJsonValue &value, QList<VersionRange> *dependencies)
{
    const QString why = parseRanges(value, isValidItemId, dependencies);
    return why.isEmpty() ? QString() : u"its dependencies: %1"_s.arg(why);
}

QString parseEdition(const QJsonValue &value, const QString &kind, Edition *edition)
{
    static const QSet<QString> keys { u"group"_s, u"role"_s };
    static const QRegularExpression group(u"^[a-z0-9][a-z0-9.-]{0,127}$"_s);
    const QJsonObject object = value.toObject();
    edition->group = object.value(u"group"_s).toString();
    edition->role = object.value(u"role"_s).toString();
    if (!value.isObject() || !unknownKey(object, keys).isEmpty() || !group.match(edition->group).hasMatch()) {
        *edition = {};
        return u"its edition names no group"_s;
    }
    const bool fits = (edition->role == u"game"_s && kind == u"game"_s)
                      || (edition->role == u"editable-project"_s && kind == u"project"_s);
    if (!fits) {
        const QString why = u"its edition's role does not fit a %1"_s.arg(printable(kind));
        *edition = {};
        return why;
    }
    return {};
}

InstallPlan resolveDependencies(const Catalog &catalog, const QString &id,
                                const QMap<QString, InstalledVersion> &installed)
{
    InstallPlan plan;
    const CatalogItem *root = catalog.item(id);
    if (!root) {
        plan.error = u"%1 is not in this catalogue."_s.arg(printable(id));
        return plan;
    }
    DependencyWalk walk;
    walk.installed = &installed;
    for (const CatalogItem &item : catalog.items)
        walk.items.insert(item.id, &item);
    if (!walk.visit(*root)) {
        plan.error = walk.error;
        return plan;
    }

    // What would stand afterwards: everything anything needs must still hold, including what is installed already and
    // not being touched. One item's update must not take another's dependency out of its range.
    QMap<QString, QString> versions;
    for (auto it = installed.cbegin(); it != installed.cend(); ++it)
        versions.insert(it.key(), it->version);
    for (const QString &step : std::as_const(walk.order))
        versions.insert(step, walk.items.value(step)->version);
    const auto holds = [&versions](const VersionRange &range) {
        const auto version = versions.constFind(range.id);
        return version != versions.cend() && isVersionInRange(*version, range.min, range.below);
    };
    const auto broken = [&plan](const QString &owner, const VersionRange &range) {
        plan.error = u"%1 needs %2 from %3 and below %4, which this would not leave it."_s.arg(owner, range.id,
                                                                                               range.min, range.below);
    };
    for (const QString &step : std::as_const(walk.order))
        for (const VersionRange &range : walk.items.value(step)->dependencies)
            if (!holds(range)) {
                broken(step, range);
                return plan;
            }
    for (auto it = installed.cbegin(); it != installed.cend(); ++it) {
        if (walk.order.contains(it.key()))
            continue;
        for (const VersionRange &range : it->dependencies)
            if (!holds(range)) {
                broken(it.key(), range);
                return plan;
            }
    }

    plan.order = walk.order;
    plan.ok = true;
    return plan;
}

QString printable(QStringView text)
{
    QString result;
    for (const QChar c : text.left(80))
        result.append(c.unicode() >= 0x20 && c.unicode() < 0x7f ? c : QChar(u'?'));
    return result;
}

CatalogResult parseCatalog(const QByteArray &json, const QByteArray &signature, const QByteArray &storeKey, QDate today)
{
    CatalogResult result;
    if (!verifySignature(storeKey, json, signature)) {
        result.error = u"The catalogue's store signature does not match."_s;
        return result;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (!document.isObject()) {
        result.error = u"The catalogue is not valid JSON: %1"_s.arg(parseError.errorString());
        return result;
    }
    const QJsonObject root = document.object();
    const int format = root.value(u"format"_s).toInt();
    if (format == 2)
        return parseFormat2(root, today.isValid() ? today : QDateTime::currentDateTimeUtc().date());
    if (format != 1) {
        result.error = u"Unsupported catalogue format."_s;
        return result;
    }
    Catalog &catalog = result.catalog;
    catalog.name = root.value(u"store"_s).toString();

    if (const QString why = parseDevelopers(root, &catalog); !why.isEmpty()) {
        result.error = why;
        return result;
    }

    for (const QJsonValue &value : root.value(u"items"_s).toArray()) {
        const QJsonObject entry = value.toObject();
        CatalogItem item;
        item.id = entry.value(u"id"_s).toString();
        item.kind = entry.value(u"kind"_s).toString();
        item.name = entry.value(u"name"_s).toString();
        item.developer = entry.value(u"developer"_s).toString();
        item.version = entry.value(u"version"_s).toString();
        item.summary = entry.value(u"summary"_s).toString();
        item.description = entry.value(u"description"_s).toString();
        item.whatsNew = entry.value(u"whatsNew"_s).toString();
        item.released = entry.value(u"released"_s).toString();
        for (const QJsonValue &tag : entry.value(u"tags"_s).toArray())
            item.tags.append(tag.toString());
        item.featured = entry.value(u"featured"_s).toBool();
        item.preview = entry.value(u"preview"_s).toObject().toVariantMap();
        const QJsonObject package = entry.value(u"package"_s).toObject();
        item.packageFile = package.value(u"file"_s).toString();
        item.packageSize = package.value(u"size"_s).toInteger();
        item.packageSha256 = QByteArray::fromHex(package.value(u"sha256"_s).toString().toLatin1());

        // QOR-0006: the Store carries free items only. `price` is read only to see that it is zero, never kept.
        if (const QJsonValue price = entry.value(u"price"_s);
            !price.isUndefined() && !(price.isDouble() && price.toDouble() == 0)) {
            result.error = u"Catalogue entry %1 has a price, and the Store carries only free items."_s.arg(
                item.id.isEmpty() ? u"?"_s : printable(item.id));
            result.catalog = {};
            return result;
        }

        const bool known = item.kind == u"theme"_s || item.kind == u"widget"_s;
        if (!isValidItemId(item.id) || !known || item.name.isEmpty() || !isValidVersion(item.version)
            || !catalog.developers.contains(item.developer) || item.packageSha256.size() != 32
            || !isSafePackagePath(item.packageFile) || catalog.item(item.id)) {
            result.error = kInvalidEntry.arg(item.id.isEmpty() ? u"?"_s : printable(item.id));
            result.catalog = {};
            return result;
        }
        catalog.items.append(item);
    }
    result.ok = true;
    return result;
}

} // namespace qor::store
