// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "storeengine.h"

#include "filerules.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QLockFile>
#include <QtCore/QLoggingCategory>
#include <QtCore/QRegularExpression>
#include <QtCore/QSaveFile>
#include <QtCore/QSet>
#include <QtCore/QTimeZone>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <algorithm>
#include <utility>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#endif

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcStore, "qor.store")

namespace qor::store {

namespace {

constexpr qint64 kMaximumCatalogSize = 4 * 1024 * 1024;
constexpr qint64 kMaximumPreviewSize = 4 * 1024 * 1024;
// More than any licence is, and few enough that a person's page can lay it out.
constexpr qint64 kMaximumLicenceSize = 256 * 1024;
// A plan's packages are held in memory until all of them are verified, so what every plan under way may come to,
// together, is bounded.
constexpr qint64 kMaximumHeldBytes = 128 * 1024 * 1024;
constexpr char kReceivedProperty[] = "qorReceived"; // what a download has been counted as holding
constexpr char kTooMuchProperty[] = "qorTooMuch";   // a download stopped because everything together would not fit
constexpr QStringView kTooMuchOnItsWay = u"Too much is on its way at once. Try this again when the rest has finished.";
constexpr int kTransferTimeoutMs = 30000;
constexpr int kLockTimeoutMs = 5000;

QJsonArray rangesToJson(const QList<VersionRange> &ranges)
{
    QJsonArray array;
    for (const VersionRange &range : ranges)
        array.append(QJsonObject { { u"id"_s, range.id }, { u"min"_s, range.min }, { u"below"_s, range.below } });
    return array;
}

// The same set of ranges, whichever order each lists them in.
bool sameRanges(QList<VersionRange> a, QList<VersionRange> b)
{
    const auto byId = [](const VersionRange &x, const VersionRange &y) { return x.id < y.id; };
    std::sort(a.begin(), a.end(), byId);
    std::sort(b.begin(), b.end(), byId);
    return a == b;
}

bool samePlatforms(QStringList a, QStringList b)
{
    a.sort();
    b.sort();
    return a == b;
}

QString journalPath(const QString &dataDir) { return dataDir + u"/plan-journal.json"_s; }

// The store's lock is never taken from a holder that is alive, however long it has held it. Qt would otherwise take a
// lock older than 30 seconds wherever the file system has no locks of its own to show the holder is there, as a
// network or FUSE one may not. A holder that has died is still found out, by its process.
bool takeLock(QLockFile &lock, int timeoutMs)
{
    lock.setStaleLockTime(0);
    return lock.tryLock(timeoutMs);
}

bool isLink(const QFileInfo &info) { return info.isSymLink() || info.isJunction(); }

// Whether anything stands at a path, a link to nothing included.
bool present(const QString &path)
{
    const QFileInfo info(path);
    return info.exists() || isLink(info);
}

bool removeLinkOrFile(const QString &path, bool link)
{
    // A link to a folder on Windows is removed as a folder, and goes without what it points to.
    return QFile::remove(path) || (link && QDir().rmdir(path));
}

// Deletes what stands at a path, and never through a link: a link, at the top or anywhere inside, goes itself, and
// what it points to stays where it is. QDir::removeRecursively follows a link it is given. It walks a folder at a
// time, with one listing open and no recursion, so however deep a tree someone plants, it needs neither a handle per
// level nor a stack frame. False if anything stayed.
bool removeTree(const QString &path)
{
    const QFileInfo info(path);
    if (isLink(info))
        return removeLinkOrFile(path, true);
    if (!info.exists())
        return true;
    if (!info.isDir())
        return QFile::remove(path);
    bool removed = true;
    QStringList folders { path }; // each after the folder that holds it
    for (qsizetype next = 0; next < folders.size(); ++next) {
        QStringList entries;
        {
            QDirIterator walk(folders.at(next), QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
            while (walk.hasNext())
                entries.append(walk.next());
        }
        for (const QString &entry : std::as_const(entries)) {
            const QFileInfo found(entry);
            if (isLink(found) || !found.isDir())
                removed = removeLinkOrFile(entry, isLink(found)) && removed;
            else
                folders.append(entry);
        }
    }
    for (auto folder = folders.crbegin(); folder != folders.crend(); ++folder)
        removed = QDir().rmdir(*folder) && removed;
    return removed;
}

} // namespace

StoreEngine::StoreEngine(const QString &dataDir, const QByteArray &storeKey, QObject *parent)
    : QObject(parent), m_dataDir(dataDir), m_storeKey(storeKey),
      m_keyId(QString::fromLatin1(QCryptographicHash::hash(storeKey, QCryptographicHash::Sha256).toHex())),
      m_network(new QNetworkAccessManager(this))
{
    m_network->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    m_network->setTransferTimeout(kTransferTimeoutMs);
    QDir().mkpath(m_dataDir + u"/items"_s);
    recoverInterrupted();
    m_installed = readInstalled();
    loadSequence();
}

StoreEngine::~StoreEngine()
{
    // Licences on their way are let go first, so that their replies, aborted, find nothing to tell.
    const QHash<QString, LicenceFetch> licences = std::exchange(m_licences, {});
    for (const LicenceFetch &licence : licences)
        if (licence.reply)
            licence.reply->abort();
    // A copy: aborting a reply finishes it at once, which ends its plan, which takes jobs out of m_jobs.
    const QHash<QString, QPointer<QNetworkReply>> jobs = m_jobs;
    for (const QPointer<QNetworkReply> &reply : jobs)
        if (reply)
            reply->abort();
}

void StoreEngine::setCatalogUrl(const QUrl &url) { m_catalogUrl = url; }

void StoreEngine::setCreatorFeatures(bool on)
{
    if (m_creatorFeatures == on)
        return;
    m_creatorFeatures = on;
    // A licence of what only the creator store carries stops on its way, lets go of what it held, and says why.
    if (!on)
        abortLicences([this](const QString &id) {
            const CatalogItem *item = m_catalog.item(id);
            return !item || !item->isPlacedIn(View::Player);
        });
    Q_EMIT creatorFeaturesChanged();
}

void StoreEngine::setEngines(const QMap<QString, QString> &engines) { m_engines = engines; }

bool StoreEngine::catalogueHasExpired() const
{
    return m_catalog.format == 2 && QDateTime::currentDateTimeUtc().date() > m_catalog.expires;
}

bool StoreEngine::viewIsClosed(View view) const
{
    return (view == View::Creator && !m_creatorFeatures) || catalogueHasExpired();
}

QList<CatalogItem> StoreEngine::listings(View view) const
{
    return viewIsClosed(view) ? QList<CatalogItem>() : m_catalog.listings(view);
}

QList<CatalogItem> StoreEngine::featured(View view) const
{
    return viewIsClosed(view) ? QList<CatalogItem>() : m_catalog.featured(view);
}

QList<CatalogItem> StoreEngine::search(View view, const QString &query) const
{
    return viewIsClosed(view) ? QList<CatalogItem>() : m_catalog.search(view, query);
}

QMap<QString, int> StoreEngine::categoryCounts(View view) const
{
    return viewIsClosed(view) ? QMap<QString, int>() : m_catalog.categoryCounts(view);
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
QNetworkReply *StoreEngine::fetch(const QString &job, const QUrl &url, qint64 maximumSize, const Done &done, bool held)
{
    const bool counted = held || !job.isEmpty();
    if (url.scheme() == u"qrc"_s || url.isLocalFile()) {
        const QString path = url.scheme() == u"qrc"_s ? u":"_s + url.path() : url.toLocalFile();
        QTimer::singleShot(0, this, [this, path, maximumSize, done, counted] {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                return done(false, {}, u"%1 cannot be read."_s.arg(path));
            if (file.size() > maximumSize)
                return done(false, {}, u"%1 is larger than expected."_s.arg(path));
            // Read whole in one go, so counted whole before it is read, as a download is as it arrives.
            if (counted && m_heldBytes + m_receivedBytes + file.size() > heldBytesLimit())
                return done(false, {}, kTooMuchOnItsWay.toString());
            done(true, file.readAll(), {});
        });
        return nullptr;
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
    // A package on its way is held in memory as it comes, so it counts against what every plan may hold together, and
    // a download that would take more than is left stops there.
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, maximumSize, counted] {
        if (reply->bytesAvailable() > maximumSize)
            return reply->abort();
        if (counted && !countReceived(reply, reply->bytesAvailable())) {
            reply->setProperty(kTooMuchProperty, true);
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, done, counted] {
        reply->deleteLater();
        const bool tooMuch =
            reply->property(kTooMuchProperty).toBool()
            || (counted && reply->error() == QNetworkReply::NoError && !countReceived(reply, reply->bytesAvailable()));
        if (counted)
            countReceived(reply, 0); // let go: what arrived is the plan's to count now
        if (tooMuch)
            return done(false, {}, kTooMuchOnItsWay.toString());
        if (reply->error() != QNetworkReply::NoError)
            return done(false, {}, reply->errorString());
        done(true, reply->readAll(), {});
    });
    return reply;
}

// Counts what a download holds now, and says whether every plan's holdings and every download's still fit together.
bool StoreEngine::countReceived(QNetworkReply *reply, qint64 bytes)
{
    m_receivedBytes += bytes - reply->property(kReceivedProperty).toLongLong();
    reply->setProperty(kReceivedProperty, bytes);
    return m_heldBytes + m_receivedBytes <= heldBytesLimit();
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
                        // The same bytes again changes nothing, so a plan under way carries on through "Check for
                        // Updates".
                        const QByteArray digest = QCryptographicHash::hash(json, QCryptographicHash::Sha256);
                        if (digest == m_catalogDigest)
                            return takeCatalog(std::move(result.catalog));
                        m_catalogDigest = digest;
                        adoptCatalog(std::move(result.catalog));
                    });
          });
}

