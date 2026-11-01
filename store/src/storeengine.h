// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once

#include "catalog.h"

#include <QtCore/QHash>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtCore/QUrl>

#include <functional>

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
// manifest. Files land in a temporary folder that replaces the old version
// only once all of them are written.
//
// There is one engine and one install folder, whichever store an item was found in: one id, one install, so a utility
// placed in both storefronts is one package. What keeps the stores apart is not any interface hiding one (QOR-0021):
// every query names its view and answers from that view alone, and install refuses what only the creator store carries
// unless the engine has been told creator features are on. For format 2 it remembers the highest sequence it has seen.
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
    // The highest sequence this store has taken, in its own folder. Zero until one has.
    qint64 lastSequence() const { return m_lastSequence; }

    // Off until the engine is told otherwise. It is a person's preference, kept elsewhere and given here. Off, the
    // creator view is empty and install refuses what only the creator store carries; and turning it off removes
    // nothing: what is installed stays installed, intact and usable.
    bool creatorFeatures() const { return m_creatorFeatures; }
    void setCreatorFeatures(bool on);

    // What a storefront shows, always of a view named by the caller. There is no default, and no fallback: an empty
    // answer stays empty, and is never made up from the other store.
    QList<CatalogItem> listings(View view) const;
    QList<CatalogItem> featured(View view) const;
    QList<CatalogItem> search(View view, const QString &query) const;
    QMap<QString, int> categoryCounts(View view) const;

    // Whether what is installed for `id` is still exactly what its developer signed, and the very package installed:
    // an older copy, signed in its day, does not pass. False until the catalogue that names the developer's key is
    // here.
    bool isIntact(const QString &id) const;

    bool isBusy(const QString &id) const { return m_jobs.contains(id); }
    double progress(const QString &id) const { return m_progress.value(id, 0); }

    // Installs the catalogue's version of `id` (an update when an older one is installed), with whatever it depends on
    // first. Ends with finished() for each. False if it did not start; a refusal says why through finished().
    bool install(const QString &id);
    // Refused, with finished(), while something installed depends on it.
    bool remove(const QString &id);

    // Fetches one of an item's previews, checks it against the catalogue's digest and that it is a picture, and ends
    // with previewFetched(). False if the item has no such preview, or only the creator store carries it and creator
    // features are off.
    bool fetchPreview(const QString &id, int index);

Q_SIGNALS:
    void stateChanged();
    void catalogChanged();
    void creatorFeaturesChanged();
    void installedChanged();
    void progressChanged(const QString &id);
    void finished(const QString &id, bool ok, const QString &error);
    void previewFetched(const QString &id, int index, bool ok, const QByteArray &data, const QString &error);

protected:
    // Takes a catalogue that has passed every check.
    void adoptCatalog(Catalog catalog);
    // Whether this stage installs a kind. A later stage changes isInstallableKind and gives the kind its file rules; a
    // test overrides this to prove what does not depend on the stage, such as one install for two listings.
    virtual bool takesKind(const QString &kind) const { return isInstallableKind(kind); }

private:
    using Done = std::function<void(bool ok, const QByteArray &data, const QString &error)>;
    void fetch(const QString &job, const QUrl &url, qint64 maximumSize, const Done &done);
    void setState(State state, const QString &error = {});
    void runSteps(const QStringList &steps);
    void complete(const QString &id, const QByteArray &data);
    void fail(const QString &id, const QString &error);
    QString refusalToInstall(const CatalogItem &item) const;
    QString admit(const Catalog &catalog, const QByteArray &json);
    QMap<QString, InstalledVersion> installedVersions() const;
    void recoverInterrupted();
    void loadInstalled();
    bool saveInstalled() const;
    void loadSequence();

    bool m_creatorFeatures = false;
    QString m_dataDir;
    QByteArray m_storeKey;
    QUrl m_catalogUrl;
    State m_state = State::Idle;
    QString m_error;
    Catalog m_catalog;
    QMap<QString, Installed> m_installed;
    QNetworkAccessManager *m_network = nullptr;
    QHash<QString, QPointer<QNetworkReply>> m_jobs;
    QHash<QString, double> m_progress;
    QHash<QString, QStringList> m_waiting; // an item being fetched → what is to follow it, in order
    int m_generation = 0;
    qint64 m_lastSequence = 0;
    QByteArray m_lastDigest;
};

} // namespace qor::store
