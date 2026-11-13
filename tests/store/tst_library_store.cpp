// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// The Library's games from the Store (prism/shell/library.h), from the hostile side: an engine of the suite's own, over
// a catalogue it signs, and what is installed then changed under it as anything running as this person could change it.
// A game is listed by the engine's record, and checked whole as it is played; the scene it plays is the one its own
// game.json names.
#include "library.h"
#include "storeengine.h"
#include "storefronts.h"
#include "testshop.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QScopeGuard>
#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <memory>

using namespace qor;
using namespace qor::store;
using namespace teststore;
using namespace Qt::StringLiterals;

namespace {

const QString kGame = u"org.example.game.one"_s;
const QString kItem = u"store:org.example.game.one"_s;
const QByteArray kScene = QByteArrayLiteral("import QQ\nScene {}\n");

QByteArray gameJson(const QString &scene)
{
    return QJsonDocument(
               QJsonObject { { u"format"_s, 1 },
                             { u"kind"_s, u"game"_s },
                             { u"title"_s, u"One climb"_s },
                             { u"scene"_s, scene },
                             { u"build"_s, 1 },
                             { u"engine"_s, QJsonObject { { u"id"_s, u"qq"_s }, { u"version"_s, u"0.1.0"_s } } } })
        .toJson();
}

// A game as QQ Studio builds one: its scene, and game.json naming it.
Pack builtGame(const QString &scene = u"scenes/climb.qml"_s)
{
    Pack game = gamePack();
    game.files = { { u"LICENSE.txt"_s, kLicence },
                   { u"game.json"_s, gameJson(scene) },
                   { u"scenes/climb.qml"_s, kScene },
                   { u"scenes/other.qml"_s, kScene } };
    return game;
}

// An engine that can be handed a catalogue as it stands, past the parser: what no catalogue may say, planted.
class Engine : public StoreEngine
{
public:
    using StoreEngine::adoptCatalog;
    using StoreEngine::StoreEngine;
};

// A catalogue with the game, an engine of the suite's own that has installed it, and the Library over that engine.
struct Installed
{
    Shop shop;
    QTemporaryDir data;
    Engine engine;
    std::unique_ptr<Library> library;
    bool ok = false;

    explicit Installed(const Pack &game, bool creatorFeatures = false) : engine(data.path(), shop.store.publicKey())
    {
        shop.add(game);
        Storefronts::useEngine(&engine, {});
        engine.setCreatorFeatures(creatorFeatures);
        ok = refreshed(engine, shop.publish()) == StoreEngine::State::Ready && installAndWait(engine, game.id).ok;
        library = std::make_unique<Library>();
    }
    ~Installed()
    {
        library.reset();
        Storefronts::useEngine(nullptr, {});
    }
    Installed(const Installed &) = delete;
    Installed &operator=(const Installed &) = delete;

    QString folder() const { return engine.installDirectory(kGame); }
    QVariantMap listed() const
    {
        for (const QVariant &game : library->games())
            if (game.toMap().value(u"id"_s) == kItem)
                return game.toMap();
        return {};
    }
};

void overwrite(const QString &path, const QByteArray &data)
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate), qPrintable(path));
    file.write(data);
}

} // namespace

class TestLibraryStore : public QObject
{
    Q_OBJECT

