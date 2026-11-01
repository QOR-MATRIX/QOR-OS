// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "storeengine.h"

#include "filerules.h"
#include "package.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QLockFile>
#include <QtCore/QLoggingCategory>
#include <QtCore/QRegularExpression>
#include <QtCore/QSaveFile>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <algorithm>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcStore, "qor.store")

namespace qor::store {

namespace {

constexpr qint64 kMaximumCatalogSize = 4 * 1024 * 1024;
constexpr qint64 kMaximumPreviewSize = 4 * 1024 * 1024;
constexpr int kTransferTimeoutMs = 30000;
constexpr int kLockTimeoutMs = 5000;
// Longer than writing the largest package takes. QLockFile's own 30 seconds could call a slow install abandoned, and
// clear it.
constexpr int kLockStaleMs = 5 * 60 * 1000;

QJsonArray rangesToJson(const QList<VersionRange> &ranges)
{
    QJsonArray array;
    for (const VersionRange &range : ranges)
        array.append(QJsonObject { { u"id"_s, range.id }, { u"min"_s, range.min }, { u"below"_s, range.below } });
    return array;
}

} // namespace

StoreEngine::StoreEngine(const QString &dataDir, const QByteArray &storeKey, QObject *parent)
    : QObject(parent), m_dataDir(dataDir), m_storeKey(storeKey), m_network(new QNetworkAccessManager(this))
{
    m_network->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    m_network->setTransferTimeout(kTransferTimeoutMs);
    QDir().mkpath(m_dataDir + u"/items"_s);
    recoverInterrupted();
    loadInstalled();
    loadSequence();
}

StoreEngine::~StoreEngine()
{
    for (const QPointer<QNetworkReply> &reply : std::as_const(m_jobs))
        if (reply)
            reply->abort();
}

void StoreEngine::setCatalogUrl(const QUrl &url) { m_catalogUrl = url; }

void StoreEngine::setCreatorFeatures(bool on)
{
    if (m_creatorFeatures == on)
        return;
    m_creatorFeatures = on;
    Q_EMIT creatorFeaturesChanged();
}

QList<CatalogItem> StoreEngine::listings(View view) const
{
    return view == View::Creator && !m_creatorFeatures ? QList<CatalogItem>() : m_catalog.listings(view);
}

QList<CatalogItem> StoreEngine::featured(View view) const
{
    return view == View::Creator && !m_creatorFeatures ? QList<CatalogItem>() : m_catalog.featured(view);
}

QList<CatalogItem> StoreEngine::search(View view, const QString &query) const
{
    return view == View::Creator && !m_creatorFeatures ? QList<CatalogItem>() : m_catalog.search(view, query);
}

QMap<QString, int> StoreEngine::categoryCounts(View view) const
{
    return view == View::Creator && !m_creatorFeatures ? QMap<QString, int>() : m_catalog.categoryCounts(view);
}

QString StoreEngine::installDirectory(const QString &id) const { return m_dataDir + u"/items/"_s + id; }

void StoreEngine::setState(State state, const QString &error)
{
    m_error = error;
    if (m_state == state && error.isEmpty())
        return;
    m_state = state;
    Q_EMIT stateChanged();
}

// Local catalogues (qrc:, file:) are read directly; anything else goes
// through the network. Either way the result arrives asynchronously.
void StoreEngine::fetch(const QString &job, const QUrl &url, qint64 maximumSize, const Done &done)
{
    if (url.scheme() == u"qrc"_s || url.isLocalFile()) {
        const QString path = url.scheme() == u"qrc"_s ? u":"_s + url.path() : url.toLocalFile();
        QTimer::singleShot(0, this, [path, maximumSize, done] {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                return done(false, {}, u"%1 cannot be read."_s.arg(path));
            if (file.size() > maximumSize)
                return done(false, {}, u"%1 is larger than expected."_s.arg(path));
            done(true, file.readAll(), {});
        });
        return;
    }
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    QNetworkReply *reply = m_network->get(request);
    if (!job.isEmpty()) {
        m_jobs.insert(job, reply);
        connect(reply, &QNetworkReply::downloadProgress, this, [this, job](qint64 received, qint64 total) {
            m_progress.insert(job, total > 0 ? 0.9 * double(received) / double(total) : 0);
            Q_EMIT progressChanged(job);
        });
    }
    connect(reply, &QNetworkReply::readyRead, reply, [reply, maximumSize] {
        if (reply->bytesAvailable() > maximumSize)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return done(false, {}, reply->errorString());
        done(true, reply->readAll(), {});
    });
}

void StoreEngine::refresh()
{
    if (m_catalogUrl.isEmpty()) {
        setState(State::Failed, u"No store catalogue is configured."_s);
        return;
    }
    const int generation = ++m_generation;
    setState(State::Loading);
    const QUrl signatureUrl(m_catalogUrl.toString() + u".sig"_s);
    fetch({}, m_catalogUrl, kMaximumCatalogSize,
          [this, generation, signatureUrl](bool ok, const QByteArray &json, const QString &error) {
              if (generation != m_generation)
                  return;
              if (!ok)
                  return setState(State::Failed, error);
              fetch({}, signatureUrl, 1024,
                    [this, generation, json](bool fetched, const QByteArray &signature, const QString &fetchError) {
                        if (generation != m_generation)
                            return;
                        if (!fetched)
                            return setState(State::Failed, fetchError);
                        CatalogResult result = parseCatalog(json, signature, m_storeKey);
                        if (result.ok)
                            result.error = admit(result.catalog, json);
                        if (!result.ok || !result.error.isEmpty()) {
                            qCWarning(lcStore) << "Catalogue refused:" << result.error;
                            return setState(State::Failed, result.error);
                        }
                        adoptCatalog(std::move(result.catalog));
                    });
          });
}

void StoreEngine::adoptCatalog(Catalog catalog)
{
    m_catalog = std::move(catalog);
    qCInfo(lcStore) << "Catalogue" << m_catalog.name << "with" << m_catalog.items.size() << "items";
    Q_EMIT catalogChanged();
    setState(State::Ready);
}

// Whether this store may take a catalogue that has passed its signature and its own checks, and remembers its sequence.
// Returns why not, or an empty string. Format 1 knows no sequence, as it never did, so it is taken as it always was,
// until this store has taken a format 2 catalogue: after that, an older format would be the way round the sequence.
QString StoreEngine::admit(const Catalog &catalog, const QByteArray &json)
{
    if (catalog.format != 2)
        return m_lastSequence > 0
                   ? u"This store has taken a format 2 catalogue, and format 1 has no sequence to show it is "
                     "not an older one."_s
                   : QString();

    // An older copy is still signed, so only remembering what has been seen tells it from a new one.
    const QByteArray digest = QCryptographicHash::hash(json, QCryptographicHash::Sha256);
    if (catalog.sequence < m_lastSequence)
        return u"This catalogue is older than one already seen: its sequence is %1, and %2 has been seen."_s
            .arg(catalog.sequence)
            .arg(m_lastSequence);
    if (catalog.sequence == m_lastSequence) {
        if (digest != m_lastDigest)
            return u"This catalogue differs from the one already seen at sequence %1."_s.arg(catalog.sequence);
        return {};
    }

    QSaveFile file(m_dataDir + u"/catalog-state.json"_s);
    const QJsonObject state { { u"format"_s, 1 },
                              { u"sequence"_s, catalog.sequence },
                              { u"sha256"_s, QString::fromLatin1(digest.toHex()) } };
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(state).toJson()) < 0 || !file.commit())
        return u"The store could not remember this catalogue's sequence, so it is not taken."_s;
    m_lastSequence = catalog.sequence;
    m_lastDigest = digest;
    return {};
}