void StoreEngine::adoptCatalog(Catalog catalog)
{
    ++m_catalogEpoch;
    abortLicences([](const QString &) { return true; });
    takeCatalog(std::move(catalog));
}

void StoreEngine::takeCatalog(Catalog catalog)
{
    m_catalog = std::move(catalog);
    qCInfo(lcStore) << "Catalogue" << m_catalog.name << "with" << m_catalog.items.size() << "items";
    Q_EMIT catalogChanged();
    setState(State::Ready);
}

// Whether this store may take a catalogue that has passed its signature and its own checks, and remembers its sequence.
// Returns why not, or an empty string. Format 1 knows no sequence, as it never did, so it is taken as it always was,
// until this store has taken a format 2 catalogue under the same key: after that, an older format would be the way
// round the sequence. What is remembered is reread under the lock, since another program may have moved on, and it is
// kept for each store key, so another store's catalogue starts its own memory and cannot lock out one that works.
QString StoreEngine::admit(const Catalog &catalog, const QByteArray &json)
{
    QLockFile lock(m_dataDir + u"/.lock"_s);
    if (!takeLock(lock, kLockTimeoutMs))
        return u"The store is busy in another program."_s;
    loadSequence();

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

    const QString path = m_dataDir + u"/catalog-state.json"_s;
    QJsonObject stores;
    {
        QFile existing(path);
        if (existing.open(QIODevice::ReadOnly))
            stores = QJsonDocument::fromJson(existing.readAll()).object().value(u"stores"_s).toObject();
    }
    stores.insert(m_keyId, QJsonObject { { u"sequence"_s, catalog.sequence },
                                         { u"sha256"_s, QString::fromLatin1(digest.toHex()) } });
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(QJsonObject { { u"format"_s, 2 }, { u"stores"_s, stores } }).toJson()) < 0
        || !file.commit())
        return u"The store could not remember this catalogue's sequence, so it is not taken."_s;
    syncFolder(m_dataDir);
    m_lastSequence = catalog.sequence;
    m_lastDigest = digest;
    return {};
}

