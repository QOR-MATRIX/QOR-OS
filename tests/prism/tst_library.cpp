// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// The Library (prism/shell/library.h): the QQ games and media it finds in
// folders made here, ARQADE's games while ARQADE can start, what was played
// or opened and in what order, and QOR Media's reports of where a film
// stopped, all kept from one run to the next.

#include "library.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <cmath>

using namespace Qt::StringLiterals;
using qor::Library;

namespace {

void touch(const QString &path, const QDateTime &modified = {})
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
    file.close();
    if (modified.isValid()) {
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(modified, QFileDevice::FileModificationTime));
    }
}

QStringList idsOf(const QVariantList &items)
{
    QStringList ids;
    for (const QVariant &item : items)
        ids.append(item.toMap().value(u"id"_s).toString());
    return ids;
}

QVariantMap byId(const QVariantList &items, const QString &id)
{
    for (const QVariant &item : items)
        if (item.toMap().value(u"id"_s) == id)
            return item.toMap();
    return {};
}

// What QQ Studio's build writes (products/qq/studio/gamebuild.cpp), but for its files.
QJsonObject builtManifest()
{
    return { { u"format"_s, 1 },
             { u"kind"_s, u"game"_s },
             { u"title"_s, u"Orb Run"_s },
             { u"scene"_s, u"scenes/level.qml"_s },
             { u"build"_s, 3 },
             { u"engine"_s, QJsonObject { { u"id"_s, u"qq"_s }, { u"version"_s, u"0.1.0"_s } } } };
}

void writeManifest(const QString &folder, const QJsonObject &manifest)
{
    QFile file(QDir(folder).filePath(u"game.json"_s));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(manifest).toJson());
}

QString historyFile()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(u"library-history.json"_s);
}

QJsonObject history()
{
    QFile file(historyFile());
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}

} // namespace