void StoreEngine::loadSequence()
{
    QFile file(m_dataDir + u"/catalog-state.json"_s);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject state = QJsonDocument::fromJson(file.read(4096)).object();
    m_lastSequence = std::max<qint64>(state.value(u"sequence"_s).toInteger(0), 0);
    m_lastDigest = QByteArray::fromHex(state.value(u"sha256"_s).toString().toLatin1());
}

// What stops an item in the catalogue from being installed. Every catalogue was checked as it arrived, and this is
// asked again here, of each thing an install would fetch: the catalogue may have expired since, a dependency must not
// carry past the switch what the switch keeps out, and what keeps the stores apart is not left to a single wall.
QString StoreEngine::refusalToInstall(const CatalogItem &item) const
{
    if (item.format != 2)
        return {};
    if (!takesKind(item.kind))
        return u"This stage does not take a %1, which %2 is."_s.arg(printable(item.kind), item.id);
    if (item.executable)
        return u"%1 is executable, and this stage takes nothing executable."_s.arg(item.id);
    if (item.entitlement != u"free"_s)
        return u"%1 is not free, and this stage takes only what is."_s.arg(item.id);
    if (QDateTime::currentDateTimeUtc().date() > m_catalog.expires)
        return u"The catalogue that lists %1 has expired."_s.arg(item.id);
    if (!item.isPlacedIn(View::Player) && !(m_creatorFeatures && item.isPlacedIn(View::Creator)))
        return item.isPlacedIn(View::Creator)
                   ? u"%1 is in the creator store, and creator features are off."_s.arg(item.id)
                   : u"%1 is placed in no store."_s.arg(item.id);
    return {};
}