// A file that cannot be read is treated as one that is not there, with a warning. It cannot be told from one deleted,
// and deleting it is open to anything running as this person, so refusing here would be an outage with no security in
// it.
void StoreEngine::loadSequence()
{
    m_lastSequence = 0;
    m_lastDigest.clear();
    QFile file(m_dataDir + u"/catalog-state.json"_s);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        qCWarning(lcStore) << "The store's memory of its catalogues cannot be read, and starts again.";
        return;
    }
    const QJsonObject state = document.object().value(u"stores"_s).toObject().value(m_keyId).toObject();
    m_lastSequence = std::max<qint64>(state.value(u"sequence"_s).toInteger(0), 0);
    m_lastDigest = QByteArray::fromHex(state.value(u"sha256"_s).toString().toLatin1());
}

// What stops an item in the catalogue from being installed. Every catalogue was checked as it arrived, and this is
// asked again here, of each thing an install would fetch and again when it arrives: the catalogue may have expired
// since, a dependency must not carry past the switch what the switch keeps out, and what keeps the stores apart is not
// left to a single wall.
QString StoreEngine::refusalToInstall(const CatalogItem &item, bool assumeCreatorFeatures) const
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
    if (!item.isPlacedIn(View::Player)
        && !((m_creatorFeatures || assumeCreatorFeatures) && item.isPlacedIn(View::Creator)))
        return item.isPlacedIn(View::Creator)
                   ? u"%1 is in the creator store, and creator features are off."_s.arg(item.id)
                   : u"%1 is placed in no store."_s.arg(item.id);
    if (!item.platforms.contains(currentPlatform()))
        return u"%1 is not for this system, %2."_s.arg(item.id, currentPlatform());
    if (!m_engines.isEmpty()) {
        const bool works = std::any_of(item.engines.cbegin(), item.engines.cend(), [this](const VersionRange &range) {
            const auto engine = m_engines.constFind(range.id);
            return engine != m_engines.cend() && isVersionInRange(*engine, range.min, range.below);
        });
        if (!works)
            return u"%1 needs an engine this system does not have."_s.arg(item.id);
    }
    return {};
}

// What stops a listing from replacing what is installed under its id: an id is one thing, and it only moves forward.
QString StoreEngine::refusalAgainstInstalled(const CatalogItem &item) const
{
    const auto installed = m_installed.constFind(item.id);
    if (installed == m_installed.cend())
        return {};
    // An old catalogue, replayed, could otherwise put back a version with a known fault.
    if (compareVersions(item.version, installed->version) < 0)
        return u"A newer version of this is already installed."_s;
    if (item.developer != installed->developer)
        return u"%1 is installed from %2, and this listing is from %3. Remove it first if that is what is meant."_s.arg(
            item.id, printable(installed->developer), printable(item.developer));
    if (item.kind != installed->kind)
        return u"%1 is installed as a %2, and this listing is a %3. Remove it first if that is what is meant."_s.arg(
            item.id, printable(installed->kind), printable(item.kind));
    return {};
}

// Whether a verified package is the one its listing names. Returns why not, or an empty string.
QString StoreEngine::mismatch(const CatalogItem &item, const PackageResult &package) const
{
    const PackageManifest &manifest = package.manifest;
    const bool same =
        manifest.id == item.id && manifest.version == item.version && manifest.kind == item.kind
        && manifest.developer == item.developer && manifest.format == item.format
        && (item.format != 2
            || (manifest.stores == item.storeNames() && manifest.edition == item.edition
                && manifest.licence == item.licence && samePlatforms(manifest.platforms, item.platforms)
                && sameRanges(manifest.engines, item.engines) && sameRanges(manifest.dependencies, item.dependencies)
                && package.unpacked == item.packageUnpacked));
    return same ? QString() : u"The package is not the one the catalogue lists."_s;
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
    const InstallPlan resolved = resolveDependencies(m_catalog, id, installedVersions());
    if (!resolved.ok) {
        fail(id, resolved.error);
        return false;
    }
    for (const QString &step : resolved.order) {
        if (isBusy(step))
            return false;
        if (const QString refusal = refusalToInstall(*m_catalog.item(step)); !refusal.isEmpty()) {
            fail(id, refusal);
            return false;
        }
    }
    // Every step holds a job from here, so none of them starts twice.
    for (const QString &step : resolved.order) {
        m_jobs.insert(step, nullptr);
        m_progress.insert(step, 0);
        Q_EMIT progressChanged(step);
    }
    auto plan = std::make_shared<Plan>();
    plan->root = id;
    plan->steps = resolved.order;
    plan->epoch = m_catalogEpoch;
    runNextStep(plan);
    return true;
}

void StoreEngine::runNextStep(const PlanPtr &plan)
{
    const QString id = plan->steps.at(plan->next);
    if (plan->epoch != m_catalogEpoch)
        return failPlan(plan, id, u"The catalogue changed while this was on its way, so nothing was installed."_s);
    const CatalogItem *item = m_catalog.item(id);
    if (!item)
        return failPlan(plan, id, u"The item is no longer in the catalogue."_s);
    const QUrl url = m_catalogUrl.resolved(QUrl(item->packageFile));
    // A format 2 catalogue says exactly how large the download is.
    const qint64 limit = item->format == 2 ? item->packageSize : item->packageSize + 1024;
    fetch(id, url, limit, [this, plan, id](bool ok, const QByteArray &data, const QString &error) {
        if (plan->over)
            return;
        if (!ok)
            return failPlan(plan, id, error);
        stepArrived(plan, id, data);
    });
}