class tst_Library : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void init() { QFile::remove(historyFile()); }

    // A folder with scenes is a QQ game: its own-named scene, else its first.
    // QOR OS's are one deep; the person's own, in Documents, two deep.
    void qq_games_are_found_by_their_scenes()
    {
        QTemporaryDir games;
        QTemporaryDir documents;
        touch(games.filePath(u"orb-run/scenes/orb-run.qml"_s));
        touch(games.filePath(u"orb-run/scenes/credits.qml"_s));
        touch(games.filePath(u"ember-climb/scenes/level.qml"_s));
        touch(games.filePath(u"notes/readme.txt"_s));
        touch(documents.filePath(u"Projects/my_first_game/scenes/main.qml"_s));
        touch(documents.filePath(u"Deep/a/b/scenes/far.qml"_s));
        // A download unpacked in Documents is nobody's own game: only what QQ Studio made or a person trusted is.
        touch(documents.filePath(u"Downloaded/strangers_game/scenes/main.qml"_s));
        const QString ownList =
            QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/qor-os/own-projects.json"_s;
        QDir().mkpath(QFileInfo(ownList).absolutePath());
        QFile own(ownList);
        QVERIFY(own.open(QIODevice::WriteOnly));
        own.write(
            QJsonDocument(QJsonArray { QFileInfo(documents.filePath(u"Projects/my_first_game"_s)).canonicalFilePath() })
                .toJson());
        own.close();

        const Library::Found found = Library::scan({ games.path() }, { documents.path() }, {});
        const QStringList ids = idsOf(found.games);
        QCOMPARE(ids.size(), 3);
        const QVariantMap orb = byId(found.games, u"qq:"_s + games.filePath(u"orb-run/scenes/orb-run.qml"_s));
        QCOMPARE(orb.value(u"title"_s).toString(), u"Orb Run"_s);
        QCOMPARE(orb.value(u"source"_s).toString(), u"qq"_s);
        QCOMPARE(orb.value(u"kind"_s).toString(), u"game"_s);
        QVERIFY(!byId(found.games, u"qq:"_s + games.filePath(u"ember-climb/scenes/level.qml"_s)).isEmpty());
        const QVariantMap mine =
            byId(found.games, u"qq:"_s + documents.filePath(u"Projects/my_first_game/scenes/main.qml"_s));
        QCOMPARE(mine.value(u"title"_s).toString(), u"My First Game"_s);
        QCOMPARE(mine.value(u"source"_s).toString(), u"mine"_s);
        QVERIFY(
            byId(found.games, u"qq:"_s + documents.filePath(u"Downloaded/strangers_game/scenes/main.qml"_s)).isEmpty());
        QFile::remove(ownList);
        // The same game twice is one.
        QCOMPARE(Library::scan({ games.path(), games.path() }, {}, {}).games.size(), 2);
    }

    // A game QQ Studio built is known by what it says of itself, and plays the
    // copy. A game being built or replaced is not listed.
    void a_built_game_is_known_by_what_it_says_of_itself()
    {
        QTemporaryDir games;
        touch(games.filePath(u"orb-run-1a2b/scenes/aaa-first.qml"_s));
        touch(games.filePath(u"orb-run-1a2b/scenes/level.qml"_s));
        touch(games.filePath(u".building-orb-run-1a2b-Xy12Zw/scenes/level.qml"_s));
        touch(games.filePath(u".old-orb-run-1a2b-Xy12Zw/scenes/level.qml"_s));
        writeManifest(games.filePath(u"orb-run-1a2b"_s), builtManifest());

        const QVariantList found = Library::scan({ games.path() }, {}, {}).games;
        QCOMPARE(found.size(), 1);
        const QVariantMap game = found.first().toMap();
        QCOMPARE(game.value(u"source"_s).toString(), u"built"_s);
        QCOMPARE(game.value(u"kind"_s).toString(), u"game"_s);
        QCOMPARE(game.value(u"title"_s).toString(), u"Orb Run"_s);
        QCOMPARE(game.value(u"build"_s).toInt(), 3);
        QVERIFY(game.value(u"subtitle"_s).toString().contains(u"3"_s));
        // Its first scene as it says, not the first by name.
        QCOMPARE(game.value(u"path"_s).toString(), games.filePath(u"orb-run-1a2b/scenes/level.qml"_s));
    }

    // Any folder here could hold such a file. Whatever is wrong with what it
    // says, nothing of it is taken, and the folder is a QQ game like any other.
    void what_a_built_game_says_is_a_strangers_word_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("another format") << u"format"_s << QJsonValue(2);
        QTest::newRow("a format that is text") << u"format"_s << QJsonValue(u"1"_s);
        QTest::newRow("no kind") << u"kind"_s << QJsonValue();
        QTest::newRow("another kind") << u"kind"_s << QJsonValue(u"asset-pack"_s);
        QTest::newRow("another engine") << u"engine"_s << QJsonValue(QJsonObject { { u"id"_s, u"other"_s } });
        QTest::newRow("no title") << u"title"_s << QJsonValue();
        QTest::newRow("a blank title") << u"title"_s << QJsonValue(u"  \n "_s);
        QTest::newRow("a title too long") << u"title"_s << QJsonValue(QString(81, u'x'));
        QTest::newRow("no build") << u"build"_s << QJsonValue();
        QTest::newRow("a scene that climbs") << u"scene"_s << QJsonValue(u"scenes/../../outside.qml"_s);
        QTest::newRow("a scene that climbs at once") << u"scene"_s << QJsonValue(u"../outside.qml"_s);
        QTest::newRow("a scene by its full path") << u"scene"_s << QJsonValue(u"/etc/outside.qml"_s);
        QTest::newRow("a scene by the other slash") << u"scene"_s << QJsonValue(u"scenes\\..\\..\\outside.qml"_s);
        QTest::newRow("a scene by the other slash, inside")
            << u"scene"_s << QJsonValue(u"scenes/..\\..\\outside.qml"_s);
        QTest::newRow("a scene not in scenes") << u"scene"_s << QJsonValue(u"logic/level.qml"_s);
        QTest::newRow("a scene deeper") << u"scene"_s << QJsonValue(u"scenes/deep/level.qml"_s);
        QTest::newRow("a scene that is not a scene") << u"scene"_s << QJsonValue(u"scenes/level.txt"_s);
        QTest::newRow("a scene that is not there") << u"scene"_s << QJsonValue(u"scenes/gone.qml"_s);
    }
    void what_a_built_game_says_is_a_strangers_word()
    {
        QFETCH(QString, key);
        QFETCH(QJsonValue, value);
        QTemporaryDir games;
        const QString folder = games.filePath(u"orb-run"_s);
        touch(QDir(folder).filePath(u"scenes/level.qml"_s));
        touch(QDir(folder).filePath(u"scenes/deep/level.qml"_s));
        touch(QDir(folder).filePath(u"scenes/level.txt"_s));
        touch(QDir(folder).filePath(u"logic/level.qml"_s));
        touch(games.filePath(u"outside.qml"_s));
        QJsonObject manifest = builtManifest();
        if (value.isUndefined() || value.isNull())
            manifest.remove(key);
        else
            manifest.insert(key, value);
        writeManifest(folder, manifest);

        const QVariantList found = Library::scan({ games.path() }, {}, {}).games;
        QCOMPARE(found.size(), 1);
        const QVariantMap game = found.first().toMap();
        QCOMPARE(game.value(u"source"_s).toString(), u"qq"_s);
        QCOMPARE(game.value(u"title"_s).toString(), u"Orb Run"_s);
        QVERIFY2(game.value(u"path"_s).toString().startsWith(folder + u"/scenes/"_s),
                 qPrintable(game.value(u"path"_s).toString()));
    }

    void a_built_game_that_is_not_json_or_too_large_is_a_game_like_any_other()
    {
        QTemporaryDir games;
        const QString broken = games.filePath(u"broken"_s);
        const QString large = games.filePath(u"large"_s);
        touch(QDir(broken).filePath(u"scenes/level.qml"_s));
        touch(QDir(large).filePath(u"scenes/level.qml"_s));
        QFile notJson(QDir(broken).filePath(u"game.json"_s));
        QVERIFY(notJson.open(QIODevice::WriteOnly));
        notJson.write("{ not json");
        notJson.close();
        // A good manifest, and past the limit only by what follows it: reading up to the limit and no further would
        // believe it, so it is the limit on the file that says no.
        QFile tooLarge(QDir(large).filePath(u"game.json"_s));
        QVERIFY(tooLarge.open(QIODevice::WriteOnly));
        QVERIFY(tooLarge.write(QJsonDocument(builtManifest()).toJson()) > 0);
        QVERIFY(tooLarge.write(QByteArray(33 * 1024 * 1024, ' ')) > 0);
        tooLarge.close();

        const QVariantList found = Library::scan({ games.path() }, {}, {}).games;
        QCOMPARE(found.size(), 2);
        for (const QVariant &game : found)
            QCOMPARE(game.toMap().value(u"source"_s).toString(), u"qq"_s);
    }

