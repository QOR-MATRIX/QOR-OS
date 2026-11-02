// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QUrl>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtQml/qqmlregistration.h>

namespace qor::store {

class StoreEngine;

// QML singleton `Storefronts`: the Player Store and the Creator Store for the shell's pages (QOR-0021, docs/STORE.md),
// beside `Store`, which keeps themes and widgets on their own engine, folder and key.
//
// Only a demonstration build has a second engine at all: QOR_STORE_DEMONSTRATION, off by default, compiles in the
// throwaway key from tests/store/fixtures-demo/ and reads a format 2 catalogue's URL from QOR_STORE_DEMONSTRATION_URL.
// Without it this is empty: `available` is false, every answer is empty, nothing is fetched, and no folder is made.
//
// Every query names its view, "player" or "creator". There is no default and no fallback, and nothing here merges them.
// What is off is refused by the engine, whatever a page shows; a page hiding the Creator Store is courtesy.
//
// Whatever comes from here is someone else's text, for a Text that sets textFormat: Text.PlainText. Maps hold no path a
// catalogue chose: a preview is asked for by its number, and handed over only after its digest and its size are
// checked.
class Storefronts : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ isAvailable CONSTANT FINAL)
    Q_PROPERTY(bool ready READ isReady NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool loading READ isLoading NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString name READ name NOTIFY listingsChanged FINAL)
    Q_PROPERTY(bool hasNextCatalogue READ hasNextCatalogue NOTIFY stateChanged FINAL)
    // Handed on from Settings by this class. A page reads it, and asks Settings to change it.
    Q_PROPERTY(bool creatorFeatures READ creatorFeatures NOTIFY creatorFeaturesChanged FINAL)
    Q_PROPERTY(QVariantList library READ library NOTIFY libraryChanged FINAL)
    Q_PROPERTY(int updateCount READ updateCount NOTIFY libraryChanged FINAL)
    // What a binding reads beside a call below, to ask again when the answer may have changed. Kept apart so a
    // download's progress moves a button and does not rebuild a page.
    Q_PROPERTY(int listingsRevision READ listingsRevision NOTIFY listingsChanged FINAL)
    Q_PROPERTY(int statusRevision READ statusRevision NOTIFY statusChanged FINAL)
    Q_PROPERTY(int previewRevision READ previewRevision NOTIFY previewsChanged FINAL)
    Q_PROPERTY(int licenceRevision READ licenceRevision NOTIFY licencesChanged FINAL)

public:
    explicit Storefronts(QObject *parent = nullptr);

    // The process-wide second engine: created on first use in a demonstration build, null in any other.
    static StoreEngine *engine();
    // Hands in an engine a test made, with a key it read from a file, and the catalogues it takes in turn. Nothing in
    // the shell calls this.
    static void useEngine(StoreEngine *engine, const QList<QUrl> &catalogues);

    bool isAvailable() const;
    bool isReady() const;
    bool isLoading() const;
    QString error() const;
    QString name() const;
    bool hasNextCatalogue() const;
    bool creatorFeatures() const;
    QVariantList library() const;
    int updateCount() const;
    int listingsRevision() const { return m_listingsRevision; }
    int statusRevision() const { return m_statusRevision; }
    int previewRevision() const { return m_previewRevision; }
    int licenceRevision() const { return m_licenceRevision; }

    // What a storefront shows, of a view named by the caller. A name that is neither answers nothing.
    Q_INVOKABLE QVariantList listings(const QString &view) const;
    Q_INVOKABLE QVariantList featured(const QString &view) const;
    Q_INVOKABLE QVariantList search(const QString &view, const QString &query) const;
    Q_INVOKABLE QVariantMap categoryCounts(const QString &view) const;
    // One listing, if it is in that view, or an empty map.
    Q_INVOKABLE QVariantMap item(const QString &view, const QString &id) const;

    // Where one id stands, of any view: `name`, `installed`, `installedVersion`, `version`, `updateAvailable`,
    // `updateWaits` (one is listed, and only the creator store carries it with creator features off), `busy` and
    // `progress`.
    Q_INVOKABLE QVariantMap status(const QString &id) const;

    Q_INVOKABLE void refresh();
    // The demonstration holds catalogues to take in turn, as a store's would move on at one address.
    Q_INVOKABLE void nextCatalogue();
    // Each ends with finished(), which says why in plain words if it did not start or did not work.
    Q_INVOKABLE bool install(const QString &id);
    Q_INVOKABLE bool remove(const QString &id);

    // Asks for a preview. When it has come, preview() answers with a URL holding the checked bytes themselves; until
    // then, or if it was refused, it answers with an empty one.
    Q_INVOKABLE bool fetchPreview(const QString &id, int index);
    Q_INVOKABLE QUrl preview(const QString &id, int index) const;

    // Asks for an item's licence. licence() answers with `asked`, then `text` or `error`.
    Q_INVOKABLE bool fetchLicence(const QString &id);
    Q_INVOKABLE QVariantMap licence(const QString &id) const;

Q_SIGNALS:
    void stateChanged();
    void listingsChanged();
    void libraryChanged();
    void statusChanged();
    void previewsChanged();
    void licencesChanged();
    void creatorFeaturesChanged();
    void finished(const QString &id, bool ok, const QString &message);
    void previewRefused(const QString &id, int index, const QString &message);

private:
    void followPreference();
    void takeCreatorFeatures(bool on);
    void forgetFetched();
    void takePreview(const QString &id, int index, bool ok, const QByteArray &data, const QString &error);
    void takeLicence(const QString &id, bool ok, const QString &text, const QString &error);

    QMetaObject::Connection m_preference;
    int m_listingsRevision = 0;
    int m_statusRevision = 0;
    int m_previewRevision = 0;
    int m_licenceRevision = 0;
    QHash<QString, QUrl> m_previews;
    QSet<QString> m_previewsAsked;
    qint64 m_previewBytes = 0;
    QHash<QString, QVariantMap> m_licences;
};

} // namespace qor::store
