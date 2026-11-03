// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once

#include "catalog.h"
#include "package.h"

#include <QtCore/QHash>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtCore/QUrl>

#include <functional>
#include <memory>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
QT_END_NAMESPACE

namespace qor::store {

// The QOR OS Store client for one user: fetches the signed catalogue, and
// downloads, verifies, installs, updates and removes packages under
// `dataDir` (items/<id>/ and installed.json).
//
// Nothing is installed unless the catalogue carries the store's signature,
// the package matches the digest the catalogue lists for it, the package's
// manifest carries its developer's signature, and every file matches the
// manifest.
//
// An install is all or nothing as seen from outside. Everything it needs is downloaded and verified first, and only
// then does anything move, under a lock and against what is on disk at that moment, since another program of this
// person's may share the folder. Writing installed.json is the moment it happens: a crash before that puts everything
// back, and a crash after it finishes the job, the next time an engine starts.
//
// There is one engine and one install folder, whichever store an item was found in: one id, one install, so a utility
// placed in both storefronts is one package. What keeps the stores apart is not any interface hiding one (QOR-0021):
// every query names its view and answers from that view alone, and install refuses what only the creator store carries
// unless the engine has been told creator features are on. For format 2 it remembers the highest sequence it has seen,
// for each store key, so another store's catalogue in the same folder starts its own memory and locks out nothing.
class StoreEngine : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, Loading, Ready, Failed };

    struct Installed
    {
        QString id;
        QString kind;
        QString name;
        QString version;
        QString developer;
        QString installedAt;
        QList<VersionRange> dependencies;
        // Taken from the installed manifest's own word, unsigned, when the record was lost: kept as an id's pins, and
        // marked as such in installed.json until the item is installed again.
        bool claimed = false;
    };

    StoreEngine(const QString &dataDir, const QByteArray &storeKey, QObject *parent = nullptr);
    ~StoreEngine() override;

    QUrl catalogUrl() const { return m_catalogUrl; }
    void setCatalogUrl(const QUrl &url);
    // Fetches the catalogue (catalog.json and its signature, catalog.json.sig).
    void refresh();

    State state() const { return m_state; }
    QString error() const { return m_error; }
    const Catalog &catalog() const { return m_catalog; }
    const QMap<QString, Installed> &installed() const { return m_installed; }
    QString dataDir() const { return m_dataDir; }
    QString installDirectory(const QString &id) const;
    // The highest sequence this store has taken under its key, in its own folder. Zero until one has.
    qint64 lastSequence() const { return m_lastSequence; }

    // Off until the engine is told otherwise. It is a person's preference, kept elsewhere and given here. Off, the
    // creator view is empty and install refuses what only the creator store carries; and turning it off removes
    // nothing: what is installed stays installed, intact and usable.
    bool creatorFeatures() const { return m_creatorFeatures; }
    void setCreatorFeatures(bool on);

    // The engines this system has, id → version, where it knows them. Then a listing that works with none of them is
    // refused. Told nothing, the engine cannot make that check and does not guess: `engines` is not enforced.
    void setEngines(const QMap<QString, QString> &engines);

    // What a storefront shows, always of a view named by the caller. There is no default, and no fallback: an empty
    // answer stays empty, and is never made up from the other store. A catalogue that has expired since it arrived
    // answers nothing.
    QList<CatalogItem> listings(View view) const;
    QList<CatalogItem> featured(View view) const;
    QList<CatalogItem> search(View view, const QString &query) const;
    QMap<QString, int> categoryCounts(View view) const;

    // Whether what is installed for `id` is still exactly what its developer signed, and the very package installed:
    // an older copy, signed in its day, does not pass. False until the catalogue that names the developer's key is
    // here.
    bool isIntact(const QString &id) const;

    // Whether what is installed for `id` has a newer version that install would take now: with creator features off,
    // the update of what only the creator store carries waits, and one that has expired or is not for this system is
    // none.
    bool hasUpdate(const QString &id) const;
    int updateCount() const;

    bool isBusy(const QString &id) const { return m_jobs.contains(id); }
    double progress(const QString &id) const { return m_progress.value(id, 0); }

    // Installs the catalogue's version of `id` (an update when an older one is installed), with whatever it depends on,
    // all of it or none of it. Ends with finished() for each. False if it did not start; a refusal says why through
    // finished().
    bool install(const QString &id);
    // Refused, with finished(), while something installed needs it.
    bool remove(const QString &id);

    // Fetches one of an item's previews, checks it against the catalogue's digest and that it starts as a picture, and
    // ends with previewFetched(). False if the item has no such preview, if only the creator store carries it and
    // creator features are off, or if its catalogue has expired.
    bool fetchPreview(const QString &id, int index);

    // A licence's text is only in the package. This fetches the item's package, checks it as an install would, installs
    // nothing, and ends with licenceFetched(). False if the item has no licence, or only the creator store carries it
    // and creator features are off.
    bool fetchLicence(const QString &id);