#ifdef Q_OS_UNIX
    // What leads out by a link is not believed, wherever in the game the link is.
    void a_built_game_is_not_believed_through_a_link_data()
    {
        QTest::addColumn<QString>("what");
        QTest::newRow("the manifest") << u"manifest"_s;
        QTest::newRow("the scene") << u"scene"_s;
        QTest::newRow("the scenes folder") << u"scenes"_s;
        QTest::newRow("the game's own folder") << u"folder"_s;
    }
    void a_built_game_is_not_believed_through_a_link()
    {
        QFETCH(QString, what);
        QTemporaryDir games;
        QTemporaryDir outside;
        const QString folder = games.filePath(u"orb-run"_s);
        // A title of its own, which no folder's name gives, so a manifest believed would show.
        QJsonObject planted = builtManifest();
        planted.insert(u"title"_s, u"Believed Through A Link"_s);
        touch(outside.filePath(u"scenes/level.qml"_s));
        writeManifest(outside.path(), planted);
        if (what == u"folder"_s) {
            QVERIFY(QFile::link(outside.path(), folder));
        } else {
            QDir().mkpath(folder);
        }
        if (what == u"folder"_s) {
            // Nothing more: the whole game is the link.
        } else if (what == u"manifest"_s) {
            touch(QDir(folder).filePath(u"scenes/level.qml"_s));
            QVERIFY(QFile::link(outside.filePath(u"game.json"_s), QDir(folder).filePath(u"game.json"_s)));
        } else if (what == u"scene"_s) {
            QDir(folder).mkpath(u"scenes"_s);
            QVERIFY(QFile::link(outside.filePath(u"scenes/level.qml"_s), QDir(folder).filePath(u"scenes/level.qml"_s)));
            writeManifest(folder, planted);
        } else {
            QVERIFY(QFile::link(outside.filePath(u"scenes"_s), QDir(folder).filePath(u"scenes"_s)));
            writeManifest(folder, planted);
        }

        // Found as any folder of scenes is, and not as a game built here, whatever it says of itself. There is one: a
        // loop over nothing would prove nothing.
        const QVariantList found = Library::scan({ games.path() }, {}, {}).games;
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first().toMap().value(u"source"_s).toString(), u"qq"_s);
        QVERIFY(found.first().toMap().value(u"title"_s).toString() != u"Believed Through A Link"_s);
    }