QMap<QString, InstalledVersion> StoreEngine::installedVersions() const
{
    QMap<QString, InstalledVersion> versions;
    for (const Installed &installed : m_installed)
        versions.insert(installed.id, { installed.version, installed.dependencies });
    return versions;
}

bool StoreEngine::install(const QString &id)
{
    if (isBusy(id))
        return false;
    const CatalogItem *item = m_catalog.item(id);
    if (!item) {
        if (const Draft *draft = m_catalog.draft(id))
            fail(id, draft->reason);
        return false;
    }
    if (const QString refusal = refusalToInstall(*item); !refusal.isEmpty()) {
        fail(id, refusal);
        return false;
    }
    const InstallPlan plan = resolveDependencies(m_catalog, id, installedVersions());
    if (!plan.ok) {
        fail(id, plan.error);
        return false;
    }
    for (const QString &step : plan.order) {
        if (isBusy(step))
            return false;
        if (const QString refusal = refusalToInstall(*m_catalog.item(step)); !refusal.isEmpty()) {
            fail(id, refusal);
            return false;
        }
    }
    // Every step holds a job from here, so none of them starts twice.
    for (const QString &step : plan.order) {
        m_jobs.insert(step, nullptr);
        m_progress.insert(step, 0);
        Q_EMIT progressChanged(step);
    }
    runSteps(plan.order);
    return true;
}

void StoreEngine::runSteps(const QStringList &steps)
{
    const QString id = steps.constFirst();
    m_waiting.insert(id, steps.mid(1));
    const CatalogItem *item = m_catalog.item(id);
    if (!item)
        return fail(id, u"The item is no longer in the catalogue."_s);
    const QUrl url = m_catalogUrl.resolved(QUrl(item->packageFile));
    // A format 2 catalogue says exactly how large the download is.
    const qint64 limit = item->format == 2 ? item->packageSize : item->packageSize + 1024;
    fetch(id, url, limit, [this, id](bool ok, const QByteArray &data, const QString &error) {
        if (!ok)
            return fail(id, error);
        complete(id, data);
    });
}