// Whether a download is the package a listing says it is: an install's checks, and a licence's. Returns why not, or an
// empty string.
QString StoreEngine::refusePackage(const CatalogItem &item, const QByteArray &data, PackageResult *package) const
{
    if (QCryptographicHash::hash(data, QCryptographicHash::Sha256) != item.packageSha256)
        return u"The download does not match the catalogue."_s;
    const Developer developer = m_catalog.developers.value(item.developer);
    // A format 2 catalogue says what the package unpacks to, and it may not unpack to a byte more.
    *package =
        item.format == 2 ? openPackage(data, developer.key, item.packageUnpacked) : openPackage(data, developer.key);
    if (!package->ok)
        return package->error;
    return mismatch(item, *package);
}

// One package has arrived. It is verified and kept in memory, and nothing moves until they all have.
void StoreEngine::stepArrived(const PlanPtr &plan, const QString &id, const QByteArray &data)
{
    if (plan->epoch != m_catalogEpoch)
        return failPlan(plan, id, u"The catalogue changed while this was on its way, so nothing was installed."_s);
    const CatalogItem *item = m_catalog.item(id);
    if (!item)
        return failPlan(plan, id, u"The item is no longer in the catalogue."_s);
    PackageResult package;
    if (const QString why = refusePackage(*item, data, &package); !why.isEmpty())
        return failPlan(plan, id, why);
    if (const QString refusal = refusalAgainstInstalled(*item); !refusal.isEmpty())
        return failPlan(plan, id, refusal);
    // Plans started together wait together, so what they hold is bounded together.
    if (plan->bytes + package.unpacked > heldBytesLimit())
        return failPlan(plan, id, u"This install would take too much at once."_s);
    if (m_heldBytes + package.unpacked > heldBytesLimit())
        return failPlan(plan, id, kTooMuchOnItsWay.toString());
    plan->bytes += package.unpacked;
    m_heldBytes += package.unpacked;

    plan->verified.append(Step { id, std::move(package) });
    ++plan->next;
    if (plan->next < plan->steps.size())
        return runNextStep(plan);
    commitPlan(plan);
}

// Everything is here and verified. Under the lock, and against what is on disk now rather than what was when this
// began, everything moves or nothing does.
void StoreEngine::commitPlan(const PlanPtr &plan)
{
    const QString &root = plan->root;
    QLockFile lock(m_dataDir + u"/.lock"_s);
    // What a person's handler does on hearing of it may need this lock, so it is let go before anyone hears.
    const auto refuse = [&](const QString &id, const QString &error) {
        lock.unlock();
        failPlan(plan, id, error);
    };
    if (!takeLock(lock, kLockTimeoutMs))
        return refuse(root, u"The store is busy in another program."_s);
    if (!recoverLocked())
        return refuse(root, u"The store could not finish clearing up after an earlier install."_s);
    m_installed = readInstalled();

    QMap<QString, InstalledVersion> incoming;
    for (const Step &step : std::as_const(plan->verified)) {
        const CatalogItem *item = m_catalog.item(step.id);
        if (!item)
            return refuse(step.id, u"The item is no longer in the catalogue."_s);
        // The switch may have been turned off, or the catalogue run out, while this was on its way.
        if (const QString refusal = refusalToInstall(*item); !refusal.isEmpty())
            return refuse(step.id, refusal);
        if (const QString refusal = refusalAgainstInstalled(*item); !refusal.isEmpty())
            return refuse(step.id, refusal);
        incoming.insert(step.id, { item->version, item->dependencies });
    }
    // Something may have been removed, or moved, while this was on its way: by this program or another.
    if (const QString why = refusalOfOutcome(installedVersions(), incoming); !why.isEmpty())
        return refuse(root, why);

    // A record is there before any journal is, so that one found missing beside a journal was lost, and is not a first
    // install's that has not been written yet.
    if (!QFileInfo::exists(m_dataDir + u"/installed.json"_s) && !saveInstalled(m_installed, QString()))
        return refuse(root, u"The store could not write down what is installed."_s);

    // Write everything beside what it replaces.
    const QString items = m_dataDir + u"/items/"_s;
    const QString transaction = QUuid::createUuid().toString(QUuid::Id128);
    QList<Move> moves;
    const auto clearStaging = [&items, &moves] {
        for (const Move &move : std::as_const(moves))
            removeTree(items + move.staging);
    };
    for (const Step &step : std::as_const(plan->verified)) {
        const QString tag = QUuid::createUuid().toString(QUuid::Id128);
        const QString target = items + step.id;
        moves.append(
            Move { step.id, u".staging-"_s + tag, QFileInfo::exists(target) ? step.id + u".old-"_s + tag : QString() });
        QMap<QString, QByteArray> files = step.package.contents;
        // Kept beside them, so what runs can be checked each time it is loaded.
        files.insert(u"manifest.json"_s, step.package.manifestBytes);
        files.insert(u"manifest.sig"_s, step.package.signature);
        for (auto it = files.cbegin(); it != files.cend(); ++it) {
            const QString path = items + moves.constLast().staging + u'/' + it.key();
            QDir().mkpath(QFileInfo(path).absolutePath());
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(it.value()) != it.value().size() || !file.commit()) {
                clearStaging();
                return refuse(step.id, u"%1 could not be written."_s.arg(it.key()));
            }
        }
    }
    // What was written is on the disk before the journal says it is there, and the journal before anything moves.
    for (const Move &move : std::as_const(moves)) {
        QDirIterator folders(items + move.staging, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot,
                             QDirIterator::Subdirectories);
        while (folders.hasNext())
            syncFolder(folders.next());
        syncFolder(items + move.staging);
    }
    syncFolder(items);
    if (!writeJournal(transaction, moves)) {
        clearStaging();
        return refuse(root, u"The store could not write down what it was about to do."_s);
    }
    syncFolder(m_dataDir);
    // Put back what moved, as recovery would. Cut short, it leaves the journal for the next engine to finish with.
    const auto giveUp = [&](const QString &id, const QString &error) {
        if (undoMoves(moves)) {
            syncFolder(items);
            removeJournal();
        }
        refuse(id, error);
    };
    if (!reachedStage(CommitStage::JournalWritten))
        return giveUp(root, u"The package could not be installed."_s);

    for (const Move &move : std::as_const(moves)) {
        const QString target = items + move.id;
        if (!move.previous.isEmpty() && !QDir().rename(target, items + move.previous))
            return giveUp(move.id, u"The installed version is in use."_s);
        if (!reachedStage(CommitStage::ItemSetAside))
            return giveUp(move.id, u"The package could not be installed."_s);
        if (!QDir().rename(items + move.staging, target))
            return giveUp(move.id, u"The package could not be installed."_s);
        if (!reachedStage(CommitStage::ItemSwapped))
            return giveUp(move.id, u"The package could not be installed."_s);
    }

    syncFolder(items);

    QMap<QString, Installed> records = m_installed;
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    for (const Step &step : std::as_const(plan->verified)) {
        const CatalogItem *item = m_catalog.item(step.id);
        Installed installed { step.id, item->kind,         item->name, item->version, item->developer,
                              now,     item->dependencies, false,      std::nullopt };
        if (item->format == 2)
            installed.provenance = item->provenance;
        records.insert(step.id, installed);
    }
    // This write is the moment it happens. A crash before it puts everything back, and one after it finishes the job.
    if (!saveInstalled(records, transaction))
        return giveUp(root, u"The store could not write down what is installed."_s);
    syncFolder(m_dataDir);
    if (!reachedStage(CommitStage::RecordWritten))
        qCWarning(lcStore) << "An install went ahead past a stage asked to stop it, since it had already happened.";

    for (const Move &move : std::as_const(moves)) {
        // An old version goes only once what replaced it is whole, as when clearing up: it may be the only good copy.
        // What is left of one that cannot all be deleted yet goes when an engine next clears up, since its id is whole.
        if (!move.previous.isEmpty() && holdsWhatItsManifestLists(items + move.id))
            removeTree(items + move.previous);
        reachedStage(CommitStage::OldRemoved);
    }
    removeJournal();
    lock.unlock();

    m_installed = records;
    releaseHeld(plan);
    plan->over = true;
    for (const QString &step : std::as_const(plan->steps)) {
        m_jobs.remove(step);
        m_progress.remove(step);
    }
    for (const Step &step : std::as_const(plan->verified))
        qCInfo(lcStore) << "Installed" << step.id << step.package.manifest.version;
    Q_EMIT installedChanged();
    for (const QString &step : std::as_const(plan->steps)) {
        Q_EMIT progressChanged(step);
        Q_EMIT finished(step, true, {});
    }
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