    QTemporaryDir m_store;

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        // Nothing of this machine's: no games folder, and the Store's data in a folder of the suite's.
        qputenv("QOR_STORE_DIR", m_store.path().toUtf8());
        qputenv("QOR_GAMES", QByteArray(m_store.path().toUtf8() + "/no-games"));
    }

    void aStoreGameIsListedAndPlaysTheSceneItsGameJsonNames()
    {
        Installed all(builtGame());
        QVERIFY(all.ok);
        const QVariantMap listed = all.listed();
        QCOMPARE(listed.value(u"source"_s).toString(), u"store"_s);
        QCOMPARE(listed.value(u"kind"_s).toString(), u"game"_s);
        QCOMPARE(listed.value(u"storeId"_s).toString(), kGame);
        // What its developer signed calls it, not what a file beside it says.
        QCOMPARE(listed.value(u"title"_s).toString(), all.engine.installed().value(kGame).name);
        QVERIFY(listed.value(u"path"_s).toString().isEmpty());
        QCOMPARE(all.library->item(kItem).value(u"id"_s).toString(), kItem);

        const QVariantMap playable = all.library->playable(kItem);
        QVERIFY2(playable.value(u"ok"_s).toBool(), qPrintable(playable.value(u"message"_s).toString()));
        // The scene game.json names, of the two the game holds, in the folder the Store installed it in.
        QCOMPARE(QFileInfo(playable.value(u"path"_s).toString()).canonicalFilePath(),
                 QFileInfo(all.folder() + u"/scenes/climb.qml"_s).canonicalFilePath());
    }

    void whatIsChangedAfterItWasInstalledDoesNotPlay_data()
    {
        QTest::addColumn<QString>("change");
        QTest::newRow("a file altered") << u"altered"_s;
        QTest::newRow("a file beside the rest") << u"extra"_s;
        QTest::newRow("a file taken away") << u"missing"_s;
        QTest::newRow("game.json pointed at the other scene") << u"redirected"_s;
#ifdef Q_OS_UNIX
        QTest::newRow("the scene a link to the same bytes elsewhere") << u"link"_s;
        QTest::newRow("a folder that is a link") << u"folder-link"_s;
#endif
    }

    void whatIsChangedAfterItWasInstalledDoesNotPlay()
    {
        QFETCH(QString, change);
        Installed all(builtGame());
        QVERIFY(all.ok);
        QVERIFY(all.library->playable(kItem).value(u"ok"_s).toBool());
        const QString scene = all.folder() + u"/scenes/climb.qml"_s;
        QTemporaryDir elsewhere;
        if (change == u"altered"_s) {
            overwrite(scene, kScene + "Item { Component.onCompleted: Qt.quit() }\n");
        } else if (change == u"extra"_s) {
            overwrite(all.folder() + u"/scenes/extra.qml"_s, kScene);
        } else if (change == u"missing"_s) {
            QVERIFY(QFile::remove(all.folder() + u"/scenes/other.qml"_s));
        } else if (change == u"redirected"_s) {
            overwrite(all.folder() + u"/game.json"_s, gameJson(u"scenes/other.qml"_s));
        } else if (change == u"link"_s) {
            const QString outside = elsewhere.filePath(u"climb.qml"_s);
            overwrite(outside, kScene);
            QVERIFY(QFile::remove(scene));
            QVERIFY(QFile::link(outside, scene));
        } else if (change == u"folder-link"_s) {
            QVERIFY(QDir().rename(all.folder() + u"/scenes"_s, elsewhere.filePath(u"scenes"_s)));
            QVERIFY(QFile::link(elsewhere.filePath(u"scenes"_s), all.folder() + u"/scenes"_s));
        }
        // Still listed, as installed: it is only refused, and says so plainly, naming it.
        QVERIFY(!all.listed().isEmpty());
        const QVariantMap playable = all.library->playable(kItem);
        QVERIFY(!playable.value(u"ok"_s).toBool());
        QVERIFY(playable.value(u"path"_s).toString().isEmpty());
        const QString message = playable.value(u"message"_s).toString();
        QVERIFY2(message.contains(u"is not as its developer signed it"_s), qPrintable(message));
        QVERIFY2(message.startsWith(all.listed().value(u"title"_s).toString()), qPrintable(message));
    }

    // Signed as it is, a game.json that names a scene anywhere but a plain file directly in the game's scenes/ is
    // refused, and nothing is guessed in its place.
    void aGameJsonNamingASceneOutsideTheGameDoesNotPlay_data()
    {
        QTest::addColumn<QString>("scene");
        QTest::newRow("above the game") << u"../climb.qml"_s;
        QTest::newRow("climbing out of scenes") << u"scenes/../../climb.qml"_s;
        QTest::newRow("an absolute path") << u"/tmp/climb.qml"_s;
        QTest::newRow("deeper than scenes") << u"scenes/deeper/climb.qml"_s;
        QTest::newRow("a backslash") << u"scenes\\climb.qml"_s;
        QTest::newRow("not a scene") << u"scenes/climb.txt"_s;
        QTest::newRow("a scene it does not hold") << u"scenes/missing.qml"_s;
        QTest::newRow("outside scenes") << u"game.json"_s;
    }

    void aGameJsonNamingASceneOutsideTheGameDoesNotPlay()
    {
        QFETCH(QString, scene);
        Installed all(builtGame(scene));
        QVERIFY(all.ok);
        QVERIFY(all.engine.isIntact(kGame));
        QVERIFY(!all.listed().isEmpty());
        const QVariantMap playable = all.library->playable(kItem);
        QVERIFY(!playable.value(u"ok"_s).toBool());
        QVERIFY(playable.value(u"path"_s).toString().isEmpty());
        const QString message = playable.value(u"message"_s).toString();
        QVERIFY2(message.contains(u"does not say which scene it starts in"_s), qPrintable(message));
    }

    // Another game installed beside it holds a real scene, so a game.json that climbs to it names a file that is there,
    // signed and whole. It is still not this game's, and does not play.
    void aGameJsonNamingAnotherGamesSceneDoesNotPlay()
    {
        Pack two = builtGame();
        two.id = u"org.example.game.two"_s;
        Pack one = builtGame(u"scenes/../../org.example.game.two/scenes/climb.qml"_s);
        Shop shop;
        shop.add(one);
        shop.add(two);
        QTemporaryDir data;
        Engine engine(data.path(), shop.store.publicKey());
        Storefronts::useEngine(&engine, {});
        const auto unhand = qScopeGuard([] { Storefronts::useEngine(nullptr, {}); });
        QCOMPARE(refreshed(engine, shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(engine, two.id).ok);
        QVERIFY(installAndWait(engine, one.id).ok);
        QVERIFY(QFileInfo(engine.installDirectory(one.id) + u"/scenes/../../org.example.game.two/scenes/climb.qml"_s)
                    .isFile());
        Library library;
        QVERIFY(library.playable(u"store:"_s + two.id).value(u"ok"_s).toBool());
        const QVariantMap playable = library.playable(kItem);
        QVERIFY(!playable.value(u"ok"_s).toBool());
        QVERIFY2(playable.value(u"message"_s).toString().contains(u"does not say which scene it starts in"_s),
                 qPrintable(playable.value(u"message"_s).toString()));
    }

    void aGameWithNoGameJsonDoesNotPlayEvenWithOneScene()
    {
        Pack game = gamePack();
        game.files = { { u"LICENSE.txt"_s, kLicence }, { u"scenes/climb.qml"_s, kScene } };
        Installed all(game);
        QVERIFY(all.ok);
        const QVariantMap playable = all.library->playable(kItem);
        QVERIFY(!playable.value(u"ok"_s).toBool());
        QVERIFY2(playable.value(u"message"_s).toString().contains(u"does not say which scene"_s),
                 qPrintable(playable.value(u"message"_s).toString()));
    }

    // No catalogue may place a game in the Creator Store alone; one is planted there past the parser, to show the
    // Library's own word: what only the Creator Store carries is not in the Library with creator features off, nor
    // played by its id, and it stays installed.
    void aCreatorOnlyGameIsNotInTheLibraryWithTheSwitchOff()
    {
        Installed all(builtGame());
        QVERIFY(all.ok);
        Catalog planted = all.engine.catalog();
        for (CatalogItem &item : planted.items)
            if (item.id == kGame)
                item.placements = { Placement { View::Creator, u"Games"_s } };
        QSignalSpy changed(all.library.get(), &Library::changed);
        all.engine.adoptCatalog(planted);
        QVERIFY(changed.count() >= 1);
        QVERIFY(all.listed().isEmpty());
        QVERIFY(all.library->item(kItem).isEmpty());
        const QVariantMap playable = all.library->playable(kItem);
        QVERIFY(!playable.value(u"ok"_s).toBool());
        QVERIFY2(playable.value(u"message"_s).toString().contains(u"not in the Library"_s),
                 qPrintable(playable.value(u"message"_s).toString()));
        // Still installed, as the Store keeps it, and with creator features on it is in the Library again.
        QVERIFY(all.engine.installed().contains(kGame));
        all.engine.setCreatorFeatures(true);
        QVERIFY(!all.listed().isEmpty());
        QVERIFY(all.library->playable(kItem).value(u"ok"_s).toBool());
        changed.clear();
        all.engine.setCreatorFeatures(false);
        QVERIFY(changed.count() >= 1);
        QVERIFY(all.listed().isEmpty());
    }

    void aRemovedGameLeavesTheLibrary()
    {
        Installed all(builtGame());
        QVERIFY(all.ok);
        QSignalSpy changed(all.library.get(), &Library::changed);
        QVERIFY(all.engine.remove(kGame));
        QVERIFY(!all.engine.installed().contains(kGame));
        QVERIFY(changed.count() >= 1);
        QVERIFY(all.listed().isEmpty());
        QVERIFY(!all.library->playable(kItem).value(u"ok"_s).toBool());
    }

    // What is not a game, a pack beside it, is no game in the Library; nor is anything the Library was not told of.
    void onlyInstalledGamesAreListed()
    {
        Installed all(builtGame());
        QVERIFY(all.ok);
        Pack pack;
        all.shop.add(pack);
        all.shop.sequence = 2;
        all.engine.setCreatorFeatures(true);
        QCOMPARE(refreshed(all.engine, all.shop.publish()), StoreEngine::State::Ready);
        QVERIFY(installAndWait(all.engine, pack.id).ok);
        int fromTheStore = 0;
        for (const QVariant &game : all.library->games())
            fromTheStore += game.toMap().value(u"source"_s) == u"store"_s ? 1 : 0;
        QCOMPARE(fromTheStore, 1);
        QVERIFY(!all.library->playable(u"store:"_s + pack.id).value(u"ok"_s).toBool());
        QVERIFY(!all.library->playable(u"store:org.example.game.none"_s).value(u"ok"_s).toBool());
        QVERIFY(!all.library->playable(u"qq:/tmp/scenes/climb.qml"_s).value(u"ok"_s).toBool());
    }

    // With no second engine, as in any build but a demonstration, there is nothing of the Store's.
    void withNoSecondEngineThereAreNoStoreGames()
    {
        Storefronts::useEngine(nullptr, {});
        Library library;
        for (const QVariant &game : library.games())
            QVERIFY(game.toMap().value(u"source"_s) != u"store"_s);
        QVERIFY(!library.playable(kItem).value(u"ok"_s).toBool());
    }
};

QTEST_GUILESS_MAIN(TestLibraryStore)
#include "tst_library_store.moc"
