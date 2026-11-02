// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "storeengine.h"

#include "filerules.h"

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
#include <QtCore/QSet>
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
// A plan's packages are held in memory until all of them are verified, so what one plan may come to is bounded.
constexpr qint64 kMaximumPlanBytes = 128 * 1024 * 1024;
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
    if (!lock.tryLock(kLockTimeoutMs))
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

// One package has arrived. It is verified and kept in memory, and nothing moves until they all have.
void StoreEngine::stepArrived(const PlanPtr &plan, const QString &id, const QByteArray &data)
{
    if (plan->epoch != m_catalogEpoch)
        return failPlan(plan, id, u"The catalogue changed while this was on its way, so nothing was installed."_s);
    const CatalogItem *item = m_catalog.item(id);
    if (!item)
        return failPlan(plan, id, u"The item is no longer in the catalogue."_s);
    if (QCryptographicHash::hash(data, QCryptographicHash::Sha256) != item->packageSha256)
        return failPlan(plan, id, u"The download does not match the catalogue."_s);
    const Developer developer = m_catalog.developers.value(item->developer);
    // A format 2 catalogue says what the package unpacks to, and it may not unpack to a byte more.
    PackageResult package =
        item->format == 2 ? openPackage(data, developer.key, item->packageUnpacked) : openPackage(data, developer.key);
    if (!package.ok)
        return failPlan(plan, id, package.error);
    if (const QString why = mismatch(*item, package); !why.isEmpty())
        return failPlan(plan, id, why);
    if (const QString refusal = refusalAgainstInstalled(*item); !refusal.isEmpty())
        return failPlan(plan, id, refusal);
    plan->bytes += package.unpacked;
    if (plan->bytes > kMaximumPlanBytes)
        return failPlan(plan, id, u"This install would take too much at once."_s);

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
    if (!lock.tryLock(kLockTimeoutMs))
        return refuse(root, u"The store is busy in another program."_s);
    recoverLocked();
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

    // Write everything beside what it replaces.
    const QString items = m_dataDir + u"/items/"_s;
    const QString transaction = QUuid::createUuid().toString(QUuid::Id128);
    QList<Move> moves;
    const auto clearStaging = [&items, &moves] {
        for (const Move &move : std::as_const(moves))
            QDir(items + move.staging).removeRecursively();
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
    if (!writeJournal(transaction, moves)) {
        clearStaging();
        return refuse(root, u"The store could not write down what it was about to do."_s);
    }
    const auto giveUp = [&](const QString &id, const QString &error) {
        undoMoves(moves);
        removeJournal();
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

    QMap<QString, Installed> records = m_installed;
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    for (const Step &step : std::as_const(plan->verified)) {
        const CatalogItem *item = m_catalog.item(step.id);
        records.insert(step.id,
                       { step.id, item->kind, item->name, item->version, item->developer, now, item->dependencies });
    }
    // This write is the moment it happens. A crash before it puts everything back, and one after it finishes the job.
    if (!saveInstalled(records, transaction))
        return giveUp(root, u"The store could not write down what is installed."_s);
    if (!reachedStage(CommitStage::RecordWritten))
        qCWarning(lcStore) << "An install went ahead past a stage asked to stop it, since it had already happened.";

    for (const Move &move : std::as_const(moves)) {
        if (!move.previous.isEmpty())
            QDir(items + move.previous).removeRecursively();
        reachedStage(CommitStage::OldRemoved);
    }
    removeJournal();
    lock.unlock();

    m_installed = records;
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
    for (const QString &step : std::as_const(plan->steps))
        m_jobs.remove(step);
    fail(id, error);
    for (const QString &step : std::as_const(plan->steps))
        if (step != id)
            fail(step, u"%1 could not be installed: %2"_s.arg(id, error));
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
    if (!lock.tryLock(kLockTimeoutMs))
        return refuse(u"The store is busy in another program."_s);
    recoverLocked();
    m_installed = readInstalled();
    const auto present = m_installed.constFind(id);
    if (present == m_installed.cend())
        return false;
    for (const Installed &other : std::as_const(m_installed))
        for (const VersionRange &range : other.dependencies)
            // A range that does not hold now is not kept holding by leaving this where it is.
            if (range.id == id && isVersionInRange(present->version, range.min, range.below))
                return refuse(u"%1 needs this, so it stays."_s.arg(other.id));

    const QString transaction = QUuid::createUuid().toString(QUuid::Id128);
    const QString items = m_dataDir + u"/items/"_s;
    const QList<Move> moves { Move { id, QString(), id + u".old-"_s + transaction } };
    QMap<QString, Installed> records = m_installed;
    records.remove(id);
    if (!writeJournal(transaction, moves))
        return refuse(u"The store could not write down what it was about to do."_s);
    if (!QDir().rename(items + id, items + moves.constFirst().previous)) {
        removeJournal();
        return refuse(u"It is in use."_s);
    }
    if (!saveInstalled(records, transaction)) {
        undoMoves(moves);
        removeJournal();
        return refuse(u"The store could not write down what is installed."_s);
    }
    QDir(items + moves.constFirst().previous).removeRecursively();
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

// Puts back whatever of `moves` happened, last first, from what is on disk. A move has two renames: what was installed
// goes aside, then what is new takes its place. A staging folder that is still there tells which of those had happened.
void StoreEngine::undoMoves(const QList<Move> &moves) const
{
    const QString items = m_dataDir + u"/items/"_s;
    for (auto move = moves.crbegin(); move != moves.crend(); ++move) {
        const QString target = items + move->id;
        const bool newLanded = move->staging.isEmpty() || !QFileInfo::exists(items + move->staging);
        const bool setAside = !move->previous.isEmpty() && QFileInfo::exists(items + move->previous);
        if (!move->staging.isEmpty())
            QDir(items + move->staging).removeRecursively();
        if (newLanded && !move->staging.isEmpty() && QFileInfo::exists(target))
            QDir(target).removeRecursively();
        if (setAside)
            QDir().rename(items + move->previous, target);
    }
}

// A crash during an install or a removal. The journal says what was being done and installed.json says whether it
// happened, because that is the last thing written. If it did, this finishes clearing up. If not, everything goes back.
// After that, anything of the older kind of leftover is cleared: a staging folder goes, and an old version set aside
// goes back if nothing replaced it. The caller holds the store's lock, so none of this is another program's install
// under way.
void StoreEngine::recoverLocked() const
{
    const QString items = m_dataDir + u"/items/"_s;
    QFile journal(journalPath(m_dataDir));
    if (journal.open(QIODevice::ReadOnly)) {
        const QJsonObject object = QJsonDocument::fromJson(journal.readAll()).object();
        journal.close();
        QList<Move> moves;
        static const QRegularExpression staging(u"^\\.staging-[0-9a-f]{32}\\z"_s);
        static const QRegularExpression previous(u"\\.old-[0-9a-f]{32}\\z"_s);
        for (const QJsonValue &value : object.value(u"moves"_s).toArray()) {
            const QJsonObject entry = value.toObject();
            const Move move { entry.value(u"id"_s).toString(), entry.value(u"staging"_s).toString(),
                              entry.value(u"previous"_s).toString() };
            // Names only ever inside items/, whatever the file says.
            if (isValidItemId(move.id) && (move.staging.isEmpty() || staging.match(move.staging).hasMatch())
                && (move.previous.isEmpty()
                    || (move.previous.startsWith(move.id) && previous.match(move.previous).hasMatch())))
                moves.append(move);
        }
        QString transaction;
        const QMap<QString, Installed> records = readInstalled(&transaction);
        Q_UNUSED(records);
        if (!transaction.isEmpty() && transaction == object.value(u"transaction"_s).toString()) {
            for (const Move &move : std::as_const(moves))
                if (!move.previous.isEmpty())
                    QDir(items + move.previous).removeRecursively();
        } else {
            undoMoves(moves);
        }
        removeJournal();
    }

    const QDir folder(items);
    static const QRegularExpression old(u"^(.+)\\.old-[0-9a-f]{32}\\z"_s);
    const QStringList names = folder.entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
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

// If another program holds the store's lock, this leaves everything alone: that is an install under way, not an
// abandoned one.
void StoreEngine::recoverInterrupted() const
{
    const QDir folder(m_dataDir + u"/items"_s);
    static const QRegularExpression old(u"\\.old-[0-9a-f]{32}\\z"_s);
    const QStringList names = folder.entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
    const auto isLeftOver = [](const QString &name) {
        return name.startsWith(u".staging-"_s) || old.match(name).hasMatch();
    };
    if (!QFileInfo::exists(journalPath(m_dataDir)) && std::none_of(names.cbegin(), names.cend(), isLeftOver))
        return;
    QLockFile lock(m_dataDir + u"/.lock"_s);
    if (lock.tryLock(0))
        recoverLocked();
}

// The records on disk now. What cannot be read is set aside and not believed: nothing it listed is lost, since a
// reinstall replaces its folder, and nothing else is kept from working by it.
QMap<QString, StoreEngine::Installed> StoreEngine::readInstalled(QString *transaction) const
{
    QMap<QString, Installed> records;
    const QString path = m_dataDir + u"/installed.json"_s;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return records;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!document.isObject()) {
        qCWarning(lcStore) << "The store's record of what is installed cannot be read, and is set aside.";
        const QString aside = path + u".unreadable"_s;
        QFile::remove(aside);
        QFile::rename(path, aside);
        return records;
    }
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
                              {} };
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
        items.insert(installed.id, entry);
    }
    QSaveFile file(m_dataDir + u"/installed.json"_s);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const QJsonObject root { { u"format"_s, 1 }, { u"transaction"_s, transaction }, { u"items"_s, items } };
    return file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0 && file.commit();
}

} // namespace qor::store