bool StoreEngine::hasUpdate(const QString &id) const
{
    const auto installed = m_installed.constFind(id);
    const CatalogItem *item = m_catalog.item(id);
    return installed != m_installed.cend() && item && compareVersions(installed->version, item->version) < 0
           && refusalToInstall(*item).isEmpty() && refusalAgainstInstalled(*item).isEmpty();
}

bool StoreEngine::updateWaitsForCreatorFeatures(const QString &id) const
{
    const auto installed = m_installed.constFind(id);
    const CatalogItem *item = m_catalog.item(id);
    return installed != m_installed.cend() && item && !m_creatorFeatures && !item->isPlacedIn(View::Player)
           && compareVersions(installed->version, item->version) < 0 && refusalToInstall(*item, true).isEmpty()
           && refusalAgainstInstalled(*item).isEmpty();
}

int StoreEngine::updateCount() const
{
    int count = 0;
    for (const Installed &installed : m_installed)
        count += hasUpdate(installed.id) ? 1 : 0;
    return count;
}

void StoreEngine::fail(const QString &id, const QString &error)
{
    qCWarning(lcStore) << "Install of" << id << "failed:" << error;
    m_jobs.remove(id);
    m_progress.remove(id);
    Q_EMIT progressChanged(id);
    Q_EMIT finished(id, false, error);
}

// Nothing has moved, or what moved has gone back: an end for every step of it, the step that failed first.
void StoreEngine::failPlan(const PlanPtr &plan, const QString &id, const QString &error)
{
    plan->over = true;
    plan->verified.clear();
    releaseHeld(plan);
    for (const QString &step : std::as_const(plan->steps))
        m_jobs.remove(step);
    fail(id, error);
    for (const QString &step : std::as_const(plan->steps))
        if (step != id)
            fail(step, u"%1 could not be installed: %2"_s.arg(id, error));
}

void StoreEngine::releaseHeld(const PlanPtr &plan)
{
    m_heldBytes -= plan->bytes;
    plan->bytes = 0;
}

bool StoreEngine::remove(const QString &id)
{
    if (isBusy(id))
        return false;
    QLockFile lock(m_dataDir + u"/.lock"_s);
    // What a person's handler does on hearing of it may need this lock, so it is let go before anyone hears.
    const auto refuse = [&](const QString &error) {
        lock.unlock();
        fail(id, error);
        return false;
    };
    if (!takeLock(lock, kLockTimeoutMs))
        return refuse(u"The store is busy in another program."_s);
    if (!recoverLocked())
        return refuse(u"The store could not finish clearing up after an earlier install."_s);
    m_installed = readInstalled();
    const auto record = m_installed.constFind(id);
    if (record == m_installed.cend())
        return false;
    for (const Installed &other : std::as_const(m_installed))
        for (const VersionRange &range : other.dependencies)
            // A range that does not hold now is not kept holding by leaving this where it is.
            if (range.id == id && isVersionInRange(record->version, range.min, range.below))
                return refuse(u"%1 needs this, so it stays."_s.arg(other.id));

    const QString transaction = QUuid::createUuid().toString(QUuid::Id128);
    const QString items = m_dataDir + u"/items/"_s;
    const QList<Move> moves { Move { id, QString(), id + u".old-"_s + transaction } };
    QMap<QString, Installed> records = m_installed;
    records.remove(id);
    if (!writeJournal(transaction, moves))
        return refuse(u"The store could not write down what it was about to do."_s);
    syncFolder(m_dataDir);
    if (!QDir().rename(items + id, items + moves.constFirst().previous)) {
        removeJournal();
        return refuse(u"It is in use."_s);
    }
    syncFolder(items);
    if (!saveInstalled(records, transaction)) {
        if (undoMoves(moves)) {
            syncFolder(items);
            removeJournal();
        }
        return refuse(u"The store could not write down what is installed."_s);
    }
    syncFolder(m_dataDir);
    // What cannot all be deleted yet keeps its journal, so the next engine finishes deleting it, rather than taking
    // what is left for an old version to put back.
    if (removeTree(items + moves.constFirst().previous))
        removeJournal();
    lock.unlock();

    m_installed = records;
    qCInfo(lcStore) << "Removed" << id;
    Q_EMIT installedChanged();
    return true;
}

