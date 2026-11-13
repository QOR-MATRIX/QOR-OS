// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "storefronts.h"

#include "catalog.h"
#include "services.h"
#include "store.h"
#include "storeengine.h"

#include <QtCore/QBuffer>
#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
#include <QtCore/QPointer>
#include <QtCore/QScopeGuard>
#include <QtGui/QImageReader>

#include <optional>

using namespace Qt::StringLiterals;

namespace qor::store {

namespace {

// A small file can claim a very large picture, and the decoder believes it. Four thousand pixels a side is more than a
// preview needs, and what it decodes to is still a size Qt's own limit lets through.
constexpr int kMaximumPreviewSide = 4096;
// What previews may hold in memory together, counted as they are held, as base64 text in a data: URL. Past this the
// oldest go first, and their cards show their glyph until their page is built again and asks for them again.
constexpr qint64 kMaximumHeldPreviews = 32 * 1024 * 1024;
// How many previews are on their way at once. The rest wait their turn, so a long page does not have every one of them
// in memory as it arrives.
constexpr qsizetype kPreviewsAtOnce = 4;

struct Shared
{
    QPointer<StoreEngine> engine;
    bool decided = false;
    QList<QUrl> catalogues;
    qsizetype current = 0;
};

Shared &shared()
{
    static Shared instance;
    return instance;
}

#ifdef QOR_STORE_DEMONSTRATION_KEY
// A catalogue's URL, or several separated by | to be taken in turn. Read only in a build that has the option.
QList<QUrl> demonstrationCatalogues()
{
    QList<QUrl> urls;
    const QStringList parts = qEnvironmentVariable("QOR_STORE_DEMONSTRATION_URL").split(u'|', Qt::SkipEmptyParts);
    for (const QString &part : parts)
        urls.append(QUrl::fromUserInput(part));
    return urls;
}
#endif

std::optional<View> viewNamed(const QString &name)
{
    if (name == viewName(View::Player))
        return View::Player;
    if (name == viewName(View::Creator))
        return View::Creator;
    return std::nullopt;
}

// A listing as a page may have it, in the view it was found in.
QVariantMap describe(const StoreEngine *store, View view, const CatalogItem &item)
{
    QVariantMap map = item.toVariant();
    // Where a catalogue says its files are is no business of a page's, and what it is called in the other store says
    // nothing here.
    map.insert(u"previewCount"_s, int(item.previews.size()));
    map.remove(u"previews"_s);
    map.remove(u"placements"_s);
    map.insert(u"categories"_s, item.categoriesIn(view));
    const Developer developer = store->catalog().developers.value(item.developer);
    map.insert(u"developerName"_s, developer.name.isEmpty() ? item.developer : developer.name);
    map.insert(u"verified"_s, developer.verified);
    return map;
}

QVariantList describeAll(const StoreEngine *store, View view, const QList<CatalogItem> &items)
{
    QVariantList list;
    for (const CatalogItem &item : items)
        list.append(describe(store, view, item));
    return list;
}

// Whether a person may see this item at all as things stand.
bool isWithinReach(const StoreEngine *store, const CatalogItem &item)
{
    return item.isPlacedIn(View::Player) || store->creatorFeatures();
}

QString previewKey(const QString &id, int index) { return id + u'#' + QString::number(index); }

// The catalogue's digest says these are the bytes it signed, and the file rules that they start like a picture. This
// says what they claim to decode to, before anything decodes them. Returns why not, or an empty string.
QString refusePicture(const QByteArray &data, QByteArray *format)
{
    QBuffer buffer;
    buffer.setData(data);
    if (!buffer.open(QIODevice::ReadOnly))
        return u"The preview cannot be read."_s;
    QImageReader reader(&buffer);
    if (!reader.canRead())
        return u"The preview is not a picture that can be shown."_s;
    const QSize size = reader.size();
    if (!size.isValid())
        return u"The preview is not a picture that can be shown."_s;
    if (size.width() > kMaximumPreviewSide || size.height() > kMaximumPreviewSide)
        return u"The preview is larger than a preview may be."_s;
    *format = reader.format();
    return {};
}

} // namespace

StoreEngine *Storefronts::engine()
{
    Shared &state = shared();
#ifdef QOR_STORE_DEMONSTRATION_KEY
    if (!state.decided) {
        state.decided = true;
        state.catalogues = demonstrationCatalogues();
        // A folder of its own, beside what Store::engine() keeps there, which looks only at items/ and installed.json.
        state.engine =
            new StoreEngine(Store::dataLocation() + u"/demonstration"_s,
                            QByteArray::fromBase64(QOR_STORE_DEMONSTRATION_KEY), QCoreApplication::instance());
        state.engine->setEngines({ { u"qq"_s, QStringLiteral(QOR_STORE_QQ_VERSION) } });
        if (!state.catalogues.isEmpty())
            state.engine->setCatalogUrl(state.catalogues.constFirst());
        state.engine->refresh();
    }
#endif
    return state.engine;
}

void Storefronts::useEngine(StoreEngine *engine, const QList<QUrl> &catalogues)
{
    Shared &state = shared();
    state.decided = true;
    state.engine = engine;
    state.catalogues = catalogues;
    state.current = 0;
    if (engine && !catalogues.isEmpty()) {
        engine->setCatalogUrl(catalogues.constFirst());
        engine->refresh();
    }
}

Storefronts::Storefronts(QObject *parent) : QObject(parent)
{
    StoreEngine *store = engine();
    if (!store)
        return;
    m_epoch = store->catalogueEpoch();
    connect(store, &StoreEngine::stateChanged, this, &Storefronts::stateChanged);
    connect(store, &StoreEngine::catalogChanged, this, [this, store] {
        // The same catalogue again, as "Check for Updates" finds it, keeps what was fetched of it.
        if (store->catalogueEpoch() != m_epoch) {
            m_epoch = store->catalogueEpoch();
            forgetFetched();
        }
        ++m_listingsRevision;
        ++m_statusRevision;
        Q_EMIT listingsChanged();
        Q_EMIT libraryChanged();
        Q_EMIT statusChanged();
    });
    connect(store, &StoreEngine::creatorFeaturesChanged, this, [this, store] {
        // What only the creator store carries is not handed over with them off, including what was fetched while they
        // were on.
        if (!store->creatorFeatures())
            forgetFetched();
        ++m_listingsRevision;
        ++m_statusRevision;
        Q_EMIT creatorFeaturesChanged();
        Q_EMIT listingsChanged();
        Q_EMIT libraryChanged();
        Q_EMIT statusChanged();
    });
    connect(store, &StoreEngine::installedChanged, this, [this] {
        ++m_statusRevision;
        Q_EMIT libraryChanged();
        Q_EMIT statusChanged();
    });
    connect(store, &StoreEngine::progressChanged, this, [this] {
        ++m_statusRevision;
        Q_EMIT statusChanged();
    });
    connect(store, &StoreEngine::finished, this, &Storefronts::finished);
    connect(store, &StoreEngine::previewFetched, this, &Storefronts::takePreview);
    connect(store, &StoreEngine::licenceFetched, this, &Storefronts::takeLicence);

    connect(Services::instance(), &Services::connectedChanged, this, &Storefronts::followPreference);
    followPreference();
}

// The preference is kept by Settings and handed to the engine, which does not remember it (docs/STORE.md).
void Storefronts::followPreference()
{
    disconnect(m_preference);
    SettingsReplica *settings = Services::instance()->settings();
    if (!settings)
        return;
    m_preference = connect(settings, &SettingsReplica::creatorFeaturesChanged, this, &Storefronts::takeCreatorFeatures);
    takeCreatorFeatures(settings->creatorFeatures());
}

void Storefronts::takeCreatorFeatures(bool on)
{
    if (StoreEngine *store = engine())
        store->setCreatorFeatures(on);
}

void Storefronts::forgetFetched()
{
    // What is on its way still arrives, and is checked then against the catalogue and the switch as they are.
    m_previews.clear();
    m_previewOrder.clear();
    m_previewQueue.clear();
    m_previewBytes = 0;
    m_licences.clear();
    ++m_previewRevision;
    ++m_licenceRevision;
    Q_EMIT previewsChanged();
    Q_EMIT licencesChanged();
    Q_EMIT previewsForgotten();
}

bool Storefronts::isAvailable() const { return engine() != nullptr; }

bool Storefronts::isReady() const
{
    const StoreEngine *store = engine();
    return store && store->state() == StoreEngine::State::Ready;
}

bool Storefronts::isLoading() const
{
    const StoreEngine *store = engine();
    return store && store->state() == StoreEngine::State::Loading;
}

QString Storefronts::error() const
{
    const StoreEngine *store = engine();
    if (!store)
        return {};
    if (store->catalogUrl().isEmpty())
        return u"This demonstration has no catalogue. Start QOR OS with QOR_STORE_DEMONSTRATION_URL set to the URL of "
               u"a "
               "format 2 catalog.json."_s;
    return store->error();
}

QString Storefronts::name() const
{
    const StoreEngine *store = engine();
    return store ? store->catalog().name : QString();
}

bool Storefronts::hasNextCatalogue() const
{
    const Shared &state = shared();
    return state.engine && state.current + 1 < state.catalogues.size();
}

bool Storefronts::creatorFeatures() const
{
    const StoreEngine *store = engine();
    return store && store->creatorFeatures();
}

QVariantList Storefronts::listings(const QString &view) const
{
    const StoreEngine *store = engine();
    const auto named = viewNamed(view);
    return store && named ? describeAll(store, *named, store->listings(*named)) : QVariantList();
}

QVariantList Storefronts::featured(const QString &view) const
{
    const StoreEngine *store = engine();
    const auto named = viewNamed(view);
    return store && named ? describeAll(store, *named, store->featured(*named)) : QVariantList();
}

QVariantList Storefronts::search(const QString &view, const QString &query) const
{
    const StoreEngine *store = engine();
    const auto named = viewNamed(view);
    return store && named ? describeAll(store, *named, store->search(*named, query)) : QVariantList();
}

QVariantMap Storefronts::categoryCounts(const QString &view) const
{
    const StoreEngine *store = engine();
    const auto named = viewNamed(view);
    QVariantMap counts;
    if (store && named) {
        const QMap<QString, int> found = store->categoryCounts(*named);
        for (auto it = found.cbegin(); it != found.cend(); ++it)
            counts.insert(it.key(), it.value());
    }
    return counts;
}

QVariantMap Storefronts::item(const QString &view, const QString &id) const
{
    const StoreEngine *store = engine();
    const auto named = viewNamed(view);
    if (!store || !named)
        return {};
    const QList<CatalogItem> listed = store->listings(*named);
    for (const CatalogItem &candidate : listed)
        if (candidate.id == id)
            return describe(store, *named, candidate);
    return {};
}

QVariantMap Storefronts::status(const QString &id) const
{
    const StoreEngine *store = engine();
    if (!store)
        return {};
    const CatalogItem *item = store->catalog().item(id);
    // What only the creator store carries, with creator features off, tells nothing of what it lists.
    const CatalogItem *listed = item && isWithinReach(store, *item) ? item : nullptr;
    const auto installed = store->installed().constFind(id);
    const bool isInstalled = installed != store->installed().cend();
    return {
        { u"name"_s, listed        ? listed->name
                     : isInstalled ? installed->name
                                   : id },
        { u"installed"_s, isInstalled },
        { u"installedVersion"_s, isInstalled ? installed->version : QString() },
        { u"version"_s, listed ? listed->version : QString() },
        { u"updateAvailable"_s, store->hasUpdate(id) },
        { u"updateWaits"_s, store->updateWaitsForCreatorFeatures(id) },
        { u"busy"_s, store->isBusy(id) },
        { u"progress"_s, store->progress(id) },
    };
}

// What is installed here, whichever store it was found in, and whether or not that store can be seen now.
QVariantList Storefronts::library() const
{
    const StoreEngine *store = engine();
    QVariantList list;
    if (!store)
        return list;
    for (const StoreEngine::Installed &installed : store->installed()) {
        const CatalogItem *found = store->catalog().item(installed.id);
        const CatalogItem *item = found && isWithinReach(store, *found) ? found : nullptr;
        const Developer developer = store->catalog().developers.value(installed.developer);
        list.append(QVariantMap {
            { u"id"_s, installed.id },
            { u"kind"_s, installed.kind },
            { u"name"_s, item ? item->name : installed.name },
            { u"developerName"_s, developer.name.isEmpty() ? installed.developer : developer.name },
            { u"previewCount"_s, item ? int(item->previews.size()) : 0 },
        });
    }
    return list;
}

int Storefronts::updateCount() const
{
    const StoreEngine *store = engine();
    return store ? store->updateCount() : 0;
}

int Storefronts::waitingUpdateCount() const
{
    const StoreEngine *store = engine();
    int count = 0;
    if (store) {
        for (const StoreEngine::Installed &installed : store->installed())
            count += store->updateWaitsForCreatorFeatures(installed.id) ? 1 : 0;
    }
    return count;
}

void Storefronts::refresh()
{
    if (StoreEngine *store = engine())
        store->refresh();
}

void Storefronts::nextCatalogue()
{
    Shared &state = shared();
    if (!state.engine || state.current + 1 >= state.catalogues.size())
        return;
    ++state.current;
    state.engine->setCatalogUrl(state.catalogues.at(state.current));
    state.engine->refresh();
}

bool Storefronts::install(const QString &id)
{
    StoreEngine *store = engine();
    if (!store)
        return false;
    // The engine says why for all it refuses, and has nothing to say of what it has never heard of.
    if (!store->catalog().item(id) && !store->catalog().draft(id)) {
        Q_EMIT finished(id, false, u"That is not in this catalogue."_s);
        return false;
    }
    return store->install(id);
}

bool Storefronts::remove(const QString &id)
{
    StoreEngine *store = engine();
    return store && store->remove(id);
}

bool Storefronts::fetchPreview(const QString &id, int index)
{
    StoreEngine *store = engine();
    if (!store)
        return false;
    const QString key = previewKey(id, index);
    if (m_previews.contains(key) || m_previewsOnTheirWay.contains(key) || m_previewQueue.contains({ id, index }))
        return true;
    if (m_previewsOnTheirWay.size() < kPreviewsAtOnce) {
        if (!store->fetchPreview(id, index))
            return false;
        m_previewsOnTheirWay.insert(key);
        return true;
    }
    // What the engine would refuse is refused now, and the rest waits its turn.
    const CatalogItem *item = store->catalog().item(id);
    if (!item || index < 0 || index >= item->previews.size() || !isWithinReach(store, *item))
        return false;
    m_previewQueue.append({ id, index });
    return true;
}

void Storefronts::startPreviews()
{
    StoreEngine *store = engine();
    while (store && m_previewsOnTheirWay.size() < kPreviewsAtOnce && !m_previewQueue.isEmpty()) {
        const auto [id, index] = m_previewQueue.takeFirst();
        if (store->fetchPreview(id, index))
            m_previewsOnTheirWay.insert(previewKey(id, index));
    }
}

void Storefronts::takePreview(const QString &id, int index, bool ok, const QByteArray &data, const QString &error)
{
    const StoreEngine *store = engine();
    const QString key = previewKey(id, index);
    // Only what was asked for here, and each answer lets the next that waits go.
    if (!m_previewsOnTheirWay.remove(key))
        return;
    const auto next = qScopeGuard([this] { startPreviews(); });
    const CatalogItem *item = store->catalog().item(id);
    // Overtaken while it was on its way: by the switch, or by a catalogue that no longer lists it.
    if (!item || index < 0 || index >= item->previews.size() || !isWithinReach(store, *item))
        return;
    // Checked against the catalogue it was asked of. If that has since moved on, its own is on its way too.
    if (ok && QCryptographicHash::hash(data, QCryptographicHash::Sha256) != item->previews.at(index).sha256)
        return;

    QByteArray format;
    const QString why = ok ? refusePicture(data, &format) : error;
    if (!why.isEmpty()) {
        Q_EMIT previewRefused(id, index, why);
        return;
    }
    const QString text =
        u"data:image/"_s + QString::fromLatin1(format) + u";base64,"_s + QString::fromLatin1(data.toBase64());
    const qint64 size = qint64(text.size()) * qint64(sizeof(QChar));
    while (!m_previewOrder.isEmpty() && m_previewBytes + size > kMaximumHeldPreviews)
        m_previewBytes -= m_previews.take(m_previewOrder.takeFirst()).size;
    m_previews.insert(key, HeldPreview { QUrl(text), size });
    m_previewOrder.append(key);
    m_previewBytes += size;
    ++m_previewRevision;
    Q_EMIT previewsChanged();
}

QUrl Storefronts::preview(const QString &id, int index) const { return m_previews.value(previewKey(id, index)).url; }

bool Storefronts::fetchLicence(const QString &id)
{
    StoreEngine *store = engine();
    if (!store || !store->fetchLicence(id))
        return false;
    m_licences.insert(id, { { u"asked"_s, true } });
    ++m_licenceRevision;
    Q_EMIT licencesChanged();
    return true;
}

void Storefronts::takeLicence(const QString &id, bool ok, const QString &text, const QString &error)
{
    // Forgotten while it was on its way, as the switch went off or the catalogue moved on: that is no longer asked for.
    if (!m_licences.contains(id))
        return;
    m_licences.insert(id, ok ? QVariantMap { { u"asked"_s, true }, { u"text"_s, text } }
                             : QVariantMap { { u"asked"_s, true }, { u"error"_s, error } });
    ++m_licenceRevision;
    Q_EMIT licencesChanged();
}

QVariantMap Storefronts::licence(const QString &id) const { return m_licences.value(id); }

} // namespace qor::store
