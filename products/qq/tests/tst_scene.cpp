// Are QQ scene files what P3.1 promises? (DIRECTION P3.1, ADR-083 decision 3)
//
// A scene file is canonical QML: read and written back it is the same bytes, and one changed value is one changed line,
// so Projects shows a creator's change as what it is. These cases drive SceneIO through a real QML engine with the
// runtime module "QQ", against the fixtures: tests/fixtures/scenes/sample.qml (every entity type, written by hand in
// the canonical layout), tests/fixtures/first-light.qq.json (the 2D preview's starter scene), and the 2D fixtures that
// tests/fixtures/make_2d.py writes (a Tiled map, its tileset and a sheet of sprite frames, P3.10).
//
// Changed for QOR OS: the look takes the colours a host passes as the runtime starts (QOR_THEME_COLORS).
// Changed for QOR OS: the Player's materials and TileMap's geometry keep their bindings when Qt has properties of the same name.
// Changed for QOR OS: game-supplied screen text is literal and remains bound to its own screen part.
// Changed for QOR OS: screen drawings made through ScreenPart keep per-instance edits and follow the overlay's scale.
// Changed for QOR OS: panels, bars and project images keep their own drawing values too.
// Changed for QOR OS: the Player shows supplied startup problems and filenames literally.
// Changed for QOR OS: Player labels follow edits to their different scene and follower objects.
// Changed for QOR OS: groups and prefabs retain their structural parent-state bindings.
// Changed for QOR OS: a Game keeps its loaded scene separate from the viewport's own scene.
// Changed for QOR OS: standalone Game tests own a hidden window so Qt can retire their scene managers.
// Changed for QOR OS: a shape's body follows view and playing changes through mixed structural parents.
// Changed for QOR OS: Shape motion commands refuse missing or mismatched physics nodes without a script error.
// Changed for QOR OS: triggers preserve parent state, collider ownership and optional entity callbacks with typed bodies.
// Changed for QOR OS: Player motion requires a character controller and follows structural scene state.
// Changed for QOR OS: Player2D carries only a QQ Sprite and preserves structural clock and carrier state.
// Changed for QOR OS: the world's initial sky and later colour edits each build one owned texture.
// Changed for QOR OS: shipped sky palettes stay baked across edits, with procedural and image fallback intact.
// Changed for QOR OS: Game keeps its structural scene and follower bindings, startup state and host messages.
// Changed for QOR OS: Behaviour forwards events to compatible logic and follows current parents, targets and reloads.
// Changed for QOR OS: runtime kinds preserve structural parent state and TileMap's owned collision shapes.
// Changed for QOR OS: 2D kinds preserve live parent state and Sprite motion rejects mismatched bodies.

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaProperty>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlListReference>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QVector3D>
#include <QtQml/qqmlextensionplugin.h>
#include <QtQuick3D/qquick3dobject.h>

#include <memory>

#include "confinement.h"
#include "input.h"
#include "kinematics.h"
#include "look.h"
#include "logicfile.h"
#include "mixer.h"
#include "run.h"
#include "sceneio.h"
#include "store.h"
#include "tilemap.h"
#include "webvoice.h"

Q_IMPORT_QML_PLUGIN(QQPlugin)

namespace {

QString fixture(const QString &name)
{
    return QDir(QStringLiteral(QQ_FIXTURES)).filePath(name);
}

QString readAll(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

/// The entity called `name`, anywhere in the scene (in groups too, P3.15).
QObject *entityNamed(QObject *scene, const QString &name)
{
    for (QQuick3DObject *child : qobject_cast<QQuick3DObject *>(scene)->childItems()) {
        if (!child->property("kind").toString().isEmpty() && child->property("name").toString() == name)
            return child;
        if (QObject *inside = entityNamed(child, name))
            return inside;
    }
    return nullptr;
}

/// The lines of `after` that differ from `before`, when both have the same number of lines.
QStringList changedLines(const QString &before, const QString &after)
{
    const QStringList a = before.split(QLatin1Char('\n'));
    const QStringList b = after.split(QLatin1Char('\n'));
    if (a.size() != b.size())
        return {QStringLiteral("<line count changed: %1 to %2>").arg(a.size()).arg(b.size())};
    QStringList changed;
    for (qsizetype i = 0; i < a.size(); ++i) {
        if (a[i] != b[i])
            changed << b[i];
    }
    return changed;
}

}  // namespace

class TstScene : public QObject
{
    Q_OBJECT

    QQmlEngine *engine = nullptr;
    qq::SceneIO *io = nullptr;

private slots:
    void initTestCase()
    {
        // Games' stores (P3.16) go to the test's own folders, never a person's.
        QStandardPaths::setTestModeEnabled(true);
        engine = new QQmlEngine(this);
        io = engine->singletonInstance<qq::SceneIO *>("QQ", "SceneIO");
        QVERIFY(io);
    }

    void cleanup()
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void numbersAreWrittenOneWayOnly()
    {
        QCOMPARE(qq::formatNumber(1.0), QStringLiteral("1"));
        QCOMPARE(qq::formatNumber(0.1 + 0.2), QStringLiteral("0.3"));
        QCOMPARE(qq::formatNumber(12.345678), QStringLiteral("12.3457"));
        QCOMPARE(qq::formatNumber(-0.00001), QStringLiteral("0"));
        QCOMPARE(qq::formatNumber(double(1.15f)), QStringLiteral("1.15"));
        QCOMPARE(qq::formatNumber(-2.5), QStringLiteral("-2.5"));
    }

    void thePlayersBodyKeepsItsColourOnEveryPart()
    {
        std::unique_ptr<QObject> scene(io->loadText(QStringLiteral(
            "import QQ\nScene { Player { name: \"Hero\"; colour: \"#3684da\" } }\n"), QUrl(), nullptr));
        QVERIFY2(scene, qPrintable(io->lastError()));
        QObject *player = entityNamed(scene.get(), QStringLiteral("Hero"));
        QVERIFY(player);
        QList<QObject *> coloured;
        int models = 0;
        for (QObject *child : player->findChildren<QObject *>()) {
            QQmlListReference materials(child, "materials");
            if (!materials.isValid())
                continue;
            ++models;
            QCOMPARE(materials.count(), 1);
            QObject *material = materials.at(0);
            QVERIFY(material);
            if (material->property("baseColor").value<QColor>() == QColor(QStringLiteral("#3684da")))
                coloured.append(material);
        }
        QCOMPARE(models, 4);
        QCOMPARE(coloured.size(), 3);
        QVERIFY(player->setProperty("colour", QColor(QStringLiteral("#c14953"))));
        for (QObject *material : coloured)
            QCOMPARE(material->property("baseColor").value<QColor>(), QColor(QStringLiteral("#c14953")));
    }

    void aTileMapsGeometryFollowsItsMapAndScale()
    {
        const QUrl file = QUrl::fromLocalFile(fixture(QStringLiteral("bindings.qml")));
        std::unique_ptr<QObject> scene(io->loadText(QStringLiteral(
            "import QQ\nScene { pixelsPerUnit: 16; TileMap { name: \"Level\"; source: \"level.tmj\" } }\n"), file, nullptr));
        QVERIFY2(scene, qPrintable(io->lastError()));
        QObject *level = entityNamed(scene.get(), QStringLiteral("Level"));
        QVERIFY(level);
        auto *geometry = level->findChild<qq::TileGeometry *>();
        QVERIFY(geometry);
        QCOMPARE(level->property("error").toString(), QString());
        QCOMPARE(geometry->tiles(), 16);
        QCOMPARE(level->property("count").toInt(), 16);
        QCOMPARE(geometry->map(), io->readTiledMap(QUrl::fromLocalFile(fixture(QStringLiteral("level.tmj")))));
        QCOMPARE(geometry->pixelsPerUnit(), 16.0);
        QVERIFY(scene->setProperty("pixelsPerUnit", 32.0));
        QCOMPARE(geometry->pixelsPerUnit(), 32.0);
        QVERIFY(level->setProperty("layer", QStringLiteral("decor")));
        QCOMPARE(geometry->tiles(), 1);
        QCOMPARE(level->property("count").toInt(), 1);
    }

    void screenLabelsShowGameTextLiterally_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::newRow("text") << QStringLiteral("ScreenText");
        QTest::newRow("button") << QStringLiteral("ScreenButton");
    }

    void screenLabelsShowGameTextLiterally()
    {
        QFETCH(QString, kind);
        // Use ScreenPart.draw(), which creates the drawing from a different component's function, on an isolated overlay.
        QQmlComponent host(engine);
        host.setData("import QtQuick\nimport QtQuick3D\nNode { property QtObject qqView: QtObject { "
                     "property Item overlay: Item { width: 1280; height: 720 } } }\n", QUrl());
        std::unique_ptr<QObject> placed(host.create());
        QVERIFY2(placed, qPrintable(host.errorString()));
        std::unique_ptr<QObject> scene(io->loadText(QStringLiteral(
            "import QQ\nScene { %1 { name: \"First\"; text: \"<b>First</b>\" } "
            "%1 { name: \"Second\"; text: \"<i>Second</i>\" } }\n").arg(kind), QUrl(), placed.get()));
        QVERIFY2(scene, qPrintable(io->lastError()));
        QObject *first = entityNamed(scene.get(), QStringLiteral("First"));
        QObject *second = entityNamed(scene.get(), QStringLiteral("Second"));
        QVERIFY(first);
        QVERIFY(second);
        QObject *firstVisual = first->property("content").value<QObject *>();
        QObject *secondVisual = second->property("content").value<QObject *>();
        QVERIFY(firstVisual);
        QVERIFY(secondVisual);
        const auto textIn = [](QObject *visual) -> QObject * {
            if (visual->metaObject()->indexOfProperty("textFormat") >= 0)
                return visual;
            for (QObject *child : visual->findChildren<QObject *>()) {
                if (child->metaObject()->indexOfProperty("textFormat") >= 0)
                    return child;
            }
            return nullptr;
        };
        QObject *firstLabel = textIn(firstVisual);
        QObject *secondLabel = textIn(secondVisual);
        QVERIFY(firstLabel);
        QVERIFY(secondLabel);
        const QMetaProperty format = firstLabel->metaObject()->property(firstLabel->metaObject()->indexOfProperty("textFormat"));
        const int plain = format.enumerator().keyToValue("PlainText");
        QVERIFY(plain >= 0);
        QCOMPARE(firstLabel->property("textFormat").toInt(), plain);
        QCOMPARE(secondLabel->property("textFormat").toInt(), plain);
        const auto shown = [&kind](const QString &text) { return kind == QStringLiteral("ScreenButton") ? text.toUpper() : text; };
        QCOMPARE(firstLabel->property("text").toString(), shown(QStringLiteral("<b>First</b>")));
        QCOMPARE(secondLabel->property("text").toString(), shown(QStringLiteral("<i>Second</i>")));
        QVERIFY(first->setProperty("text", QStringLiteral("<b>Changed</b>")));
        QCOMPARE(firstLabel->property("text").toString(), shown(QStringLiteral("<b>Changed</b>")));
        QCOMPARE(secondLabel->property("text").toString(), shown(QStringLiteral("<i>Second</i>")));
        const int firstSize = firstLabel->property("font").value<QFont>().pixelSize();
        const int secondSize = secondLabel->property("font").value<QFont>().pixelSize();
        QVERIFY(firstSize > 0);
        QVERIFY(secondSize > 0);
        QObject *layer = first->property("layer").value<QObject *>();
        QVERIFY(layer);
        QVERIFY(layer->setProperty("height", 1440));
        QCOMPARE(firstLabel->property("font").value<QFont>().pixelSize(), firstSize * 2);
        QCOMPARE(secondLabel->property("font").value<QFont>().pixelSize(), secondSize * 2);
        if (kind == QStringLiteral("ScreenText")) {
            QVERIFY(first->setProperty("size", 40));
            QCOMPARE(firstLabel->property("font").value<QFont>().pixelSize(), 80);
            QCOMPARE(secondLabel->property("font").value<QFont>().pixelSize(), secondSize * 2);
        } else {
            QVERIFY(first->setProperty("width", 230));
            QCOMPARE(firstVisual->property("width").toDouble(), 460.0);
            QVERIFY(secondVisual->property("width").toDouble() != 460.0);
        }
    }