bool StoreEngine::fetchPreview(const QString &id, int index)
{
    const CatalogItem *item = m_catalog.item(id);
    if (!item || index < 0 || index >= item->previews.size() || catalogueHasExpired())
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

bool StoreEngine::fetchLicence(const QString &id)
{
    const CatalogItem *item = m_catalog.item(id);
    if (!item || item->format != 2 || catalogueHasExpired() || (!item->isPlacedIn(View::Player) && !m_creatorFeatures))
        return false;
    // Asked again while it is on its way: it is already coming.
    if (m_licences.contains(id))
        return true;
    const quint64 asking = ++m_licenceAskings;
    m_licences.insert(id, LicenceFetch { {}, asking, m_catalogEpoch });
    // The whole package comes, so it is counted as an install's would be.
    QNetworkReply *reply = fetch(
        {}, m_catalogUrl.resolved(QUrl(item->packageFile)), item->packageSize,
        [this, id, asking](bool ok, const QByteArray &data, const QString &error) {
            // Let go of with the engine: nothing more to say.
            const auto found = m_licences.constFind(id);
            if (found == m_licences.cend() || found->asking != asking)
                return;
            const quint64 epoch = found->epoch;
            m_licences.remove(id);
            QString text;
            const QString why = epoch != m_catalogEpoch ? u"The catalogue changed while this was on its way."_s
                                                        : licenceIn(id, ok, data, error, &text);
            Q_EMIT licenceFetched(id, why.isEmpty(), text, why);
        },
        true);
    if (const auto found = m_licences.find(id); found != m_licences.end() && found->asking == asking)
        found->reply = reply;
    return true;
}

// Stops the downloads of the licences on their way that `which` names. Aborting finishes each at once, and its answer
// then says why, as one that arrived would. One read from a local file is not stopped, and says the same as it arrives.
void StoreEngine::abortLicences(const std::function<bool(const QString &id)> &which)
{
    QList<QPointer<QNetworkReply>> replies;
    for (auto it = m_licences.cbegin(); it != m_licences.cend(); ++it)
        if (it->reply && which(it.key()))
            replies.append(it->reply);
    for (const QPointer<QNetworkReply> &reply : std::as_const(replies))
        if (reply)
            reply->abort();
}

// The licence's text in a download, once it is shown to be the package this catalogue lists: `arrived`, or `error`
// says why not. Returns why not, or an empty string. The file rules have already said it is UTF-8 with no control
// characters or markup.
QString StoreEngine::licenceIn(const QString &id, bool arrived, const QByteArray &data, const QString &error,
                               QString *text) const
{
    const CatalogItem *item = m_catalog.item(id);
    if (!item)
        return u"The item is no longer in the catalogue."_s;
    // The switch may have been turned off while this was on its way, which also stops a download.
    if (!item->isPlacedIn(View::Player) && !m_creatorFeatures)
        return u"%1 is in the creator store, and creator features are off."_s.arg(item->id);
    if (!arrived)
        return error;
    PackageResult package;
    if (const QString refusal = refusePackage(*item, data, &package); !refusal.isEmpty())
        return refusal;
    const auto licence = package.contents.constFind(item->licence.file);
    if (licence == package.contents.cend())
        return u"The package does not hold its licence."_s;
    if (licence->size() > kMaximumLicenceSize)
        return u"This licence is too long to show here. It comes with the package, as %1."_s.arg(item->licence.file);
    *text = QString::fromUtf8(*licence);
    return {};
}

// ── The journal, and what a crash leaves ───────────────────────────────────

bool StoreEngine::writeJournal(const QString &transaction, const QList<Move> &moves) const
{
    QJsonArray steps;
    for (const Move &move : moves)
        steps.append(
            QJsonObject { { u"id"_s, move.id }, { u"staging"_s, move.staging }, { u"previous"_s, move.previous } });
    QSaveFile file(journalPath(m_dataDir));
    return file.open(QIODevice::WriteOnly)
           && file.write(
                  QJsonDocument(QJsonObject { { u"transaction"_s, transaction }, { u"moves"_s, steps } }).toJson())
                  >= 0
           && file.commit();
}

void StoreEngine::removeJournal() const { QFile::remove(journalPath(m_dataDir)); }

// Puts back whatever of `moves` happened, last first. A move has two renames: what was installed goes aside, then
// what is new takes its place. This may be cut short at any point and run again over the same journal, by a crash or
// a journal that could not be removed, so it decides from what is on disk and never from an absence it may have made
// itself. The old version set aside is what it looks for: while that is there, whatever stands under the id came in
// with this transaction and goes, then the old version goes back. Once it is back, the id is not touched again. Where
// nothing stood under the id before, whatever stands there now came in with this transaction. False if any step did
// not happen, as when a file being scanned cannot be deleted yet, or if a test cut it short: then the journal stays,
// and the next try starts again from what is on disk.
bool StoreEngine::undoMoves(const QList<Move> &moves)
{
    const QString items = m_dataDir + u"/items/"_s;
    for (auto move = moves.crbegin(); move != moves.crend(); ++move) {
        const QString target = items + move->id;
        if (!move->staging.isEmpty() && present(items + move->staging)) {
            if (!removeTree(items + move->staging) || !reachedStage(CommitStage::RecoveryStep))
                return false;
        }
        if (!move->previous.isEmpty()) {
            if (!present(items + move->previous))
                continue; // never set aside, or already back
            if (present(target) && (!removeTree(target) || !reachedStage(CommitStage::RecoveryStep)))
                return false;
            if (!QDir().rename(items + move->previous, target) || !reachedStage(CommitStage::RecoveryStep))
                return false;
        } else if (!move->staging.isEmpty() && present(target)) {
            if (!removeTree(target) || !reachedStage(CommitStage::RecoveryStep))
                return false;
        }
    }
    return true;
}

// Whether a move of a plan got all the way: an install's new version in place and whole, from a staging folder that is
// gone, or a removal's item gone.
bool StoreEngine::hasLanded(const Move &move) const
{
    const QString items = m_dataDir + u"/items/"_s;
    if (move.staging.isEmpty())
        return !present(items + move.id);
    return !present(items + move.staging) && holdsWhatItsManifestLists(items + move.id);
}

// A crash during an install or a removal. The journal says what was being done and installed.json says whether it
// happened, because that is the last thing written. If it did, this finishes clearing up. If not, everything goes back.
// Either may run again over the same journal and comes to the same end. After that, anything of the older kind of
// leftover is cleared: a staging folder goes, and an old version set aside goes back if nothing replaced it. The caller
// holds the store's lock, so none of this is another program's install under way. False if it was cut short, which only
// a test does, and then the journal stays for the next engine.
bool StoreEngine::recoverLocked()
{
    const QString items = m_dataDir + u"/items/"_s;
    QFile journal(journalPath(m_dataDir));
    if (journal.open(QIODevice::ReadOnly)) {
        const QJsonObject object = QJsonDocument::fromJson(journal.readAll()).object();
        journal.close();
        QList<Move> moves;
        static const QRegularExpression staging(u"^\\.staging-[0-9a-f]{32}\\z"_s);
        static const QRegularExpression aside(u"^\\.old-[0-9a-f]{32}\\z"_s);
        for (const QJsonValue &value : object.value(u"moves"_s).toArray()) {
            const QJsonObject entry = value.toObject();
            const Move move { entry.value(u"id"_s).toString(), entry.value(u"staging"_s).toString(),
                              entry.value(u"previous"_s).toString() };
            // Names only ever inside items/, whatever the file says: an id, and the id set aside, exactly.
            if (isValidItemId(move.id) && (move.staging.isEmpty() || staging.match(move.staging).hasMatch())
                && (move.previous.isEmpty()
                    || (move.previous.startsWith(move.id)
                        && aside.match(move.previous.mid(move.id.size())).hasMatch())))
                moves.append(move);
        }
        QString transaction;
        bool recordRead = false;
        const QMap<QString, Installed> records = readInstalled(&transaction, &recordRead);
        Q_UNUSED(records);
        // With no record to say whether it happened, what is on disk says: a plan whose every move got all the way
        // stands, rather than a first install that may have been recorded being deleted for a record that is lost.
        const bool happened =
            recordRead
                ? !transaction.isEmpty() && transaction == object.value(u"transaction"_s).toString()
                : std::all_of(moves.cbegin(), moves.cend(), [this](const Move &move) { return hasLanded(move); });
        if (happened) {
            for (const Move &move : std::as_const(moves)) {
                if (move.previous.isEmpty() || !present(items + move.previous))
                    continue;
                // An old version goes only once what replaced it is whole. If not, it stays, as the sweep below
                // leaves it.
                if (!move.staging.isEmpty() && !holdsWhatItsManifestLists(items + move.id))
                    continue;
                if (!removeTree(items + move.previous) || !reachedStage(CommitStage::RecoveryStep))
                    return false;
            }
        } else if (!undoMoves(moves)) {
            return false;
        }
        syncFolder(items);
        removeJournal();
    }

    const QDir folder(items);
    static const QRegularExpression old(u"^(.+)\\.old-[0-9a-f]{32}\\z"_s);
    const QStringList names = folder.entryList(QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QString &name : names) {
        if (name.startsWith(u".staging-"_s)) {
            removeTree(folder.filePath(name));
            continue;
        }
        const QRegularExpressionMatch match = old.match(name);
        if (!match.hasMatch())
            continue;
        const QString id = match.captured(1);
        if (isValidItemId(id) && !present(folder.filePath(id)))
            QDir().rename(folder.filePath(name), folder.filePath(id));
        // An old version set aside is let go only when what stands under its id is whole: it may be the only good copy.
        else if (!isValidItemId(id) || holdsWhatItsManifestLists(folder.filePath(id)))
            removeTree(folder.filePath(name));
        else
            qCWarning(lcStore) << "An old version of" << id << "is kept, since what stands in its place is not whole.";
    }
    return true;
}

// If another program holds the store's lock, this leaves everything alone: that is an install under way, not an
// abandoned one.
void StoreEngine::recoverInterrupted()
{
    const QDir folder(m_dataDir + u"/items"_s);
    static const QRegularExpression old(u"\\.old-[0-9a-f]{32}\\z"_s);
    const QStringList names = folder.entryList(QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    const auto isLeftOver = [](const QString &name) {
        return name.startsWith(u".staging-"_s) || old.match(name).hasMatch();
    };
    if (!QFileInfo::exists(journalPath(m_dataDir)) && std::none_of(names.cbegin(), names.cend(), isLeftOver))
        return;
    QLockFile lock(m_dataDir + u"/.lock"_s);
    if (takeLock(lock, 0))
        recoverLocked();
}

// What is installed, read from the folders themselves: each manifest's claims, unchecked. Only for when the record
// cannot be read or is gone, so that an id still keeps its version, developer and kind against a catalogue that would
// move it back or hand it to someone else. A same-user process could write a manifest here, as it could the record.
// The provenance an entry of installed.json keeps, if it says it is a listing's and holds what a listing would: the
// four fields, each text no longer than a catalogue's. Anything else is not believed, and the entry has none.
static std::optional<Provenance> provenanceIn(const QJsonObject &entry)
{
    const QJsonObject provenance = entry.value(u"provenance"_s).toObject();
    if (entry.value(u"provenanceFrom"_s).toString() != u"listing"_s || provenance.size() != 4)
        return std::nullopt;
    QStringList fields;
    for (const QString &key : { u"origin"_s, u"author"_s, u"source"_s, u"permission"_s }) {
        const QJsonValue value = provenance.value(key);
        if (!value.isString() || value.toString().size() > kMaximumCatalogText)
            return std::nullopt;
        fields.append(value.toString());
    }
    return Provenance { fields.at(0), fields.at(1), fields.at(2), fields.at(3) };
}

QMap<QString, StoreEngine::Installed> StoreEngine::installedFromFolders() const
{
    QMap<QString, Installed> records;
    const QDir folder(m_dataDir + u"/items"_s);
    const QStringList names = folder.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &name : names) {
        PackageManifest manifest;
        if (!isValidItemId(name) || !readInstalledManifest(folder.filePath(name), &manifest) || manifest.id != name)
            continue;
        records.insert(name, { name, manifest.kind, manifest.name, manifest.version, manifest.developer,
                               QFileInfo(folder.filePath(name)).lastModified(QTimeZone::UTC).toString(Qt::ISODate),
                               manifest.dependencies, true, std::nullopt });
    }
    return records;
}

// The records on disk now. What cannot be read is set aside and not believed. What the folders hold stands in for it,
// as it does when there is no record at all: nothing it listed is lost, since a reinstall replaces its folder, and
// nothing else is kept from working by it. `recordRead` says whether there was a record to read.
QMap<QString, StoreEngine::Installed> StoreEngine::readInstalled(QString *transaction, bool *recordRead) const
{
    if (recordRead)
        *recordRead = false;
    QMap<QString, Installed> records;
    const QString path = m_dataDir + u"/installed.json"_s;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return installedFromFolders();
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!document.isObject()) {
        qCWarning(lcStore) << "The store's record of what is installed cannot be read. It is set aside, and what the "
                              "folders hold stands in for it.";
        const QString aside = path + u".unreadable"_s;
        QFile::remove(aside);
        QFile::rename(path, aside);
        return installedFromFolders();
    }
    if (recordRead)
        *recordRead = true;
    if (transaction)
        *transaction = document.object().value(u"transaction"_s).toString();
    const QJsonObject items = document.object().value(u"items"_s).toObject();
    for (auto it = items.begin(); it != items.end(); ++it) {
        const QJsonObject entry = it.value().toObject();
        Installed installed { it.key(),
                              entry.value(u"kind"_s).toString(),
                              entry.value(u"name"_s).toString(),
                              entry.value(u"version"_s).toString(),
                              entry.value(u"developer"_s).toString(),
                              entry.value(u"installedAt"_s).toString(),
                              {},
                              entry.value(u"claimed"_s).toBool(),
                              provenanceIn(entry) };
        if (entry.contains(u"dependencies"_s)
            && !parseDependencies(entry.value(u"dependencies"_s), &installed.dependencies).isEmpty())
            continue;
        // Only what is actually on disk.
        if (isValidItemId(installed.id) && QFileInfo(installDirectory(installed.id)).isDir())
            records.insert(installed.id, installed);
    }
    return records;
}