void StoreEngine::complete(const QString &id, const QByteArray &data)
{
    const CatalogItem *item = m_catalog.item(id);
    if (!item)
        return fail(id, u"The item is no longer in the catalogue."_s);
    // The switch may have been turned off, or the catalogue run out, while this was on its way.
    if (const QString refusal = refusalToInstall(*item); !refusal.isEmpty())
        return fail(id, refusal);
    if (QCryptographicHash::hash(data, QCryptographicHash::Sha256) != item->packageSha256)
        return fail(id, u"The download does not match the catalogue."_s);
    const Developer developer = m_catalog.developers.value(item->developer);
    // A format 2 catalogue says what the package unpacks to, and it may not unpack to a byte more.
    const PackageResult package =
        item->format == 2 ? openPackage(data, developer.key, item->packageUnpacked) : openPackage(data, developer.key);
    if (!package.ok)
        return fail(id, package.error);
    const PackageManifest &manifest = package.manifest;
    if (manifest.id != item->id || manifest.version != item->version || manifest.kind != item->kind
        || manifest.developer != item->developer || manifest.format != item->format
        || (item->format == 2
            && (manifest.stores != item->storeNames() || manifest.edition != item->edition
                || manifest.licence != item->licence || manifest.dependencies != item->dependencies
                || package.unpacked != item->packageUnpacked)))
        return fail(id, u"The package is not the one the catalogue lists."_s);
    // An old catalogue, replayed, could otherwise put back a version with a known fault.
    if (const auto installed = m_installed.constFind(id);
        installed != m_installed.cend() && compareVersions(item->version, installed->version) < 0)
        return fail(id, u"A newer version of this is already installed."_s);

    // Another process of this person's may share this folder. It clears what looks abandoned as it starts, so it must
    // not find this install half done.
    QLockFile lock(m_dataDir + u"/.lock"_s);
    lock.setStaleLockTime(kLockStaleMs);
    if (!lock.tryLock(kLockTimeoutMs))
        return fail(id, u"The store is busy in another program."_s);

    // Write everything beside the old version, then swap.
    const QString target = installDirectory(id);
    const QString staging = m_dataDir + u"/items/.staging-"_s + QUuid::createUuid().toString(QUuid::Id128);
    QMap<QString, QByteArray> files = package.contents;
    // Kept beside them, so what runs can be checked each time it is loaded.
    files.insert(u"manifest.json"_s, package.manifestBytes);
    files.insert(u"manifest.sig"_s, package.signature);
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        const QString path = staging + u'/' + it.key();
        QDir().mkpath(QFileInfo(path).absolutePath());
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(it.value()) != it.value().size() || !file.commit()) {
            QDir(staging).removeRecursively();
            return fail(id, u"%1 could not be written."_s.arg(it.key()));
        }
    }
    const QString previous = target + u".old-"_s + QUuid::createUuid().toString(QUuid::Id128);
    if (QFileInfo::exists(target) && !QDir().rename(target, previous)) {
        QDir(staging).removeRecursively();
        return fail(id, u"The installed version is in use."_s);
    }
    if (!QDir().rename(staging, target)) {
        QDir().rename(previous, target);
        QDir(staging).removeRecursively();
        return fail(id, u"The package could not be installed."_s);
    }
    QDir(previous).removeRecursively();
    lock.unlock();

    m_installed.insert(id, { id, item->kind, item->name, item->version, item->developer,
                             QDateTime::currentDateTimeUtc().toString(Qt::ISODate), item->dependencies });
    saveInstalled();
    m_jobs.remove(id);
    m_progress.remove(id);
    const QStringList next = m_waiting.take(id);
    qCInfo(lcStore) << "Installed" << id << item->version;
    Q_EMIT installedChanged();
    Q_EMIT progressChanged(id);
    Q_EMIT finished(id, true, {});
    if (!next.isEmpty())
        runSteps(next);
}

bool StoreEngine::isIntact(const QString &id) const
{
    const auto installed = m_installed.constFind(id);
    if (installed == m_installed.cend())
        return false;
    const auto developer = m_catalog.developers.constFind(installed->developer);
    if (developer == m_catalog.developers.cend())
        return false;
    PackageManifest manifest;
    return verifyInstalled(installDirectory(id), developer->key, &manifest) && manifest.id == installed->id
           && manifest.kind == installed->kind && manifest.version == installed->version
           && manifest.developer == installed->developer;
}

void StoreEngine::fail(const QString &id, const QString &error)
{
    qCWarning(lcStore) << "Install of" << id << "failed:" << error;
    const QStringList next = m_waiting.take(id);
    m_jobs.remove(id);
    m_progress.remove(id);
    Q_EMIT progressChanged(id);
    Q_EMIT finished(id, false, error);
    // What was waiting on it needs it, so it stops here too.
    for (const QString &step : next) {
        m_jobs.remove(step);
        m_progress.remove(step);
        Q_EMIT progressChanged(step);
        Q_EMIT finished(step, false, u"%1 could not be installed: %2"_s.arg(id, error));
    }
}

bool StoreEngine::remove(const QString &id)
{
    if (!m_installed.contains(id) || isBusy(id))
        return false;
    for (const Installed &other : std::as_const(m_installed))
        for (const VersionRange &range : other.dependencies)
            if (range.id == id) {
                fail(id, u"%1 needs this, so it stays."_s.arg(other.id));
                return false;
            }
    QDir(installDirectory(id)).removeRecursively();
    m_installed.remove(id);
    saveInstalled();
    qCInfo(lcStore) << "Removed" << id;
    Q_EMIT installedChanged();
    return true;
}