Q_SIGNALS:
    void stateChanged();
    void catalogChanged();
    void creatorFeaturesChanged();
    void installedChanged();
    void progressChanged(const QString &id);
    void finished(const QString &id, bool ok, const QString &error);
    void previewFetched(const QString &id, int index, bool ok, const QByteArray &data, const QString &error);
    void licenceFetched(const QString &id, bool ok, const QString &text, const QString &error);

protected:
    // Takes a catalogue that has passed every check. A plan under way against another catalogue stops.
    void adoptCatalog(Catalog catalog);
    // Whether this stage installs a kind. A later stage changes isInstallableKind and gives the kind its file rules; a
    // test overrides this to prove what does not depend on the stage, such as one install for two listings.
    virtual bool takesKind(const QString &kind) const { return isInstallableKind(kind); }

    // Where an install has got to as it moves things, and where putting back or clearing up has got to, one step on
    // disk at a time. Nothing is done with these here. A test overrides this to look at what a crash at that moment
    // would leave, and returning false stands in for a step failing there, or for recovery being cut short as a death
    // would cut it.
    enum class CommitStage { JournalWritten, ItemSetAside, ItemSwapped, RecordWritten, OldRemoved, RecoveryStep };
    virtual bool reachedStage(CommitStage stage)
    {
        Q_UNUSED(stage);
        return true;
    }
    // Writes installed.json, atomically. Virtual so that a test can stand in for a disk that refuses the write.
    virtual bool saveInstalled(const QMap<QString, Installed> &records, const QString &transaction) const;
    // Makes what a folder lists durable, where the system allows it: on Linux and macOS, fsync on the folder. Windows
    // journals what a folder lists itself. Virtual so that a test can see that it is asked at the moments it must be.
    virtual void syncFolder(const QString &path) const;
    // What every plan under way may hold in memory together, in bytes, while it waits for the rest of itself.
    virtual qint64 heldBytesLimit() const;

private:
    struct Step
    {
        QString id;
        PackageResult package;
    };
    struct Plan
    {
        QString root;
        QStringList steps;
        qsizetype next = 0; // the step being fetched
        quint64 epoch = 0;  // the catalogue it was made from
        qint64 bytes = 0;   // what it holds in memory so far
        QList<Step> verified;
        bool over = false;
    };
    using PlanPtr = std::shared_ptr<Plan>;
    struct Move
    {
        QString id;
        QString staging;  // in items/, or empty when it is being removed
        QString previous; // in items/, or empty when nothing was installed
    };

    using Done = std::function<void(bool ok, const QByteArray &data, const QString &error)>;
    void fetch(const QString &job, const QUrl &url, qint64 maximumSize, const Done &done);
    bool countReceived(QNetworkReply *reply, qint64 bytes);
    void setState(State state, const QString &error = {});
    void takeCatalog(Catalog catalog);

    void runNextStep(const PlanPtr &plan);
    void stepArrived(const PlanPtr &plan, const QString &id, const QByteArray &data);
    void commitPlan(const PlanPtr &plan);
    void failPlan(const PlanPtr &plan, const QString &id, const QString &error);
    void fail(const QString &id, const QString &error);

    QString refusalToInstall(const CatalogItem &item) const;
    QString refusalAgainstInstalled(const CatalogItem &item) const;
    QString mismatch(const CatalogItem &item, const PackageResult &package) const;
    QString refusePackage(const CatalogItem &item, const QByteArray &data, PackageResult *package) const;
    QString licenceIn(const QString &id, const QByteArray &data, QString *text) const;
    bool catalogueHasExpired() const;
    bool viewIsClosed(View view) const;

    QString admit(const Catalog &catalog, const QByteArray &json);
    void loadSequence();

    QMap<QString, InstalledVersion> installedVersions() const;
    QMap<QString, Installed> readInstalled(QString *transaction = nullptr, bool *recordRead = nullptr) const;
    QMap<QString, Installed> installedFromFolders() const;
    bool writeJournal(const QString &transaction, const QList<Move> &moves) const;
    bool undoMoves(const QList<Move> &moves);
    bool hasLanded(const Move &move) const;
    void removeJournal() const;
    bool recoverLocked();
    void recoverInterrupted();
    void releaseHeld(const PlanPtr &plan);

    bool m_creatorFeatures = false;
    QMap<QString, QString> m_engines;
    QString m_dataDir;
    QByteArray m_storeKey;
    QString m_keyId;
    QUrl m_catalogUrl;
    State m_state = State::Idle;
    QString m_error;
    Catalog m_catalog;
    quint64 m_catalogEpoch = 0;
    QByteArray m_catalogDigest;
    QMap<QString, Installed> m_installed;
    QNetworkAccessManager *m_network = nullptr;
    QHash<QString, QPointer<QNetworkReply>> m_jobs;
    QHash<QString, double> m_progress;
    qint64 m_heldBytes = 0;     // what every plan under way holds in memory, together
    qint64 m_receivedBytes = 0; // what every package on its way holds in memory so far, together
    int m_generation = 0;
    qint64 m_lastSequence = 0;
    QByteArray m_lastDigest;
};

} // namespace qor::store