bool StoreEngine::saveInstalled(const QMap<QString, Installed> &records, const QString &transaction) const
{
    QJsonObject items;
    for (const Installed &installed : records) {
        QJsonObject entry { { u"kind"_s, installed.kind },
                            { u"name"_s, installed.name },
                            { u"version"_s, installed.version },
                            { u"developer"_s, installed.developer },
                            { u"installedAt"_s, installed.installedAt } };
        if (!installed.dependencies.isEmpty())
            entry.insert(u"dependencies"_s, rangesToJson(installed.dependencies));
        // What a manifest said of itself when the record was lost, and no install has said since.
        if (installed.claimed)
            entry.insert(u"claimed"_s, true);
        // The listing's word on where it came from. A rebuilt record says it has none; format 1 says nothing.
        if (installed.provenance) {
            entry.insert(u"provenance"_s, QJsonObject { { u"origin"_s, installed.provenance->origin },
                                                        { u"author"_s, installed.provenance->author },
                                                        { u"source"_s, installed.provenance->source },
                                                        { u"permission"_s, installed.provenance->permission } });
            entry.insert(u"provenanceFrom"_s, u"listing"_s);
        } else if (installed.claimed) {
            entry.insert(u"provenanceFrom"_s, u"none"_s);
        }
        items.insert(installed.id, entry);
    }
    QSaveFile file(m_dataDir + u"/installed.json"_s);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const QJsonObject root { { u"format"_s, 1 }, { u"transaction"_s, transaction }, { u"items"_s, items } };
    return file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0 && file.commit();
}

void StoreEngine::syncFolder(const QString &path) const
{
#ifdef Q_OS_UNIX
    const int folder = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (folder < 0)
        return;
    if (::fsync(folder) != 0)
        qCWarning(lcStore) << "A folder of the store's could not be made durable:" << path;
    ::close(folder);
#else
    Q_UNUSED(path);
#endif
}

qint64 StoreEngine::heldBytesLimit() const { return kMaximumHeldBytes; }

} // namespace qor::store