    void screenShapesFollowTheirOwnPart_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::newRow("panel") << QStringLiteral("ScreenPanel");
        QTest::newRow("bar") << QStringLiteral("ScreenBar");
        QTest::newRow("image") << QStringLiteral("ScreenImage");
    }

    void screenShapesFollowTheirOwnPart()
    {
        QFETCH(QString, kind);
        QTemporaryDir project;
        QVERIFY(project.isValid());
        if (kind == QStringLiteral("ScreenImage")) {
            QImage picture(4, 4, QImage::Format_RGB32);
            picture.fill(Qt::blue);
            QVERIFY(picture.save(QDir(project.path()).filePath(QStringLiteral("probe.png"))));
        }
        QQmlComponent host(engine);
        host.setData("import QtQuick\nimport QtQuick3D\nNode { property QtObject qqView: QtObject { "
                     "property Item overlay: Item { width: 1280; height: 720 } } }\n", QUrl());
        std::unique_ptr<QObject> placed(host.create());
        QVERIFY2(placed, qPrintable(host.errorString()));
        const QString source = kind == QStringLiteral("ScreenImage") ? QStringLiteral("source: \"probe.png\";") : QString();
        std::unique_ptr<QObject> scene(io->loadText(QStringLiteral(
            "import QQ\nScene { %1 { name: \"First\"; width: 120; height: 60; %2 } "
            "%1 { name: \"Second\"; width: 80; height: 40; %2 } }\n").arg(kind, source),
            QUrl::fromLocalFile(QDir(project.path()).filePath(QStringLiteral("scene.qml"))), placed.get()));
        QVERIFY2(scene, qPrintable(io->lastError()));
        QObject *first = entityNamed(scene.get(), QStringLiteral("First"));
        QObject *second = entityNamed(scene.get(), QStringLiteral("Second"));
        QVERIFY(first);
        QVERIFY(second);
        QObject *firstVisual = first->property("content").value<QObject *>();
        QObject *secondVisual = second->property("content").value<QObject *>();
        QVERIFY(firstVisual);
        QVERIFY(secondVisual);
        QCOMPARE(firstVisual->property("width").toDouble(), 120.0);
        QCOMPARE(secondVisual->property("width").toDouble(), 80.0);
        QVERIFY(first->setProperty("width", 100));
        QCOMPARE(firstVisual->property("width").toDouble(), 100.0);
        QCOMPARE(secondVisual->property("width").toDouble(), 80.0);
        QObject *layer = first->property("layer").value<QObject *>();
        QVERIFY(layer);
        QVERIFY(layer->setProperty("height", 1440));
        QCOMPARE(firstVisual->property("width").toDouble(), 200.0);
        QCOMPARE(secondVisual->property("width").toDouble(), 160.0);
        QCOMPARE(firstVisual->property("height").toDouble(), 120.0);
        QCOMPARE(secondVisual->property("height").toDouble(), 80.0);

        if (kind == QStringLiteral("ScreenPanel")) {
            QVERIFY(first->setProperty("width", 0));
            QCOMPARE(firstVisual->property("width").toDouble(), 1280.0);
            QCOMPARE(firstVisual->property("height").toDouble(), 1440.0);
            QCOMPARE(secondVisual->property("width").toDouble(), 160.0);
            QVERIFY(first->setProperty("shade", 0.25));
            QVERIFY(qAbs(firstVisual->property("color").value<QColor>().alphaF() - 0.25) < 0.001);
            QVERIFY(qAbs(secondVisual->property("color").value<QColor>().alphaF() - 0.6) < 0.001);
        } else if (kind == QStringLiteral("ScreenBar")) {
            QObject *fill = firstVisual->findChild<QObject *>(QStringLiteral("fill"));
            QObject *otherFill = secondVisual->findChild<QObject *>(QStringLiteral("fill"));
            QVERIFY(fill);
            QVERIFY(otherFill);
            QVERIFY(first->setProperty("value", 0.25));
            QTRY_COMPARE(fill->property("width").toDouble(), 49.0);
            QTRY_COMPARE(otherFill->property("width").toDouble(), 156.0);
        } else {
            QTRY_VERIFY(first->property("ready").toBool());
            QTRY_VERIFY(second->property("ready").toBool());
            QCOMPARE(firstVisual->property("source").toUrl(), QUrl::fromLocalFile(QDir(project.path()).filePath(QStringLiteral("probe.png"))));
            QVERIFY(first->setProperty("crisp", true));
            QCOMPARE(firstVisual->property("smooth").toBool(), false);
            QCOMPARE(firstVisual->property("mipmap").toBool(), false);
            QCOMPARE(secondVisual->property("smooth").toBool(), true);
            QCOMPARE(secondVisual->property("mipmap").toBool(), true);
            QFile broken(QDir(project.path()).filePath(QStringLiteral("broken.png")));
            QVERIFY(broken.open(QIODevice::WriteOnly));
            const QByteArray malformed("not a PNG");
            QCOMPARE(broken.write(malformed), qint64(malformed.size()));
            broken.close();
            QVERIFY(first->setProperty("source", QUrl::fromLocalFile(broken.fileName())));
            const QMetaProperty status = firstVisual->metaObject()->property(firstVisual->metaObject()->indexOfProperty("status"));
            const int error = status.enumerator().keyToValue("Error");
            QVERIFY(error >= 0);
            QTRY_COMPARE(firstVisual->property("status").toInt(), error);
            QCOMPARE(first->property("ready").toBool(), false);
            QCOMPARE(second->property("ready").toBool(), true);
        }
    }

    void aSceneFileIsReadAndWrittenBackByteForByte()
    {
        const QString path = fixture(QStringLiteral("scenes/sample.qml"));
        QObject *scene = io->load(QUrl::fromLocalFile(path), nullptr);
        QVERIFY2(scene, qPrintable(io->lastError()));
        const QString written = io->write(scene, QUrl::fromLocalFile(path));
        QVERIFY2(written == readAll(path), qPrintable(changedLines(readAll(path), written).join(QLatin1Char('\n'))));
        // Every entity type is in it, in its file order.
        QStringList kinds;
        for (QQuick3DObject *child : qobject_cast<QQuick3DObject *>(scene)->childItems())
            kinds << child->property("kind").toString();
        QCOMPARE(kinds, (QStringList{QStringLiteral("Sun"), QStringLiteral("Ground"), QStringLiteral("Shape"),
                                     QStringLiteral("Lamp"), QStringLiteral("Prop"), QStringLiteral("Player"),
                                     QStringLiteral("Emitter"), QStringLiteral("Sound"), QStringLiteral("Behaviour"),
                                     QStringLiteral("Trigger"), QStringLiteral("ScreenText"), QStringLiteral("ScreenBar"),
                                     QStringLiteral("ScreenImage"), QStringLiteral("ScreenPanel"),
                                     QStringLiteral("ScreenButton"), QStringLiteral("InputAction"),
                                     QStringLiteral("Sprite"), QStringLiteral("TileMap"), QStringLiteral("Player2D"),
                                     QStringLiteral("Music"), QStringLiteral("Spotlight"), QStringLiteral("Group"),
                                     QStringLiteral("Prefab")}));
        // Every kind there is.
        QStringList sorted = kinds;
        sorted.sort();
        QStringList all = io->kinds();
        all.sort();
        QCOMPARE(sorted, all);
        delete scene;
    }

    void changingOneValueChangesOneLine()
    {
        const QString path = fixture(QStringLiteral("scenes/sample.qml"));
        const QUrl url = QUrl::fromLocalFile(path);
        QObject *scene = io->load(url, nullptr);
        QVERIFY(scene);
        const QString before = io->write(scene, url);

        QObject *plinth = entityNamed(scene, QStringLiteral("Plinth"));
        QVERIFY(plinth);
        plinth->setProperty("colour", QColor(QStringLiteral("#ff6a00")));
        QStringList changed = changedLines(before, io->write(scene, url));
        QCOMPARE(changed, QStringList{QStringLiteral("        colour: \"#ff6a00\"")});

        plinth->setProperty("position", QVector3D(0, 0.25f, -1.5f));
        changed = changedLines(before, io->write(scene, url));
        QCOMPARE(changed.size(), 2);
        QVERIFY(changed.contains(QStringLiteral("        position: Qt.vector3d(0, 0.25, -1.5)")));
        delete scene;
    }

    void aSavedSceneKeepsItsModelRelativeAndLoadsAgain()
    {
        QTemporaryDir project;
        QVERIFY(project.isValid());
        const QUrl projectUrl = QUrl::fromLocalFile(project.path());

        QObject *scene = io->load(QUrl::fromLocalFile(fixture(QStringLiteral("scenes/sample.qml"))), nullptr);
        QVERIFY(scene);
        QObject *orb = entityNamed(scene, QStringLiteral("Orb"));
        const QUrl adopted = io->adopt(io->resolvedUrl(orb, orb->property("source").toUrl()), projectUrl);
        QVERIFY2(!adopted.isEmpty(), qPrintable(io->lastError()));
        QCOMPARE(adopted.toLocalFile(), QDir(project.path()).filePath(QStringLiteral("assets/orb.gltf")));
        orb->setProperty("source", adopted);

        const QUrl file = QUrl::fromLocalFile(QDir(project.path()).filePath(QStringLiteral("scenes/sample.qml")));
        QCOMPARE(io->save(scene, file), QString());
        const QString saved = readAll(file.toLocalFile());
        QVERIFY2(saved.contains(QStringLiteral("        source: \"../assets/orb.gltf\"\n")), qPrintable(saved));
        QVERIFY(!saved.contains(project.path()));

        QObject *again = io->load(file, nullptr);
        QVERIFY2(again, qPrintable(io->lastError()));
        QObject *orbAgain = entityNamed(again, QStringLiteral("Orb"));
        QCOMPARE(io->resolvedUrl(orbAgain, orbAgain->property("source").toUrl()), adopted);
        QCOMPARE(io->write(again, file), saved);
        delete again;
        delete scene;
    }

    void adoptingAModelBringsItsFilesAndNeverOverwrites()
    {
        QTemporaryDir outside;
        QTemporaryDir project;
        const QDir out(outside.path());
        // A .gltf that keeps its buffer in a file beside it.
        QFile bin(out.filePath(QStringLiteral("tri.bin")));
        QVERIFY(bin.open(QIODevice::WriteOnly));
        bin.write(QByteArray(36, '\1'));
        bin.close();
        QFile gltf(out.filePath(QStringLiteral("tri.gltf")));
        QVERIFY(gltf.open(QIODevice::WriteOnly));
        gltf.write(R"({"asset":{"version":"2.0"},"buffers":[{"uri":"tri.bin","byteLength":36}]})");
        gltf.close();

        const QUrl projectUrl = QUrl::fromLocalFile(project.path());
        const QUrl first = io->adopt(QUrl::fromLocalFile(gltf.fileName()), projectUrl);
        QVERIFY2(!first.isEmpty(), qPrintable(io->lastError()));
        QVERIFY(QFileInfo::exists(QDir(project.path()).filePath(QStringLiteral("assets/tri.bin"))));
        // The same file again is the same asset.
        QCOMPARE(io->adopt(QUrl::fromLocalFile(gltf.fileName()), projectUrl), first);
        // A different file of the same name is kept beside it.
        QVERIFY(gltf.open(QIODevice::WriteOnly | QIODevice::Append));
        gltf.write(" ");
        gltf.close();
        const QUrl second = io->adopt(QUrl::fromLocalFile(gltf.fileName()), projectUrl);
        QCOMPARE(QFileInfo(second.toLocalFile()).fileName(), QStringLiteral("tri-2.gltf"));
        // A model already in the project stays where it is.
        QCOMPARE(io->adopt(first, projectUrl), first);
    }

    void soundsAndLogicAreAdoptedIntoTheirOwnFolders()
    {
        QTemporaryDir project;
        const QUrl projectUrl = QUrl::fromLocalFile(project.path());
        const QUrl sound = io->adopt(QUrl::fromLocalFile(fixture(QStringLiteral("chime.wav"))), projectUrl);
        QCOMPARE(sound.toLocalFile(), QDir(project.path()).filePath(QStringLiteral("assets/chime.wav")));
        const QUrl logic = io->adopt(QUrl::fromLocalFile(fixture(QStringLiteral("logic/spin.qml"))), projectUrl);
        QCOMPARE(logic.toLocalFile(), QDir(project.path()).filePath(QStringLiteral("logic/spin.qml")));
        QCOMPARE(readAll(logic.toLocalFile()), readAll(fixture(QStringLiteral("logic/spin.qml"))));
    }

    void playingIsACopyAndTheSceneIsNotTouched()
    {
        const QUrl url = QUrl::fromLocalFile(fixture(QStringLiteral("scenes/sample.qml")));
        QObject *scene = io->load(url, nullptr);
        QVERIFY(scene);
        const QString before = io->write(scene, url);
        QObject *copy = io->play(scene, url, nullptr);
        QVERIFY2(copy, qPrintable(io->lastError()));
        QVERIFY(copy != scene);
        QCOMPARE(copy->property("playing").toBool(), true);
        QCOMPARE(scene->property("playing").toBool(), false);
        // Every entity of the copy that changes in play knows it plays; none of the scene's does.
        int live = 0;
        for (const QVariant &e : io->entities(copy))
            live += e.value<QObject *>()->property("live").toBool() ? 1 : 0;
        // The ground, the shape, the model (its clip plays), the player, the sound, the behaviour, the trigger, the five
        // screen parts, the action, the sprite, the tile map, the 2D player, the music, the spotlight (its mark hidden),
        // and the group, the shape in it and the prefab (P3.15); the rest are the same in play.
        QCOMPARE(live, 21);
        for (const QVariant &e : io->entities(scene))
            QVERIFY(!e.value<QObject *>()->property("live").toBool());
        // The copy is the scene, and the scene is as it was; `playing` is not part of either's file.
        QCOMPARE(io->write(copy, url), before);
        QCOMPARE(io->write(scene, url), before);
        QVERIFY(!before.contains(QStringLiteral("playing")));
        QVERIFY(!io->play(entityNamed(scene, QStringLiteral("Orb")), url, nullptr));
        io->discard(copy);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        delete scene;
    }

    void whatIsNotASceneIsRefusedWithAReason()
    {
        QVERIFY(!io->loadText(QStringLiteral("import QQ\nShape {}\n"), QUrl(), nullptr));
        QVERIFY2(io->lastError().contains(QStringLiteral("not a QQ scene")), qPrintable(io->lastError()));
        QVERIFY(!io->loadText(QStringLiteral("import QQ\nScene { this is not QML"), QUrl(), nullptr));
        QVERIFY(!io->lastError().isEmpty());
        QVERIFY(!io->load(QUrl::fromLocalFile(fixture(QStringLiteral("scenes/no-such-scene.qml"))), nullptr));
        QVERIFY(!io->lastError().isEmpty());
    }

    void aTwoDimensionalSceneIsImportedAsACanonicalOne()
    {
        const QVariantMap result = io->importQqJson(QUrl::fromLocalFile(fixture(QStringLiteral("first-light.qq.json"))));
        QCOMPARE(result.value(QStringLiteral("error")).toString(), QString());
        QCOMPARE(result.value(QStringLiteral("imported")).toInt(), 4);  // three shapes and an emitter
        const QStringList skipped = result.value(QStringLiteral("skipped")).toStringList();
        QCOMPARE(skipped.size(), 2);
        QVERIFY(skipped.filter(QStringLiteral("Core: follow")).size() == 1);
        QVERIFY(skipped.filter(QStringLiteral("Ring: motion")).size() == 1);

        const QString qml = result.value(QStringLiteral("qml")).toString();
        QObject *scene = io->loadText(qml, QUrl(), nullptr);
        QVERIFY2(scene, qPrintable(io->lastError()));
        QCOMPARE(io->write(scene), qml);  // canonical
        // Still a 2D scene (P3.10): flat, 100 pixels to a unit, the view its height of 540 pixels, its backdrop its own.
        QCOMPARE(scene->property("view").toString(), QStringLiteral("2d"));
        QCOMPARE(scene->property("pixelsPerUnit").toDouble(), 100.0);
        QCOMPARE(scene->property("viewHeight").toDouble(), 5.4);
        QCOMPARE(scene->property("pixelArt").toBool(), false);
        QCOMPARE(scene->property("skyTop").value<QColor>(), QColor(QStringLiteral("#07080c")));
        QObject *core = entityNamed(scene, QStringLiteral("Core"));
        QVERIFY(core);
        QCOMPARE(core->property("form").toString(), QStringLiteral("sphere"));
        QCOMPARE(core->property("position").value<QVector3D>(), QVector3D(0, 2.7f, 0));
        QCOMPARE(core->property("emissive").value<QColor>(), QColor(QStringLiteral("#ff6a00")));
        QObject *ring = entityNamed(scene, QStringLiteral("Ring"));
        QCOMPARE(ring->property("form").toString(), QStringLiteral("cube"));
        QCOMPARE(ring->property("position").value<QVector3D>(), QVector3D(-2.6f, 2.7f, 0));
        // The 2D emitter, as a 3D one where the core is: 110 pixels a second is 1.1 metres, and a full circle a burst.
        QObject *sparks = entityNamed(scene, QStringLiteral("Core sparks"));
        QVERIFY(sparks);
        QCOMPARE(sparks->property("kind").toString(), QStringLiteral("Emitter"));
        QCOMPARE(sparks->property("position").value<QVector3D>(), QVector3D(0, 2.7f, 0));
        QCOMPARE(sparks->property("rate").toDouble(), 90.0);
        QCOMPARE(sparks->property("spread").toDouble(), 1.1);
        QCOMPARE(sparks->property("colour").value<QColor>(), QColor(QStringLiteral("#ffb15c")));
        delete scene;
    }

    void aSceneSavedElsewhereStillPointsAtTheSameModel()
    {
        QTemporaryDir other;
        const QUrl from = QUrl::fromLocalFile(fixture(QStringLiteral("scenes/sample.qml")));
        QObject *scene = io->load(from, nullptr);
        QVERIFY(scene);
        const QUrl moved = QUrl::fromLocalFile(QDir(other.path()).filePath(QStringLiteral("deep/down/moved.qml")));
        QCOMPARE(io->save(scene, moved), QString());
        QObject *again = io->load(moved, nullptr);
        QVERIFY2(again, qPrintable(io->lastError()));
        QObject *orb = entityNamed(again, QStringLiteral("Orb"));
        QCOMPARE(io->resolvedUrl(orb, orb->property("source").toUrl()),
                 QUrl::fromLocalFile(fixture(QStringLiteral("orb.gltf"))));
        delete again;
        delete scene;
    }

    void entitiesAreAddedAndRemovedAndTheFileFollows()
    {
        const QUrl url = QUrl::fromLocalFile(fixture(QStringLiteral("scenes/sample.qml")));
        QObject *scene = io->load(url, nullptr);
        QVERIFY(scene);
        QCOMPARE(io->entities(scene).size(), 24);  // the group's shape among them, not the prefab's parts

        QObject *crystal = io->add(scene, QStringLiteral("Shape"),
                                   {{QStringLiteral("name"), QStringLiteral("Crystal")},
                                    {QStringLiteral("form"), QStringLiteral("cone")},
                                    {QStringLiteral("position"), QVector3D(2, 0.5f, -1)}});
        QVERIFY2(crystal, qPrintable(io->lastError()));
        QCOMPARE(io->entities(scene).size(), 25);
        const QString text = io->write(scene, url);
        QVERIFY2(text.endsWith(QStringLiteral("    Shape {\n        name: \"Crystal\"\n        position: Qt.vector3d(2, 0.5, -1)\n")
                               + QStringLiteral("        eulerRotation: Qt.vector3d(0, 0, 0)\n        scale: Qt.vector3d(1, 1, 1)\n")
                               + QStringLiteral("        form: \"cone\"\n        colour: \"#5ad1ff\"\n        metalness: 0\n")
                               + QStringLiteral("        roughness: 0.4\n        emissive: \"#000000\"\n        emissivePower: 0\n")
                               + QStringLiteral("        image: \"\"\n        imageSize: 1\n        normalMap: \"\"\n        crisp: false\n")
                               + QStringLiteral("        body: \"static\"\n        mass: 1\n        bounce: 0.2\n        friction: 0.6\n")
                               + QStringLiteral("    }\n}\n")),
                 qPrintable(text.right(400)));

        // What is not an entity kind, or not a scene, is refused.
        QVERIFY(!io->add(scene, QStringLiteral("Window"), {}));
        QVERIFY(io->lastError().contains(QStringLiteral("no entity called")));
        QVERIFY(!io->add(crystal, QStringLiteral("Shape"), {}));

        QVERIFY(io->remove(crystal));
        QVERIFY(!io->remove(scene));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(io->entities(scene).size(), 24);
        QCOMPARE(io->write(scene, url), readAll(url.toLocalFile()));
        delete scene;
    }

    void aProjectFolderListsItsScenes()
    {
        QTemporaryDir project;
        const QDir root(project.path());
        root.mkpath(QStringLiteral("scenes"));
        for (const char *name : {"b-level.qml", "a-level.qml", "notes.txt"}) {
            QFile f(root.filePath(QStringLiteral("scenes/") + QLatin1String(name)));
            QVERIFY(f.open(QIODevice::WriteOnly));
        }
        QVariantMap info = io->project(QUrl::fromLocalFile(project.path()));
        QCOMPARE(info.value(QStringLiteral("scenes")).toStringList(),
                 (QStringList{QStringLiteral("a-level"), QStringLiteral("b-level")}));
        QCOMPARE(info.value(QStringLiteral("repository")).toBool(), false);
        root.mkdir(QStringLiteral(".git"));
        info = io->project(QUrl::fromLocalFile(project.path()));
        QCOMPARE(info.value(QStringLiteral("repository")).toBool(), true);
    }

    void theBundledSampleCanBeAdoptedIntoAProject()
    {
        QTemporaryDir project;
        const QUrl adopted = io->adopt(QUrl(QStringLiteral("qrc:/fixtures/orb.gltf")),
                                       QUrl::fromLocalFile(project.path()));
        QVERIFY2(!adopted.isEmpty(), qPrintable(io->lastError()));
        QVERIFY(QFileInfo::exists(adopted.toLocalFile()));
    }

    void aFileOfAnotherFormatIsNotImported()
    {
        QTemporaryDir dir;
        QFile f(QDir(dir.path()).filePath(QStringLiteral("two.qq.json")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"qq": 2, "entities": []})");
        f.close();
        const QVariantMap result = io->importQqJson(QUrl::fromLocalFile(f.fileName()));
        QVERIFY(result.value(QStringLiteral("error")).toString().contains(QStringLiteral("format 1")));
        QVERIFY(result.value(QStringLiteral("qml")).toString().isEmpty());
    }

    // ── 2D (P3.10, ADR-105) ──

    void aTiledMapIsReadWithItsTilesetAndItsSolidsMerged()
    {
        const QUrl level = QUrl::fromLocalFile(fixture(QStringLiteral("level.tmj")));
        QVariantMap map = io->readTiledMap(level);
        QCOMPARE(map.value(QStringLiteral("error")).toString(), QString());
        QCOMPARE(map.value(QStringLiteral("width")).toInt(), 10);
        QCOMPARE(map.value(QStringLiteral("height")).toInt(), 5);
        QCOMPARE(map.value(QStringLiteral("tileWidth")).toInt(), 16);
        // The tileset is the external one beside the map, and its image is beside that.
        QCOMPARE(map.value(QStringLiteral("image")).toUrl(), QUrl::fromLocalFile(fixture(QStringLiteral("tiles.png"))));
        QCOMPARE(map.value(QStringLiteral("imageWidth")).toInt(), 32);
        QCOMPARE(map.value(QStringLiteral("tileColumns")).toInt(), 2);
        QCOMPARE(map.value(QStringLiteral("tileCount")).toInt(), 4);
        QCOMPARE(map.value(QStringLiteral("firstGid")).toInt(), 1);
        const QVariantList gids = map.value(QStringLiteral("gids")).toList();
        QCOMPARE(gids.size(), 50);
        QCOMPARE(gids.at(9).toLongLong(), 3);                       // the wall's top
        QCOMPARE(gids.at(44).toLongLong(), qint64(0x80000002u));    // the mirrored green, its flag kept
        // The wall, the ledge and the floor: three colliders for sixteen tiles.
        const QVariantList solids = map.value(QStringLiteral("solids")).toList();
        QCOMPARE(solids, (QVariantList{QVariant(QVariantList{9, 0, 1, 4}), QVariant(QVariantList{5, 1, 2, 1}),
                                        QVariant(QVariantList{0, 4, 10, 1})}));

        // Another layer, by its name.
        map = io->readTiledMap(level, QStringLiteral("decor"));
        QCOMPARE(map.value(QStringLiteral("solids")).toList(), (QVariantList{QVariant(QVariantList{2, 2, 1, 1})}));
        map = io->readTiledMap(level, QStringLiteral("clouds"));
        QVERIFY2(map.value(QStringLiteral("error")).toString().contains(QStringLiteral("no tile layer called clouds")),
                 qPrintable(map.value(QStringLiteral("error")).toString()));

        // The image's size, for a sprite's.
        QCOMPARE(io->imageSize(QUrl::fromLocalFile(fixture(QStringLiteral("sheet.png")))), QSize(32, 16));
        QCOMPARE(io->imageSize(QUrl::fromLocalFile(fixture(QStringLiteral("no-such.png")))), QSize());
    }

    void aMapSavedAsBase64WithItsTilesetInsideReadsTheSame()
    {
        // Tiled's other common way: tiles as zlib-compressed Base64, the tileset kept in the map.
        QFile f(fixture(QStringLiteral("level.tmj")));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QJsonObject json = QJsonDocument::fromJson(f.readAll()).object();
        QJsonArray layers = json.value(QStringLiteral("layers")).toArray();
        QJsonObject ground = layers.at(0).toObject();
        QByteArray bytes;
        for (const QJsonValue &v : ground.value(QStringLiteral("data")).toArray()) {
            const auto gid = quint32(v.toDouble());
            for (int i = 0; i < 4; ++i)
                bytes.append(char((gid >> (8 * i)) & 0xff));
        }
        // qCompress puts the size first; Tiled's stream is the zlib stream alone.
        ground.insert(QStringLiteral("data"), QString::fromLatin1(qCompress(bytes).mid(4).toBase64()));
        ground.insert(QStringLiteral("encoding"), QStringLiteral("base64"));
        ground.insert(QStringLiteral("compression"), QStringLiteral("zlib"));
        layers.replace(0, ground);
        json.insert(QStringLiteral("layers"), layers);
        json.insert(QStringLiteral("tilesets"), QJsonArray{QJsonObject{
            {QStringLiteral("firstgid"), 1}, {QStringLiteral("columns"), 2}, {QStringLiteral("image"), QStringLiteral("art/tiles.png")},
            {QStringLiteral("imagewidth"), 32}, {QStringLiteral("imageheight"), 32}, {QStringLiteral("tilecount"), 4},
            {QStringLiteral("tilewidth"), 16}, {QStringLiteral("tileheight"), 16}}});
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("art")));
        QVERIFY(QFile::copy(fixture(QStringLiteral("tiles.png")), QDir(dir.path()).filePath(QStringLiteral("art/tiles.png"))));
        QFile out(QDir(dir.path()).filePath(QStringLiteral("packed.tmj")));
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(QJsonDocument(json).toJson());
        out.close();

        const QVariantMap plain = io->readTiledMap(QUrl::fromLocalFile(fixture(QStringLiteral("level.tmj"))));
        const QVariantMap packed = io->readTiledMap(QUrl::fromLocalFile(out.fileName()));
        QCOMPARE(packed.value(QStringLiteral("error")).toString(), QString());
        QCOMPARE(packed.value(QStringLiteral("gids")), plain.value(QStringLiteral("gids")));
        QCOMPARE(packed.value(QStringLiteral("solids")), plain.value(QStringLiteral("solids")));
        QCOMPARE(packed.value(QStringLiteral("image")).toUrl(),
                 QUrl::fromLocalFile(QDir(dir.path()).filePath(QStringLiteral("art/tiles.png"))));
        QCOMPARE(qq::tiledMapNeeds(out.fileName()), QStringList{QStringLiteral("art/tiles.png")});
    }

    void whatAMapCannotBeIsRefusedWithAReason_data()
    {
        QTest::addColumn<QByteArray>("change");
        QTest::addColumn<QString>("reason");
        QTest::newRow("infinite") << QByteArray("\"infinite\": true") << QStringLiteral("Infinite maps");
        QTest::newRow("isometric") << QByteArray("\"orientation\": \"isometric\"") << QStringLiteral("orthogonal");
        QTest::newRow("zstd") << QByteArray("\"compression\": \"zstd\"") << QStringLiteral("zstd");
        QTest::newRow("short") << QByteArray("\"width\": 11") << QStringLiteral("room for 55");
        QTest::newRow("no tileset") << QByteArray("\"tilesets\": []") << QStringLiteral("no tileset");
        QTest::newRow("images") << QByteArray("\"tilesets\": [{\"firstgid\": 1, \"tiles\": []}]")
                                << QStringLiteral("collection of images");
    }

    void whatAMapCannotBeIsRefusedWithAReason()
    {
        QFETCH(QByteArray, change);
        QFETCH(QString, reason);
        QFile f(fixture(QStringLiteral("level.tmj")));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QJsonObject json = QJsonDocument::fromJson(f.readAll()).object();
        const QJsonObject patch = QJsonDocument::fromJson("{" + change + "}").object();
        QVERIFY(!patch.isEmpty());
        for (auto it = patch.begin(); it != patch.end(); ++it) {
            if (it.key() == QStringLiteral("compression")) {
                QJsonArray layers = json.value(QStringLiteral("layers")).toArray();
                QJsonObject ground = layers.at(0).toObject();
                ground.insert(QStringLiteral("encoding"), QStringLiteral("base64"));
                ground.insert(QStringLiteral("data"), QStringLiteral("AAAA"));
                ground.insert(it.key(), it.value());
                layers.replace(0, ground);
                json.insert(QStringLiteral("layers"), layers);
            } else {
                json.insert(it.key(), it.value());
            }
        }
        QTemporaryDir dir;
        QFile out(QDir(dir.path()).filePath(QStringLiteral("bad.tmj")));
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(QJsonDocument(json).toJson());
        out.close();
        const QString error = io->readTiledMap(QUrl::fromLocalFile(out.fileName())).value(QStringLiteral("error")).toString();
        QVERIFY2(error.contains(reason), qPrintable(error));
    }

    void solidTilesBecomeTheFewestRectangles()
    {
        // A U: each side is a run joined down the rows, and its foot is one run.
        //   x . . x
        //   x . . x
        //   x x x x
        const QList<int> grid{1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1};
        QCOMPARE(qq::mergedSolids(grid, 4, 3),
                 (QList<QList<int>>{{0, 0, 1, 2}, {3, 0, 1, 2}, {0, 2, 4, 1}}));
        QCOMPARE(qq::mergedSolids(QList<int>(6, 0), 3, 2), QList<QList<int>>{});
        QCOMPARE(qq::mergedSolids(QList<int>(6, 1), 3, 2), (QList<QList<int>>{{0, 0, 3, 2}}));
    }

    void aMapAdoptedIntoAProjectBringsItsTilesetAndImage()
    {
        QTemporaryDir project;
        const QUrl adopted = io->adopt(QUrl::fromLocalFile(fixture(QStringLiteral("level.tmj"))),
                                       QUrl::fromLocalFile(project.path()));
        QVERIFY2(!adopted.isEmpty(), qPrintable(io->lastError()));
        const QDir assets(QDir(project.path()).filePath(QStringLiteral("assets")));
        QVERIFY(QFileInfo::exists(assets.filePath(QStringLiteral("tiles.tsj"))));
        QVERIFY(QFileInfo::exists(assets.filePath(QStringLiteral("tiles.png"))));
        const QVariantMap map = io->readTiledMap(adopted);
        QCOMPARE(map.value(QStringLiteral("error")).toString(), QString());
        QCOMPARE(map.value(QStringLiteral("image")).toUrl(), QUrl::fromLocalFile(assets.filePath(QStringLiteral("tiles.png"))));

        // A map whose tileset is outside its folder could not keep that path, so it is refused, and nothing is copied.
        QTemporaryDir elsewhere;
        const QDir root(elsewhere.path());
        QVERIFY(root.mkpath(QStringLiteral("maps")));
        QVERIFY(QFile::copy(fixture(QStringLiteral("tiles.tsj")), root.filePath(QStringLiteral("tiles.tsj"))));
        QFile f(fixture(QStringLiteral("level.tmj")));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QByteArray text = f.readAll();
        text.replace("\"tiles.tsj\"", "\"../tiles.tsj\"");
        QFile out(root.filePath(QStringLiteral("maps/far.tmj")));
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(text);
        out.close();
        QVERIFY(io->adopt(QUrl::fromLocalFile(out.fileName()), QUrl::fromLocalFile(project.path())).isEmpty());
        QVERIFY2(io->lastError().contains(QStringLiteral("outside its folder")), qPrintable(io->lastError()));
        QVERIFY(!QFileInfo::exists(assets.filePath(QStringLiteral("far.tmj"))));
    }

    // ── steps and runs (P3.11, ADR-105) ──

    void theSameSeedGivesTheSameNumbersEverywhere()
    {
        // Pinned against an independent implementation of xoshiro128** seeded by splitmix32 (written in Python for this
        // check): a change here would make every run kept before it replay differently.
        qq::Random random;
        random.reset(42);
        QCOMPARE(random.next(), 0.6606157208001621);
        QCOMPARE(random.next(), 0.1117019639532224);
        QCOMPARE(random.next(), 0.0791056456144914);
        QCOMPARE(random.drawn(), 3);
        random.reset(42);
        QList<int> dice;
        for (int i = 0; i < 10; ++i)
            dice << random.between(1, 6);
        QCOMPARE(dice, (QList<int>{4, 1, 1, 1, 6, 2, 6, 5, 3, 4}));
        // Another seed, other numbers; never outside [0, 1) or the range asked.
        random.reset(43);
        QVERIFY(random.next() != 0.6606157208001621);
        for (int i = 0; i < 10000; ++i) {
            const double v = random.next();
            QVERIFY(v >= 0 && v < 1);
            const int d = random.between(-3, 3);
            QVERIFY(d >= -3 && d <= 3);
        }
        QVERIFY(qq::Random::freshSeed() > 0);
    }

    void aBoxIsStoppedByWhatStandsInItsWay()
    {
        // A floor from x -10 to 10, its top at y 0, and a wall from x 2 to 3, up to y 5.
        const QList<qq::Box> solids{{-10, 10, -1, 0, -1, 1}, {2, 3, 0, 5, -1, 1}};
        // Falling onto the floor: stopped with its foot exactly on the top.
        qq::Moved m = qq::sweep2D(solids, 0, 0.6, 0, 0.5, 0.5, 0, -0.3);
        QCOMPARE(m.y, 0.5);
        QVERIFY(m.down && !m.up && !m.left && !m.right);
        // Resting on it, touching is not overlapping: it walks along freely.
        m = qq::sweep2D(solids, 0, 0.5, 0, 0.5, 0.5, 0.4, 0);
        QCOMPARE(m.x, 0.4);
        QCOMPARE(m.y, 0.5);
        QVERIFY(!m.right && !m.down);
        // Into the wall: stopped with its side on the wall's.
        m = qq::sweep2D(solids, 1.4, 0.5, 0, 0.5, 0.5, 0.3, 0);
        QCOMPARE(m.x, 1.5);
        QVERIFY(m.right);
        // From the other side, and under a ceiling.
        m = qq::sweep2D(solids, 3.6, 0.5, 0, 0.5, 0.5, -0.3, 0);
        QCOMPARE(m.x, 3.5);
        QVERIFY(m.left);
        const QList<qq::Box> roof{{-10, 10, 2, 3, -1, 1}};
        m = qq::sweep2D(roof, 0, 1.4, 0, 0.5, 0.5, 0, 0.2);
        QCOMPARE(m.y, 1.5);
        QVERIFY(m.up);
        // What is at another depth is not in the way.
        m = qq::sweep2D(solids, 1.4, 0.5, 5, 0.5, 0.5, 0.3, 0);
        QCOMPARE(m.x, 1.7);
        QVERIFY(!m.right);
        // Across first, then down: a box falling past a wall's corner keeps to its side.
        m = qq::sweep2D(solids, 1.4, 6, 0, 0.5, 0.5, 0.3, -0.6);
        QCOMPARE(m.x, 1.7);
        QCOMPARE(m.y, 5.5);
        QVERIFY(m.down);

        QVERIFY(qq::overlaps({0, 1, 0, 1, 0, 1}, {0.5, 2, 0.5, 2, 0.5, 2}));
        QVERIFY(!qq::overlaps({0, 1, 0, 1, 0, 1}, {1, 2, 0, 1, 0, 1}));  // touching
    }

    void aRunIsKeptAndReadBack()
    {
        qq::RunRecord run;
        run.setScene(QStringLiteral("climb"));
        run.setSeed(7);
        const QVariantMap still{{QStringLiteral("move"), QVariantList{0.0, 0.0}},
                                {QStringLiteral("look"), QVariantList{0.0, 0.0}},
                                {QStringLiteral("held"), QStringList()},
                                {QStringLiteral("edges"), QVariantList()}};
        QVariantMap right = still;
        right[QStringLiteral("move")] = QVariantList{1.0, 0.0};
        right[QStringLiteral("held")] = QStringList{QStringLiteral("run")};
        right[QStringLiteral("edges")] = QVariantList{QVariant(QVariantList{QStringLiteral("pressed"), QStringLiteral("run")})};
        run.capture(0, still, {});
        run.capture(1, still, {QStringLiteral("Play")});
        run.capture(2, right, {});
        QVariantMap holding = right;
        holding[QStringLiteral("edges")] = QVariantList();
        run.capture(3, holding, {});
        run.setOutcome({{QStringLiteral("Rules"), QVariantMap{{QStringLiteral("gathered"), 2}}}});
        // The game's store as the run found it (P3.16).
        run.setStore({{QStringLiteral("best"), 3}, {QStringLiteral("name"), QStringLiteral("Ada")}});
        QCOMPARE(run.ticks(), 4);

        // Only what changed is kept, and every tick is given back as it was taken.
        auto check = [](const qq::RunRecord &r) {
            QCOMPARE(r.handsAt(0).value(QStringLiteral("move")).toList(), (QVariantList{0.0, 0.0}));
            QCOMPARE(r.clicksAt(1), QStringList{QStringLiteral("Play")});
            QCOMPARE(r.handsAt(2).value(QStringLiteral("move")).toList(), (QVariantList{1.0, 0.0}));
            QCOMPARE(r.handsAt(2).value(QStringLiteral("edges")).toList().size(), 1);
            QCOMPARE(r.handsAt(3).value(QStringLiteral("held")).toStringList(), QStringList{QStringLiteral("run")});
            QCOMPARE(r.handsAt(3).value(QStringLiteral("edges")).toList().size(), 0);
            QCOMPARE(r.handsAt(250).value(QStringLiteral("move")).toList(), (QVariantList{1.0, 0.0}));
        };
        check(run);
        const QByteArray json = run.toJson();
        QCOMPARE(QJsonDocument::fromJson(json).object().value(QStringLiteral("hands")).toArray().size(), 2);

        qq::RunRecord back;
        QString why;
        QVERIFY2(back.fromJson(json, &why), qPrintable(why));
        QCOMPARE(back.scene(), QStringLiteral("climb"));
        QCOMPARE(back.seed(), 7);
        QCOMPARE(back.ticks(), 4);
        QCOMPARE(back.outcome().value(QStringLiteral("Rules")).toMap().value(QStringLiteral("gathered")).toInt(), 2);
        QCOMPARE(back.store().value(QStringLiteral("best")).toInt(), 3);
        QCOMPARE(back.store().value(QStringLiteral("name")).toString(), QStringLiteral("Ada"));
        check(back);
        QCOMPARE(back.toJson(), json);
        // A run kept before P3.16 has no store: its game found nothing kept.
        QJsonObject older = QJsonDocument::fromJson(json).object();
        older.remove(QStringLiteral("store"));
        qq::RunRecord old;
        QVERIFY(old.fromJson(QJsonDocument(older).toJson()));
        QVERIFY(old.store().isEmpty());
        QCOMPARE(old.seed(), 7);

        QVERIFY(!back.fromJson("{\"qqRun\": 2}", &why));
        QVERIFY2(why.contains(QStringLiteral("format 1")), qPrintable(why));
        QVERIFY(!back.fromJson("not json", &why));
    }

    // ── groups and prefabs (P3.15) ──

    void spriteMotionCommandsRequireADynamicBody_data()
    {
        QTest::addColumn<QString>("bodyMode");
        QTest::addColumn<QString>("replacement");
        QTest::addColumn<bool>("accepted");
        QTest::newRow("dynamic") << QStringLiteral("dynamic") << QString() << true;
        QTest::newRow("static") << QStringLiteral("static") << QString() << false;
        QTest::newRow("kinematic") << QStringLiteral("kinematic") << QString() << false;
        QTest::newRow("none") << QStringLiteral("none") << QString() << false;
        QTest::newRow("missing") << QStringLiteral("dynamic") << QStringLiteral("null") << false;
        QTest::newRow("ordinary-node") << QStringLiteral("dynamic") << QStringLiteral("ordinary") << false;
        QTest::newRow("mismatched-static-body") << QStringLiteral("dynamic") << QStringLiteral("standing") << false;
    }

    void spriteMotionCommandsRequireADynamicBody()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        QFETCH(QString, bodyMode);
        QFETCH(QString, replacement);
        QFETCH(bool, accepted);
        QQmlComponent component(engine);
        component.setData(QStringLiteral(
            "import QtQuick3D\nimport QtQuick3D.Physics\nimport QQ\nNode {\n"
            "    property bool playing: %2\n    property string view: \"3d\"\n"
            "    Sprite { objectName: \"shape\"; body: \"%1\" }\n"
            "    Node { objectName: \"ordinary\" }\n"
            "    StaticRigidBody { objectName: \"standing\" }\n}\n")
                .arg(bodyMode, replacement == QStringLiteral("null") ? QStringLiteral("false") : QStringLiteral("true")).toUtf8(),
            QUrl(QStringLiteral("qrc:/sprite-motion-commands.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QObject *shape = object->findChild<QObject *>(QStringLiteral("shape"));
        QVERIFY(shape);
        QObject *originalBody = shape->property("physics").value<QObject *>();
        QCOMPARE(originalBody != nullptr, bodyMode != QStringLiteral("none") && replacement != QStringLiteral("null"));
        const auto restore = qScopeGuard([&] { shape->setProperty("physics", QVariant::fromValue(originalBody)); });
        if (!replacement.isEmpty() && replacement != QStringLiteral("null")) {
            QObject *other = object->findChild<QObject *>(replacement);
            QVERIFY(other);
            QVERIFY(shape->setProperty("physics", QVariant::fromValue(other)));
        }
        const QVariant vector = QVariant::fromValue(QVector3D(1, 2, 3));
        for (const char *method : {"push", "applyForce", "setVelocity"}) {
            QVariant result;
            QVERIFY2(QMetaObject::invokeMethod(shape, method, Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, vector)), method);
            QVERIFY2(result.metaType() == QMetaType::fromType<bool>(), method);
            QCOMPARE(result.toBool(), accepted);
        }
    }

    void twoDimensionalKindsKeepTheirParentState_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QString>("mode");
        for (const char *mode : {"static", "dynamic", "kinematic"})
            QTest::newRow(mode) << QStringLiteral("Sprite") << QString::fromLatin1(mode);
        QTest::newRow("tile-map") << QStringLiteral("TileMap") << QString();
    }

    void twoDimensionalKindsKeepTheirParentState()
    {
        QFETCH(QString, kind);
        QFETCH(QString, mode);
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        const bool sprite = kind == QStringLiteral("Sprite");
        QQmlComponent component(engine);
        component.setData(QStringLiteral(
            "import QtQuick\nimport QtQuick3D\nimport QtQuick3D.Physics\nimport QQ\nNode {\n"
            " property var playing: false; property var paused: false; property string view: \"2d\"; property real pixelsPerUnit: 16\n"
            " readonly property int depthLock: DynamicRigidBody.LockZ\n"
            " readonly property int tiltLocks: DynamicRigidBody.LockX | DynamicRigidBody.LockY\n"
            " readonly property int noLock: DynamicRigidBody.LockNone\n"
            " %1 { objectName: \"entity\"; source: \"%2\"; %3 }\n"
            " Group { objectName: \"group\" }\n Prefab { objectName: \"prefab\" }\n"
            " Node { objectName: \"compatible\"; property var playing: true; property var paused: true;"
            " property string view: \"3d\"; property real pixelsPerUnit: 64 }\n"
            " Node { objectName: \"ordinary\" }\n}\n")
                .arg(kind, sprite ? QStringLiteral("sheet.png") : QStringLiteral("level.tmj"),
                     sprite ? QStringLiteral("body: \"%1\"; columns: 4; rows: 2; first: 1; count: 3; fps: 10").arg(mode) : QString()).toUtf8(),
            QUrl::fromLocalFile(fixture(QStringLiteral("two-dimensional-state.qml"))));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuick3DObject *>(object.get());
        auto *entity = object->findChild<QQuick3DObject *>(QStringLiteral("entity"));
        auto *group = object->findChild<QQuick3DObject *>(QStringLiteral("group"));
        auto *prefab = object->findChild<QQuick3DObject *>(QStringLiteral("prefab"));
        auto *compatible = object->findChild<QQuick3DObject *>(QStringLiteral("compatible"));
        auto *ordinary = object->findChild<QQuick3DObject *>(QStringLiteral("ordinary"));
        QVERIFY(root && entity && group && prefab && compatible && ordinary);
        const auto restore = qScopeGuard([&] { entity->setParentItem(root); entity->setParent(root); });
        QVERIFY(!entity->property("live").toBool());
        QVERIFY(!entity->property("physics").value<QObject *>());
        for (const QVariant &truthy : {QVariant(QStringLiteral("true")), QVariant(1)}) {
            QVERIFY(root->setProperty("playing", truthy));
            QVERIFY(!entity->property("live").toBool());
            QVERIFY(!entity->property("physics").value<QObject *>());
        }
        QVERIFY(root->setProperty("playing", true));
        QPointer<QObject> body = entity->property("physics").value<QObject *>();
        QVERIFY(body);
        QCOMPARE(qobject_cast<QQuick3DObject *>(body.data())->parentItem(), root);
        QCOMPARE(body->property("entity").value<QObject *>(), entity);
        QQmlListReference shapes(body.data(), "collisionShapes");
        QVERIFY(shapes.isValid() && shapes.count() > 0);
        QObject *collider = shapes.at(0);
        QVERIFY(collider);
        QCOMPARE(qobject_cast<QQuick3DObject *>(collider)->parentItem(), qobject_cast<QQuick3DObject *>(body.data()));
        QObject *animation = nullptr;
        if (sprite) {
            QCOMPARE(shapes.count(), 1);
            for (QObject *child : entity->findChildren<QObject *>())
                if (child->metaObject()->indexOfProperty("frameTime") >= 0)
                    animation = child;
            QVERIFY(animation && animation->property("running").toBool());
            QCOMPARE(entity->property("frame").toInt(), 1);
            QVERIFY(root->setProperty("paused", QStringLiteral("false")));
            QTRY_VERIFY(!animation->property("running").toBool());
            QVERIFY(root->setProperty("paused", false));
            QTRY_VERIFY(animation->property("running").toBool());
        }
        const QVariant originalExtent = entity->property(sprite ? "size" : "extent");
        for (QQuick3DObject *parent : {root, group, prefab}) {
            entity->setParentItem(parent);
            QVERIFY(root->setProperty("pixelsPerUnit", 32.0));
            QTRY_COMPARE(entity->property("pixelsPerUnit").toDouble(), 32.0);
            if (sprite) {
                const QVector3D expected = originalExtent.value<QVector3D>();
                QTRY_COMPARE(entity->property("size").value<QVector3D>(), QVector3D(expected.x() / 2, expected.y() / 2, 1));
                QCOMPARE(collider->property("extents"), entity->property("size"));
            } else {
                const QSizeF expected = originalExtent.toSizeF();
                QTRY_COMPARE(entity->property("extent").toSizeF(), QSizeF(expected.width() / 2, expected.height() / 2));
            }
            QVERIFY(root->setProperty("pixelsPerUnit", 16.0));
            QTRY_COMPARE(entity->property(sprite ? "size" : "extent"), originalExtent);
            QVERIFY(root->setProperty("view", QStringLiteral("3d")));
            if (sprite) {
                QTRY_VERIFY(!entity->property("flat").toBool());
                if (mode == QStringLiteral("dynamic"))
                    QTRY_COMPARE(body->property("linearAxisLock").toInt(), root->property("noLock").toInt());
            }
            QVERIFY(root->setProperty("view", QStringLiteral("2d")));
            if (sprite && mode == QStringLiteral("dynamic")) {
                QTRY_COMPARE(body->property("linearAxisLock").toInt(), root->property("depthLock").toInt());
                QTRY_COMPARE(body->property("angularAxisLock").toInt(), root->property("tiltLocks").toInt());
                QVERIFY(entity->setProperty("turns", false));
                QTRY_COMPARE(body->property("angularAxisLock").toInt(), root->property("tiltLocks").toInt() | root->property("depthLock").toInt());
                QVERIFY(entity->setProperty("turns", true));
            }
            QVERIFY(root->setProperty("playing", false));
            QTRY_VERIFY(!entity->property("live").toBool());
            QVERIFY(root->setProperty("playing", true));
            QTRY_VERIFY(entity->property("live").toBool());
            QCOMPARE(entity->property("physics").value<QObject *>(), body.data());
            QCOMPARE(shapes.at(0), collider);
        }
        entity->setParentItem(compatible);
        QTRY_COMPARE(entity->property("pixelsPerUnit").toDouble(), 64.0);
        QTRY_VERIFY(entity->property("live").toBool());
        if (animation)
            QTRY_VERIFY(!animation->property("running").toBool());
        for (QQuick3DObject *parent : {ordinary, static_cast<QQuick3DObject *>(nullptr)}) {
            entity->setParentItem(parent);
            QTRY_VERIFY(!entity->property("live").toBool());
            QTRY_COMPARE(entity->property("pixelsPerUnit").toDouble(), 16.0);
            if (animation)
                QTRY_VERIFY(!animation->property("running").toBool());
        }
        entity->setParentItem(root);
        QTRY_VERIFY(entity->property("live").toBool());
        QCOMPARE(entity->property("physics").value<QObject *>(), body.data());
        QCOMPARE(shapes.at(0), collider);
    }

    void musicFollowsItsCurrentParentsPlayAndReplayState()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        QQmlComponent component(engine);
        component.setData(
            "import QtQuick\nimport QtQuick3D\nimport QQ\nNode {\n"
            " property var playing: false; property var replaying: false; property var paused: false\n"
            " Music { objectName: \"music\"; web: true; source: \"chime.wav\" }\n"
            " Node { objectName: \"other\"; property var playing: true; property var replaying: false }\n"
            " Node { objectName: \"ordinary\" }\n}\n",
            QUrl::fromLocalFile(fixture(QStringLiteral("music-parent.qml"))));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuick3DObject *>(object.get());
        auto *music = object->findChild<QQuick3DObject *>(QStringLiteral("music"));
        auto *other = object->findChild<QQuick3DObject *>(QStringLiteral("other"));
        auto *ordinary = object->findChild<QQuick3DObject *>(QStringLiteral("ordinary"));
        QVERIFY(root && music && other && ordinary);
        const auto restore = qScopeGuard([&] { music->setParentItem(root); music->setParent(root); });
        const auto voice = [&]() -> QObject * {
            QVariant result;
            if (!QMetaObject::invokeMethod(music, "voice", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, 0)))
                return nullptr;
            return result.value<QObject *>();
        };
        for (const QVariant &truthy : {QVariant(QStringLiteral("true")), QVariant(1)}) {
            QVERIFY(root->setProperty("playing", truthy));
            QVERIFY(!music->property("live").toBool());
            QVERIFY(!voice());
        }
        QVERIFY(root->setProperty("playing", true));
        QTRY_VERIFY(qobject_cast<qq::WebVoice *>(voice()));
        QObject *playing = voice();
        QVERIFY(root->setProperty("paused", true));
        QCOMPARE(voice(), playing); // Music keeps sounding while the world is paused.
        for (const QVariant &replaying : {QVariant(true), QVariant(QStringLiteral("false")), QVariant(1)}) {
            QVERIFY(root->setProperty("replaying", replaying));
            QTRY_VERIFY(!music->property("sounding").toBool());
            QTRY_VERIFY(!voice());
            QVERIFY(root->setProperty("replaying", false));
            QTRY_VERIFY(qobject_cast<qq::WebVoice *>(voice()));
        }
        music->setParentItem(other);
        QTRY_VERIFY(music->property("sounding").toBool());
        QVERIFY(root->setProperty("replaying", true));
        QVERIFY(music->property("sounding").toBool());
        QVERIFY(other->setProperty("replaying", true));
        QTRY_VERIFY(!voice());
        for (QQuick3DObject *parent : {ordinary, static_cast<QQuick3DObject *>(nullptr)}) {
            music->setParentItem(parent);
            QTRY_VERIFY(!music->property("live").toBool());
            QTRY_VERIFY(!voice());
        }
        QVERIFY(root->setProperty("replaying", false));
        music->setParentItem(root);
        QTRY_VERIFY(qobject_cast<qq::WebVoice *>(voice()));
    }

    void runtimeKindsFollowTheirNearestParentsState_data()
    {
        QTest::addColumn<QString>("kind");
        for (const char *kind : {"Emitter", "Ground", "InputAction", "Prop", "Sound", "Spotlight"})
            QTest::newRow(kind) << QString::fromLatin1(kind);
    }

    void runtimeKindsFollowTheirNearestParentsState()
    {
        QFETCH(QString, kind);
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        QQmlComponent component(engine);
        component.setData(QStringLiteral(
            "import QtQuick\nimport QtQuick3D\nimport QQ\nNode {\n"
            "    property var playing: false\n    property var paused: false\n    property var replaying: false\n"
            "    %1 { objectName: \"entity\"; name: \"parent-state-check\"; %2 }\n"
            "    Group { objectName: \"group\" }\n    Prefab { objectName: \"prefab\" }\n"
            "    Node { objectName: \"compatible\"; property var playing: false; property var paused: true; property var replaying: false }\n"
            "    Node { objectName: \"ordinary\" }\n}\n")
                .arg(kind, kind == QStringLiteral("Sound") ? QStringLiteral("web: true") : QString()).toUtf8(),
            QUrl::fromLocalFile(fixture(QStringLiteral("parent-state.qml"))));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuick3DObject *>(object.get());
        auto *entity = object->findChild<QQuick3DObject *>(QStringLiteral("entity"));
        auto *group = object->findChild<QQuick3DObject *>(QStringLiteral("group"));
        auto *prefab = object->findChild<QQuick3DObject *>(QStringLiteral("prefab"));
        auto *compatible = object->findChild<QQuick3DObject *>(QStringLiteral("compatible"));
        auto *ordinary = object->findChild<QQuick3DObject *>(QStringLiteral("ordinary"));
        QVERIFY(root && entity && group && prefab && compatible && ordinary);
        const auto restore = qScopeGuard([&] { entity->setParentItem(root); entity->setParent(root); });
        QObject *particles = nullptr;
        QObject *marker = nullptr;
        for (QObject *child : entity->findChildren<QObject *>()) {
            if (child->metaObject()->indexOfProperty("loggingData") >= 0)
                particles = child;
            if (child->property("pickable").toBool() && child->metaObject()->indexOfProperty("source") >= 0)
                marker = child;
        }
        const bool emitter = kind == QStringLiteral("Emitter");
        if (emitter)
            QVERIFY(particles && marker);
        if (kind == QStringLiteral("Sound") || kind == QStringLiteral("Spotlight"))
            QVERIFY(marker);
        const auto active = [&] { return emitter ? !marker->property("visible").toBool() : entity->property("live").toBool(); };
        QVERIFY(!active());
        for (const QVariant &truthy : {QVariant(QStringLiteral("true")), QVariant(1)}) {
            QVERIFY(root->setProperty("playing", truthy));
            QVERIFY(!active());
        }
        QVERIFY(root->setProperty("playing", true));
        QTRY_VERIFY(active());
        if (marker && kind != QStringLiteral("Prop"))
            QVERIFY(!marker->property("visible").toBool());
        QPointer<QObject> body = entity->property("physics").value<QObject *>();
        if (kind == QStringLiteral("Ground")) {
            QVERIFY(body);
            QCOMPARE(qobject_cast<QQuick3DObject *>(body.data())->parentItem(), root);
            QCOMPARE(body->property("entity").value<QObject *>(), entity);
        }
        auto *input = engine->singletonInstance<qq::Input *>("QQ", "Input");
        QVERIFY(input);
        if (kind == QStringLiteral("InputAction")) {
            QCOMPARE(entity->property("declared").toString(), QStringLiteral("parent-state-check"));
            QVERIFY(input->actions().contains(QStringLiteral("parent-state-check")));
            input->define(QStringLiteral("parent-state-check"), {QStringLiteral("F2")}, {});
        }
        if (particles) {
            QVERIFY(root->setProperty("paused", true));
            QTRY_VERIFY(particles->property("paused").toBool());
            QVERIFY(root->setProperty("paused", QStringLiteral("true")));
            QTRY_VERIFY(!particles->property("paused").toBool());
            QVERIFY(root->setProperty("paused", false));
        }
        if (kind == QStringLiteral("Sound")) {
            QVERIFY(entity->setProperty("source", QUrl::fromLocalFile(fixture(QStringLiteral("chime.wav")))));
            QTRY_VERIFY(entity->property("sounding").toBool());
            QTRY_VERIFY(qobject_cast<qq::WebVoice *>(entity->property("playing").value<QObject *>()));
            for (const QVariant &replay : {QVariant(true), QVariant(QStringLiteral("false")), QVariant(1)}) {
                QVERIFY(root->setProperty("replaying", replay));
                QTRY_VERIFY(!entity->property("sounding").toBool());
                QTRY_VERIFY(!entity->property("playing").value<QObject *>());
            }
            QVERIFY(root->setProperty("replaying", false));
            QTRY_VERIFY(entity->property("sounding").toBool());
            QVERIFY(entity->setProperty("source", QUrl()));
            QTRY_VERIFY(!entity->property("sounding").toBool());
        }
        for (QQuick3DObject *parent : {group, prefab, compatible, ordinary}) {
            entity->setParentItem(parent);
            QTRY_COMPARE(active(), parent == group || parent == prefab);
            if (particles)
                QTRY_COMPARE(particles->property("paused").toBool(), parent == compatible);
        }
        entity->setParentItem(compatible);
        QVERIFY(compatible->setProperty("playing", true));
        QTRY_VERIFY(active());
        QVERIFY(compatible->setProperty("playing", QStringLiteral("true")));
        QTRY_VERIFY(!active());
        entity->setParentItem(nullptr);
        QTRY_VERIFY(!active());
        if (particles)
            QTRY_VERIFY(!particles->property("paused").toBool());
        entity->setParentItem(root);
        QTRY_VERIFY(active());
        if (kind == QStringLiteral("Ground"))
            QCOMPARE(entity->property("physics").value<QObject *>(), body.data());
        if (kind == QStringLiteral("InputAction")) {
            // Activation and reparenting do not overwrite a binding the player has changed.
            QCOMPARE(input->describe(QStringLiteral("parent-state-check")), QStringLiteral("F2"));
        }
    }

    void aPropsFramesReadItsCurrentParentsClock()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        QQmlComponent component(engine);
        component.setData(
            "import QtQuick\nimport QtQuick3D\nimport QQ\nNode {\n"
            "    property var playing: true\n    property var replaying: false\n    property var paused: false\n"
            "    property real _owed: 0.25\n"
            "    Prop { objectName: \"prop\"; source: \"rig.gltf\"; clip: \"bend\" }\n"
            "    Node { objectName: \"other\"; property bool playing: true; property var replaying: false;"
            " property var paused: false; property real _owed: 0.75 }\n}\n",
            QUrl::fromLocalFile(fixture(QStringLiteral("prop-clock.qml"))));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuick3DObject *>(object.get());
        auto *prop = object->findChild<QQuick3DObject *>(QStringLiteral("prop"));
        auto *other = object->findChild<QQuick3DObject *>(QStringLiteral("other"));
        QVERIFY(root && prop && other);
        const auto restore = qScopeGuard([&] { prop->setParentItem(root); prop->setParent(root); });
        QTRY_COMPARE(prop->property("status").toInt(), 1);
        QCOMPARE(prop->property("clipLength").toDouble(), 2.0);
        QVariant part;
        QVERIFY(QMetaObject::invokeMethod(prop, "part", Q_RETURN_ARG(QVariant, part), Q_ARG(QVariant, QStringLiteral("Tip"))));
        QObject *tip = part.value<QObject *>();
        QVERIFY(tip);
        QObject *animation = nullptr;
        for (QObject *child : prop->findChildren<QObject *>())
            if (child->metaObject()->indexOfProperty("frameTime") >= 0 && child->metaObject()->indexOfSignal("triggered()") >= 0)
                animation = child;
        QVERIFY(animation);
        const auto angle = [&] { return tip->property("eulerRotation").value<QVector3D>().z(); };
        QVERIFY(animation->property("running").toBool());
        QVERIFY(QMetaObject::invokeMethod(animation, "triggered"));
        QVERIFY(qAbs(angle() - 22.5) < 0.01);
        QCOMPARE(prop->property("clipTime").toDouble(), 0.0);
        QVERIFY(root->setProperty("_owed", 0.5));
        QVERIFY(QMetaObject::invokeMethod(animation, "triggered"));
        QVERIFY(qAbs(angle() - 45) < 0.01);
        QVERIFY(root->setProperty("paused", QStringLiteral("false")));
        QVERIFY(QMetaObject::invokeMethod(animation, "triggered"));
        QVERIFY(qAbs(angle()) < 0.01); // Any truthy paused value holds the pose at the last step.
        QVERIFY(root->setProperty("replaying", 1));
        QTRY_VERIFY(!animation->property("running").toBool());
        prop->setParentItem(other);
        QTRY_VERIFY(animation->property("running").toBool());
        QVERIFY(QMetaObject::invokeMethod(animation, "triggered"));
        QVERIFY(qAbs(angle() - 67.5) < 0.01);
        QVERIFY(other->setProperty("_owed", 0.5));
        QVERIFY(prop->setProperty("clipSpeed", 2.0));
        QVERIFY(QMetaObject::invokeMethod(animation, "triggered"));
        QVERIFY(qAbs(angle() - 90) < 0.01);
        QVERIFY(prop->setProperty("clip", QString()));
        QTRY_VERIFY(!animation->property("running").toBool());
        QVERIFY(qAbs(angle()) < 0.01);
        prop->setParentItem(nullptr);
        QVERIFY(!prop->property("live").toBool());
        QVERIFY(!animation->property("running").toBool());
    }

    void aTileMapKeepsItsOwnedCollisionShapes()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        QQmlComponent component(engine);
        component.setData(
            "import QtQuick\nimport QtQuick3D\nimport QQ\nNode {\n"
            "    property bool playing: false\n    property real pixelsPerUnit: 32\n"
            "    TileMap { objectName: \"map\"; source: \"level.tmj\"; solid: false;"
            " position: Qt.vector3d(3, 4, 5); eulerRotation: Qt.vector3d(0, 0, 30) }\n}\n",
            QUrl::fromLocalFile(fixture(QStringLiteral("tile-body.qml"))));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuick3DObject *>(object.get());
        QObject *map = object->findChild<QObject *>(QStringLiteral("map"));
        QVERIFY(root && map);
        QCOMPARE(map->property("error").toString(), QString());
        QVERIFY(!map->property("physics").value<QObject *>());
        QVERIFY(root->setProperty("playing", true));
        QVERIFY(!map->property("physics").value<QObject *>());
        QVERIFY(root->setProperty("playing", false));
        QVERIFY(map->setProperty("solid", true));
        QVERIFY(root->setProperty("playing", true));
        QPointer<QObject> body = map->property("physics").value<QObject *>();
        QVERIFY(body);
        QCOMPARE(qobject_cast<QQuick3DObject *>(body.data())->parentItem(), root);
        QCOMPARE(body->property("entity").value<QObject *>(), map);
        QCOMPARE(body->property("position"), map->property("position"));
        QCOMPARE(body->property("rotation"), map->property("rotation"));
        QVERIFY(body->property("sendContactReports").toBool());
        QQmlListReference shapes(body.data(), "collisionShapes");
        QVERIFY(shapes.isValid());
        const QVariantList solids = io->readTiledMap(QUrl::fromLocalFile(fixture(QStringLiteral("level.tmj"))))
                                        .value(QStringLiteral("solids")).toList();
        QVERIFY(!solids.isEmpty());
        QCOMPARE(shapes.count(), solids.size());
        const double tw = map->property("tileWidth").toDouble();
        const double th = map->property("tileHeight").toDouble();
        QList<QObject *> identities;
        for (qsizetype i = 0; i < solids.size(); ++i) {
            const QVariantList r = solids[i].toList();
            QCOMPARE(r.size(), 4);
            QObject *shape = shapes.at(i);
            QVERIFY(shape);
            identities << shape;
            QCOMPARE(qobject_cast<QQuick3DObject *>(shape)->parentItem(), qobject_cast<QQuick3DObject *>(body.data()));
            QCOMPARE(shape->property("position").value<QVector3D>(),
                     QVector3D((r[0].toDouble() + r[2].toDouble() / 2) * tw, -(r[1].toDouble() + r[3].toDouble() / 2) * th, 0));
            QCOMPARE(shape->property("extents").value<QVector3D>(), QVector3D(r[2].toDouble() * tw, r[3].toDouble() * th, 1));
        }
        QVERIFY(QMetaObject::invokeMethod(map, "makeBody"));
        QVERIFY(root->setProperty("playing", false));
        QVERIFY(root->setProperty("playing", true));
        QCOMPARE(map->property("physics").value<QObject *>(), body.data());
        QCOMPARE(shapes.count(), identities.size());
        for (qsizetype i = 0; i < identities.size(); ++i)
            QCOMPARE(shapes.at(i), identities[i]);
        object.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(body.isNull());
    }

    void aBehaviourKeepsItsStructuralLogicAndParent_data()
    {
        QTest::addColumn<bool>("structural");
        QTest::newRow("qq-logic") << false;
        QTest::newRow("compatible-object") << true;
    }

    void aBehaviourKeepsItsStructuralLogicAndParent()
    {
        QFETCH(bool, structural);
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        QTemporaryDir project;
        QVERIFY(project.isValid());
        const QString path = QDir(project.path()).filePath(QStringLiteral("events.qml"));
        QByteArray source = "import QtQuick\nimport QQ\n";
        source += structural
            ? "QtObject {\n    readonly property bool qqLogic: true\n"
              "    property QtObject target\n    property QtObject scene\n"
              "    signal step(real dt)\n    signal frame(real dt)\n    signal told(var message)\n"
              "    signal pressed(string action)\n    signal released(string action)\n"
              "    signal contact(QtObject other, var point, var normal)\n"
              "    signal entered(QtObject other)\n    signal exited(QtObject other)\n    signal clicked()\n"
              "    function _step(dt) { step(dt) }\n    function _frame(dt) { frame(dt) }\n"
              "    function _told(message) { told(message) }\n"
              "    function _hear(kind, action) { if (kind === \"pressed\") pressed(action); else released(action) }\n"
            : "Logic {\n";
        source += "    property int stepCalls: 0\n    property var received\n"
                  "    property int revision: 1\n"
                  "    onStep: (dt) => ++stepCalls\n    onTold: (message) => received = message\n}\n";
        const auto save = [&] {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(source) == source.size();
        };
        QVERIFY(save());
        QQmlComponent component(engine);
        component.setData(
            "import QtQuick\nimport QtQuick3D\nimport QQ\nNode {\n    id: root\n"
            "    readonly property string kind: \"Scene\"\n"
            "    property var playing: false\n    property var paused: true\n    property var replaying: false\n"
            "    Node { objectName: \"global\"; property string kind: \"Shape\"; property string name: \"Target\" }\n"
            "    Node { id: silent; objectName: \"silent\"; property string kind: \"Shape\"; property string name: \"Silent\" }\n"
            "    Node { id: prefab; property string kind: \"Prefab\"\n"
            "        property bool playing: root.playing === true\n"
            "        property bool paused: root.paused === true\n    property bool replaying: root.replaying === true\n"
            "        Node { id: own; objectName: \"own\"; property string kind: \"Shape\"; property string name: \"Target\"\n"
            "            signal contact(QtObject other, var point, var normal)\n"
            "            signal entered(QtObject other)\n    signal exited(QtObject other)\n    signal clicked()\n        }\n"
            "        Group { objectName: \"group\"\n"
            "            Behaviour { objectName: \"behaviour\"; source: \"events.qml\"; target: \"Target\" }\n        }\n    }\n"
            "    function sendEvents() { own.contact(silent, Qt.vector3d(1, 2, 3), Qt.vector3d(0, 1, 0));"
            " own.entered(silent); own.exited(silent); own.clicked() }\n}\n",
            QUrl::fromLocalFile(QDir(project.path()).filePath(QStringLiteral("scene.qml"))));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuick3DObject *>(object.get());
        auto *behaviour = object->findChild<QQuick3DObject *>(QStringLiteral("behaviour"));
        auto *group = object->findChild<QQuick3DObject *>(QStringLiteral("group"));
        QObject *own = object->findChild<QObject *>(QStringLiteral("own"));
        QObject *silent = object->findChild<QObject *>(QStringLiteral("silent"));
        QObject *global = object->findChild<QObject *>(QStringLiteral("global"));
        QVERIFY(root && behaviour && group && own && silent && global);
        const auto restore = qScopeGuard([&] { behaviour->setParentItem(group); behaviour->setParent(group); });
        auto logic = [&] { return behaviour->property("logic").value<QObject *>(); };
        QVERIFY(!behaviour->property("live").toBool());
        QVERIFY(behaviour->property("ready").toBool());
        QVERIFY(!logic());
        QVERIFY(root->setProperty("playing", true));
        QTRY_VERIFY(logic());
        QCOMPARE(behaviour->property("builds").toInt(), 1);
        QCOMPARE(behaviour->property("error").toString(), QString());
        QCOMPARE(logic()->property("scene").value<QObject *>(), group);
        QCOMPARE(logic()->property("target").value<QObject *>(), own);

        QSignalSpy step(logic(), SIGNAL(step(double)));
        QSignalSpy frame(logic(), SIGNAL(frame(double)));
        QSignalSpy pressed(logic(), SIGNAL(pressed(QString)));
        QSignalSpy released(logic(), SIGNAL(released(QString)));
        QSignalSpy contact(logic(), SIGNAL(contact(QObject*,QVariant,QVariant)));
        QSignalSpy entered(logic(), SIGNAL(entered(QObject*)));
        QSignalSpy exited(logic(), SIGNAL(exited(QObject*)));
        QSignalSpy clicked(logic(), SIGNAL(clicked()));
        QVERIFY(step.isValid() && frame.isValid() && pressed.isValid() && released.isValid());
        QVERIFY(contact.isValid() && entered.isValid() && exited.isValid() && clicked.isValid());
        QVERIFY(QMetaObject::invokeMethod(behaviour, "_stepLogic", Q_ARG(QVariant, 1.0 / 60.0)));
        QCOMPARE(step.size(), 1);
        QCOMPARE(step.first().first().toDouble(), 1.0 / 60.0);
        QCOMPARE(logic()->property("stepCalls").toInt(), 1);
        const QVariantMap message{{QStringLiteral("items"), QVariantList{1, QStringLiteral("two"), true}},
                                  {QStringLiteral("nested"), QVariantMap{{QStringLiteral("value"), 7}}}};
        QVERIFY(QMetaObject::invokeMethod(behaviour, "_told", Q_ARG(QVariant, message)));
        const QVariant received = logic()->property("received");
        const QVariant decoded = received.metaType() == QMetaType::fromType<QJSValue>()
            ? received.value<QJSValue>().toVariant() : received;
        QCOMPARE(decoded.toMap(), message);
        QVERIFY(QMetaObject::invokeMethod(behaviour, "_hear", Q_ARG(QVariant, QStringLiteral("pressed")),
                                         Q_ARG(QVariant, QStringLiteral("jump"))));
        QVERIFY(QMetaObject::invokeMethod(behaviour, "_hear", Q_ARG(QVariant, QStringLiteral("released")),
                                         Q_ARG(QVariant, QStringLiteral("jump"))));
        QCOMPARE(pressed.size(), 1);
        QCOMPARE(released.size(), 1);
        QCOMPARE(pressed.first().first().toString(), QStringLiteral("jump"));
        QCOMPARE(released.first().first().toString(), QStringLiteral("jump"));
        QVERIFY(QMetaObject::invokeMethod(root, "sendEvents"));
        QCOMPARE(contact.size(), 1);
        QCOMPARE(contact.first().at(0).value<QObject *>(), silent);
        QCOMPARE(contact.first().at(1).value<QVector3D>(), QVector3D(1, 2, 3));
        QCOMPARE(contact.first().at(2).value<QVector3D>(), QVector3D(0, 1, 0));
        QCOMPARE(entered.size(), 1);
        QCOMPARE(exited.size(), 1);
        QCOMPARE(entered.first().first().value<QObject *>(), silent);
        QCOMPARE(exited.first().first().value<QObject *>(), silent);
        QCOMPARE(clicked.size(), 1);

        QObject *animation = nullptr;
        for (QObject *child : behaviour->findChildren<QObject *>()) {
            if (child->metaObject()->indexOfProperty("frameTime") >= 0
                && child->metaObject()->indexOfSignal("triggered()") >= 0) {
                QVERIFY(!animation);
                animation = child;
            }
        }
        QVERIFY(animation);
        QVERIFY(!animation->property("running").toBool());
        QVERIFY(root->setProperty("paused", false));
        QTRY_VERIFY(animation->property("running").toBool());
        QVERIFY(root->setProperty("replaying", true));
        QTRY_VERIFY(!animation->property("running").toBool());
        QVERIFY(root->setProperty("replaying", false));
        QTRY_VERIFY(animation->property("running").toBool());
        const qsizetype frames = frame.size();
        QVERIFY(QMetaObject::invokeMethod(animation, "triggered"));
        QCOMPARE(frame.size(), frames + 1);
        QCOMPARE(frame.last().first().toDouble(), qMin(animation->property("frameTime").toDouble(), 0.05));
        QVERIFY(root->setProperty("paused", true));
        QVERIFY(behaviour->setProperty("target", QStringLiteral("Silent")));
        QTRY_COMPARE(logic()->property("target").value<QObject *>(), silent);
        QVERIFY(QMetaObject::invokeMethod(root, "sendEvents"));
        QCOMPARE(clicked.size(), 1);
        QCOMPARE(contact.size(), 1);
        QVERIFY(behaviour->setProperty("target", QStringLiteral("Target")));
        QTRY_COMPARE(logic()->property("target").value<QObject *>(), own);
        QVERIFY(QMetaObject::invokeMethod(root, "sendEvents"));
        QCOMPARE(clicked.size(), 2);
        QCOMPARE(contact.size(), 2);

        // A different parent changes both lookup scope and the state supplied to the same live logic.
        QPointer<QObject> first = logic();
        behaviour->setParentItem(root);
        QTRY_COMPARE(logic()->property("scene").value<QObject *>(), root);
        QTRY_COMPARE(logic()->property("target").value<QObject *>(), global);
        QCOMPARE(logic(), first.data());
        QVERIFY(root->setProperty("paused", QStringLiteral("false")));
        QVERIFY(!animation->property("running").toBool()); // A truthy structural value still pauses frames.
        QVERIFY(root->setProperty("paused", false));
        QTRY_VERIFY(animation->property("running").toBool());
        QVERIFY(root->setProperty("replaying", 1));
        QTRY_VERIFY(!animation->property("running").toBool());
        QVERIFY(root->setProperty("replaying", false));
        QVERIFY(root->setProperty("paused", true));
        behaviour->setParentItem(group);
        QTRY_COMPARE(logic()->property("scene").value<QObject *>(), group);
        QTRY_COMPARE(logic()->property("target").value<QObject *>(), own);

        // Reload forwards the very next callback to the replacement, without a cached QObject binding.
        auto *file = behaviour->findChild<qq::LogicFile *>();
        QVERIFY(file);
        source.replace("revision: 1", "revision: 2");
        QVERIFY(save());
        file->reload();
        QVERIFY(logic() && logic() != first.data());
        QCOMPARE(behaviour->property("builds").toInt(), 2);
        QCOMPARE(logic()->property("revision").toInt(), 2);
        QVERIFY(QMetaObject::invokeMethod(behaviour, "_stepLogic", Q_ARG(QVariant, 0.125)));
        QCOMPARE(logic()->property("stepCalls").toInt(), 1);
        QCOMPARE(first->property("stepCalls").toInt(), 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(first.isNull());
        QVERIFY(root->setProperty("playing", QStringLiteral("true")));
        QTRY_VERIFY(!logic());
        QVERIFY(!behaviour->property("live").toBool());
        QVERIFY(behaviour->property("ready").toBool());
        QVERIFY(QMetaObject::invokeMethod(behaviour, "_stepLogic", Q_ARG(QVariant, 0.25)));
        QVERIFY(QMetaObject::invokeMethod(root, "sendEvents"));
        behaviour->setParentItem(nullptr);
        QVERIFY(!behaviour->property("live").toBool());
        QVERIFY(root->setProperty("playing", true));
        behaviour->setParentItem(group);
        QTRY_VERIFY(logic());
        QCOMPARE(logic()->property("revision").toInt(), 2);
        QCOMPARE(logic()->property("stepCalls").toInt(), 0);
    }

    void groupsAndPrefabsKeepTheirNearestParentsState_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::newRow("group") << QStringLiteral("Group");
        QTest::newRow("prefab") << QStringLiteral("Prefab");
    }

    void groupsAndPrefabsKeepTheirNearestParentsState()
    {
        QFETCH(QString, kind);
        QQmlComponent component(engine);
        component.setData(QStringLiteral("import QtQuick3D\nimport QQ\nNode {\n"
                          "    property var playing: true\n    property var replaying: true\n    property var paused: true\n"
                          "    property real _owed: 0.5\n    property string view: \"2d\"\n    property real pixelsPerUnit: 24\n"
                          "    %1 { objectName: \"outer\"; Group { objectName: \"inner\" } }\n"
                          "    Node { objectName: \"ordinary\" }\n}\n").arg(kind).toUtf8(),
                          QUrl(QStringLiteral("qrc:/parent-state.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuick3DObject *>(object.get());
        auto *outer = object->findChild<QQuick3DObject *>(QStringLiteral("outer"));
        auto *inner = object->findChild<QQuick3DObject *>(QStringLiteral("inner"));
        auto *ordinary = object->findChild<QQuick3DObject *>(QStringLiteral("ordinary"));
        QVERIFY(root && outer && inner && ordinary);
        const auto restore = qScopeGuard([&] { outer->setParentItem(root); outer->setParent(root); });
        const auto state = [](QObject *node) {
            QVariantMap values;
            for (const char *field : {"playing", "replaying", "paused", "_owed", "view", "pixelsPerUnit", "live"})
                values.insert(QLatin1String(field), node->property(field));
            return values;
        };
        QVariantMap expected{{QStringLiteral("playing"), true}, {QStringLiteral("replaying"), true},
                             {QStringLiteral("paused"), true}, {QStringLiteral("_owed"), 0.5},
                             {QStringLiteral("view"), QStringLiteral("2d")}, {QStringLiteral("pixelsPerUnit"), 24.0},
                             {QStringLiteral("live"), true}};
        QCOMPARE(state(outer), expected);
        QCOMPARE(state(inner), expected);
        // State flags require the boolean true, rather than a truthy value from a structural parent.
        QVERIFY(object->setProperty("playing", QStringLiteral("true")));
        QVERIFY(object->setProperty("replaying", 1));
        QVERIFY(object->setProperty("paused", QStringLiteral("false")));
        for (const char *field : {"playing", "replaying", "paused", "live"})
            expected.insert(QLatin1String(field), false);
        QTRY_COMPARE(state(outer), expected);
        QTRY_COMPARE(state(inner), expected);
        QVERIFY(object->setProperty("playing", true));
        QVERIFY(object->setProperty("_owed", 1.25));
        QVERIFY(object->setProperty("view", QStringLiteral("3d")));
        QVERIFY(object->setProperty("pixelsPerUnit", 48.0));
        expected[QStringLiteral("playing")] = expected[QStringLiteral("live")] = true;
        expected[QStringLiteral("_owed")] = 1.25;
        expected[QStringLiteral("view")] = QStringLiteral("3d");
        expected[QStringLiteral("pixelsPerUnit")] = 48.0;
        QTRY_COMPARE(state(outer), expected);
        QTRY_COMPARE(state(inner), expected);
        const QVariantMap fallback{{QStringLiteral("playing"), false}, {QStringLiteral("replaying"), false},
                                   {QStringLiteral("paused"), false}, {QStringLiteral("_owed"), 0.0},
                                   {QStringLiteral("view"), QStringLiteral("3d")}, {QStringLiteral("pixelsPerUnit"), 16.0},
                                   {QStringLiteral("live"), false}};
        outer->setParentItem(ordinary);
        QTRY_COMPARE(state(outer), fallback);
        QTRY_COMPARE(state(inner), fallback);
        outer->setParentItem(nullptr);
        QTRY_COMPARE(state(outer), fallback);
        QTRY_COMPARE(state(inner), fallback);
        outer->setParentItem(root);
        QTRY_COMPARE(state(outer), expected);
        QTRY_COMPARE(state(inner), expected);
    }

    void shapeMotionCommandsRequireADynamicBody_data()
    {
        QTest::addColumn<QString>("bodyMode");
        QTest::addColumn<QString>("replacement");
        QTest::addColumn<bool>("accepted");
        QTest::newRow("dynamic") << QStringLiteral("dynamic") << QString() << true;
        QTest::newRow("static") << QStringLiteral("static") << QString() << false;
        QTest::newRow("kinematic") << QStringLiteral("kinematic") << QString() << false;
        QTest::newRow("none") << QStringLiteral("none") << QString() << false;
        QTest::newRow("missing") << QStringLiteral("dynamic") << QStringLiteral("null") << false;
        QTest::newRow("ordinary-node") << QStringLiteral("dynamic") << QStringLiteral("ordinary") << false;
        QTest::newRow("mismatched-static-body") << QStringLiteral("dynamic") << QStringLiteral("standing") << false;
    }

    void shapeMotionCommandsRequireADynamicBody()
    {
        QFETCH(QString, bodyMode);
        QFETCH(QString, replacement);
        QFETCH(bool, accepted);
        QQmlComponent component(engine);
        component.setData(QStringLiteral(
            "import QtQuick3D\nimport QtQuick3D.Physics\nimport QQ\nNode {\n"
            "    property bool playing: %2\n    property string view: \"3d\"\n"
            "    Shape { objectName: \"shape\"; body: \"%1\" }\n"
            "    Node { objectName: \"ordinary\" }\n"
            "    StaticRigidBody { objectName: \"standing\" }\n}\n")
                .arg(bodyMode, replacement == QStringLiteral("null") ? QStringLiteral("false") : QStringLiteral("true")).toUtf8(),
            QUrl(QStringLiteral("qrc:/shape-motion-commands.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QObject *shape = object->findChild<QObject *>(QStringLiteral("shape"));
        QVERIFY(shape);
        QObject *originalBody = shape->property("physics").value<QObject *>();
        QCOMPARE(originalBody != nullptr, bodyMode != QStringLiteral("none") && replacement != QStringLiteral("null"));
        const auto restore = qScopeGuard([&] { shape->setProperty("physics", QVariant::fromValue(originalBody)); });
        if (!replacement.isEmpty() && replacement != QStringLiteral("null")) {
            QObject *other = object->findChild<QObject *>(replacement);
            QVERIFY(other);
            QVERIFY(shape->setProperty("physics", QVariant::fromValue(other)));
        }
        const QVariant vector = QVariant::fromValue(QVector3D(1, 2, 3));
        for (const char *method : {"push", "applyForce", "setVelocity", "setSpin"}) {
            QVariant result;
            QVERIFY2(QMetaObject::invokeMethod(shape, method, Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, vector)), method);
            QVERIFY2(result.metaType() == QMetaType::fromType<bool>(), method);
            QCOMPARE(result.toBool(), accepted);
        }
    }

    void aShapesBodyFollowsItsNearestParentsState()
    {
        QQmlComponent component(engine);
        component.setData("import QtQuick3D\nimport QtQuick3D.Physics\nimport QQ\nNode {\n"
                          "    property var playing: false\n    property string view: \"2d\"\n"
                          "    readonly property int depthLock: DynamicRigidBody.LockZ\n"
                          "    readonly property int tiltLocks: DynamicRigidBody.LockX | DynamicRigidBody.LockY\n"
                          "    readonly property int noLock: DynamicRigidBody.LockNone\n"
                          "    Shape { objectName: \"shape\"; body: \"dynamic\" }\n"
                          "    Group { objectName: \"group\" }\n"
                          "    Prefab { objectName: \"prefab\" }\n"
                          "    Node { objectName: \"ordinary\" }\n}\n",
                          QUrl(QStringLiteral("qrc:/shape-parent-state.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuick3DObject *>(object.get());
        auto *shape = object->findChild<QQuick3DObject *>(QStringLiteral("shape"));
        auto *group = object->findChild<QQuick3DObject *>(QStringLiteral("group"));
        auto *prefab = object->findChild<QQuick3DObject *>(QStringLiteral("prefab"));
        auto *ordinary = object->findChild<QQuick3DObject *>(QStringLiteral("ordinary"));
        QVERIFY(root && shape && group && prefab && ordinary);
        const auto restore = qScopeGuard([&] { shape->setParentItem(root); shape->setParent(root); });
        QVERIFY(!shape->property("live").toBool());
        QVERIFY(!shape->property("physics").value<QObject *>());
        QVERIFY(object->setProperty("playing", QStringLiteral("true")));
        QVERIFY(!shape->property("live").toBool());
        QVERIFY(!shape->property("physics").value<QObject *>());
        QVERIFY(object->setProperty("playing", true));
        QTRY_VERIFY(shape->property("live").toBool());
        QObject *body = shape->property("physics").value<QObject *>();
        QVERIFY(body);
        QCOMPARE(body->property("entity").value<QObject *>(), static_cast<QObject *>(shape));
        const int depthLock = root->property("depthLock").toInt();
        const int tiltLocks = root->property("tiltLocks").toInt();
        const int noLock = root->property("noLock").toInt();
        for (auto *parent : {root, group, prefab}) {
            shape->setParentItem(parent);
            QVERIFY(object->setProperty("view", QStringLiteral("2d")));
            QTRY_COMPARE(body->property("linearAxisLock").toInt(), depthLock);
            QTRY_COMPARE(body->property("angularAxisLock").toInt(), tiltLocks);
            QVERIFY(object->setProperty("view", QStringLiteral("3d")));
            QTRY_COMPARE(body->property("linearAxisLock").toInt(), noLock);
            QTRY_COMPARE(body->property("angularAxisLock").toInt(), noLock);
            QVERIFY(object->setProperty("playing", false));
            QTRY_VERIFY(!shape->property("live").toBool());
            QVERIFY(object->setProperty("playing", true));
            QTRY_VERIFY(shape->property("live").toBool());
            QCOMPARE(shape->property("physics").value<QObject *>(), body);
        }
        QVERIFY(object->setProperty("view", QStringLiteral("2d")));
        for (auto *parent : {ordinary, static_cast<QQuick3DObject *>(nullptr)}) {
            shape->setParentItem(parent);
            QTRY_VERIFY(!shape->property("live").toBool());
            QTRY_COMPARE(body->property("linearAxisLock").toInt(), noLock);
            QTRY_COMPARE(body->property("angularAxisLock").toInt(), noLock);
        }
        shape->setParentItem(root);
        QTRY_VERIFY(shape->property("live").toBool());
        QTRY_COMPARE(body->property("linearAxisLock").toInt(), depthLock);
        QTRY_COMPARE(body->property("angularAxisLock").toInt(), tiltLocks);
        QCOMPARE(shape->property("physics").value<QObject *>(), body);
    }

    void aTriggersBodyKeepsItsOwnerAndParentState_data()
    {
        QTest::addColumn<QString>("form");
        QTest::newRow("box") << QStringLiteral("box");
        QTest::newRow("sphere") << QStringLiteral("sphere");
    }

    void aTriggersBodyKeepsItsOwnerAndParentState()
    {
        QFETCH(QString, form);
        QQmlComponent component(engine);
        component.setData(QStringLiteral(
            "import QtQuick3D\nimport QQ\nNode {\n"
            "    property var playing: false\n"
            "    Trigger { objectName: \"trigger\"; form: \"%1\"; scale: Qt.vector3d(2, 3, 4) }\n"
            "    Trigger { objectName: \"other\"; form: \"%1\"; scale: Qt.vector3d(5, 6, 7) }\n"
            "    Group { objectName: \"group\"; position: Qt.vector3d(8, 0, 0) }\n"
            "    Prefab { objectName: \"prefab\" }\n"
            "    Node { objectName: \"ordinary\" }\n}\n").arg(form).toUtf8(),
            QUrl(QStringLiteral("qrc:/trigger-parent-state.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuick3DObject *>(object.get());
        auto *trigger = object->findChild<QQuick3DObject *>(QStringLiteral("trigger"));
        auto *other = object->findChild<QQuick3DObject *>(QStringLiteral("other"));
        auto *group = object->findChild<QQuick3DObject *>(QStringLiteral("group"));
        auto *prefab = object->findChild<QQuick3DObject *>(QStringLiteral("prefab"));
        auto *ordinary = object->findChild<QQuick3DObject *>(QStringLiteral("ordinary"));
        QVERIFY(root && trigger && other && group && prefab && ordinary);
        const auto restore = qScopeGuard([&] { trigger->setParentItem(root); trigger->setParent(root); });
        QVERIFY(!trigger->property("live").toBool());
        QVERIFY(root->setProperty("playing", QStringLiteral("true")));
        QVERIFY(!trigger->property("live").toBool());
        QVERIFY(!trigger->property("physics").value<QObject *>());
        QVERIFY(root->setProperty("playing", true));
        QTRY_VERIFY(trigger->property("live").toBool());
        QObject *body = trigger->property("physics").value<QObject *>();
        QObject *otherBody = other->property("physics").value<QObject *>();
        QVERIFY(body && otherBody && body != otherBody);
        QCOMPARE(body->property("entity").value<QObject *>(), static_cast<QObject *>(trigger));
        QCOMPARE(otherBody->property("entity").value<QObject *>(), static_cast<QObject *>(other));
        QQmlListReference colliders(body, "collisionShapes");
        QQmlListReference otherColliders(otherBody, "collisionShapes");
        QCOMPARE(colliders.count(), 1);
        QCOMPARE(otherColliders.count(), 1);
        QVERIFY(colliders.at(0) != otherColliders.at(0));
        if (form == QStringLiteral("box")) {
            QCOMPARE(colliders.at(0)->property("extents").value<QVector3D>(), QVector3D(2, 3, 4));
            QCOMPARE(otherColliders.at(0)->property("extents").value<QVector3D>(), QVector3D(5, 6, 7));
        } else {
            QCOMPARE(colliders.at(0)->property("diameter").toDouble(), 4.0);
            QCOMPARE(otherColliders.at(0)->property("diameter").toDouble(), 7.0);
        }
        QVERIFY(trigger->setProperty("scale", QVariant::fromValue(QVector3D(4, 5, 6))));
        if (form == QStringLiteral("box")) {
            QTRY_COMPARE(colliders.at(0)->property("extents").value<QVector3D>(), QVector3D(4, 5, 6));
            QCOMPARE(otherColliders.at(0)->property("extents").value<QVector3D>(), QVector3D(5, 6, 7));
        } else {
            QTRY_COMPARE(colliders.at(0)->property("diameter").toDouble(), 6.0);
            QCOMPARE(otherColliders.at(0)->property("diameter").toDouble(), 7.0);
        }
        for (auto *parent : {root, group, prefab}) {
            trigger->setParentItem(parent);
            QTRY_COMPARE(body->property("position").value<QVector3D>(), trigger->property("scenePosition").value<QVector3D>());
            QVERIFY(root->setProperty("playing", false));
            QTRY_VERIFY(!trigger->property("live").toBool());
            QVERIFY(root->setProperty("playing", true));
            QTRY_VERIFY(trigger->property("live").toBool());
            QCOMPARE(trigger->property("physics").value<QObject *>(), body);
        }
        for (auto *parent : {ordinary, static_cast<QQuick3DObject *>(nullptr)}) {
            trigger->setParentItem(parent);
            QTRY_VERIFY(!trigger->property("live").toBool());
        }
        trigger->setParentItem(root);
        QTRY_VERIFY(trigger->property("live").toBool());
        QCOMPARE(trigger->property("physics").value<QObject *>(), body);
    }

    void aTriggerCountsOnlyItsActualVolume()
    {
        QQmlComponent component(engine);
        component.setData("import QtQuick3D\nimport QQ\nNode {\n"
                          "    property bool playing: true\n"
                          "    Trigger { id: zone; objectName: \"trigger\" }\n"
                          "    Node { objectName: \"ordinary\"; property int collisionCount: 17 }\n"
                          "    function clearBody() { zone.physics = null }\n}\n",
                          QUrl(QStringLiteral("qrc:/trigger-count.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QObject *trigger = object->findChild<QObject *>(QStringLiteral("trigger"));
        QObject *ordinary = object->findChild<QObject *>(QStringLiteral("ordinary"));
        QVERIFY(trigger && ordinary);
        QObject *body = trigger->property("physics").value<QObject *>();
        QVERIFY(body);
        const auto restore = qScopeGuard([&] { trigger->setProperty("physics", QVariant::fromValue(body)); });
        QCOMPARE(trigger->property("inside").toInt(), 0);
        QVERIFY(trigger->setProperty("physics", QVariant::fromValue(ordinary)));
        QCOMPARE(trigger->property("inside").toInt(), 0);
        QVERIFY(ordinary->setProperty("collisionCount", 23));
        QCOMPARE(trigger->property("inside").toInt(), 0);
        QVERIFY(QMetaObject::invokeMethod(object.get(), "clearBody"));
        QVERIFY(!trigger->property("physics").value<QObject *>());
        QCOMPARE(trigger->property("inside").toInt(), 0);
        QVERIFY(trigger->setProperty("physics", QVariant::fromValue(body)));
        QCOMPARE(trigger->property("inside").toInt(), body->property("collisionCount").toInt());
    }

    void aTriggerReportsOptionalEntityMetadata()
    {
        QQmlComponent component(engine);
        component.setData("import QtQml\nimport QtQuick3D\nimport QtQuick3D.Physics\nimport QQ\nNode {\n"
                          "    id: fixture\n    property bool playing: true\n"
                          "    property QtObject marker: QtObject {}\n"
                          "    Trigger { id: zone; objectName: \"trigger\" }\n"
                          "    DynamicRigidBody { id: marked; property QtObject entity: fixture.marker }\n"
                          "    StaticRigidBody { id: unmarked }\n"
                          "    function notifyMarked() { const volume = zone.physics as TriggerBody;\n"
                          "        volume.bodyEntered(marked); volume.bodyExited(marked) }\n"
                          "    function notifyUnmarked() { const volume = zone.physics as TriggerBody;\n"
                          "        volume.bodyEntered(unmarked); volume.bodyExited(unmarked) }\n"
                          "    function notifyNull() { const volume = zone.physics as TriggerBody;\n"
                          "        volume.bodyEntered(null); volume.bodyExited(null) }\n}\n",
                          QUrl(QStringLiteral("qrc:/trigger-entity.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QObject *trigger = object->findChild<QObject *>(QStringLiteral("trigger"));
        QObject *marker = object->property("marker").value<QObject *>();
        QVERIFY(trigger && marker);
        QSignalSpy entered(trigger, SIGNAL(entered(QObject *)));
        QSignalSpy exited(trigger, SIGNAL(exited(QObject *)));
        QVERIFY(entered.isValid() && exited.isValid());
        for (const char *method : {"notifyMarked", "notifyUnmarked", "notifyNull"}) {
            QVERIFY(QMetaObject::invokeMethod(object.get(), method));
            QCOMPARE(entered.count(), 1);
            QCOMPARE(exited.count(), 1);
            QObject *expected = QByteArray(method) == "notifyMarked" ? marker : nullptr;
            QCOMPARE(entered.takeFirst().at(0).value<QObject *>(), expected);
            QCOMPARE(exited.takeFirst().at(0).value<QObject *>(), expected);
        }
    }

    void playerMotionRequiresItsCharacterController_data()
    {
        QTest::addColumn<QString>("replacement");
        QTest::newRow("character") << QString();
        QTest::newRow("missing") << QStringLiteral("null");
        QTest::newRow("ordinary-node") << QStringLiteral("ordinary");
        QTest::newRow("static-body") << QStringLiteral("standing");
        QTest::newRow("trigger-body") << QStringLiteral("volume");
    }

    void playerMotionRequiresItsCharacterController()
    {
        QFETCH(QString, replacement);
        QQmlComponent component(engine);
        component.setData("import QtQuick\nimport QtQuick3D\nimport QtQuick3D.Physics\nimport QQ\nNode {\n"
                          "    property bool playing: true\n    property bool paused: false\n"
                          "    property real gravity: 9.81\n    property QtObject player: null\n"
                          "    Player { id: hero; objectName: \"hero\" }\n"
                          "    Node { objectName: \"ordinary\"; y: -50\n"
                          "        property int collisions: 0\n"
                          "        property vector3d movement: Qt.vector3d(7, 8, 9)\n"
                          "        property int teleports: 0\n"
                          "        function teleport(where) { ++teleports }\n    }\n"
                          "    StaticRigidBody { objectName: \"standing\" }\n"
                          "    TriggerBody { objectName: \"volume\" }\n"
                          "    function clearBody() { hero.physics = null }\n}\n",
                          QUrl(QStringLiteral("qrc:/player-controller.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QObject *hero = object->findChild<QObject *>(QStringLiteral("hero"));
        QVERIFY(hero);
        QObject *body = hero->property("physics").value<QObject *>();
        QVERIFY(body);
        QObject *animation = nullptr;
        for (QObject *child : hero->findChildren<QObject *>()) {
            if (child->metaObject()->indexOfProperty("frameTime") >= 0
                && child->metaObject()->indexOfProperty("running") >= 0
                && child->metaObject()->indexOfSignal("triggered()") >= 0) {
                QVERIFY(!animation);
                animation = child;
            }
        }
        QVERIFY(animation);
        QVERIFY(animation->property("running").toBool());
        const auto restore = qScopeGuard([&] { hero->setProperty("physics", QVariant::fromValue(body)); });
        QObject *other = nullptr;
        if (replacement == QStringLiteral("null")) {
            QVERIFY(QMetaObject::invokeMethod(object.get(), "clearBody"));
            QVERIFY(!hero->property("physics").value<QObject *>());
        } else if (!replacement.isEmpty()) {
            other = object->findChild<QObject *>(replacement);
            QVERIFY(other);
            QVERIFY(hero->setProperty("physics", QVariant::fromValue(other)));
        }
        // Even a delivered frame must leave a node with lookalike controller properties untouched.
        if (replacement == QStringLiteral("ordinary")) {
            QVERIFY(QMetaObject::invokeMethod(animation, "triggered"));
            QCOMPARE(other->property("movement").value<QVector3D>(), QVector3D(7, 8, 9));
            QCOMPARE(other->property("teleports").toInt(), 0);
        }
        QCOMPARE(animation->property("running").toBool(), replacement.isEmpty());
        QVERIFY(hero->setProperty("physics", QVariant::fromValue(body)));
        QVERIFY(animation->property("running").toBool());
        QCOMPARE(body->property("entity").value<QObject *>(), hero);
    }

    void aPlayerKeepsItsSceneOwnerAndPauseState_data()
    {
        QTest::addColumn<bool>("alreadySelected");
        QTest::newRow("empty-selection") << false;
        QTest::newRow("existing-selection") << true;
    }

    void aPlayerKeepsItsSceneOwnerAndPauseState()
    {
        QFETCH(bool, alreadySelected);
        QQmlComponent component(engine);
        component.setData("import QtQuick\nimport QtQuick3D\nimport QQ\nNode {\n"
                          "    property var playing: false\n    property bool paused: false\n"
                          "    property real gravity: 9.81\n    property QtObject player: null\n"
                          "    Player { objectName: \"hero\"; position: Qt.vector3d(2, 3, 4) }\n"
                          "    Node { objectName: \"ordinary\" }\n"
                          "    Node { objectName: \"compatible\"; property bool playing: true\n"
                          "        property bool paused: true; property real gravity: 4\n"
                          "        property QtObject player: null\n    }\n}\n",
                          QUrl(QStringLiteral("qrc:/player-scene-state.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuick3DObject *>(object.get());
        auto *hero = object->findChild<QQuick3DObject *>(QStringLiteral("hero"));
        auto *ordinary = object->findChild<QQuick3DObject *>(QStringLiteral("ordinary"));
        auto *compatible = object->findChild<QQuick3DObject *>(QStringLiteral("compatible"));
        QVERIFY(root && hero && ordinary && compatible);
        const auto restore = qScopeGuard([&] { hero->setParentItem(root); hero->setParent(root); });
        QObject *animation = nullptr;
        for (QObject *child : hero->findChildren<QObject *>()) {
            if (child->metaObject()->indexOfProperty("frameTime") >= 0
                && child->metaObject()->indexOfProperty("running") >= 0
                && child->metaObject()->indexOfSignal("triggered()") >= 0) {
                QVERIFY(!animation);
                animation = child;
            }
        }
        QVERIFY(animation);
        QVERIFY(!animation->property("running").toBool());
        QVERIFY(!hero->property("physics").value<QObject *>());
        if (alreadySelected)
            QVERIFY(root->setProperty("player", QVariant::fromValue(static_cast<QObject *>(ordinary))));
        QVERIFY(root->setProperty("playing", QStringLiteral("true")));
        QVERIFY(!hero->property("live").toBool());
        QVERIFY(!hero->property("physics").value<QObject *>());
        QVERIFY(root->setProperty("playing", true));
        QTRY_VERIFY(animation->property("running").toBool());
        QObject *body = hero->property("physics").value<QObject *>();
        QVERIFY(body);
        QCOMPARE(body->property("entity").value<QObject *>(), static_cast<QObject *>(hero));
        QCOMPARE(body->property("position").value<QVector3D>(), QVector3D(2, 3.9f, 4));
        QCOMPARE(root->property("player").value<QObject *>(), static_cast<QObject *>(alreadySelected ? ordinary : hero));
        QVERIFY(root->setProperty("paused", true));
        QTRY_VERIFY(!animation->property("running").toBool());
        QVERIFY(root->setProperty("paused", false));
        QTRY_VERIFY(animation->property("running").toBool());
        hero->setParentItem(compatible);
        QTRY_VERIFY(hero->property("live").toBool());
        QTRY_VERIFY(!animation->property("running").toBool());
        QVERIFY(compatible->setProperty("paused", false));
        QTRY_VERIFY(animation->property("running").toBool());
        QVERIFY(compatible->setProperty("playing", false));
        QTRY_VERIFY(!animation->property("running").toBool());
        QVERIFY(compatible->setProperty("playing", true));
        QTRY_VERIFY(animation->property("running").toBool());
        for (auto *parent : {ordinary, static_cast<QQuick3DObject *>(nullptr)}) {
            hero->setParentItem(parent);
            QTRY_VERIFY(!hero->property("live").toBool());
            QTRY_VERIFY(!animation->property("running").toBool());
        }
        hero->setParentItem(root);
        QTRY_VERIFY(animation->property("running").toBool());
        QCOMPARE(hero->property("physics").value<QObject *>(), body);
        QCOMPARE(root->property("player").value<QObject *>(), static_cast<QObject *>(alreadySelected ? ordinary : hero));
    }

    void a2DPlayerCarriesOnlyASprite_data()
    {
        QTest::addColumn<QString>("lookName");
        QTest::addColumn<QString>("objectName");
        QTest::addColumn<bool>("carried");
        QTest::newRow("sprite") << QStringLiteral("Look") << QStringLiteral("real") << true;
        QTest::newRow("lookalike-node") << QStringLiteral("Lookalike") << QStringLiteral("ordinary") << false;
        QTest::newRow("other-entity") << QStringLiteral("Other") << QStringLiteral("other") << false;
        QTest::newRow("missing-name") << QStringLiteral("Missing") << QString() << false;
        QTest::newRow("empty-name") << QString() << QString() << false;
    }

    void a2DPlayerCarriesOnlyASprite()
    {
        QFETCH(QString, lookName);
        QFETCH(QString, objectName);
        QFETCH(bool, carried);
        QTemporaryDir dir;
        const QString asset = dir.filePath(QStringLiteral("look.png"));
        QImage image(16, 32, QImage::Format_RGBA8888);
        image.fill(Qt::white);
        QVERIFY(image.save(asset));
        QQmlComponent component(engine);
        component.setData("import QtQuick\nimport QtQuick3D\nimport QQ as Runtime\nNode {\n"
                          "    property bool playing: false\n    property bool paused: true\n"
                          "    property bool replaying: true\n    property real gravity: 10\n"
                          "    property string view: \"2d\"\n    property real pixelsPerUnit: 16\n"
                          "    property QtObject player: null\n"
                          "    Runtime.Player2D { objectName: \"hero\"; height: 2 }\n"
                          "    Runtime.Sprite { objectName: \"real\"; name: \"Look\"; scale: Qt.vector3d(2, 3, 1); position: Qt.vector3d(8, 9, 6) }\n"
                          "    Node { objectName: \"ordinary\"; property string name: \"Lookalike\"\n"
                          "        property string kind: \"Sprite\"; property bool flip: false\n"
                          "        property vector3d size: Qt.vector3d(100, 200, 1); position: Qt.vector3d(8, 9, 6) }\n"
                          "    Runtime.Shape { objectName: \"other\"; name: \"Other\"; body: \"none\"; position: Qt.vector3d(8, 9, 6) }\n}\n",
                          QUrl(QStringLiteral("qrc:/player2d-look.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QObject *hero = object->findChild<QObject *>(QStringLiteral("hero"));
        QObject *real = object->findChild<QObject *>(QStringLiteral("real"));
        QObject *ordinary = object->findChild<QObject *>(QStringLiteral("ordinary"));
        QObject *other = object->findChild<QObject *>(QStringLiteral("other"));
        QVERIFY(hero && real && ordinary && other);
        QVERIFY(real->setProperty("source", QUrl::fromLocalFile(asset)));
        QCOMPARE(real->property("size").value<QVector3D>(), QVector3D(1, 2, 1));
        QVERIFY(object->setProperty("playing", true));
        QVERIFY(hero->property("physics").value<QObject *>());
        QVERIFY(hero->setProperty("look", lookName));
        QObject *selected = objectName.isEmpty() ? nullptr : object->findChild<QObject *>(objectName);
        QCOMPARE(hero->property("sprite").value<QObject *>(), selected);
        QVERIFY(hero->setProperty("_lastX", 1.0));
        QVERIFY(hero->setProperty("_lastY", 2.0));
        QVERIFY(hero->setProperty("atX", 5.0));
        QVERIFY(hero->setProperty("atY", 10.0));
        QVERIFY(hero->setProperty("facingLeft", true));
        QSignalSpy warnings(engine, &QQmlEngine::warnings);
        QVERIFY(warnings.isValid());
        QVERIFY(QMetaObject::invokeMethod(hero, "_show", Q_ARG(QVariant, QVariant(0.25))));
        QCOMPARE(warnings.count(), 0);
        QCOMPARE(hero->property("_shownX").toDouble(), 2.0);
        QCOMPARE(hero->property("_shownY").toDouble(), 4.0);
        QCOMPARE(real->property("position").value<QVector3D>(), carried ? QVector3D(2, 6, 6) : QVector3D(8, 9, 6));
        QCOMPARE(real->property("flip").toBool(), carried);
        QCOMPARE(ordinary->property("position").value<QVector3D>(), QVector3D(8, 9, 6));
        QCOMPARE(ordinary->property("flip").toBool(), false);
        QCOMPARE(other->property("position").value<QVector3D>(), QVector3D(8, 9, 6));
    }

    void a2DPlayerKeepsItsCarrierAndSceneClock_data()
    {
        QTest::addColumn<bool>("alreadySelected");
        QTest::newRow("empty-selection") << false;
        QTest::newRow("existing-selection") << true;
    }

    void a2DPlayerKeepsItsCarrierAndSceneClock()
    {
        QFETCH(bool, alreadySelected);
        QQmlComponent component(engine);
        component.setData("import QtQuick\nimport QtQuick3D\nimport QQ as Runtime\nNode {\n"
                          "    property var playing: false\n    property bool paused: true\n"
                          "    property bool replaying: false\n    property real gravity: 10\n"
                          "    property real _owed: 0.005\n    property real stepRate: 50\n"
                          "    property QtObject player: null\n"
                          "    Runtime.Player2D { objectName: \"hero\"; height: 2; position: Qt.vector3d(2, 3, 4) }\n"
                          "    Node { objectName: \"ordinary\" }\n"
                          "    Node { objectName: \"compatible\"; property bool playing: true\n"
                          "        property bool paused: false; property bool replaying: true; property real gravity: 4\n"
                          "        property real _owed: 0; property real stepRate: 60; property QtObject player: null }\n}\n",
                          QUrl(QStringLiteral("qrc:/player2d-clock.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuick3DObject *>(object.get());
        auto *hero = object->findChild<QQuick3DObject *>(QStringLiteral("hero"));
        auto *ordinary = object->findChild<QQuick3DObject *>(QStringLiteral("ordinary"));
        auto *compatible = object->findChild<QQuick3DObject *>(QStringLiteral("compatible"));
        QVERIFY(root && hero && ordinary && compatible);
        const auto restore = qScopeGuard([&] { hero->setParentItem(root); hero->setParent(root); });
        QObject *animation = nullptr;
        for (QObject *child : hero->findChildren<QObject *>()) {
            if (child->metaObject()->indexOfProperty("frameTime") >= 0
                && child->metaObject()->indexOfProperty("running") >= 0
                && child->metaObject()->indexOfSignal("triggered()") >= 0) {
                QVERIFY(!animation);
                animation = child;
            }
        }
        QVERIFY(animation);
        QVERIFY(!animation->property("running").toBool());
        if (alreadySelected)
            QVERIFY(root->setProperty("player", QVariant::fromValue(static_cast<QObject *>(ordinary))));
        QVERIFY(root->setProperty("playing", QStringLiteral("true")));
        QVERIFY(!hero->property("live").toBool());
        QVERIFY(!hero->property("physics").value<QObject *>());
        QVERIFY(root->setProperty("playing", true));
        QTRY_VERIFY(animation->property("running").toBool());
        QObject *body = hero->property("physics").value<QObject *>();
        QVERIFY(body);
        QCOMPARE(body->property("entity").value<QObject *>(), static_cast<QObject *>(hero));
        QCOMPARE(body->property("isKinematic").toBool(), true);
        QCOMPARE(body->property("kinematicPosition").value<QVector3D>(), QVector3D(2, 4, 4));
        QCOMPARE(root->property("player").value<QObject *>(), static_cast<QObject *>(alreadySelected ? ordinary : hero));
        QQmlListReference colliders(body, "collisionShapes");
        QCOMPARE(colliders.count(), 1);
        QCOMPARE(colliders.at(0)->property("extents").value<QVector3D>(), QVector3D(0.75f, 2, 1));
        QVERIFY(hero->setProperty("width", 2.0));
        QVERIFY(hero->setProperty("height", 3.0));
        QCOMPARE(colliders.at(0)->property("extents").value<QVector3D>(), QVector3D(2, 3, 1));
        QVERIFY(hero->setProperty("_lastX", 1.0));
        QVERIFY(hero->setProperty("atX", 5.0));
        QVERIFY(QMetaObject::invokeMethod(animation, "triggered"));
        QCOMPARE(hero->property("_shownX").toDouble(), 5.0);  // paused: draw the current place
        QVERIFY(root->setProperty("paused", false));
        QVERIFY(QMetaObject::invokeMethod(animation, "triggered"));
        QCOMPARE(hero->property("_shownX").toDouble(), 2.0);  // quarter of the next step
        QVERIFY(root->setProperty("stepRate", 100.0));
        QVERIFY(QMetaObject::invokeMethod(animation, "triggered"));
        QCOMPARE(hero->property("_shownX").toDouble(), 3.0);
        QVERIFY(root->setProperty("_owed", 0.02));
        QVERIFY(QMetaObject::invokeMethod(animation, "triggered"));
        QCOMPARE(hero->property("_shownX").toDouble(), 5.0);  // interpolation never passes the current place
        hero->setParentItem(compatible);
        QTRY_VERIFY(!animation->property("running").toBool());
        QVERIFY(compatible->setProperty("replaying", false));
        QTRY_VERIFY(animation->property("running").toBool());
        QVERIFY(QMetaObject::invokeMethod(animation, "triggered"));
        QCOMPARE(hero->property("_shownX").toDouble(), 1.0);
        QVERIFY(compatible->setProperty("playing", false));
        QTRY_VERIFY(!hero->property("live").toBool());
        QTRY_VERIFY(!animation->property("running").toBool());
        QVERIFY(compatible->setProperty("playing", true));
        for (auto *parent : {ordinary, static_cast<QQuick3DObject *>(nullptr)}) {
            hero->setParentItem(parent);
            QTRY_VERIFY(!hero->property("live").toBool());
            QTRY_VERIFY(!animation->property("running").toBool());
        }
        hero->setParentItem(root);
        QTRY_VERIFY(animation->property("running").toBool());
        QCOMPARE(hero->property("physics").value<QObject *>(), body);
        QCOMPARE(root->property("player").value<QObject *>(), static_cast<QObject *>(alreadySelected ? ordinary : hero));
    }

    void groupsNestEntitiesAndTheFileKeepsThem()
    {
        QTemporaryDir dir;
        const QUrl url = QUrl::fromLocalFile(QDir(dir.path()).filePath(QStringLiteral("scenes/nest.qml")));
        QObject *scene = io->loadText(QStringLiteral(
            "import QQ\nScene {\n    name: \"nest\"\n"
            "    Group {\n        name: \"Post\"\n        position: Qt.vector3d(5, 0, 0)\n"
            "        Shape { name: \"Pole\"; position: Qt.vector3d(1, 0, 0); scale: Qt.vector3d(0.2, 3, 0.2) }\n"
            "        Group { name: \"Top\"; position: Qt.vector3d(0, 3, 0)\n"
            "            Lamp { name: \"Light\" }\n        }\n    }\n"
            "    Shape { name: \"Floor\" }\n}\n"), url, nullptr);
        QVERIFY2(scene, qPrintable(io->lastError()));
        // Kept by the file: written inside its group, and read back the same bytes.
        const QString text = io->write(scene, url);
        QVERIFY(text.contains(QStringLiteral("    Group {\n        name: \"Post\"\n")));
        QVERIFY(text.contains(QStringLiteral("\n        Shape {\n            name: \"Pole\"\n")));
        QVERIFY(text.contains(QStringLiteral("\n            Lamp {\n                name: \"Light\"\n")));
        QObject *again = io->loadText(text, url, nullptr);
        QCOMPARE(io->write(again, url), text);
        delete again;

        // Listed in file order, each after its group; the scene found from inside.
        QStringList names;
        for (const QVariant &e : io->entities(scene))
            names << e.value<QObject *>()->property("name").toString();
        QCOMPARE(names, (QStringList{QStringLiteral("Post"), QStringLiteral("Pole"), QStringLiteral("Top"),
                                     QStringLiteral("Light"), QStringLiteral("Floor")}));
        QObject *light = entityNamed(scene, QStringLiteral("Light"));
        QCOMPARE(io->sceneOf(light), scene);
        QCOMPARE(light->property("scenePosition").value<QVector3D>(), QVector3D(5, 3, 0));

        // Added into a group; a player cannot be.
        QObject *post = entityNamed(scene, QStringLiteral("Post"));
        QObject *sign = io->add(scene, QStringLiteral("Shape"), {{QStringLiteral("name"), QStringLiteral("Sign")}}, post);
        QVERIFY2(sign, qPrintable(io->lastError()));
        QCOMPARE(qobject_cast<QQuick3DObject *>(sign)->parentItem(), qobject_cast<QQuick3DObject *>(post));
        QVERIFY(!io->add(scene, QStringLiteral("Player"), {}, post));
        QVERIFY(io->lastError().contains(QStringLiteral("cannot be in a group")));

        // Moved between groups where it is in the world: out to the scene, and into the top.
        QObject *pole = entityNamed(scene, QStringLiteral("Pole"));
        QVERIFY(io->reparent(pole, scene));
        QCOMPARE(pole->property("position").value<QVector3D>(), QVector3D(6, 0, 0));
        QObject *top = entityNamed(scene, QStringLiteral("Top"));
        QVERIFY(io->reparent(pole, top));
        QCOMPARE(pole->property("position").value<QVector3D>(), QVector3D(1, -3, 0));
        QCOMPARE(pole->property("scenePosition").value<QVector3D>(), QVector3D(6, 0, 0));
        // A group cannot go into what is in it.
        QVERIFY(!io->reparent(post, top));

        // Removed, a group takes what is in it.
        QVERIFY(io->remove(post));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        names.clear();
        for (const QVariant &e : io->entities(scene))
            names << e.value<QObject *>()->property("name").toString();
        QCOMPARE(names, QStringList{QStringLiteral("Floor")});
        delete scene;
    }

    void kinematicsMeasureThroughGroupsInDoubles()
    {
        // A wall in a group moved 5 along and turned a quarter about the depth axis: its box, 1 by 2 in the group, is 2
        // by 1 in the world, at (5, 1): a 2D mover from the left stops against it, and an overlap is measured there.
        QObject *scene = io->loadText(QStringLiteral(
            "import QQ\nScene {\n    name: \"walls\"\n    view: \"2d\"\n"
            "    Group {\n        name: \"Turned\"\n        position: Qt.vector3d(5, 1, 0)\n"
            "        eulerRotation: Qt.vector3d(0, 0, 90)\n"
            "        Shape { name: \"Wall\"; scale: Qt.vector3d(1, 2, 1) }\n"
            "        Trigger { name: \"Zone\"; position: Qt.vector3d(0, -3, 0) }\n    }\n"
            "    Trigger { name: \"Probe\"; position: Qt.vector3d(8, 1, 0) }\n}\n"),
            QUrl::fromLocalFile(QStringLiteral("/tmp/walls.qml")), nullptr);
        QVERIFY2(scene, qPrintable(io->lastError()));
        QObject *probe = entityNamed(scene, QStringLiteral("Probe"));
        // A step of 4 (a mover's steps are small: this is where it ends that is measured).
        const QVariantMap moved = io->move2D(scene, probe, 0, 1, 0, 0.5, 0.25, 4, 0);
        // The wall's left side is at 5 - 1 = 4; the mover, half a unit wide, stops at 3.5.
        QCOMPARE(moved.value(QStringLiteral("x")).toDouble(), 3.5);
        QVERIFY(moved.value(QStringLiteral("right")).toBool());
        // The zone, 3 below the group's middle before its turn, is 3 to the right after it: at (8, 1), over the probe.
        QVERIFY(io->overlaps(entityNamed(scene, QStringLiteral("Zone")), probe));
        delete scene;
    }

    void aPrefabIsMadeFromItsFileAndNeverWrittenIntoTheScene()
    {
        QTemporaryDir project;
        const QDir root(project.path());
        root.mkpath(QStringLiteral("prefabs"));
        root.mkpath(QStringLiteral("scenes"));
        const QString prefab = QStringLiteral(
            "import QQ\n\nGroup {\n    name: \"Lamp post\"\n    position: Qt.vector3d(0, 0, 0)\n"
            "    eulerRotation: Qt.vector3d(0, 0, 0)\n    scale: Qt.vector3d(1, 1, 1)\n\n"
            "    Shape { name: \"Pole\"; scale: Qt.vector3d(0.2, 3, 0.2) }\n    Lamp { name: \"Light\"; position: Qt.vector3d(0, 3, 0) }\n}\n");
        {
            QFile f(root.filePath(QStringLiteral("prefabs/lamp-post.qml")));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(prefab.toUtf8());
        }
        const QUrl url = QUrl::fromLocalFile(root.filePath(QStringLiteral("scenes/street.qml")));
        QObject *scene = io->loadText(QStringLiteral(
            "import QQ\nScene {\n    name: \"street\"\n"
            "    Prefab { name: \"West\"; source: \"../prefabs/lamp-post.qml\"; position: Qt.vector3d(-4, 0, 0) }\n"
            "    Prefab { name: \"East\"; source: \"../prefabs/lamp-post.qml\"; position: Qt.vector3d(4, 0, 0) }\n}\n"),
            url, nullptr);
        QVERIFY2(scene, qPrintable(io->lastError()));
        // The scene's own entities are the two prefabs; each is made of the file's parts, where it stands.
        QCOMPARE(io->entities(scene).size(), 2);
        QCOMPARE(io->everyEntity(scene).size(), 8);  // each prefab, its root group, the pole and the light
        QObject *east = entityNamed(scene, QStringLiteral("East"));
        QCOMPARE(east->property("error").toString(), QString());
        QVariant light;
        QMetaObject::invokeMethod(east, "part", Q_RETURN_ARG(QVariant, light), Q_ARG(QVariant, QStringLiteral("Light")));
        QVERIFY(light.value<QObject *>());
        QCOMPARE(light.value<QObject *>()->property("scenePosition").value<QVector3D>(), QVector3D(4, 3, 0));
        // Written, the scene keeps where each stands and its file; never their parts.
        const QString text = io->write(scene, url);
        QVERIFY(text.contains(QStringLiteral("        source: \"../prefabs/lamp-post.qml\"\n")));
        QVERIFY(!text.contains(QStringLiteral("Pole")));
        QVERIFY(!text.contains(QStringLiteral("Lamp {")));
        // A project lists its prefabs.
        QCOMPARE(io->project(QUrl::fromLocalFile(project.path())).value(QStringLiteral("prefabs")).toStringList(),
                 QStringList{QStringLiteral("lamp-post")});

        // A file whose root is not a Group is no prefab, and says so.
        {
            QFile f(root.filePath(QStringLiteral("prefabs/not.qml")));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("import QQ\nShape { name: \"Lonely\" }\n");
        }
        QObject *bad = io->add(scene, QStringLiteral("Prefab"), {{QStringLiteral("source"), QStringLiteral("../prefabs/not.qml")}});
        QVERIFY(bad);
        QVERIFY2(bad->property("error").toString().contains(QStringLiteral("root is not a Group")),
                 qPrintable(bad->property("error").toString()));

        // A group written as a prefab: at the origin, the same bytes as the file a person would keep; never over one.
        QObject *street = io->loadText(QStringLiteral(
            "import QQ\nScene {\n    name: \"street\"\n"
            "    Group {\n        name: \"Lamp post\"\n        position: Qt.vector3d(7, 0, 2)\n"
            "        Shape { name: \"Pole\"; scale: Qt.vector3d(0.2, 3, 0.2) }\n"
            "        Lamp { name: \"Light\"; position: Qt.vector3d(0, 3, 0) }\n    }\n}\n"), url, nullptr);
        QObject *group = entityNamed(street, QStringLiteral("Lamp post"));
        QVERIFY(io->writePrefab(group, QUrl::fromLocalFile(project.path()), QStringLiteral("Lamp post")).isEmpty());
        QVERIFY(io->lastError().contains(QStringLiteral("there already")));
        const QUrl written = io->writePrefab(group, QUrl::fromLocalFile(project.path()), QStringLiteral("Corner Lamp"));
        QCOMPARE(written, QUrl::fromLocalFile(root.filePath(QStringLiteral("prefabs/corner-lamp.qml"))));
        const QString kept = readAll(written.toLocalFile());
        QVERIFY(kept.startsWith(QStringLiteral("import QQ\n\nGroup {\n    name: \"Lamp post\"\n    position: Qt.vector3d(0, 0, 0)\n")));
        QVERIFY(kept.contains(QStringLiteral("        name: \"Pole\"\n")));
        QCOMPARE(group->property("position").value<QVector3D>(), QVector3D(7, 0, 2));  // the group itself stays put

        // Brought into another project, a prefab goes into its prefabs/ folder.
        QTemporaryDir other;
        const QUrl adopted = io->adopt(written, QUrl::fromLocalFile(other.path()));
        QCOMPARE(adopted, QUrl::fromLocalFile(QDir(other.path()).filePath(QStringLiteral("prefabs/corner-lamp.qml"))));
        delete street;
        delete scene;
    }

    void aProjectListsItsAssetsByKindAndLogicIsCheckedByLine()
    {
        // The Studio's asset browser and logic editor (P3.15): what a project's assets/ holds that a scene can use, by
        // kind, sorted, a model's buffer not among them; logic built where its file is, each error by its line.
        QTemporaryDir project;
        const QDir root(project.path());
        auto touch = [&root](const QString &path, const QByteArray &bytes = "x") {
            root.mkpath(QFileInfo(root.filePath(path)).absolutePath());
            QFile f(root.filePath(path));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(bytes);
        };
        for (const char *path : {"assets/orb.gltf", "assets/orb.bin", "assets/pictures/stone.png", "assets/beep.wav",
                                 "assets/dusk.hdr", "assets/level.tmj", "assets/notes.txt"})
            touch(QLatin1String(path));
        QStringList seen;
        for (const QVariant &a : io->project(QUrl::fromLocalFile(project.path())).value(QStringLiteral("assets")).toList()) {
            const QVariantMap m = a.toMap();
            seen << m.value(QStringLiteral("path")).toString() + QLatin1Char('=') + m.value(QStringLiteral("kind")).toString();
            QCOMPARE(m.value(QStringLiteral("url")).toUrl(), QUrl::fromLocalFile(root.filePath(m.value(QStringLiteral("path")).toString())));
        }
        QCOMPARE(seen, (QStringList{QStringLiteral("assets/beep.wav=sound"), QStringLiteral("assets/dusk.hdr=sky"),
                                    QStringLiteral("assets/level.tmj=map"), QStringLiteral("assets/orb.gltf=model"),
                                    QStringLiteral("assets/pictures/stone.png=picture")}));

        // Logic that builds has no errors; logic that does not, its errors by line.
        const QUrl file = QUrl::fromLocalFile(root.filePath(QStringLiteral("logic/spin.qml")));
        QVERIFY(io->checkLogic(QStringLiteral("import QQ\nLogic {\n    onStep: (dt) => {}\n}\n"), file).isEmpty());
        const QVariantList problems = io->checkLogic(QStringLiteral("import QQ\nLogic {\n    onStep: (dt) => {\n}\n"), file);
        QVERIFY(!problems.isEmpty());
        QCOMPARE(problems.first().toMap().value(QStringLiteral("line")).toInt(), 5);
        QVERIFY(!problems.first().toMap().value(QStringLiteral("message")).toString().isEmpty());
        // An unknown type is an error too, where it is written.
        const QVariantList unknown = io->checkLogic(QStringLiteral("import QQ\nLogic {\n    Dragon {}\n}\n"), file);
        QCOMPARE(unknown.size(), 1);
        QCOMPARE(unknown.first().toMap().value(QStringLiteral("line")).toInt(), 3);

        // A text file read whole; one that is not there is nothing, and said.
        touch(QStringLiteral("logic/spin.qml"), "import QQ\nLogic {}\n");
        QCOMPARE(io->readText(file).toString(), QStringLiteral("import QQ\nLogic {}\n"));
        QVERIFY(!io->readText(QUrl::fromLocalFile(root.filePath(QStringLiteral("logic/none.qml")))).isValid());
        QVERIFY(io->lastError().contains(QStringLiteral("could not be read")));
    }

    // ── saving (P3.16, ADR-105) ──

    void aStoreHasATypedQmlContractButCannotBeConstructedFromQml()
    {
        // A replay's copy stays in memory: this contract check needs no player's files.
        qq::GameStore store(QVariantMap{{QStringLiteral("best"), 7}}, nullptr);
        QQmlEngine::setObjectOwnership(&store, QQmlEngine::CppOwnership);
        QQmlComponent view(engine);
        view.setData("import QtQml\nimport QQ\nQtObject {\n"
                     "    required property GameStore saved\n"
                     "    readonly property int count: saved ? saved.keys.length : 0\n"
                     "    readonly property int bytes: saved ? saved.size : 0\n"
                     "    readonly property string problem: saved ? saved.problem : \"\"\n"
                     "    function forget() { saved.clear() }\n}\n", QUrl(QStringLiteral("qrc:/store-contract.qml")));
        QVERIFY2(view.isReady(), qPrintable(view.errorString()));
        std::unique_ptr<QObject> object(view.createWithInitialProperties(
            {{QStringLiteral("saved"), QVariant::fromValue(static_cast<QObject *>(&store))}}));
        QVERIFY2(object, qPrintable(view.errorString()));
        QCOMPARE(object->property("count").toInt(), 1);
        QVERIFY(object->property("bytes").toInt() > 0);
        QCOMPARE(object->property("problem").toString(), QString());
        QVERIFY(!store.setValue(QStringLiteral("bad"), QJSValue(QJSValue::UndefinedValue)));
        QVERIFY(!store.problem().isEmpty());
        QTRY_COMPARE(object->property("problem").toString(), store.problem());
        QVERIFY(QMetaObject::invokeMethod(object.get(), "forget"));
        QVERIFY(store.keys().isEmpty());
        QTRY_COMPARE(object->property("count").toInt(), 0);

        // The type describes an instance supplied by the runtime; a game cannot choose its own store.
        QQmlComponent direct(engine);
        direct.setData("import QQ\nGameStore {}\n", QUrl(QStringLiteral("qrc:/chosen-store.qml")));
        QCOMPARE(direct.status(), QQmlComponent::Error);
        QVERIFY2(direct.errorString().contains(QStringLiteral("supplied by the runtime")), qPrintable(direct.errorString()));
    }

    void aSceneRecorderAcceptsRunsAndRefusesUnrelatedObjects()
    {
        std::unique_ptr<QObject> scene(io->loadText(QStringLiteral("import QQ\nScene {}\n"), QUrl(), nullptr));
        QVERIFY2(scene, qPrintable(io->lastError()));
        QVERIFY(!scene->property("recorder").value<QObject *>());
        qq::RunRecord record;
        QVERIFY(scene->setProperty("recorder", QVariant::fromValue(static_cast<QObject *>(&record))));
        QCOMPARE(scene->property("recorder").value<QObject *>(), static_cast<QObject *>(&record));
        QObject unrelated;
        QVERIFY(!scene->setProperty("recorder", QVariant::fromValue(&unrelated)));
        QCOMPARE(scene->property("recorder").value<QObject *>(), static_cast<QObject *>(&record));
        // Existing callers clear through an invalid QVariant rather than a typed pointer.
        QVERIFY(scene->setProperty("recorder", QVariant()));
        QVERIFY(!scene->property("recorder").value<QObject *>());
    }

    void aWorldBuildsItsSkyOncePerColourBatch()
    {
        QQuickWindow host;
        QQmlComponent component(engine);
        component.setData("import QQ\nWorld {\n"
                          "    property int skyChanges: 0\n"
                          "    onMadeSkyChanged: ++skyChanges\n"
                          "    skyTop: \"#120d1f\"; skyHorizon: \"#3a1630\"; groundHorizon: \"#0a0710\"\n"
                          "}\n", QUrl(QStringLiteral("qrc:/sky-batches.qml")));
        std::unique_ptr<QObject> world(component.create());
        QVERIFY2(world, qPrintable(component.errorString()));
        auto *viewport = qobject_cast<QQuickItem *>(world.get());
        QVERIFY(viewport);
        viewport->setParentItem(host.contentItem());
        QTRY_VERIFY(world->property("madeSky").value<QObject *>());
        QCoreApplication::processEvents();
        QCOMPARE(world->property("skyChanges").toInt(), 1);
        QObject *first = world->property("madeSky").value<QObject *>();
        QObject *texture = world->property("made").value<QObject *>();
        QVERIFY(texture);
        QCOMPARE(first->parent(), texture);
        QCOMPARE(first->property("skyTopColor").value<QColor>(), QColor(QStringLiteral("#120d1f")));
        QCOMPARE(first->property("skyHorizonColor").value<QColor>(), QColor(QStringLiteral("#3a1630")));
        QCOMPARE(first->property("groundHorizonColor").value<QColor>(), QColor(QStringLiteral("#0a0710")));
        QCOMPARE(first->property("groundBottomColor").value<QColor>(), world->property("groundBottom").value<QColor>());
        QSignalSpy discarded(first, &QObject::destroyed);

        QVERIFY(world->setProperty("skyTop", QColor(QStringLiteral("#112233"))));
        QVERIFY(world->setProperty("skyHorizon", QColor(QStringLiteral("#445566"))));
        QVERIFY(world->setProperty("groundBottom", QColor(QStringLiteral("#778899"))));
        QCOMPARE(world->property("madeSky").value<QObject *>(), first);
        QTRY_COMPARE(world->property("skyChanges").toInt(), 2);
        QObject *second = world->property("madeSky").value<QObject *>();
        QVERIFY(second && second != first);
        QCOMPARE(second->parent(), texture);
        QCOMPARE(second->property("skyTopColor").value<QColor>(), QColor(QStringLiteral("#112233")));
        QCOMPARE(second->property("skyHorizonColor").value<QColor>(), QColor(QStringLiteral("#445566")));
        QCOMPARE(second->property("groundHorizonColor").value<QColor>(), QColor(QStringLiteral("#0a0710")));
        QCOMPARE(second->property("groundBottomColor").value<QColor>(), QColor(QStringLiteral("#778899")));
        QTRY_COMPARE(discarded.count(), 1);
        QVERIFY(world->setProperty("skyTop", QColor(QStringLiteral("#112233"))));
        QCoreApplication::processEvents();
        QCOMPARE(world->property("madeSky").value<QObject *>(), second);
        QCOMPARE(world->property("skyChanges").toInt(), 2);
    }

    void aWorldSelectsBakedLightingByItsColoursAndKeepsCustomSkies()
    {
        QQuickWindow host;
        QQmlComponent component(engine);
        component.setData("import QQ\nWorld {}\n", QUrl(QStringLiteral("qrc:/baked-sky.qml")));
        std::unique_ptr<QObject> world(component.create());
        QVERIFY2(world, qPrintable(component.errorString()));
        auto *viewport = qobject_cast<QQuickItem *>(world.get());
        QVERIFY(viewport);
        viewport->setParentItem(host.contentItem());
        QObject *environment = world->property("environment").value<QObject *>();
        QObject *baked = world->property("bakedSky").value<QObject *>();
        QObject *made = world->property("made").value<QObject *>();
        QObject *photographed = world->property("photographed").value<QObject *>();
        QVERIFY(environment && baked && made && photographed);
        QCOMPARE(environment->property("lightProbe").value<QObject *>(), baked);

        const auto bakedResource = [&] {
            return qmlContext(baked)->resolvedUrl(baked->property("source").toUrl());
        };
        const QUrl defaultResource = bakedResource();
        QCOMPARE(defaultResource.scheme(), QStringLiteral("qrc"));
        QFile defaultImage(QLatin1Char(':') + defaultResource.path());
        QVERIFY(defaultImage.open(QIODevice::ReadOnly));
        QCOMPARE(defaultImage.read(12), QByteArray::fromHex("ab4b5458203131bb0d0a1a0a"));
        QVERIFY(defaultImage.seek(0));
        QCOMPARE(QCryptographicHash::hash(defaultImage.readAll(), QCryptographicHash::Sha256).toHex(),
                 QByteArray("5ea4c877f220890665c20a8b1c2045c0142885eeaa01260ae10b9e201b1ce097"));

        const char *fields[] = {"skyTop", "skyHorizon", "groundHorizon", "groundBottom"};
        const QColor defaults[] = {QColor("#0b1430"), QColor("#c7623a"), QColor("#2a1a1a"), QColor("#07080c")};
        for (int i = 0; i < 4; ++i) {
            QVERIFY(world->setProperty(fields[i], QColor("#123456")));
            QCOMPARE(environment->property("lightProbe").value<QObject *>(), made);
            QVERIFY(world->property("bakedSkySource").toUrl().isEmpty());
            QTRY_VERIFY(world->property("madeSky").value<QObject *>());
            QVERIFY(world->setProperty(fields[i], defaults[i]));
            QCOMPARE(environment->property("lightProbe").value<QObject *>(), baked);
            QCOMPARE(bakedResource(), defaultResource);
        }

        QVERIFY(world->setProperty("skyTop", QColor("#120d1f")));
        QVERIFY(world->setProperty("skyHorizon", QColor("#3a1630")));
        QVERIFY(world->setProperty("groundHorizon", QColor("#0a0710")));
        QCOMPARE(environment->property("lightProbe").value<QObject *>(), baked);
        QVERIFY(bakedResource() != defaultResource);
        QFile sampleImage(QLatin1Char(':') + bakedResource().path());
        QVERIFY(sampleImage.open(QIODevice::ReadOnly));
        QCOMPARE(sampleImage.read(12), QByteArray::fromHex("ab4b5458203131bb0d0a1a0a"));
        QVERIFY(sampleImage.seek(0));
        QCOMPARE(QCryptographicHash::hash(sampleImage.readAll(), QCryptographicHash::Sha256).toHex(),
                 QByteArray("a9900cb3f246c8dceb1daaf7174563b304d191741b3174292e58f52c29ff1858"));
        QVERIFY(world->setProperty("groundBottom", QColor("#8807080c")));
        QCOMPARE(environment->property("lightProbe").value<QObject *>(), made);
        QVERIFY(world->setProperty("groundBottom", QColor("#07080c")));
        QCOMPARE(environment->property("lightProbe").value<QObject *>(), baked);

        QVERIFY(world->setProperty("skyTurn", 90.0));
        QVERIFY(world->setProperty("skyLight", 1.25));
        QCOMPARE(environment->property("lightProbe").value<QObject *>(), baked);
        QCOMPARE(environment->property("probeOrientation").value<QVector3D>(), QVector3D(0, 90, 0));
        QCOMPARE(environment->property("probeExposure").toDouble(), 1.25);
        QVERIFY(world->setProperty("sky", QUrl(QStringLiteral("qrc:/custom-sky.hdr"))));
        QCOMPARE(environment->property("lightProbe").value<QObject *>(), photographed);
        QVERIFY(world->setProperty("sky", QUrl()));
        QCOMPARE(environment->property("lightProbe").value<QObject *>(), baked);
    }

    void aGamesLoadedSceneDoesNotReplaceItsViewportScene()
    {
        QTemporaryDir project;
        QVERIFY(project.isValid());
        for (const QString &name : {QStringLiteral("first"), QStringLiteral("second")}) {
            QFile file(project.filePath(name + QStringLiteral(".qml")));
            QVERIFY(file.open(QIODevice::WriteOnly));
            const QByteArray text = QStringLiteral("import QQ\nScene { name: \"%1\"; view: \"2d\" }\n").arg(name).toUtf8();
            QCOMPARE(file.write(text), qint64(text.size()));
        }
        QQuickWindow host;
        QQmlComponent component(engine);
        component.setData("import QQ\nGame { hearing: false }\n", QUrl(QStringLiteral("qrc:/game-viewport.qml")));
        std::unique_ptr<QObject> game(component.create());
        QVERIFY2(game, qPrintable(component.errorString()));
        auto *viewport = qobject_cast<QQuickItem *>(game.get());
        QVERIFY(viewport);
        viewport->setParentItem(host.contentItem());
        const auto release = qScopeGuard([&game] { QMetaObject::invokeMethod(game.get(), "release"); });
        QObject *viewportScene = game->property("scene").value<QObject *>();
        QVERIFY(viewportScene);
        QVERIFY(!game->property("playingScene").value<QObject *>());
        for (const QString &name : {QStringLiteral("first"), QStringLiteral("second")}) {
            QVERIFY(game->setProperty("file", QUrl::fromLocalFile(project.filePath(name + QStringLiteral(".qml")))));
            QObject *playing = game->property("playingScene").value<QObject *>();
            QVERIFY(playing);
            QVERIFY(playing != viewportScene);
            QCOMPARE(playing->property("name").toString(), name);
            QCOMPARE(game->property("scene").value<QObject *>(), viewportScene);
            QVERIFY(game->property("flat").toBool());
            QVERIFY(game->property("settled").toBool());
        }
        QVERIFY(QMetaObject::invokeMethod(game.get(), "release"));
        QVERIFY(!game->property("playingScene").value<QObject *>());
        QCOMPARE(game->property("scene").value<QObject *>(), viewportScene);
        QVERIFY(!game->property("settled").toBool());
    }

    void aGameFollowsItsLoadedSceneInterface_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::newRow("scene") << QByteArray("import QQ\nScene { name: \"interface\" }\n");
        // SceneIO admits a 3D node by its scene interface, not by one generated QML class.
        QTest::newRow("structural-node") << QByteArray(
            "import QtQuick\n"
            "import QtQuick3D\n"
            "Node {\n"
            "    property string kind: \"Scene\"\n"
            "    property QtObject player: null\n"
            "    property string view: \"3d\"\n"
            "    property real pixelsPerUnit: 16\n"
            "    property real viewHeight: 12\n"
            "    property bool pixelArt: true\n"
            "    property real exposure: 1\n"
            "    property bool bloom: true\n"
            "    property real skyLight: 0.75\n"
            "    property color skyTop: \"#0b1430\"\n"
            "    property color skyHorizon: \"#c7623a\"\n"
            "    property color groundHorizon: \"#2a1a1a\"\n"
            "    property color groundBottom: \"#07080c\"\n"
            "    property url sky\n"
            "    property real skyTurn: 0\n"
            "    property int playSeed: 0\n"
            "    property QtObject recorder: null\n"
            "    property bool playing: false\n"
            "    signal asked(var message)\n"
            "    function tell(message) { return playing && message === \"hello\" }\n"
            "    function keptInView(point, halfWidth, halfHeight) { return point }\n"
            "}\n"
        );
    }

    void aGameFollowsItsLoadedSceneInterface()
    {
        QFETCH(QByteArray, text);
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        QTemporaryDir project;
        QVERIFY(project.isValid());
        QFile source(project.filePath(QStringLiteral("scene.qml")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write(text), qint64(text.size()));
        source.close();

        QQuickWindow host;
        qq::RunRecord recorder;
        QQmlComponent component(engine);
        component.setData("import QQ\nGame { hearing: false; width: 640; height: 360 }\n",
                          QUrl(QStringLiteral("qrc:/game-interface.qml")));
        std::unique_ptr<QObject> game(component.create());
        QVERIFY2(game, qPrintable(component.errorString()));
        auto *viewport = qobject_cast<QQuickItem *>(game.get());
        QVERIFY(viewport);
        viewport->setParentItem(host.contentItem());
        const auto release = qScopeGuard([&game] { QMetaObject::invokeMethod(game.get(), "release"); });
        QVERIFY(game->setProperty("seed", 83));
        QVERIFY(game->setProperty("recorder", QVariant::fromValue<QObject *>(&recorder)));
        QVERIFY(game->setProperty("file", QUrl::fromLocalFile(source.fileName())));
        QObject *scene = game->property("playingScene").value<QObject *>();
        QVERIFY2(scene, qPrintable(game->property("problem").toString()));
        QVERIFY(scene->property("playing").toBool());
        QCOMPARE(scene->property("playSeed").toInt(), 83);
        QCOMPARE(scene->property("recorder").value<QObject *>(), &recorder);
        QVERIFY(game->property("settled").toBool());

        const QVariantMap edits{{QStringLiteral("pixelsPerUnit"), 32}, {QStringLiteral("viewHeight"), 9},
            {QStringLiteral("pixelArt"), false}, {QStringLiteral("exposure"), 1.5}, {QStringLiteral("bloom"), false},
            {QStringLiteral("skyLight"), 0.9}, {QStringLiteral("skyTop"), QColor("#120d1f")},
            {QStringLiteral("skyHorizon"), QColor("#3a1630")}, {QStringLiteral("groundHorizon"), QColor("#0a0710")},
            {QStringLiteral("groundBottom"), QColor("#07080c")}, {QStringLiteral("skyTurn"), 45}};
        for (auto it = edits.cbegin(); it != edits.cend(); ++it) {
            const QByteArray field = it.key().toUtf8();
            QVERIFY(scene->setProperty(field.constData(), it.value()));
            QCOMPARE(game->property(field.constData()), scene->property(field.constData()));
        }
        const QUrl sky = QUrl::fromLocalFile(project.filePath(QStringLiteral("edited.hdr")));
        QVERIFY(scene->setProperty("sky", sky));
        QCOMPARE(game->property("sky").toUrl(), sky);
        QVERIFY(scene->setProperty("sky", QUrl()));
        QCOMPARE(game->property("sky").toUrl(), QUrl());

        QQmlComponent followerComponent(engine);
        followerComponent.setData("import QtQuick\nQtObject { property vector3d eye: Qt.vector3d(1, 2, 3); "
                                  "property vector3d lookAt: Qt.vector3d(4, 5, 6) }\n",
                                  QUrl(QStringLiteral("qrc:/game-follower.qml")));
        std::unique_ptr<QObject> first(followerComponent.create());
        std::unique_ptr<QObject> second(followerComponent.create());
        QVERIFY2(first && second, qPrintable(followerComponent.errorString()));
        for (QObject *follower : {first.get(), second.get()}) {
            QVERIFY(scene->setProperty("player", QVariant::fromValue(follower)));
            QCOMPARE(game->property("follow").value<QObject *>(), follower);
            QCOMPARE(game->property("eye").value<QVector3D>(), QVector3D(1, 2, 3));
            QCOMPARE(game->property("target").value<QVector3D>(), QVector3D(4, 5, 6));
            QVERIFY(follower->setProperty("eye", QVariant::fromValue(QVector3D(2, 3, 4))));
            QCOMPARE(game->property("eye").value<QVector3D>(), QVector3D(2, 3, 4));
            QVERIFY(follower->setProperty("lookAt", QVariant::fromValue(QVector3D(7, 8, 9))));
            for (const QString &view : {QStringLiteral("2d"), QStringLiteral("3d")}) {
                QVERIFY(scene->setProperty("view", view));
                QCOMPARE(game->property("flat").toBool(), view == QStringLiteral("2d"));
                QCOMPARE(game->property("target").value<QVector3D>(), QVector3D(7, 8, 9));
            }
        }
        QSignalSpy asked(game.get(), SIGNAL(asked(QVariant)));
        QVERIFY(asked.isValid());
        QVERIFY(QMetaObject::invokeMethod(scene, "asked", Q_ARG(QVariant, QVariant(QStringLiteral("question")))));
        QCOMPARE(asked.size(), 1);
        QVariant told;
        QVERIFY(QMetaObject::invokeMethod(game.get(), "tell", Q_RETURN_ARG(QVariant, told),
                                          Q_ARG(QVariant, QVariant(QStringLiteral("hello")))));
        QVERIFY(told.toBool());
        QVERIFY(scene->setProperty("player", QVariant::fromValue<QObject *>(nullptr)));
        QVERIFY(!game->property("follow").value<QObject *>());
        QVERIFY(QMetaObject::invokeMethod(game.get(), "release"));
        QVERIFY(!game->property("playingScene").value<QObject *>());
        QVERIFY(!game->property("settled").toBool());
        QVERIFY(!game->property("flat").toBool());
        QCOMPARE(game->property("exposure").toDouble(), 1.0);
        QVERIFY(QMetaObject::invokeMethod(game.get(), "tell", Q_RETURN_ARG(QVariant, told),
                                          Q_ARG(QVariant, QVariant(QStringLiteral("hello")))));
        QVERIFY(!told.toBool());
    }

    void playerLabelsFollowTheirSceneAndFollower()
    {
        QTemporaryDir project;
        QVERIFY(project.isValid());
        QFile source(project.filePath(QStringLiteral("labels.qml")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        const QByteArray text("import QQ\nScene { name: \"First\"; view: \"2d\"; Player2D { style: \"top-down\" } }\n");
        QCOMPARE(source.write(text), qint64(text.size()));
        source.close();
        QQmlComponent component(engine, QUrl::fromLocalFile(QDir(QStringLiteral(QQ_FIXTURES)).filePath(QStringLiteral("../../player/Main.qml"))));
        std::unique_ptr<QObject> window(component.createWithInitialProperties({
            {QStringLiteral("scene"), QUrl::fromLocalFile(source.fileName())}, {QStringLiteral("visible"), false}}));
        QVERIFY2(window, qPrintable(component.errorString()));
        QObject *game = window->findChild<QObject *>(QStringLiteral("game"));
        QVERIFY(game);
        QTRY_COMPARE(window->property("title").toString(), QStringLiteral("First — QQ Player"));
        QObject *scene = game->property("playingScene").value<QObject *>();
        QObject *follower = game->property("follow").value<QObject *>();
        QVERIFY(scene);
        QVERIFY(follower);
        QObject *hint = nullptr;
        for (QObject *child : window->findChildren<QObject *>()) {
            if (child->metaObject()->indexOfProperty("text") >= 0
                && child->property("text").toString().startsWith(QStringLiteral("WASD or the left stick"))) {
                hint = child;
                break;
            }
        }
        QVERIFY(hint);
        QVERIFY(scene->setProperty("name", QStringLiteral("Renamed <b>scene</b>")));
        QTRY_COMPARE(window->property("title").toString(), QStringLiteral("Renamed <b>scene</b> — QQ Player"));
        QVERIFY(follower->setProperty("style", QStringLiteral("platform")));
        QTRY_VERIFY(hint->property("text").toString().startsWith(QStringLiteral("A and D or the left stick")));
        QVERIFY(follower->setProperty("style", QStringLiteral("top-down")));
        QTRY_VERIFY(hint->property("text").toString().startsWith(QStringLiteral("WASD or the left stick")));
        QVERIFY(scene->setProperty("player", QVariant::fromValue<QObject *>(nullptr)));
        QTRY_COMPARE(hint->property("text").toString(), QString());
        QVERIFY(scene->setProperty("view", QStringLiteral("3d")));
        QTRY_VERIFY(hint->property("text").toString().startsWith(QStringLiteral("Drag to turn")));
        QVERIFY(QMetaObject::invokeMethod(window.get(), "release"));
        QTRY_COMPARE(window->property("title").toString(), QStringLiteral("QQ — QQ Player"));
    }

    void playerStartupErrorsShowFilenamesAsPlainText()
    {
        QTemporaryDir project;
        QVERIFY(project.isValid());
        const auto missing = [&](const QString &name) {
            return QUrl::fromLocalFile(QDir(project.path()).filePath(name));
        };
        QQmlComponent component(engine, QUrl::fromLocalFile(QDir(QStringLiteral(QQ_FIXTURES)).filePath(QStringLiteral("../../player/Main.qml"))));
        std::unique_ptr<QObject> window(component.createWithInitialProperties({
            {QStringLiteral("scene"), missing(QStringLiteral("<b>First.qml"))},
            {QStringLiteral("visible"), false}}));
        QVERIFY2(window, qPrintable(component.errorString()));
        QObject *game = window->findChild<QObject *>(QStringLiteral("game"));
        QVERIFY(game);
        QTRY_VERIFY(window->property("problem").toString().contains(QStringLiteral("First.qml")));
        QObject *label = nullptr;
        for (QObject *child : window->findChildren<QObject *>()) {
            if (child->metaObject()->indexOfProperty("textFormat") >= 0
                && child->property("text").toString().startsWith(QStringLiteral("This game could not start: "))) {
                label = child;
                break;
            }
        }
        QVERIFY(label);
        QCOMPARE(label->property("textFormat").toInt(), int(Qt::PlainText));
        QCOMPARE(label->property("text").toString(), QStringLiteral("This game could not start: ") + window->property("problem").toString());
        QVERIFY(game->setProperty("problem", QStringLiteral("<b>External game problem</b>")));
        QTRY_VERIFY(label->property("text").toString().contains(QStringLiteral("<b>External game problem</b>")));
        QCOMPARE(label->property("textFormat").toInt(), int(Qt::PlainText));
        QVERIFY(game->setProperty("file", missing(QStringLiteral("<i>Second.qml"))));
        QTRY_VERIFY(label->property("text").toString().contains(QStringLiteral("Second.qml")));
        QCOMPARE(label->property("textFormat").toInt(), int(Qt::PlainText));
        QVERIFY(QMetaObject::invokeMethod(window.get(), "release"));
    }

    void aGameReportsSceneStartupFailuresAndCanRetry_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::newRow("object") << QByteArray("import QtQml\nQtObject { objectName: \"rejectedScene\"; property string kind: \"Scene\" }\n");
        QTest::newRow("node") << QByteArray("import QtQuick3D\nNode { objectName: \"rejectedScene\"; property string kind: \"Scene\" }\n");
    }

    void aGameReportsSceneStartupFailuresAndCanRetry()
    {
        QFETCH(QByteArray, text);
        QTemporaryDir project;
        auto write = [&](const QString &name, const QByteArray &contents) {
            QFile file(QDir(project.path()).filePath(name));
            return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
        };
        QVERIFY(write(QStringLiteral("bad.qml"), text));
        QVERIFY(write(QStringLiteral("good.qml"), "import QQ\nScene { name: \"recovered\" }\n"));
        QQuickWindow host;
        QQmlComponent component(engine);
        component.setData("import QQ\nGame { hearing: false }\n", QUrl(QStringLiteral("qrc:/game-errors.qml")));
        std::unique_ptr<QObject> game(component.create());
        QVERIFY2(game, qPrintable(component.errorString()));
        auto *viewport = qobject_cast<QQuickItem *>(game.get());
        QVERIFY(viewport);
        viewport->setParentItem(host.contentItem());
        const auto release = qScopeGuard([&game] { QMetaObject::invokeMethod(game.get(), "release"); });
        QVERIFY(game->setProperty("file", QUrl::fromLocalFile(QDir(project.path()).filePath(QStringLiteral("bad.qml")))));
        QTRY_VERIFY2(!game->property("problem").toString().isEmpty(), "a scene that could not start needs a reported problem");
        QVERIFY(!game->property("playingScene").value<QObject *>());
        QVERIFY(!game->property("settled").toBool());
        QTRY_VERIFY(!game->findChild<QObject *>(QStringLiteral("rejectedScene")));

        // A failed attempt neither leaves a scene behind nor prevents a valid file from starting.
        QVERIFY(game->setProperty("file", QUrl::fromLocalFile(QDir(project.path()).filePath(QStringLiteral("good.qml")))));
        QTRY_VERIFY(game->property("playingScene").value<QObject *>());
        QCOMPARE(game->property("playingScene").value<QObject *>()->property("name").toString(), QStringLiteral("recovered"));
        QCOMPARE(game->property("problem").toString(), QString());
        QVERIFY(game->property("settled").toBool());
        QVERIFY(QMetaObject::invokeMethod(game.get(), "release"));
    }

    void aStoreKeepsWhatJsonKeepsAndRefusesTheRest()
    {
        QTemporaryDir game;
        qq::GameStore *store = qq::GameStore::storeFor(QUrl::fromLocalFile(game.path()));
        QVERIFY(store);
        QVERIFY(store->kept());
        auto js = [this](const QString &text) { return engine->evaluate(text); };

        // Numbers, text, true and false, null, and lists and objects of them, given back as they were kept.
        QVERIFY(store->setValue(QStringLiteral("best"), js(QStringLiteral("12"))));
        QVERIFY(store->setValue(QStringLiteral("profile"),
                                js(QStringLiteral("({ name: 'Ada', levels: [1, 2.5, 3], sound: true, rival: null, "
                                                  "deep: { er: ['x'] } })"))));
        QCOMPARE(store->value(QStringLiteral("best")).toDouble(), 12.0);
        const QVariantMap profile = store->value(QStringLiteral("profile")).toMap();
        QCOMPARE(profile.value(QStringLiteral("name")).toString(), QStringLiteral("Ada"));
        QCOMPARE(profile.value(QStringLiteral("levels")).toList().at(1).toDouble(), 2.5);
        QCOMPARE(profile.value(QStringLiteral("sound")).toBool(), true);
        QVERIFY(profile.contains(QStringLiteral("rival")) && profile.value(QStringLiteral("rival")).isNull());
        QCOMPARE(profile.value(QStringLiteral("deep")).toMap().value(QStringLiteral("er")).toList().first().toString(),
                 QStringLiteral("x"));
        QVERIFY(store->problem().isEmpty());
        QCOMPARE(store->value(QStringLiteral("absent"), 7).toInt(), 7);  // nothing kept there: the fallback

        // Refused, each with its reason, and nothing of it kept.
        const QList<QPair<QString, QString>> refused = {
            {QString(), QStringLiteral("1")},
            {QString(101, QLatin1Char('k')), QStringLiteral("1")},
            {QStringLiteral("function"), QStringLiteral("(() => 1)")},
            {QStringLiteral("vector"), QStringLiteral("Qt.vector3d(1, 2, 3)")},
            {QStringLiteral("colour"), QStringLiteral("Qt.rgba(1, 0, 0, 1)")},
            {QStringLiteral("date"), QStringLiteral("new Date(0)")},
            {QStringLiteral("nan"), QStringLiteral("NaN")},
            {QStringLiteral("infinite"), QStringLiteral("1 / 0")},
            {QStringLiteral("undefined"), QStringLiteral("undefined")},
            {QStringLiteral("inList"), QStringLiteral("[1, () => 2]")},
            {QStringLiteral("inObject"), QStringLiteral("({ at: Qt.vector3d(0, 1, 0) })")},
            {QStringLiteral("deep"), QStringLiteral("(() => { let v = 1; for (let i = 0; i < 40; ++i) v = [v]; return v })()")},
            {QStringLiteral("deepest"), QStringLiteral("(() => { let v = {}; for (let i = 0; i < 300000; ++i) v = { v }; return v })()")},
            {QStringLiteral("huge"), QStringLiteral("'x'.repeat(70000)")},
        };
        for (const auto &[key, text] : refused) {
            QVERIFY2(!store->setValue(key, js(text)), qPrintable(text));
            QVERIFY2(!store->problem().isEmpty(), qPrintable(text));
        }
        QVERIFY(!store->setValue(QStringLiteral("object"), engine->newQObject(new QObject(this))));
        QCOMPARE(store->keys(), (QStringList{QStringLiteral("best"), QStringLiteral("profile")}));

        // Up to its 64 KiB it keeps, and the problem is gone; forgetting a key takes it away.
        QVERIFY(store->setValue(QStringLiteral("long"), js(QStringLiteral("'x'.repeat(60000)"))));
        QVERIFY(store->problem().isEmpty());
        QVERIFY(store->size() > 60000 && store->size() <= qq::GameStore::maxBytes);
        QVERIFY(store->remove(QStringLiteral("long")));
        QVERIFY(!store->remove(QStringLiteral("long")));
        store->clear();
        store->flush();
        QVERIFY(!QFileInfo::exists(qq::GameStore::placeOf(store->game())));
    }

    void aStoreIsWrittenAMomentAfterAChangeAndReadBackAsItWas()
    {
        QTemporaryDir game;
        const QUrl package = QUrl::fromLocalFile(game.path());
        qq::GameStore *store = qq::GameStore::storeFor(package);
        QCOMPARE(qq::GameStore::storeFor(package), store);  // one per game
        const QString file = qq::GameStore::placeOf(store->game());
        QVERIFY(file.startsWith(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)));
        QVERIFY(store->setValue(QStringLiteral("plays"), QJSValue(3)));
        QVERIFY(!QFileInfo::exists(file));  // not on every change,
        QTRY_VERIFY(QFileInfo::exists(file));  // but a moment later
        // As the next start of the game reads it: a store made afresh from the file.
        {
            qq::GameStore again(store->game());
            QCOMPARE(again.value(QStringLiteral("plays")).toInt(), 3);
            QVERIFY(again.problem().isEmpty());
        }
        // Forgetting everything leaves nothing behind.
        store->clear();
        store->flush();
        QVERIFY(!QFileInfo::exists(file));
    }

    void anUnreadableStoreIsKeptAsideAndTheGameStartsWithoutIt()
    {
        const QByteArray tooLarge = "{\"x\": \"" + QByteArray(qq::GameStore::maxBytes, 'x') + "\"}";
        for (const QByteArray &bad : {QByteArray("not json"), QByteArray("[1, 2]"), tooLarge}) {
            QTemporaryDir game;
            const QString id = qq::GameStore::gameOf(QUrl::fromLocalFile(game.path()));
            const QString file = qq::GameStore::placeOf(id);
            QVERIFY(QDir().mkpath(QFileInfo(file).absolutePath()));
            {
                QFile f(file);
                QVERIFY(f.open(QIODevice::WriteOnly));
                f.write(bad);
            }
            qq::GameStore store(id);
            QVERIFY(store.keys().isEmpty());
            QVERIFY(!store.problem().isEmpty());
            QVERIFY(!QFileInfo::exists(file));
            QFile aside(file + QStringLiteral(".unreadable"));
            QVERIFY(aside.open(QIODevice::ReadOnly));
            QCOMPARE(aside.readAll(), bad);
            aside.close();
            QVERIFY(aside.remove());
        }
    }

    void aGameIsItsFolderHoweverItIsWritten()
    {
        const QString one = qq::GameStore::gameOf(QUrl::fromLocalFile(QStringLiteral("C:/Games/Orb Run")));
        QVERIFY2(QRegularExpression(QStringLiteral("^orb-run-[0-9a-f]{16}$")).match(one).hasMatch(), qPrintable(one));
        QCOMPARE(qq::GameStore::gameOf(QUrl::fromLocalFile(QStringLiteral("C:/Games/Orb Run/"))), one);
        QCOMPARE(qq::GameStore::gameOf(QUrl::fromLocalFile(QStringLiteral("C:/Games/x/../Orb Run"))), one);
#ifdef Q_OS_WIN
        QCOMPARE(qq::GameStore::gameOf(QUrl::fromLocalFile(QStringLiteral("c:/games/orb run"))), one);
#endif
        // Found from a scene file as the Studio and the Player both find it.
        QCOMPARE(qq::GameStore::gameOf(qq::Confinement::packageOf(
                     QUrl::fromLocalFile(QStringLiteral("C:/Games/Orb Run/scenes/arena.qml")))),
                 one);
        // Another folder of the same name is another game.
        const QString another = qq::GameStore::gameOf(QUrl::fromLocalFile(QStringLiteral("D:/Elsewhere/Orb Run")));
        QVERIFY(another != one && another.startsWith(QStringLiteral("orb-run-")));
        // The bundled game is one; a package that is no folder is none.
        QVERIFY(qq::GameStore::gameOf(QUrl(QStringLiteral("qrc:/playground"))).startsWith(QStringLiteral("playground-")));
        QCOMPARE(qq::GameStore::gameOf(QUrl()), QString());
        QCOMPARE(qq::GameStore::gameOf(QUrl::fromLocalFile(QStringLiteral("relative/folder"))), QString());
        QCOMPARE(qq::GameStore::gameOf(QUrl(QStringLiteral("https://example.com/game"))), QString());
        QVERIFY(!qq::GameStore::storeFor(QUrl()));
    }

    // ── the one look (ADR-108) ──

    void theLookIsTheOneLooksSourceValueByValue()
    {
        // Read from docs/design/tokens.json itself, not from the header generated from it: a value that went stale
        // anywhere between the two would show here.
        QFile file(QStringLiteral(QQ_TOKENS));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject tokens = QJsonDocument::fromJson(file.readAll()).object();
        auto *look = engine->singletonInstance<QObject *>("QQ", "Look");
        QVERIFY(look);
        const QJsonObject colours = tokens.value(QStringLiteral("color")).toObject();
        QVERIFY(colours.size() >= 21);
        for (auto it = colours.constBegin(); it != colours.constEnd(); ++it) {
            // `void` is a word C++ and JavaScript keep: Look calls it `deepest`.
            const QString name = it.key() == QStringLiteral("void") ? QStringLiteral("deepest") : it.key();
            QCOMPARE(look->property(name.toUtf8()).value<QColor>(), QColor(it.value().toString()));
        }
        const QJsonObject radius = tokens.value(QStringLiteral("radius")).toObject();
        QCOMPARE(look->property("radiusControl").toInt(), radius.value(QStringLiteral("control")).toInt());
        QCOMPARE(look->property("radiusPanel").toInt(), radius.value(QStringLiteral("panel")).toInt());
        const QJsonObject size = tokens.value(QStringLiteral("size")).toObject();
        QCOMPARE(look->property("sizeBody").toInt(), size.value(QStringLiteral("body")).toInt());
        QCOMPARE(look->property("sizeFigure").toInt(), size.value(QStringLiteral("figure")).toInt());
        const QJsonObject font = tokens.value(QStringLiteral("font")).toObject();
        QCOMPARE(look->property("display").toString(),
                 font.value(QStringLiteral("display")).toObject().value(QStringLiteral("family")).toString());
        QCOMPARE(look->property("ui").toString(),
                 font.value(QStringLiteral("interface")).toObject().value(QStringLiteral("family")).toString());
        // Motion: the token durations, all 0 with reduced motion; the easing's control points.
        const QJsonObject motion = tokens.value(QStringLiteral("motion")).toObject();
        const bool was = look->property("reducedMotion").toBool();
        look->setProperty("reducedMotion", false);
        QCOMPARE(look->property("standard").toInt(), motion.value(QStringLiteral("standard")).toInt());
        QCOMPARE(look->property("fade").toInt(), motion.value(QStringLiteral("standard")).toInt());
        look->setProperty("reducedMotion", true);
        for (const char *name : {"quick", "standard", "enter", "sheen", "fade"})
            QCOMPARE(look->property(name).toInt(), 0);
        look->setProperty("reducedMotion", was);
        const QVariantList ease = look->property("ease").toList();
        QCOMPARE(ease.size(), 6);
        QCOMPARE(ease.at(0).toDouble(), 0.16);
        QCOMPARE(ease.at(2).toDouble(), 0.3);
    }

    void theLaunchersWordOnMotionComesBeforeTheSystems()
    {
        qputenv("QOR_REDUCED_MOTION", "1");
        QVERIFY(qq::Look::systemPrefersReducedMotion());
        qputenv("QOR_REDUCED_MOTION", "true");
        QVERIFY(qq::Look::systemPrefersReducedMotion());
        qputenv("QOR_REDUCED_MOTION", "0");
        QVERIFY(!qq::Look::systemPrefersReducedMotion());
        qunsetenv("QOR_REDUCED_MOTION");
    }

    // The colours a host passes as the runtime starts (QOR_THEME_COLORS), as Prism passes Lumen's (QOR-0016), are the
    // look's, its focus ring following the bright accent; a colour left out or misspelt stays the look's own.
    void aHostsColoursAreTheLooks()
    {
        qunsetenv("QOR_THEME_COLORS");
        const qq::Look own;
        qputenv("QOR_THEME_COLORS", QJsonDocument(QJsonObject{{"void", "#0b0e14"},
                                                              {"ink", "#f5f7f9"},
                                                              {"accent-bright", "#a9b9ff"},
                                                              {"edge", "#1cffffff"},
                                                              {"gilt", "not a colour"}})
                                        .toJson(QJsonDocument::Compact));
        const qq::Look hosted;
        qunsetenv("QOR_THEME_COLORS");
        QCOMPARE(hosted.property("deepest").value<QColor>(), QColor(0x0b, 0x0e, 0x14));
        QCOMPARE(hosted.property("ink").value<QColor>(), QColor(0xf5, 0xf7, 0xf9));
        QCOMPARE(hosted.property("accentBright").value<QColor>(), QColor(0xa9, 0xb9, 0xff));
        QCOMPARE(hosted.property("focus").value<QColor>(), QColor(0xa9, 0xb9, 0xff));
        QCOMPARE(hosted.property("edge").value<QColor>(), QColor(0xff, 0xff, 0xff, 0x1c));
        QCOMPARE(hosted.property("gilt").value<QColor>(), own.property("gilt").value<QColor>());
        QCOMPARE(hosted.property("base").value<QColor>(), own.property("base").value<QColor>());
        QVERIFY(own.property("deepest").value<QColor>() != hosted.property("deepest").value<QColor>());
    }

    // ── sound (P3.13, ADR-105) ──

    void aSoundFilesLengthAndTheMixersLevelsAreRead()
    {
        // chime.wav (make_chime.py) is one second long; what is not a WAV file has no length that can be told.
        qq::Mixer mixer;
        QCOMPARE(mixer.lengthOf(QUrl::fromLocalFile(fixture(QStringLiteral("chime.wav")))), 1.0);
        QCOMPARE(mixer.lengthOf(QUrl::fromLocalFile(fixture(QStringLiteral("orb.gltf")))), 0.0);
        QCOMPARE(qq::wavLength(QByteArray("RIFF\0\0\0\0WAVE", 12)), 0.0);
        // A group never set is at 1; levels are held to 0 to 1; what a sound is multiplied by is its group's level
        // times the master's.
        QCOMPARE(mixer.level(QStringLiteral("music")), 1.0);
        mixer.setLevel(QStringLiteral("music"), 0.5);
        mixer.setLevel(QStringLiteral("master"), 0.5);
        mixer.setLevel(QStringLiteral("ui"), 2);
        QCOMPARE(mixer.level(QStringLiteral("ui")), 1.0);
        QCOMPARE(mixer.gain(QStringLiteral("music")), 0.25);
        QCOMPARE(mixer.gain(QStringLiteral("effects")), 0.5);
        QCOMPARE(mixer.gain(QStringLiteral("master")), 0.5);
        QCOMPARE(mixer.levels().value(QStringLiteral("music")).toDouble(), 0.5);
        // Reset (the Studio's Stop): every group back at 1.
        mixer.reset();
        QCOMPARE(mixer.gain(QStringLiteral("music")), 1.0);
        QCOMPARE(mixer.levels().size(), 1);

        // A file replaced under the same name is measured again: half a second, then two.
        const auto wav = [](quint32 bytes) {
            QByteArray b("RIFF");
            const auto u32 = [&b](quint32 v) {
                for (int i = 0; i < 4; ++i)
                    b.append(char((v >> (8 * i)) & 0xff));
            };
            const auto u16 = [&b](quint16 v) {
                b.append(char(v & 0xff));
                b.append(char(v >> 8));
            };
            u32(36 + bytes);
            b.append("WAVEfmt ");
            u32(16);
            u16(1);     // PCM
            u16(1);     // one channel
            u32(1000);  // samples a second
            u32(1000);  // bytes a second
            u16(1);
            u16(8);
            b.append("data");
            u32(bytes);
            b.append(QByteArray(qsizetype(bytes), '\x80'));
            return b;
        };
        QTemporaryDir dir;
        const QString path = QDir(dir.path()).filePath(QStringLiteral("hit.wav"));
        const auto write = [&path](const QByteArray &bytes) {
            QFile f(path);
            return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
        };
        QVERIFY(write(wav(500)));
        QCOMPARE(mixer.lengthOf(QUrl::fromLocalFile(path)), 0.5);
        QVERIFY(write(wav(2000)));
        QCOMPARE(mixer.lengthOf(QUrl::fromLocalFile(path)), 2.0);

        // A browser's voice is panned from all left to all right, and no further.
        qq::WebVoice voice;
        voice.setPan(3);
        QCOMPARE(voice.pan(), 1.0);
        voice.setPan(-0.25);
        QCOMPARE(voice.pan(), -0.25);
        voice.setVolume(-1);
        QCOMPARE(voice.volume(), 0.0);
    }
};

QTEST_MAIN(TstScene)
#include "tst_scene.moc"
