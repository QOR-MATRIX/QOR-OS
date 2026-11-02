// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// The Store's real pages over a catalogue this suite signs: every field of it markup, a game placed for players, and a
// pack placed for creators that is featured and worded so that anything merging the two views would show it.
// tst_storefronts.qml walks them. No demonstration build is needed, and nothing reaches a display: an engine is handed
// in, Qt's offscreen platform holds the window, and the software scene graph draws it.
#include "kit.h"
#include "serviceendpoint.h"
#include "services.h"
#include "simservices.h"
#include "storefronts.h"
#include "testshop.h"

#include <QtCore/QBuffer>
#include <QtCore/QTemporaryDir>
#include <QtGui/QImage>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlExtensionPlugin>
#include <QtQuickControls2/QQuickStyle>
#include <QtQuickTest/quicktest.h>

#include <memory>

using namespace qor::store;
using namespace teststore;
using namespace Qt::StringLiterals;

Q_IMPORT_QML_PLUGIN(QOR_KitPlugin)
Q_IMPORT_QML_PLUGIN(LumenPlugin)
Q_IMPORT_QML_PLUGIN(QOR_ServicesPlugin)
Q_IMPORT_QML_PLUGIN(QOR_AppsPlugin)

namespace {

const QString kGame = QStringLiteral("org.example.game.one");
const QString kPack = QStringLiteral("org.example.pack.art");

QByteArray smallPicture()
{
    QImage image(16, 16, QImage::Format_RGB32);
    image.fill(Qt::darkCyan);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

// A listing whose every field is someone else's markup, each marked so a test can see it reached a page.
void hostile(QJsonObject &listing, const QString &who)
{
    const auto marked = [&who](const char *field) {
        return u"<b>%1 %2</b><img src=\"https://example.org/x.png\">"_s.arg(who, QString::fromLatin1(field));
    };
    listing.insert(u"name"_s, marked("name"));
    listing.insert(u"summary"_s, marked("summary"));
    listing.insert(u"description"_s, marked("description"));
    listing.insert(u"whatsNew"_s, marked("whatsNew"));
    listing.insert(u"provenance"_s, QJsonObject { { u"origin"_s, marked("origin") },
                                                  { u"author"_s, marked("author") },
                                                  { u"source"_s, marked("source") },
                                                  { u"permission"_s, marked("permission") } });
}

} // namespace

class Setup : public QObject
{
    Q_OBJECT
public Q_SLOTS:
    void applicationAvailable()
    {
        qor::Kit::initialize();
        QQuickStyle::setStyle(u"Lumen"_s);

        qor::sim::SimOptions options;
        options.url = qor::services::endpoint(u"qor-test-"_s + QString::number(QCoreApplication::applicationPid()));
        options.persistent = false;
        options.silent = true;
        m_services = std::make_unique<qor::sim::SimServices>();
        if (!m_services->start(options) || !qor::Services::instance()->connectTo(m_services->url(), 5000))
            qFatal("The simulated services did not start");

        qputenv("QOR_STORE_DIR", m_data.path().toUtf8());
        const QByteArray picture = smallPicture();
        writeFile(m_shop.dir.path() + u"/previews/hero.png"_s, picture);
        const QJsonArray previews { QJsonObject { { u"file"_s, u"previews/hero.png"_s },
                                                  { u"sha256"_s, hexDigest(picture) } } };

        Pack game = gamePack();
        game.category = u"<b>Platformers</b>"_s;
        game.files.insert(u"LICENSE.txt"_s,
                          QByteArrayLiteral("Copyright Someone <someone@example.org>.\nFree to use.\n"));
        // Signed once: each signature differs, and both catalogues list this one package.
        const QByteArray gamePackage = game.package(m_shop.developer);
        const auto stock = [&](const QString &packVersion) {
            m_shop.items.clear();
            QJsonObject &forPlayers = m_shop.addPackage(game, gamePackage);
            hostile(forPlayers, u"game"_s);
            forPlayers.insert(u"tags"_s, QJsonArray { u"one"_s });
            forPlayers.insert(u"featured"_s, true);
            forPlayers.insert(u"previews"_s, previews);
            Pack pack;
            pack.version = packVersion;
            pack.category = u"<b>2D art</b>"_s;
            QJsonObject &forCreators = m_shop.add(pack);
            hostile(forCreators, u"pack"_s);
            forCreators.insert(u"name"_s, u"<b>pack name</b> one"_s);
            forCreators.insert(u"tags"_s, QJsonArray { u"one"_s, u"zebra"_s });
            forCreators.insert(u"featured"_s, true);
            forCreators.insert(u"previews"_s, previews);
        };
        stock(u"1.0.0"_s);
        m_shop.sequence = 1;
        const QUrl first = m_shop.publish();
        stock(u"1.1.0"_s);
        m_shop.sequence = 2;
        const QByteArray second = m_shop.json();
        writeFile(m_shop.dir.path() + u"/second.json"_s, second);
        writeFile(m_shop.dir.path() + u"/second.json.sig"_s, m_shop.store.sign(second));

        m_engine = std::make_unique<StoreEngine>(m_data.path() + u"/demonstration"_s, m_shop.store.publicKey());
        Storefronts::useEngine(m_engine.get(), { first, QUrl::fromLocalFile(m_shop.dir.path() + u"/second.json"_s) });
    }

private:
    QTemporaryDir m_data;
    Shop m_shop;
    std::unique_ptr<qor::sim::SimServices> m_services;
    std::unique_ptr<StoreEngine> m_engine;
};

QUICK_TEST_MAIN_WITH_SETUP(storefronts, Setup)
#include "tst_storefronts_qml.moc"