#endif

    // Video, music and pictures, newest first, three deep at most; nothing
    // else, and nothing hidden.
    void media_is_found_newest_first_and_by_kind()
    {
        QTemporaryDir media;
        const QDateTime now = QDateTime::currentDateTime();
        touch(media.filePath(u"film.mkv"_s), now.addDays(-3));
        touch(media.filePath(u"Albums/song.flac"_s), now.addDays(-2));
        touch(media.filePath(u"photo.JPG"_s), now.addDays(-1));
        touch(media.filePath(u"notes.txt"_s), now);
        touch(media.filePath(u".hidden/secret.mp4"_s), now);
        touch(media.filePath(u"a/b/c/too-deep.mp4"_s), now);

        const Library::Found found = Library::scan({}, {}, { media.path() });
        const QVariantList items = found.media;
        QCOMPARE(idsOf(items), (QStringList { u"media:"_s + media.filePath(u"photo.JPG"_s),
                                              u"media:"_s + media.filePath(u"Albums/song.flac"_s),
                                              u"media:"_s + media.filePath(u"film.mkv"_s) }));
        QCOMPARE(items.at(0).toMap().value(u"mediaKind"_s).toString(), u"picture"_s);
        QCOMPARE(items.at(1).toMap().value(u"mediaKind"_s).toString(), u"audio"_s);
        QCOMPARE(items.at(2).toMap().value(u"mediaKind"_s).toString(), u"video"_s);
        QCOMPARE(items.at(2).toMap().value(u"title"_s).toString(), u"film"_s);
    }

    // ARQADE's seven games, while ARQADE can be started here.
    void arqades_games_are_there_while_it_can_start()
    {
        Library library;
        QVERIFY(idsOf(library.games()).filter(u"arqade:"_s).isEmpty());
        QSignalSpy changed(&library, &Library::changed);
        library.setArqade(true);
        QCOMPARE(changed.size(), 1);
        const QStringList arqade = idsOf(library.games()).filter(u"arqade:"_s);
        QCOMPARE(arqade, (QStringList { u"arqade:void"_s, u"arqade:synapse"_s, u"arqade:orbital"_s, u"arqade:rift"_s,
                                        u"arqade:everrift"_s, u"arqade:darkveil"_s, u"arqade:bellwick"_s,
                                        u"arqade:flux"_s, u"arqade:reversi"_s }));
        QCOMPARE(library.item(u"arqade:orbital"_s).value(u"game"_s).toString(), u"orbital"_s);
        library.setArqade(false);
        QVERIFY(library.item(u"arqade:orbital"_s).isEmpty());
    }

    // What was played comes first, and is remembered from one run to the
    // next.
    void what_was_played_comes_first_and_is_remembered()
    {
        {
            Library library;
            library.setArqade(true);
            QVERIFY(library.recent().isEmpty());
            library.used(u"arqade:orbital"_s);
            QTest::qWait(5);
            library.used(u"arqade:void"_s);
            QCOMPARE(idsOf(library.recent()), (QStringList { u"arqade:void"_s, u"arqade:orbital"_s }));
            QVERIFY(library.item(u"arqade:void"_s).value(u"lastUsed"_s).toLongLong() > 0);
        }
        Library again;
        again.setArqade(true);
        QCOMPARE(idsOf(again.recent()), (QStringList { u"arqade:void"_s, u"arqade:orbital"_s }));
    }

    // QOR Media's reports: where a film stopped, and how long it is, kept by
    // its path. Anything else, or anything malformed, is not kept.
    void where_a_film_stopped_is_kept()
    {
        Library library;
        const QString path = u"/srv/films/clip.mkv"_s;
        library.reported({ { u"type"_s, u"learned"_s }, { u"path"_s, path }, { u"duration"_s, 100.0 } });
        library.reported({ { u"type"_s, u"position"_s }, { u"path"_s, path }, { u"position"_s, 42.5 } });
        const QJsonObject kept = history().value(u"media:"_s + path).toObject();
        QCOMPARE(kept.value(u"position"_s).toDouble(), 42.5);
        QCOMPARE(kept.value(u"duration"_s).toDouble(), 100.0);
        QVERIFY(kept.value(u"lastUsed"_s).toInteger() > 0);

        const QJsonObject before = history();
        library.reported({ { u"type"_s, u"position"_s }, { u"path"_s, path }, { u"position"_s, -1.0 } });
        library.reported({ { u"type"_s, u"position"_s }, { u"path"_s, path }, { u"position"_s, std::nan("") } });
        library.reported({ { u"type"_s, u"closed"_s }, { u"path"_s, path } });
        library.reported({ { u"type"_s, u"position"_s }, { u"path"_s, QString(5000, u'a') }, { u"position"_s, 1.0 } });
        library.reported({ { u"type"_s, u"position"_s }, { u"position"_s, 1.0 } });
        QCOMPARE(history(), before);
    }

    // refresh() reads QOR_GAMES' folders on a worker thread, then says so.
    void refresh_reads_the_folders_on_a_worker()
    {
        QTemporaryDir games;
        touch(games.filePath(u"orb-run/scenes/orb-run.qml"_s));
        qputenv("QOR_GAMES", games.path().toUtf8());
        QCOMPARE(Library::gameFolders(), QStringList { games.path() });
        Library library;
        QSignalSpy changed(&library, &Library::changed);
        library.refresh();
        QTRY_VERIFY(changed.size() >= 1);
        QVERIFY(!library.scanning());
        QVERIFY(idsOf(library.games()).contains(u"qq:"_s + games.filePath(u"orb-run/scenes/orb-run.qml"_s)));
        // Read again only when forced, within ten seconds.
        library.refresh();
        QVERIFY(!library.scanning());
        library.refresh(true);
        QTRY_VERIFY(!library.scanning());
        qunsetenv("QOR_GAMES");
    }
};

QTEST_GUILESS_MAIN(tst_Library)
#include "tst_library.moc"