bool StoreEngine::fetchPreview(const QString &id, int index)
{
    const CatalogItem *item = m_catalog.item(id);
    if (!item || index < 0 || index >= item->previews.size())
        return false;
    if (!item->isPlacedIn(View::Player) && !m_creatorFeatures)
        return false;
    const Preview preview = item->previews.at(index);
    fetch({}, m_catalogUrl.resolved(QUrl(preview.file)), kMaximumPreviewSize,
          [this, id, index, preview](bool ok, const QByteArray &data, const QString &error) {
              QString why = error;
              if (ok && QCryptographicHash::hash(data, QCryptographicHash::Sha256) != preview.sha256)
                  why = u"The preview does not match the catalogue."_s;
              else if (ok)
                  why = refusePreviewFile(preview.file, data);
              Q_EMIT previewFetched(id, index, why.isEmpty(), why.isEmpty() ? data : QByteArray(), why);
          });
    return true;
}

// A crash during an install can leave a staging folder, or an item's old version beside it or instead of it. What was
// mid-swap goes back where it was, and the rest goes. If another process holds the store's lock, this leaves it alone:
// that is an install under way, not an abandoned one.
void StoreEngine::recoverInterrupted()
{
    const QString items = m_dataDir + u"/items"_s;
    const QDir folder(items);
    const QStringList names = folder.entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
    static const QRegularExpression old(u"^(.+)\\.old-[0-9a-f]{32}$"_s);
    const auto isLeftOver = [](const QString &name) {
        return name.startsWith(u".staging-"_s) || old.match(name).hasMatch();
    };
    if (std::none_of(names.cbegin(), names.cend(), isLeftOver))
        return;
    QLockFile lock(m_dataDir + u"/.lock"_s);
    lock.setStaleLockTime(kLockStaleMs);
    if (!lock.tryLock(0))
        return;
    for (const QString &name : names) {
        if (name.startsWith(u".staging-"_s)) {
            QDir(folder.filePath(name)).removeRecursively();
            continue;
        }
        const QRegularExpressionMatch match = old.match(name);
        if (!match.hasMatch())
            continue;
        const QString id = match.captured(1);
        if (isValidItemId(id) && !QFileInfo::exists(folder.filePath(id)))
            QDir().rename(folder.filePath(name), folder.filePath(id));
        else
            QDir(folder.filePath(name)).removeRecursively();
    }
}

void StoreEngine::loadInstalled()
{
    QFile file(m_dataDir + u"/installed.json"_s);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject items = QJsonDocument::fromJson(file.readAll()).object().value(u"items"_s).toObject();
    for (auto it = items.begin(); it != items.end(); ++it) {
        const QJsonObject entry = it.value().toObject();
        Installed installed { it.key(),
                              entry.value(u"kind"_s).toString(),
                              entry.value(u"name"_s).toString(),
                              entry.value(u"version"_s).toString(),
                              entry.value(u"developer"_s).toString(),
                              entry.value(u"installedAt"_s).toString(),
                              {} };
        if (entry.contains(u"dependencies"_s)
            && !parseDependencies(entry.value(u"dependencies"_s), &installed.dependencies).isEmpty())
            continue;
        // Only what is actually on disk.
        if (isValidItemId(installed.id) && QFileInfo(installDirectory(installed.id)).isDir())
            m_installed.insert(installed.id, installed);
    }
}

bool StoreEngine::saveInstalled() const
{
    QJsonObject items;
    for (const Installed &installed : m_installed) {
        QJsonObject entry { { u"kind"_s, installed.kind },
                            { u"name"_s, installed.name },
                            { u"version"_s, installed.version },
                            { u"developer"_s, installed.developer },
                            { u"installedAt"_s, installed.installedAt } };
        if (!installed.dependencies.isEmpty())
            entry.insert(u"dependencies"_s, rangesToJson(installed.dependencies));
        items.insert(installed.id, entry);
    }
    QSaveFile file(m_dataDir + u"/installed.json"_s);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(
        QJsonDocument(QJsonObject { { u"format"_s, 1 }, { u"items"_s, items } }).toJson(QJsonDocument::Indented));
    return file.commit();
}

} // namespace qor::store
