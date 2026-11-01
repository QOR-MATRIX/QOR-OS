// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// Does each project come back as it was left, with nothing of any other in it? The real Studio window over real folders,
// as a person works it: a question answered by its buttons, a list clicked, a window closed and a new Studio started. And
// the hostile side: what the Studio remembers is read back as a stranger's would be, so each way that file can be wrong
// is planted, and the Studio opens anyway, says what a person needs to know, and reads nothing outside the project.
//
// A new window takes seconds to draw its first frame in software, so what only needs a project to be open in the Studio
// shares one window, moved from project to project as a person moves it; what is about starting or closing has its own.

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QScreen>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <system_error>
#include <vector>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

#include "confinement.h"
#include "designloop.h"
#include "gpu.h"
#include "keychain.h"
#include "mcpserver.h"
#include "projectworkspace.h"
#include "sceneio.h"
#include "studiotools.h"

static const QString chosenGpu = qq::preferHighPerformanceGpu();

Q_IMPORT_QML_PLUGIN(QQPlugin)
Q_IMPORT_QML_PLUGIN(QQ_StudioPlugin)

namespace {

QString fixture(const QString &name)
{
    return QDir(QStringLiteral(QQ_FIXTURES)).filePath(name);
}

bool writeFile(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
}

QByteArray readBytes(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

/// A symbolic link, where the system lets this user make one.
bool makeLink(const QString &target, const QString &where)
{
    QDir().mkpath(QFileInfo(where).absolutePath());
    std::error_code error;
    std::filesystem::create_symlink(target.toStdString(), where.toStdString(), error);
    return !error;
}

/// A project on disk, in a folder of its own.
struct Project {
    QTemporaryDir dir;

    QString path() const { return dir.path(); }
    QString real() const { return qq::Confinement::realPath(dir.path()); }
    QUrl url() const { return QUrl::fromLocalFile(dir.path()); }
    QString file(const QString &relative) const { return QDir(dir.path()).filePath(relative); }
    QString keptFile() const { return qq::ProjectWorkspace::fileFor(real()); }

    /// scenes/<name>.qml, with one shape in it that says whose it is.
    void scene(const QString &name, const QString &marker) const
    {
        writeFile(file(QStringLiteral("scenes/%1.qml").arg(name)),
                  QStringLiteral("import QQ\n\nScene {\n    name: \"%1\"\n    Shape {\n        name: \"%2\"\n    }\n}\n")
                      .arg(name, marker).toUtf8());
    }
    void logic(const QString &name, const QString &marker) const
    {
        writeFile(file(QStringLiteral("logic/%1.qml").arg(name)), logicText(marker).toUtf8());
    }
    static QString logicText(const QString &marker)
    {
        return QStringLiteral("import QQ\n\nLogic {\n    // %1\n}\n").arg(marker);
    }

    /// Every file in it, by what it holds.
    QMap<QString, QByteArray> contents() const
    {
        QMap<QString, QByteArray> all;
        QDirIterator it(path(), QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString found = it.next();
            all.insert(QDir(path()).relativeFilePath(found), readBytes(found));
        }
        return all;
    }
};

/// Made or trusted on this computer, as a person's own projects are.
void trust(const QUrl &project)
{
    qq::SceneIO().trustProject(project);
}

/// A scene whose code leaves a mark where the test can see it, as nothing else would.
QByteArray markingScene(const QString &mark)
{
    return QStringLiteral("import QtQml\nimport QQ\nScene {\n    name: \"%1\"\n    Component.onCompleted: Qt.application.domain = \"%1\"\n}\n")
        .arg(mark).toUtf8();
}

/// One open Studio.
struct Studio {
    QQmlApplicationEngine engine;
    QQuickWindow *window = nullptr;

    bool open(const QUrl &project, const QVariantMap &more = {})
    {
        QVariantMap properties{{QStringLiteral("model"), QUrl::fromLocalFile(fixture(QStringLiteral("orb.gltf")))},
                               {QStringLiteral("project"), project}};
        properties.insert(more);
        engine.setInitialProperties(properties);
        engine.loadFromModule("QQ.Studio", "Main");
        if (engine.rootObjects().isEmpty())
            return false;
        window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        if (!window || !QTest::qWaitForWindowExposed(window))
            return false;
        if (!QTest::qWaitFor([this] { return window->property("settled").toBool(); }, 15000))
            return false;
        [[maybe_unused]] const bool hasKeyboard = QTest::qWaitForWindowActive(window, 15000);
        return true;
    }

    ~Studio() { quit(); }

    /// As the Studio does when it quits.
    void quit()
    {
        if (window)
            QMetaObject::invokeMethod(window, "release");
        window = nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    QVariant call(const char *function, const QVariant &a = {}, const QVariant &b = {})
    {
        QVariant result;
        if (!a.isValid())
            QMetaObject::invokeMethod(window, function, Q_RETURN_ARG(QVariant, result));
        else if (!b.isValid())
            QMetaObject::invokeMethod(window, function, Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, a));
        else
            QMetaObject::invokeMethod(window, function, Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, a), Q_ARG(QVariant, b));
        return result;
    }

    qq::SceneIO *io() { return engine.singletonInstance<qq::SceneIO *>("QQ", "SceneIO"); }
    QVariant get(const char *name) const { return window->property(name); }
    QObject *child(const char *name) const { return window->findChild<QObject *>(QLatin1String(name)); }
    QObject *scene() const { return get("scene").value<QObject *>(); }
    QString sceneName() const { return scene() ? scene()->property("name").toString() : QString(); }
    QObject *editor() const { return child("logicEditor"); }
    QString logicName() const { return editor()->property("name").toString(); }
    QString logicText() const { return editor()->property("text").toString(); }
    QString notice() const { return get("notice").toString(); }
    QObject *history() const { return get("history").value<QObject *>(); }

    QObject *entity(const QString &name) const
    {
        for (const QVariant &e : get("entityList").toList()) {
            if (e.value<QObject *>()->property("name").toString() == name)
                return e.value<QObject *>();
        }
        return nullptr;
    }

    /// An edit a person would make, with what the Studio does after one.
    void edit()
    {
        QObject *first = get("entityList").toList().value(0).value<QObject *>();
        QVERIFY(first);
        first->setProperty("name", first->property("name").toString() + QStringLiteral(" edited"));
        call("refresh");
    }

    /// The logic editor's text is now this.
    void type(const QString &text) { child("logicText")->setProperty("text", text); }
    /// Text typed at the start of what is there, which the editor's own undo keeps.
    void typeAtStart(const QString &text)
    {
        QVERIFY(QMetaObject::invokeMethod(child("logicText"), "insert", Q_ARG(int, 0), Q_ARG(QString, text)));
    }

    void click(const char *objectName)
    {
        auto *item = window->findChild<QQuickItem *>(QLatin1String(objectName));
        QVERIFY2(item, objectName);
        clickOn(item);
    }
    void clickOn(QQuickItem *item)
    {
        QTest::mouseClick(window, Qt::LeftButton, {}, item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
    }

    /// Let a few frames pass, so what a layout does has been done.
    void settleFrames()
    {
        for (int i = 0; i < 6; ++i) {
            window->update();
            QTest::qWait(30);
        }
    }

    bool asking() const { return child("unsavedDialog")->property("opened").toBool(); }

    /// The question is on the screen, and the button with this name is pressed.
    void answer(const char *button)
    {
        QObject *dialog = child("unsavedDialog");
        QTRY_VERIFY2(dialog->property("opened").toBool(), "the question was not asked");
        click(button);
        QTRY_VERIFY(!dialog->property("visible").toBool());
    }

    /// Whether the trust question has come up, by the time it could have.
    void waitForTrustQuestion()
    {
        QTRY_VERIFY2(child("trustDialog")->property("opened").toBool(), "the trust question was not asked");
    }
    void answerTrust(const char *button)
    {
        waitForTrustQuestion();
        click(button);
        QTRY_VERIFY(!child("trustDialog")->property("visible").toBool());
    }

    QStringList recentPaths() const
    {
        QStringList paths;
        for (const QVariant &row : get("recentProjects").toList())
            paths << row.toMap().value(QStringLiteral("path")).toString();
        return paths;
    }

    /// Everything Cancel must leave as it was.
    QVariantMap state() const
    {
        return {{QStringLiteral("project"), get("project")},
                {QStringLiteral("scene"), QVariant::fromValue(scene())},
                {QStringLiteral("selection"), get("selection")},
                {QStringLiteral("sceneFile"), get("sceneFile")},
                {QStringLiteral("sceneName"), get("sceneName")},
                {QStringLiteral("savedText"), get("savedText")},
                {QStringLiteral("dirty"), get("dirty")},
                {QStringLiteral("text"), const_cast<Studio *>(this)->io()->write(scene(), get("sceneFile").toUrl())},
                {QStringLiteral("notice"), get("notice")},
                {QStringLiteral("playScene"), get("playScene")},
                {QStringLiteral("stats"), get("showStats")},
                {QStringLiteral("trusted"), get("projectTrusted")},
                {QStringLiteral("logicName"), logicName()},
                {QStringLiteral("logicText"), logicText()},
                {QStringLiteral("logicDirty"), editor()->property("dirty")},
                {QStringLiteral("historyAt"), history()->property("at")},
                {QStringLiteral("historyTexts"), history()->property("texts").toStringList()},
                {QStringLiteral("recent"), recentPaths()}};
    }
};

/// The Studio's two projects, each with a scene and a logic file of its own, and each person's own.
struct TwoProjects {
    Project a;
    Project b;

    TwoProjects()
    {
        a.scene(QStringLiteral("alpha"), QStringLiteral("Only in alpha"));
        a.logic(QStringLiteral("spin"), QStringLiteral("alpha's logic"));
        b.scene(QStringLiteral("beta"), QStringLiteral("Only in beta"));
        b.logic(QStringLiteral("walk"), QStringLiteral("beta's logic"));
        trust(a.url());
        trust(b.url());
    }
};

/// Everything a hostile workspace is planted among: the project, a file outside it, and what a good workspace holds.
struct Hostile {
    Project project;
    Project elsewhere;

    Hostile()
    {
        project.scene(QStringLiteral("good"), QStringLiteral("A good scene"));
        project.logic(QStringLiteral("good"), QStringLiteral("good logic"));
        trust(project.url());
        writeFile(secret(), markingScene(QStringLiteral("the-secret-ran")));
        writeFile(elsewhere.file(QStringLiteral("secret-logic.qml")), Project::logicText(QStringLiteral("elsewhere's logic")).toUtf8());
    }

    QString secret() const { return elsewhere.file(QStringLiteral("secret.qml")); }

    QJsonObject good() const
    {
        return {{QStringLiteral("version"), 1},
                {QStringLiteral("project"), project.real()},
                {QStringLiteral("scene"), QStringLiteral("scenes/good.qml")},
                {QStringLiteral("logic"), QStringLiteral("logic/good.qml")},
                {QStringLiteral("panels"), QJsonObject{{QStringLiteral("stats"), true}}},
                {QStringLiteral("window"), QJsonObject{{QStringLiteral("width"), 900}, {QStringLiteral("height"), 600}}}};
    }

    /// The workspace kept for the project is exactly these bytes.
    void plant(const QByteArray &bytes) const { QVERIFY(writeFile(project.keptFile(), bytes)); }
    /// The good one, with these fields changed.
    void plant(const QJsonObject &changes) const
    {
        QJsonObject kept = good();
        for (auto it = changes.begin(); it != changes.end(); ++it)
            kept.insert(it.key(), it.value());
        plant(QJsonDocument(kept).toJson());
    }
};

using Plant = std::function<void(const Hostile &)>;

/// A stand-in for the provider, so the agent's design loop runs whole with no key and no bill. It answers once: done.
class StandInProvider
{
public:
    QTcpServer server;
    QByteArray target = "QQ Studio workspace test key";

    StandInProvider()
    {
        server.listen(QHostAddress::LocalHost);
        QObject::connect(&server, &QTcpServer::newConnection, &server, [this] {
            while (QTcpSocket *s = server.nextPendingConnection()) {
                QObject::connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
                auto *received = new QByteArray;
                QObject::connect(s, &QTcpSocket::destroyed, s, [received] { delete received; });
                QObject::connect(s, &QTcpSocket::readyRead, s, [s, received] {
                    received->append(s->readAll());
                    const int end = received->indexOf("\r\n\r\n");
                    if (end < 0)
                        return;
                    const QRegularExpression length(QStringLiteral("content-length: *(\\d+)"), QRegularExpression::CaseInsensitiveOption);
                    const int size = length.match(QString::fromLatin1(received->left(end))).captured(1).toInt();
                    if (received->size() < end + 4 + size)
                        return;
                    const QJsonObject said{
                        {QStringLiteral("id"), QStringLiteral("msg_test")},
                        {QStringLiteral("type"), QStringLiteral("message")},
                        {QStringLiteral("role"), QStringLiteral("assistant")},
                        {QStringLiteral("model"), QStringLiteral("claude-opus-5-5")},
                        {QStringLiteral("content"), QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
                                                                           {QStringLiteral("text"), QStringLiteral("Built.")}}}},
                        {QStringLiteral("stop_reason"), QStringLiteral("end_turn")},
                        {QStringLiteral("usage"), QJsonObject{{QStringLiteral("input_tokens"), 1}, {QStringLiteral("output_tokens"), 1}}}};
                    const QByteArray body = QJsonDocument(said).toJson(QJsonDocument::Compact);
                    s->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "
                             + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                    s->flush();
                    s->disconnectFromHost();
                });
            }
        });
        qputenv("QQ_KEYCHAIN_TARGET", target);
        qputenv("QQ_ANTHROPIC_URL", QStringLiteral("http://127.0.0.1:%1/v1/messages").arg(server.serverPort()).toUtf8());
    }
    ~StandInProvider()
    {
        qq::keychain::remove();
        qunsetenv("QQ_KEYCHAIN_TARGET");
        qunsetenv("QQ_ANTHROPIC_URL");
    }
};

}  // namespace

Q_DECLARE_METATYPE(Plant)

/// `s`: the window the cases share, in this project.
#define STUDIO_IN(project)            \
    QVERIFY(openSharedStudio());      \
    Studio &s = *m_shared;            \
    QVERIFY2(s.call("switchProject", project).toBool(), qPrintable(s.notice()))

class TstWorkspace : public QObject
{
    Q_OBJECT

    std::unique_ptr<Project> m_home;
    std::unique_ptr<Studio> m_shared;

    bool openSharedStudio()
    {
        if (m_shared)
            return true;
        m_home = std::make_unique<Project>();
        trust(m_home->url());
        m_shared = std::make_unique<Studio>();
        return m_shared->open(m_home->url());
    }

    /// What every hostile workspace ends in.
    void openedSafely(Studio &s, const Hostile &h, const QString &says, const QString &opensScene, const QString &opensLogic)
    {
        QCOMPARE(s.get("project").toUrl(), h.project.url());
        QVERIFY(s.window->isVisible());
        QVERIFY(s.get("projectTrusted").toBool());

        // Nothing of outside ran, or was read into the editor.
        QCOMPARE(QCoreApplication::organizationDomain(), QString());
        QVERIFY(!s.logicText().contains(QStringLiteral("elsewhere")));

        // What it could keep it kept, and only that.
        QCOMPARE(s.sceneName(), opensScene);
        QCOMPARE(s.logicName(), opensLogic);
        // A new scene is never saved, as ever; one opened is as it was.
        QCOMPARE(s.get("dirty").toBool(), opensScene == QStringLiteral("untitled"));
        // And it is a whole one a person can work in.
        QVERIFY(s.scene());
        QTRY_VERIFY(s.get("settled").toBool());

        // Told, where they need to be.
        if (says.isEmpty()) {
            QVERIFY2(!s.get("noticeIsProblem").toBool(), qPrintable(s.notice()));
        } else {
            QVERIFY2(s.notice().contains(says), qPrintable(s.notice()));
            QVERIFY2(s.get("noticeIsProblem").toBool(), qPrintable(s.notice()));
        }
        QCOMPARE(s.child("noticeLabel")->property("textFormat").toInt(), int(Qt::PlainText));
        // A panel opens only for a real yes.
        QVERIFY(!s.get("agentOpen").toBool());
    }

private slots:
    void initTestCase()
    {
        QQuickStyle::setStyle(QStringLiteral("Material"));
        // Everything kept goes to the test's own folders, never a person's.
        QStandardPaths::setTestModeEnabled(true);
        // A case closes the window on purpose.
        QGuiApplication::setQuitOnLastWindowClosed(false);
    }

    void init()
    {
        // QQ's own QML says nothing is wrong with it.
        QTest::failOnWarning(QRegularExpression(QStringLiteral("^qrc:/qt/qml/QQ/")));
        // Each case starts with nothing remembered.
        QDir(qq::ProjectWorkspace::storeFolder()).removeRecursively();
        QCoreApplication::setOrganizationDomain({});
    }

    // The shared window goes home with nothing left over, whatever the case did.
    void cleanup()
    {
        if (!m_shared)
            return;
        Studio &s = *m_shared;
        s.call("answerUnsaved_", QStringLiteral("cancel"));
        QVERIFY(QMetaObject::invokeMethod(s.child("recentPopup"), "close"));
        s.call("stop");
        QVERIFY(QMetaObject::invokeMethod(s.editor(), "reset"));
        s.call("newScene");
        QVERIFY2(s.call("switchProject", m_home->url()).toBool(), qPrintable(s.notice()));
        QTRY_VERIFY(!s.child("unsavedDialog")->property("visible").toBool());
        QTRY_VERIFY(!s.child("trustDialog")->property("visible").toBool());
        QTRY_VERIFY(!s.child("recentPopup")->property("visible").toBool());
    }

    void cleanupTestCase()
    {
        m_shared.reset();
        m_home.reset();
    }

    // ── each project comes back as it was left ──

    void switchingBackAndForthBringsEachProjectBackAsItWasLeft()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());

        // A as a person leaves it: a scene and a logic file open, the statistics on, the window a size of its own.
        QVERIFY(s.call("openScene", QStringLiteral("alpha")).toBool());
        QVERIFY(s.call("openLogic", QStringLiteral("spin")).toBool());
        s.window->setProperty("showStats", true);
        s.window->setProperty("width", 1200);
        s.window->setProperty("height", 800);
        QTRY_COMPARE(s.window->width(), 1200);
        QTRY_COMPARE(s.window->height(), 800);

        // B, for the first time: nothing of A comes along, and B is left in its own way.
        QVERIFY(s.call("switchProject", two.b.url()).toBool());
        QCOMPARE(s.get("project").toUrl(), two.b.url());
        QCOMPARE(s.sceneName(), QStringLiteral("untitled"));
        QVERIFY(!s.entity(QStringLiteral("Only in alpha")));
        QCOMPARE(s.logicName(), QString());
        QCOMPARE(s.get("showStats").toBool(), false);
        QVERIFY(s.call("openScene", QStringLiteral("beta")).toBool());
        QVERIFY(s.call("openLogic", QStringLiteral("walk")).toBool());
        s.window->setProperty("agentOpen", true);
        s.window->setProperty("width", 1000);
        s.window->setProperty("height", 700);
        QTRY_COMPARE(s.window->width(), 1000);
        QTRY_COMPARE(s.window->height(), 700);

        // Back to A: all of it.
        QVERIFY(s.call("switchProject", two.a.url()).toBool());
        QCOMPARE(s.get("project").toUrl(), two.a.url());
        QCOMPARE(s.sceneName(), QStringLiteral("alpha"));
        QVERIFY(s.entity(QStringLiteral("Only in alpha")));
        QVERIFY(!s.entity(QStringLiteral("Only in beta")));
        QCOMPARE(s.get("sceneFile").toUrl(), QUrl::fromLocalFile(two.a.file(QStringLiteral("scenes/alpha.qml"))));
        QCOMPARE(s.get("dirty").toBool(), false);
        QCOMPARE(s.logicName(), QStringLiteral("spin"));
        QCOMPARE(s.logicText(), Project::logicText(QStringLiteral("alpha's logic")));
        QCOMPARE(s.get("showStats").toBool(), true);
        QCOMPARE(s.child("statsButton")->property("checked").toBool(), true);
        QCOMPARE(s.get("agentOpen").toBool(), false);
        QTRY_COMPARE(s.window->width(), 1200);
        QTRY_COMPARE(s.window->height(), 800);
        QVERIFY(s.notice().contains(two.a.dir.path().split(QLatin1Char('/')).last()));

        // And on to B: all of that.
        QVERIFY(s.call("switchProject", two.b.url()).toBool());
        QCOMPARE(s.sceneName(), QStringLiteral("beta"));
        QVERIFY(s.entity(QStringLiteral("Only in beta")));
        QVERIFY(!s.entity(QStringLiteral("Only in alpha")));
        QCOMPARE(s.logicName(), QStringLiteral("walk"));
        QCOMPARE(s.logicText(), Project::logicText(QStringLiteral("beta's logic")));
        QCOMPARE(s.get("showStats").toBool(), false);
        QCOMPARE(s.get("agentOpen").toBool(), true);
        QTRY_COMPARE(s.window->width(), 1000);
        QTRY_COMPARE(s.window->height(), 700);
    }

    // What is kept is the person's, in the place and the form it should be, and nothing of it is in the project.
    void whatIsKeptIsOutsideTheProjectAndIsOnlyWhatItShouldBe()
    {
        TwoProjects two;
        const auto before = two.a.contents();
        STUDIO_IN(two.a.url());
        s.call("openScene", QStringLiteral("alpha"));
        s.call("openLogic", QStringLiteral("spin"));
        s.window->setProperty("showStats", true);
        QVERIFY(s.call("switchProject", two.b.url()).toBool());
        QCOMPARE(two.a.contents(), before);

        const QByteArray hash = QCryptographicHash::hash(two.a.real().toUtf8(), QCryptographicHash::Sha256).toHex();
        const QString file = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                             + QStringLiteral("/workspaces/") + QString::fromLatin1(hash) + QStringLiteral(".json");
        QCOMPARE(two.a.keptFile(), file);
        const QJsonObject kept = QJsonDocument::fromJson(readBytes(file)).object();
        QCOMPARE(kept.value(QStringLiteral("version")).toInt(), qq::ProjectWorkspace::schemaVersion);
        QCOMPARE(kept.value(QStringLiteral("project")).toString(), two.a.real());
        QCOMPARE(kept.value(QStringLiteral("scene")).toString(), QStringLiteral("scenes/alpha.qml"));
        QCOMPARE(kept.value(QStringLiteral("logic")).toString(), QStringLiteral("logic/spin.qml"));
        QCOMPARE(kept.value(QStringLiteral("panels")).toObject().value(QStringLiteral("stats")).toBool(), true);
        QVERIFY(kept.value(QStringLiteral("window")).toObject().value(QStringLiteral("width")).toInt() > 0);
        QCOMPARE(kept.keys().size(), 6);
        // A permission given to the agent is not something to come back to.
        QVERIFY(!readBytes(file).contains("ayRun"));
    }

    // ── unsaved work ──

    void cancelChangesNothingAtAll()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        s.call("openScene", QStringLiteral("alpha"));
        s.call("openLogic", QStringLiteral("spin"));
        s.window->setProperty("selection", QVariant::fromValue(s.entity(QStringLiteral("Only in alpha"))));
        s.edit();
        s.type(QStringLiteral("// typed, not saved\n"));
        const QVariantMap before = s.state();
        const auto inA = two.a.contents();
        const auto inB = two.b.contents();

        QVERIFY(!s.call("switchProject", two.b.url()).toBool());
        QObject *question = s.child("unsavedText");
        QTRY_VERIFY(s.asking());
        QVERIFY(question->property("text").toString().contains(QStringLiteral("logic/spin.qml")));
        QVERIFY(question->property("text").toString().contains(QStringLiteral("alpha")));
        QCOMPARE(question->property("textFormat").toInt(), int(Qt::PlainText));
        s.answer("unsavedCancel");

        QCOMPARE(s.state(), before);
        QCOMPARE(two.a.contents(), inA);
        QCOMPARE(two.b.contents(), inB);
        QVERIFY(!QFileInfo::exists(two.b.keptFile()));

        // Esc is Cancel too, and the same question can be asked again after it.
        QVERIFY(!s.call("switchProject", two.b.url()).toBool());
        QTRY_VERIFY(s.asking());
        QTest::keyClick(s.window, Qt::Key_Escape);
        QTRY_VERIFY(!s.child("unsavedDialog")->property("visible").toBool());
        QCOMPARE(s.get("askingFor").isNull() || !s.get("askingFor").isValid(), true);
        QCOMPARE(s.state(), before);
    }

    void saveKeepsTheWorkInItsOwnProjectThenGoes()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        s.call("openScene", QStringLiteral("alpha"));
        s.call("openLogic", QStringLiteral("spin"));
        s.edit();
        s.type(QStringLiteral("// kept in A\n"));
        const auto inB = two.b.contents();

        QVERIFY(!s.call("switchProject", two.b.url()).toBool());
        s.answer("unsavedSave");

        QCOMPARE(s.get("project").toUrl(), two.b.url());
        QVERIFY(QString::fromUtf8(readBytes(two.a.file(QStringLiteral("scenes/alpha.qml")))).contains(QStringLiteral(" edited")));
        QCOMPARE(QString::fromUtf8(readBytes(two.a.file(QStringLiteral("logic/spin.qml")))), QStringLiteral("// kept in A\n"));
        QCOMPARE(two.b.contents(), inB);

        // And it is what comes back.
        QVERIFY(s.call("switchProject", two.a.url()).toBool());
        QCOMPARE(s.get("dirty").toBool(), false);
        QCOMPARE(s.logicText(), QStringLiteral("// kept in A\n"));
    }

    void discardLeavesTheFilesAsTheyWereThenGoes()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        s.call("openScene", QStringLiteral("alpha"));
        s.call("openLogic", QStringLiteral("spin"));
        s.edit();
        s.type(QStringLiteral("// thrown away\n"));
        const auto inA = two.a.contents();

        QVERIFY(!s.call("switchProject", two.b.url()).toBool());
        s.answer("unsavedDiscard");

        QCOMPARE(s.get("project").toUrl(), two.b.url());
        QCOMPARE(two.a.contents(), inA);
        QVERIFY(s.call("switchProject", two.a.url()).toBool());
        QCOMPARE(s.get("dirty").toBool(), false);
        QCOMPARE(s.logicName(), QStringLiteral("spin"));
        QCOMPARE(s.logicText(), Project::logicText(QStringLiteral("alpha's logic")));
        QVERIFY(s.entity(QStringLiteral("Only in alpha")));
    }

    // Only the scene unsaved, or only the logic: it asks about what there is.
    void theQuestionIsAboutWhatIsUnsaved()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        s.call("openScene", QStringLiteral("alpha"));
        s.call("openLogic", QStringLiteral("spin"));
        s.type(QStringLiteral("// only the logic\n"));
        QVERIFY(!s.call("switchProject", two.b.url()).toBool());
        QTRY_VERIFY(s.asking());
        QVERIFY(!s.child("unsavedText")->property("text").toString().contains(QStringLiteral("scene")));
        s.answer("unsavedSave");
        QCOMPARE(s.get("project").toUrl(), two.b.url());
        QVERIFY(!QString::fromUtf8(readBytes(two.a.file(QStringLiteral("scenes/alpha.qml")))).contains(QStringLiteral(" edited")));
        QCOMPARE(QString::fromUtf8(readBytes(two.a.file(QStringLiteral("logic/spin.qml")))), QStringLiteral("// only the logic\n"));
    }

    // Work that cannot be saved stays, with the reason, in the project it belongs to.
    void aSaveThatFailsStaysWhereItIs()
    {
        TwoProjects two;
        // Where the new scene has to go is a file, so its folder cannot be made.
        Project cannotSave;
        trust(cannotSave.url());
        QVERIFY(writeFile(cannotSave.file(QStringLiteral("scenes")), "in the way"));
        STUDIO_IN(cannotSave.url());
        s.edit();

        QVERIFY(!s.call("switchProject", two.b.url()).toBool());
        s.answer("unsavedSave");
        QCOMPARE(s.get("project").toUrl(), cannotSave.url());
        QVERIFY(s.get("noticeIsProblem").toBool());
        QVERIFY(s.notice().contains(QStringLiteral("could not be made")));
        QVERIFY(s.io()->write(s.scene(), s.get("sceneFile").toUrl()).contains(QStringLiteral(" edited")));
        QVERIFY(s.call("sceneHasWork").toBool());
    }

    // A new scene nobody has touched is not work, though it has never been saved.
    void anUntouchedNewSceneIsNotAskedAbout()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        QVERIFY(s.get("dirty").toBool());  // never saved, as the window says
        QVERIFY(!s.call("sceneHasWork").toBool());
        QVERIFY(s.call("switchProject", two.b.url()).toBool());
        QVERIFY(!s.asking());
        QCOMPARE(s.get("project").toUrl(), two.b.url());
        // A new scene edited and edited back is the same.
        s.edit();
        QVERIFY(s.call("sceneHasWork").toBool());
    }

    // Saved, then edited back to the text it was new with, is unsaved all the same.
    void aSavedSceneEditedBackToNewIsStillWork()
    {
        Project p;
        trust(p.url());
        STUDIO_IN(p.url());
        QVERIFY(!s.call("sceneHasWork").toBool());
        s.edit();
        QVERIFY(s.call("saveScene").toBool());
        QVERIFY(!s.call("sceneHasWork").toBool());
        QVERIFY(s.call("undo").toBool());  // back to the very text it was new with
        QVERIFY(s.call("sceneHasWork").toBool());
    }

    // The window closing is a way of leaving too.
    void closingTheWindowAsksFirst()
    {
        TwoProjects two;
        Studio s;
        QVERIFY(s.open(two.a.url()));
        s.call("openScene", QStringLiteral("alpha"));
        s.edit();

        s.window->close();
        QTRY_VERIFY(s.asking());
        QVERIFY(s.window->isVisible());
        s.answer("unsavedCancel");
        QVERIFY(s.window->isVisible());
        QVERIFY(s.get("dirty").toBool());

        s.window->close();
        s.answer("unsavedSave");
        QTRY_VERIFY(!s.window->isVisible());
        QVERIFY(QString::fromUtf8(readBytes(two.a.file(QStringLiteral("scenes/alpha.qml")))).contains(QStringLiteral(" edited")));
    }

    void closingWithNothingUnsavedJustCloses()
    {
        TwoProjects two;
        Studio s;
        QVERIFY(s.open(two.a.url()));
        s.window->close();
        QTRY_VERIFY(!s.window->isVisible());
        QVERIFY(!s.asking());
    }

    void closingAndDiscardingCloses()
    {
        TwoProjects two;
        Studio s;
        QVERIFY(s.open(two.a.url()));
        s.call("openScene", QStringLiteral("alpha"));
        s.edit();
        const auto inA = two.a.contents();
        s.window->close();
        s.answer("unsavedDiscard");
        QTRY_VERIFY(!s.window->isVisible());
        QCOMPARE(two.a.contents(), inA);
    }

    // Within a project too: what a person does that would replace unsaved work asks first.
    void newSceneAndOpeningAnotherAskFirst()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        two.a.scene(QStringLiteral("other"), QStringLiteral("In another scene"));
        s.call("openScene", QStringLiteral("alpha"));
        s.edit();

        // New, Cancel: it stays.
        s.click("newButton");
        s.answer("unsavedCancel");
        QCOMPARE(s.sceneName(), QStringLiteral("alpha"));
        QVERIFY(s.get("dirty").toBool());

        // A scene from the project's files, Cancel, and then Discard.
        const QVariantMap scene{{QStringLiteral("kind"), QStringLiteral("scene")}, {QStringLiteral("name"), QStringLiteral("other")}};
        s.call("useFile", scene);
        s.answer("unsavedCancel");
        QCOMPARE(s.sceneName(), QStringLiteral("alpha"));
        s.call("useFile", scene);
        s.answer("unsavedDiscard");
        QCOMPARE(s.sceneName(), QStringLiteral("other"));
        QVERIFY(s.entity(QStringLiteral("In another scene")));

        // New with nothing to save asks nothing.
        s.click("newButton");
        QTRY_COMPARE(s.sceneName(), QStringLiteral("untitled"));
        QVERIFY(!s.asking());
    }

    void anotherLogicFileAndCloseAskFirst()
    {
        TwoProjects two;
        two.a.logic(QStringLiteral("other"), QStringLiteral("other logic"));
        STUDIO_IN(two.a.url());
        QVERIFY(s.call("openLogic", QStringLiteral("spin")).toBool());
        s.type(QStringLiteral("// typing\n"));

        // The one already open is not read over what is being typed.
        QVERIFY(s.call("openLogic", QStringLiteral("spin")).toBool());
        QCOMPARE(s.logicText(), QStringLiteral("// typing\n"));
        QVERIFY(!s.asking());

        // Another: Cancel keeps it; Save writes it and goes on.
        QVERIFY(!s.call("openLogic", QStringLiteral("other")).toBool());
        s.answer("unsavedCancel");
        QCOMPARE(s.logicName(), QStringLiteral("spin"));
        QCOMPARE(s.logicText(), QStringLiteral("// typing\n"));
        QVERIFY(!s.call("openLogic", QStringLiteral("other")).toBool());
        s.answer("unsavedSave");
        QCOMPARE(s.logicName(), QStringLiteral("other"));
        QCOMPARE(QString::fromUtf8(readBytes(two.a.file(QStringLiteral("logic/spin.qml")))), QStringLiteral("// typing\n"));

        // Close: asked, Discard leaves nothing; and its undo goes with it.
        s.typeAtStart(QStringLiteral("// typing again\n"));
        QVERIFY(s.child("logicText")->property("canUndo").toBool());
        s.settleFrames();
        s.click("logicClose");
        s.answer("unsavedDiscard");
        QCOMPARE(s.logicName(), QString());
        QCOMPARE(s.logicText(), QString());
        QVERIFY(!s.child("logicText")->property("canUndo").toBool());
        QCOMPARE(QString::fromUtf8(readBytes(two.a.file(QStringLiteral("logic/other.qml")))), Project::logicText(QStringLiteral("other logic")));
    }

    // ── restarting ──

    void aFreshStudioRestoresFromDisk()
    {
        TwoProjects two;
        {
            Studio s;
            QVERIFY(s.open(two.a.url()));
            s.call("openScene", QStringLiteral("alpha"));
            s.call("openLogic", QStringLiteral("spin"));
            s.window->setProperty("showStats", true);
            s.window->setProperty("width", 1100);
            s.window->setProperty("height", 750);
            QTRY_COMPARE(s.window->width(), 1100);
        }
        Studio again;
        QVERIFY(again.open(two.a.url()));
        QCOMPARE(again.sceneName(), QStringLiteral("alpha"));
        QVERIFY(again.entity(QStringLiteral("Only in alpha")));
        QCOMPARE(again.get("dirty").toBool(), false);
        QCOMPARE(again.logicName(), QStringLiteral("spin"));
        QCOMPARE(again.logicText(), Project::logicText(QStringLiteral("alpha's logic")));
        QCOMPARE(again.get("showStats").toBool(), true);
        QTRY_COMPARE(again.window->width(), 1100);
        QTRY_COMPARE(again.window->height(), 750);
        // Nothing to undo back past where it opened.
        QVERIFY(!again.history()->property("canUndo").toBool());
    }

    void aBareStartGoesBackToTheLastProject()
    {
        TwoProjects two;
        {
            Studio s;
            QVERIFY(s.open(two.a.url()));
            s.call("openScene", QStringLiteral("alpha"));
            QVERIFY(s.call("switchProject", two.b.url()).toBool());
            s.call("openScene", QStringLiteral("beta"));
        }
        Studio bare;
        QVERIFY(bare.open(QUrl(), {{QStringLiteral("reopenLast"), true}}));
        QCOMPARE(bare.get("project").toUrl(), two.b.url());
        QCOMPARE(bare.sceneName(), QStringLiteral("beta"));
    }

    void aProjectOnTheCommandLineAlwaysWins()
    {
        TwoProjects two;
        {
            Studio s;
            QVERIFY(s.open(two.b.url()));
            s.call("openScene", QStringLiteral("beta"));
        }
        Studio named;
        QVERIFY(named.open(two.a.url(), {{QStringLiteral("reopenLast"), false}}));
        QCOMPARE(named.get("project").toUrl(), two.a.url());
        QVERIFY(!named.entity(QStringLiteral("Only in beta")));
    }

    // Anything but a bare start leaves the last project alone: a model on the command line is what main.cpp means by it.
    void aStartThatIsNotBareOpensNoProject()
    {
        TwoProjects two;
        {
            Studio s;
            QVERIFY(s.open(two.b.url()));
        }
        Studio started;
        QVERIFY(started.open(QUrl()));
        QCOMPARE(started.get("project").toUrl(), QUrl());
        QCOMPARE(started.sceneName(), QStringLiteral("untitled"));
    }

    void aBareStartWhenTheLastProjectHasGoneSaysSo()
    {
        auto gone = std::make_unique<Project>();
        trust(gone->url());
        qq::ProjectWorkspace().noteRecent(gone->url());
        const QString name = QFileInfo(gone->path()).fileName();
        gone.reset();
        Studio bare;
        QVERIFY(bare.open(QUrl(), {{QStringLiteral("reopenLast"), true}}));
        QCOMPARE(bare.get("project").toUrl(), QUrl());
        QCOMPARE(bare.sceneName(), QStringLiteral("untitled"));
        QVERIFY(bare.get("noticeIsProblem").toBool());
        QVERIFY(bare.notice().contains(name));
    }

    // A look at something (--capture) leaves nothing behind.
    void aCaptureKeepsNothing()
    {
        TwoProjects two;
        {
            Studio s;
            QVERIFY(s.open(two.a.url(), {{QStringLiteral("remembers"), false}}));
            s.window->setProperty("showStats", true);
            QVERIFY(!s.child("workspaceSaveLater")->property("running").toBool());
        }
        QVERIFY(!QFileInfo::exists(two.a.keptFile()));
        QVERIFY(!QFileInfo::exists(qq::ProjectWorkspace::recentFile()));
    }

    // ── no mix-ups ──

    void undoDoesNotBringOneProjectIntoAnother()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        s.call("openScene", QStringLiteral("alpha"));
        s.edit();
        s.edit();
        QVERIFY(s.history()->property("canUndo").toBool());
        QVERIFY(!s.call("switchProject", two.b.url()).toBool());
        s.answer("unsavedDiscard");

        QVERIFY(!s.history()->property("canUndo").toBool());
        QVERIFY(!s.history()->property("canRedo").toBool());
        QVERIFY(!s.call("undo").toBool());
        QVERIFY(!s.call("redo").toBool());
        QVERIFY(!s.window->findChild<QQuickItem *>(QStringLiteral("undoButton"))->isEnabled());
        QVERIFY(!s.entity(QStringLiteral("Only in alpha")));
        QCOMPARE(s.sceneName(), QStringLiteral("untitled"));
        const QStringList texts = s.history()->property("texts").toStringList();
        QCOMPARE(texts.size(), 1);
        QVERIFY(!texts.first().contains(QStringLiteral("Only in alpha")));
    }

    void savingInOneProjectWritesNothingInAnother()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        s.call("openScene", QStringLiteral("alpha"));
        s.call("openLogic", QStringLiteral("spin"));
        const auto inA = two.a.contents();
        QVERIFY(s.call("switchProject", two.b.url()).toBool());

        // The new scene, then a scene of B's, then logic of B's, all saved.
        s.edit();
        QVERIFY(s.call("saveScene").toBool());
        QVERIFY(QFileInfo::exists(two.b.file(QStringLiteral("scenes/untitled.qml"))));
        s.call("openScene", QStringLiteral("beta"));
        s.edit();
        QVERIFY(s.call("saveScene").toBool());
        QVERIFY(s.call("openLogic", QStringLiteral("walk")).toBool());
        s.type(QStringLiteral("// B's\n"));
        QVariant written;
        QVERIFY(QMetaObject::invokeMethod(s.editor(), "save", Q_RETURN_ARG(QVariant, written)));
        QVERIFY(written.toBool());

        QCOMPARE(two.a.contents(), inA);
        QCOMPARE(QString::fromUtf8(readBytes(two.b.file(QStringLiteral("logic/walk.qml")))), QStringLiteral("// B's\n"));
    }

    void theLogicEditorCarriesNothingOver()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        QVERIFY(s.call("openLogic", QStringLiteral("spin")).toBool());
        s.typeAtStart(QStringLiteral("// A's secret\n"));
        QVERIFY(s.child("logicText")->property("canUndo").toBool());
        QVERIFY(!s.call("switchProject", two.b.url()).toBool());
        s.answer("unsavedDiscard");

        QCOMPARE(s.logicName(), QString());
        QCOMPARE(s.logicText(), QString());
        QVERIFY(!s.editor()->property("dirty").toBool());
        QCOMPARE(s.editor()->property("saved").toString(), QString());
        QCOMPARE(s.editor()->property("problems").toList().size(), 0);
        // Its own undo goes too: Ctrl+Z in the next file must not bring this one's text back.
        QVERIFY(!s.child("logicText")->property("canUndo").toBool());
        QVERIFY(!s.child("logicEditor")->property("visible").toBool());
        // And its save has nowhere to go.
        QVariant written;
        QVERIFY(QMetaObject::invokeMethod(s.editor(), "save", Q_RETURN_ARG(QVariant, written)));
        QVERIFY(!written.toBool());
        QVERIFY(!QFileInfo::exists(two.b.file(QStringLiteral("logic/spin.qml"))));
    }

    void anAssetInOneProjectDoesNotShowInAnother()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        s.call("openScene", QStringLiteral("alpha"));
        // Brought in from outside, and into A's assets by saving.
        s.call("add", QStringLiteral("Sound"), QVariantMap{{QStringLiteral("name"), QStringLiteral("Chime")},
                                                            {QStringLiteral("source"), QUrl::fromLocalFile(fixture(QStringLiteral("chime.wav")))}});
        QVERIFY(s.call("saveScene").toBool());
        QVERIFY(QFileInfo::exists(two.a.file(QStringLiteral("assets/chime.wav"))));
        const auto assetsOf = [&s] { return s.get("projectInfo").toMap().value(QStringLiteral("assets")).toList(); };
        QCOMPARE(assetsOf().size(), 1);
        QObject *panel = s.child("projectPanel");
        const auto listed = [panel] {
            QStringList names;
            for (const QVariant &f : panel->property("files").toList())
                names << f.toMap().value(QStringLiteral("name")).toString();
            return names;
        };
        QVERIFY(listed().contains(QStringLiteral("chime.wav")));
        s.child("assetFilter")->setProperty("text", QStringLiteral("chime"));

        QVERIFY(s.call("switchProject", two.b.url()).toBool());
        QCOMPARE(assetsOf().size(), 0);
        QVERIFY(!listed().contains(QStringLiteral("chime.wav")));
        QVERIFY(!listed().contains(QStringLiteral("alpha")));
        QVERIFY(listed().contains(QStringLiteral("beta")));
        // What was typed to find it there is not left over here.
        QCOMPARE(s.child("assetFilter")->property("text").toString(), QString());
        QVERIFY(!QFileInfo::exists(two.b.file(QStringLiteral("assets"))));

        // Saved in B, it brings in only what B's scene uses.
        s.edit();
        QVERIFY(s.call("saveScene").toBool());
        QVERIFY(!QFileInfo::exists(two.b.file(QStringLiteral("assets/chime.wav"))));
    }

    // What A's folder holds is out of reach from B: the guard's project moved too.
    void theFilesTheStudioMayReadMoveWithIt()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        const QUrl inA = QUrl::fromLocalFile(two.a.file(QStringLiteral("logic/spin.qml")));
        QVERIFY(s.io()->readText(inA).isValid());
        QVERIFY(s.call("switchProject", two.b.url()).toBool());
        QVERIFY(!s.io()->readText(inA).isValid());
        QVERIFY(s.io()->readText(QUrl::fromLocalFile(two.b.file(QStringLiteral("logic/walk.qml")))).isValid());
    }

    void whatWasSelectedAndHowItWasSeenDoNotCarryOver()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        s.call("openScene", QStringLiteral("alpha"));
        s.window->setProperty("selection", QVariant::fromValue(s.entity(QStringLiteral("Only in alpha"))));
        s.window->setProperty("distance", 33.0);
        s.window->setProperty("yaw", 140.0);
        QVERIFY(s.call("switchProject", two.b.url()).toBool());
        QVERIFY(!s.get("selection").value<QObject *>());
        QVERIFY(!s.child("inspector")->property("target").value<QObject *>());
        QCOMPARE(s.get("distance").toDouble(), 8.0);
        QCOMPARE(s.get("yaw").toDouble(), 25.0);
    }

    // The agent's run belongs to the project it ran in: its undo would put A's scene into B, and what it showed, and what
    // was typed for it, and what it was allowed, stay behind.
    void theAgentsRunStaysWithItsProject()
    {
        TwoProjects two;
        qq::McpServer server;
        Studio s;
        QVERIFY(s.open(two.a.url()));
        qq::StudioTools tools(s.window, &s.engine, &server);
        StandInProvider provider;
        auto *designer = new qq::DesignLoop(&server, s.window, s.io(), s.window);
        s.window->setProperty("designer", QVariant::fromValue<QObject *>(designer));
        QCOMPARE(designer->setKey(QStringLiteral("sk-ant-test-workspace")), QString());
        s.call("openScene", QStringLiteral("alpha"));

        QSignalSpy finished(designer, &qq::DesignLoop::finished);
        designer->start(QStringLiteral("something for A"));
        QVERIFY(finished.wait(30000));
        QCOMPARE(finished.first().first().toString(), QStringLiteral("done"));
        QVERIFY(designer->canUndo());
        QVERIFY(!designer->steps().isEmpty());
        s.child("agentBrief")->setProperty("text", QStringLiteral("A game for A"));
        s.window->setProperty("agentMayRun", true);

        // Cancel: all of it stays.
        s.edit();
        QVERIFY(!s.call("switchProject", two.b.url()).toBool());
        s.answer("unsavedCancel");
        QVERIFY(designer->canUndo());
        QVERIFY(s.get("agentMayRun").toBool());
        QCOMPARE(s.child("agentBrief")->property("text").toString(), QStringLiteral("A game for A"));

        // Gone: none of it does.
        QVERIFY(!s.call("switchProject", two.b.url()).toBool());
        s.answer("unsavedDiscard");
        QCOMPARE(s.get("project").toUrl(), two.b.url());
        QVERIFY(!designer->canUndo());
        QVERIFY(designer->steps().isEmpty());
        QCOMPARE(designer->status(), QString());
        QVERIFY(!s.get("agentMayRun").toBool());
        // The old panel is gone once the event loop has turned, and the one in its place has nothing in it.
        QTRY_COMPARE(s.child("agentBrief")->property("text").toString(), QString());
        const QString inB = s.io()->write(s.scene(), s.get("sceneFile").toUrl());
        QVERIFY(!designer->undo());
        QCOMPARE(s.io()->write(s.scene(), s.get("sceneFile").toUrl()), inB);

        s.window->setProperty("designer", QVariant::fromValue<QObject *>(nullptr));
    }

    // ── play ──

    void playRunningAtASwitchIsStopped()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        s.call("openScene", QStringLiteral("alpha"));
        QVERIFY(s.call("play").toBool());
        QVERIFY(s.get("playing").toBool());

        QVERIFY(s.call("switchProject", two.b.url()).toBool());
        QVERIFY(!s.get("playing").toBool());
        QVERIFY(!s.get("playScene").value<QObject *>());
        QVERIFY(!s.child("world")->property("hearing").toBool());
        QCOMPARE(s.get("project").toUrl(), two.b.url());
        QCOMPARE(s.sceneName(), QStringLiteral("untitled"));
        QVERIFY(!s.entity(QStringLiteral("Only in alpha")));
        // And B plays as itself.
        QVERIFY(s.call("play").toBool());
        QVERIFY(s.call("stop").toBool());
    }

    // Cancel changes nothing at all, so play goes on while the question is up, and stops only for an answer that leaves.
    void playGoesOnThroughCancel()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        s.call("openScene", QStringLiteral("alpha"));
        s.edit();
        QVERIFY(s.call("play").toBool());
        const QVariantMap before = s.state();

        QVERIFY(!s.call("switchProject", two.b.url()).toBool());
        s.answer("unsavedCancel");
        QVERIFY(s.get("playing").toBool());
        QCOMPARE(s.state(), before);

        // Save: stopped first, since nothing is saved from play; saved in A; then in B.
        QVERIFY(!s.call("switchProject", two.b.url()).toBool());
        s.answer("unsavedSave");
        QVERIFY(!s.get("playing").toBool());
        QCOMPARE(s.get("project").toUrl(), two.b.url());
        QVERIFY(QString::fromUtf8(readBytes(two.a.file(QStringLiteral("scenes/alpha.qml")))).contains(QStringLiteral(" edited")));
    }

    // ── the trust check ──

    // A remembered scene goes past the same question as any other: a project from elsewhere runs nothing until a person
    // says so, and no saved state, however it was made or forged, says so for them.
    void aSwitchAsksTheTrustCheckBeforeAnythingRuns()
    {
        TwoProjects two;
        Project stranger;
        QVERIFY(writeFile(stranger.file(QStringLiteral("scenes/old.qml")), markingScene(QStringLiteral("the-scene-ran"))));
        stranger.logic(QStringLiteral("old"), QStringLiteral("a stranger's logic"));
        // It says it is trusted, for what that is worth.
        const QJsonObject forged{{QStringLiteral("version"), 1},
                                 {QStringLiteral("project"), stranger.real()},
                                 {QStringLiteral("trusted"), true},
                                 {QStringLiteral("scene"), QStringLiteral("scenes/old.qml")},
                                 {QStringLiteral("logic"), QStringLiteral("logic/old.qml")}};
        QVERIFY(writeFile(stranger.keptFile(), QJsonDocument(forged).toJson()));
        const QByteArray kept = readBytes(stranger.keptFile());

        STUDIO_IN(two.a.url());
        QVERIFY(s.call("switchProject", stranger.url()).toBool());
        QVERIFY(!s.get("projectTrusted").toBool());
        s.waitForTrustQuestion();
        QCOMPARE(QCoreApplication::organizationDomain(), QString());
        QCOMPARE(s.sceneName(), QStringLiteral("untitled"));
        QVERIFY(!s.get("sceneFile").toUrl().isLocalFile());
        QCOMPARE(s.logicName(), QString());

        // Kept closed: nothing runs, and leaving does not overwrite what it remembered.
        s.answerTrust("keepClosed");
        QCOMPARE(QCoreApplication::organizationDomain(), QString());
        QCOMPARE(s.sceneName(), QStringLiteral("untitled"));
        QVERIFY(s.call("switchProject", two.a.url()).toBool());
        QCOMPARE(readBytes(stranger.keptFile()), kept);

        // A question left open is no answer for the project after it.
        QVERIFY(s.call("switchProject", stranger.url()).toBool());
        s.waitForTrustQuestion();
        QVERIFY(s.call("switchProject", two.b.url()).toBool());
        QTRY_VERIFY(!s.child("trustDialog")->property("visible").toBool());
        QVERIFY(!s.get("whenTrusted").isValid() || s.get("whenTrusted").isNull());
        QVERIFY(s.get("projectTrusted").toBool());
        QCOMPARE(QCoreApplication::organizationDomain(), QString());

        // Trusted: then it comes back.
        QVERIFY(s.call("switchProject", stranger.url()).toBool());
        s.answerTrust("trustProject");
        QTRY_COMPARE(s.sceneName(), QStringLiteral("the-scene-ran"));
        QCOMPARE(QCoreApplication::organizationDomain(), QStringLiteral("the-scene-ran"));
        QCOMPARE(s.logicName(), QStringLiteral("old"));
        QCoreApplication::setOrganizationDomain({});
    }

    void aStartAsksTheTrustCheckToo()
    {
        Project stranger;
        QVERIFY(writeFile(stranger.file(QStringLiteral("scenes/old.qml")), markingScene(QStringLiteral("the-scene-ran"))));
        const QJsonObject kept{{QStringLiteral("version"), 1}, {QStringLiteral("project"), stranger.real()},
                               {QStringLiteral("scene"), QStringLiteral("scenes/old.qml")}};
        QVERIFY(writeFile(stranger.keptFile(), QJsonDocument(kept).toJson()));
        {
            Studio named;
            QVERIFY(named.open(stranger.url()));
            named.waitForTrustQuestion();
            QCOMPARE(QCoreApplication::organizationDomain(), QString());
            QCOMPARE(named.sceneName(), QStringLiteral("untitled"));
        }
        // A bare start, from the recent list.
        qq::ProjectWorkspace().noteRecent(stranger.url());
        Studio bare;
        QVERIFY(bare.open(QUrl(), {{QStringLiteral("reopenLast"), true}}));
        QCOMPARE(bare.get("project").toUrl(), QUrl::fromLocalFile(stranger.real()));
        bare.waitForTrustQuestion();
        QCOMPARE(QCoreApplication::organizationDomain(), QString());
        QCOMPARE(bare.sceneName(), QStringLiteral("untitled"));
    }

    // Saved there, a scene is built among a stranger's files from then on, so saving asks first as opening does.
    void savingInAProjectNotTrustedAsksFirst()
    {
        Project stranger;
        stranger.scene(QStringLiteral("old"), QStringLiteral("A stranger's"));
        STUDIO_IN(stranger.url());
        QVERIFY(!s.get("projectTrusted").toBool());
        s.edit();
        const auto before = stranger.contents();
        QVERIFY(!s.call("saveScene").toBool());
        s.waitForTrustQuestion();
        QCOMPARE(stranger.contents(), before);
        s.answerTrust("trustProject");
        QTRY_VERIFY(QFileInfo::exists(stranger.file(QStringLiteral("scenes/untitled.qml"))));
    }

    // An imported scene has no place among a stranger's files either, for the same reason as a new one.
    void importingInAProjectNotTrustedStaysOutOfIt()
    {
        Project stranger;
        stranger.scene(QStringLiteral("old"), QStringLiteral("A stranger's"));
        QVERIFY(writeFile(stranger.file(QStringLiteral("scenes/Sun.qml")), "import QQ\nShape { name: \"not the sun\" }\n"));
        STUDIO_IN(stranger.url());
        s.call("importScene", QUrl::fromLocalFile(fixture(QStringLiteral("first-light.qq.json"))));
        QVERIFY(!s.get("sceneFile").toUrl().isLocalFile());
        QVERIFY(!s.entity(QStringLiteral("not the sun")));
    }

    // ── every way of choosing a project ──

    void theChooserGoesTheSameWay()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        s.call("openScene", QStringLiteral("alpha"));
        s.edit();
        // Asked, as anywhere else.
        QVERIFY(!s.call("folderChosen", two.b.url()).toBool());
        s.answer("unsavedCancel");
        QCOMPARE(s.get("project").toUrl(), two.a.url());
        QVERIFY(!s.call("folderChosen", two.b.url()).toBool());
        s.answer("unsavedDiscard");
        QCOMPARE(s.get("project").toUrl(), two.b.url());
    }

    // The name it used to have goes the same way too.
    void theOldNameGoesTheSameWay()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        s.call("openScene", QStringLiteral("alpha"));
        s.edit();
        QVERIFY(!s.call("chooseProject", two.b.url()).toBool());
        s.answer("unsavedCancel");
        QCOMPARE(s.get("project").toUrl(), two.a.url());
    }

    void theSameProjectAgainIsLookedAtAgainAndNothingIsLost()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        s.call("openScene", QStringLiteral("alpha"));
        s.edit();
        two.a.scene(QStringLiteral("newly-there"), QStringLiteral("x"));
        QObject *before = s.scene();
        QVERIFY(s.call("switchProject", two.a.url()).toBool());
        QVERIFY(!s.asking());
        QCOMPARE(s.scene(), before);
        QVERIFY(s.get("dirty").toBool());
        QVERIFY(s.get("projectInfo").toMap().value(QStringLiteral("scenes")).toStringList().contains(QStringLiteral("newly-there")));
    }

    void aFolderThatIsNotThereIsRefused()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        s.call("openScene", QStringLiteral("alpha"));
        s.edit();
        const QVariantMap before = s.state();
        QVERIFY(!s.call("switchProject", QUrl::fromLocalFile(two.a.file(QStringLiteral("no-such-folder")))).toBool());
        QVERIFY(!s.asking());
        QVERIFY(s.get("noticeIsProblem").toBool());
        QCOMPARE(s.get("project").toUrl(), before.value(QStringLiteral("project")).toUrl());
        // A web address is no folder, nor is a file.
        QVERIFY(!s.call("switchProject", QUrl(QStringLiteral("https://example.com/project"))).toBool());
        QVERIFY(!s.call("switchProject", QUrl::fromLocalFile(two.a.file(QStringLiteral("scenes/alpha.qml")))).toBool());
    }

    // A scene in no project comes from none: choosing a project asks, and Save means saving it there.
    void aSceneInNoProjectGoesWithThePerson()
    {
        Project empty;
        Studio s;
        QVERIFY(s.open(QUrl()));
        QCOMPARE(s.get("project").toUrl(), QUrl());
        s.edit();
        QVERIFY(!s.call("switchProject", empty.url()).toBool());
        s.answer("unsavedCancel");
        QCOMPARE(s.get("project").toUrl(), QUrl());

        QVERIFY(!s.call("switchProject", empty.url()).toBool());
        s.answer("unsavedSave");
        QCOMPARE(s.get("project").toUrl(), empty.url());
        QVERIFY(!s.call("sceneHasWork").toBool());
        QVERIFY(QString::fromUtf8(readBytes(empty.file(QStringLiteral("scenes/untitled.qml")))).contains(QStringLiteral(" edited")));
    }

    void aSceneInNoProjectCanBeDiscarded()
    {
        TwoProjects two;
        Studio s;
        QVERIFY(s.open(QUrl()));
        s.edit();
        QVERIFY(!s.call("switchProject", two.a.url()).toBool());
        s.answer("unsavedDiscard");
        QCOMPARE(s.get("project").toUrl(), two.a.url());
        QVERIFY(!s.call("sceneHasWork").toBool());
    }

    // Save before any project is chosen: the chooser's answer is where it goes.
    void savingWithNoProjectChoosesOneAndSavesThere()
    {
        Project empty;
        Studio s;
        QVERIFY(s.open(QUrl()));
        s.edit();
        QVERIFY(!s.call("saveScene").toBool());
        QObject *chooser = s.child("projectChooser");
        QVERIFY(chooser->property("thenSave").toBool());
        QVERIFY(s.call("folderChosen", empty.url()).toBool());
        QCOMPARE(s.get("project").toUrl(), empty.url());
        QVERIFY(!chooser->property("thenSave").toBool());
        QVERIFY(QFileInfo::exists(empty.file(QStringLiteral("scenes/untitled.qml"))));
    }

    // Saved into a stranger's project, it waits for the same question, and is not lost by a no.
    void savingIntoAProjectNotTrustedAsksAndLosesNothing()
    {
        Project stranger;
        stranger.scene(QStringLiteral("old"), QStringLiteral("A stranger's"));
        Studio s;
        QVERIFY(s.open(QUrl()));
        s.edit();
        const QString text = s.io()->write(s.scene(), {});
        const auto before = stranger.contents();
        QVERIFY(!s.call("saveInto", stranger.url()).toBool());
        s.answerTrust("keepClosed");
        QCOMPARE(stranger.contents(), before);
        QCOMPARE(s.io()->write(s.scene(), {}), text);
        // The one way to ask again is to save again; and yes saves it.
        QVERIFY(!s.call("saveScene").toBool());
        s.answerTrust("trustProject");
        QTRY_VERIFY(QFileInfo::exists(stranger.file(QStringLiteral("scenes/untitled.qml"))));
    }

    // ── the recent list ──

    void theRecentListIsLatestFirstAndShort()
    {
        qq::ProjectWorkspace workspace;
        std::vector<std::unique_ptr<Project>> many;
        for (int i = 0; i < qq::ProjectWorkspace::mostRecent + 3; ++i) {
            many.push_back(std::make_unique<Project>());
            workspace.noteRecent(many.back()->url());
        }
        QVariantList rows = workspace.recentProjects();
        QCOMPARE(rows.size(), qq::ProjectWorkspace::mostRecent);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("path")).toString(), many.back()->real());

        // Noted again: first, and still only once.
        workspace.noteRecent(many[many.size() - 4]->url());
        rows = workspace.recentProjects();
        QCOMPARE(rows.size(), qq::ProjectWorkspace::mostRecent);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("path")).toString(), many[many.size() - 4]->real());
        QCOMPARE(rows.first().toMap().value(QStringLiteral("name")).toString(), QFileInfo(many[many.size() - 4]->real()).fileName());
        QCOMPARE(rows.first().toMap().value(QStringLiteral("missing")).toBool(), false);
        int times = 0;
        for (const QVariant &row : rows)
            times += row.toMap().value(QStringLiteral("path")).toString() == many[many.size() - 4]->real() ? 1 : 0;
        QCOMPARE(times, 1);
    }

    void recentProjectsAreOpenedFromTheListAndAMissingOneIsNot()
    {
        TwoProjects two;
        auto gone = std::make_unique<Project>();
        const QString goneName = QFileInfo(gone->path()).fileName();
        {
            qq::ProjectWorkspace workspace;
            workspace.noteRecent(gone->url());
            gone.reset();
            workspace.noteRecent(two.b.url());
        }
        STUDIO_IN(two.a.url());
        // Latest first: A, B and the missing one.
        s.click("recentButton");
        QTRY_VERIFY(s.child("recentPopup")->property("opened").toBool());
        auto *list = s.window->findChild<QQuickItem *>(QStringLiteral("recentList"));
        QVERIFY(list);
        QTRY_COMPARE(list->property("count").toInt(), 3);
        const auto row = [list](int i) {
            QQuickItem *item = nullptr;
            QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, item), Q_ARG(int, i));
            return item;
        };
        QTRY_VERIFY(row(2));
        QCOMPARE(row(2)->property("modelData").toMap().value(QStringLiteral("name")).toString(), goneName);
        QVERIFY(!row(2)->isEnabled());
        QCOMPARE(row(2)->findChild<QQuickItem *>(QStringLiteral("recentMissing"))->isVisible(), true);
        // A press on it does nothing.
        s.clickOn(row(2));
        s.settleFrames();
        QCOMPARE(s.get("project").toUrl(), two.a.url());

        // Project names are text.
        QCOMPARE(row(1)->findChild<QObject *>(QStringLiteral("recentName"))->property("textFormat").toInt(), int(Qt::PlainText));
        QCOMPARE(s.child("projectName")->property("textFormat").toInt(), int(Qt::PlainText));
        QCOMPARE(s.child("noticeLabel")->property("textFormat").toInt(), int(Qt::PlainText));

        // Another opens, the same way as any change.
        QTRY_VERIFY(s.child("recentPopup")->property("opened").toBool());
        s.clickOn(row(1));
        QTRY_COMPARE(s.get("project").toUrl(), QUrl::fromLocalFile(two.b.real()));
        QTRY_VERIFY(!s.child("recentPopup")->property("visible").toBool());
    }

    // A folder gone between the list being read and the click is refused like any other.
    void aFolderGoneSinceTheListWasReadIsRefused()
    {
        TwoProjects two;
        auto gone = std::make_unique<Project>();
        trust(gone->url());
        const QUrl url = QUrl::fromLocalFile(gone->real());
        STUDIO_IN(two.a.url());
        gone.reset();
        QVERIFY(!s.call("switchProject", url).toBool());
        QCOMPARE(s.get("project").toUrl(), two.a.url());
        QVERIFY(s.get("noticeIsProblem").toBool());
    }

    void aHostileRecentListIsOnlyAnEmptyOne_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("not JSON") << QByteArray("{ \"version\": 1, \"proj");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("a list") << QByteArray("[1, 2, 3]");
        QTest::newRow("unknown version") << QByteArray("{\"version\": 77, \"projects\": [\"/tmp\"]}");
        QTest::newRow("no list") << QByteArray("{\"version\": 1, \"projects\": 5}");
        QTest::newRow("too large") << QByteArray("{\"version\": 1, \"projects\": [\"/tmp\"], \"pad\": \"")
                                          + QByteArray(qq::ProjectWorkspace::largestFile, 'x') + QByteArray("\"}");
        QTest::newRow("not paths") << QByteArray("{\"version\": 1, \"projects\": [5, null, {}, \"\", \"relative/path\", \"../up\", \"file:///tmp\"]}");
        QTest::newRow("control characters") << QByteArray("{\"version\": 1, \"projects\": [\"/tmp/a\\nb\"]}");
    }

    void aHostileRecentListIsOnlyAnEmptyOne()
    {
        QFETCH(QByteArray, bytes);
        QVERIFY(writeFile(qq::ProjectWorkspace::recentFile(), bytes));
        QCOMPARE(qq::ProjectWorkspace().recentProjects().size(), 0);
        Studio s;
        QVERIFY(s.open(QUrl(), {{QStringLiteral("reopenLast"), true}}));
        QCOMPARE(s.get("project").toUrl(), QUrl());
        QCOMPARE(s.sceneName(), QStringLiteral("untitled"));
        // It is replaced the next time a project is noted, whole.
        Project p;
        qq::ProjectWorkspace().noteRecent(p.url());
        QCOMPARE(qq::ProjectWorkspace().recentProjects().size(), 1);
    }

    void aRecentListWithSomeGoodEntriesKeepsThem()
    {
        Project p;
        const QJsonObject list{{QStringLiteral("version"), 1},
                               {QStringLiteral("projects"), QJsonArray{5, QStringLiteral("relative"), p.real(), p.real(), p.file(QStringLiteral("no-such-folder"))}}};
        QVERIFY(writeFile(qq::ProjectWorkspace::recentFile(), QJsonDocument(list).toJson()));
        const QVariantList rows = qq::ProjectWorkspace().recentProjects();
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows.at(0).toMap().value(QStringLiteral("missing")).toBool(), false);
        QCOMPARE(rows.at(1).toMap().value(QStringLiteral("missing")).toBool(), true);
    }

#ifndef Q_OS_WIN
    // A folder's name can be anything, and none of it is markup: drawn as text, it fetches no picture.
    void aProjectNamedLikeMarkupIsShownAsText()
    {
        QTemporaryDir parent;
        const QString name = QStringLiteral("<img src=\"x.png\" width=\"900\">");
        QVERIFY(QDir(parent.path()).mkdir(name));
        const QUrl url = QUrl::fromLocalFile(QDir(parent.path()).filePath(name));
        trust(url);
        STUDIO_IN(url);
        QCOMPARE(s.child("projectName")->property("text").toString(), name);
        QCOMPARE(s.child("projectName")->property("textFormat").toInt(), int(Qt::PlainText));
        // And it is recorded as it is, and found again.
        QCOMPARE(qq::ProjectWorkspace().recentProjects().first().toMap().value(QStringLiteral("name")).toString(), name);
    }
#endif

    // ── saved only when something changed ──

    void nothingIsWrittenWhileNothingChanges()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        QObject *later = s.child("workspaceSaveLater");
        QVERIFY(later);
        // Whatever settling the window on the display asked for has gone by.
        QTRY_VERIFY_WITH_TIMEOUT(!later->property("running").toBool(), 5000);
        QFile::remove(two.a.keptFile());

        QTest::qWait(1500);
        QVERIFY(!later->property("running").toBool());
        QVERIFY(!QFileInfo::exists(two.a.keptFile()));

        // A real change is kept, a moment later, without the switch or the quit.
        s.window->setProperty("showStats", true);
        QVERIFY(later->property("running").toBool());
        QVERIFY(!QFileInfo::exists(two.a.keptFile()));
        QTRY_VERIFY(QFileInfo::exists(two.a.keptFile()));
        QVERIFY(!later->property("running").toBool());
        QCOMPARE(QJsonDocument::fromJson(readBytes(two.a.keptFile())).object().value(QStringLiteral("panels")).toObject().value(QStringLiteral("stats")).toBool(), true);
    }

    // Each change puts the write off, and what is written in the end is the last of them.
    void quickChangesEndInTheLastOfThem()
    {
        TwoProjects two;
        STUDIO_IN(two.a.url());
        QObject *later = s.child("workspaceSaveLater");
        QTRY_VERIFY_WITH_TIMEOUT(!later->property("running").toBool(), 5000);
        QFile::remove(two.a.keptFile());
        for (int i = 0; i < 5; ++i) {
            s.window->setProperty("showStats", i % 2 == 0);
            QVERIFY(later->property("running").toBool());
            QTest::qWait(100);
        }
        QTRY_VERIFY(QFileInfo::exists(two.a.keptFile()));
        QCOMPARE(QJsonDocument::fromJson(readBytes(two.a.keptFile())).object().value(QStringLiteral("panels")).toObject().value(QStringLiteral("stats")).toBool(), true);
    }

    // ── from the hostile side ──

    void aHostileWorkspaceOpensSafely_data()
    {
        QTest::addColumn<Plant>("plant");
        QTest::addColumn<QString>("says");        // what the notice has to say, or "" for nothing needed
        QTest::addColumn<QString>("opensScene");  // the scene that ends up open
        QTest::addColumn<QString>("opensLogic");  // the logic file that does

        const QString refused = QStringLiteral("not a plain file inside this project");
        const QString untitled = QStringLiteral("untitled");
        const QString good = QStringLiteral("good");

        // The whole file.
        QTest::newRow("not JSON") << Plant([](const Hostile &h) { h.plant(QByteArray("{ \"version\": 1, \"proj")); })
                                  << QStringLiteral("could not be read") << untitled << QString();
        QTest::newRow("empty") << Plant([](const Hostile &h) { h.plant(QByteArray()); })
                               << QStringLiteral("could not be read") << untitled << QString();
        QTest::newRow("a list") << Plant([](const Hostile &h) { h.plant(QByteArray("[1, 2, 3]")); })
                                << QStringLiteral("could not be read") << untitled << QString();
        QTest::newRow("binary") << Plant([](const Hostile &h) { h.plant(QByteArray("\x00\xff\xfe\x01 not text", 12)); })
                                << QStringLiteral("could not be read") << untitled << QString();
        QTest::newRow("nested beyond reason") << Plant([](const Hostile &h) {
            h.plant(QByteArray(20000, '[') + QByteArray(20000, ']'));
        }) << QStringLiteral("could not be read") << untitled << QString();
        QTest::newRow("oversized") << Plant([](const Hostile &h) {
            h.plant(QJsonObject{{QStringLiteral("padding"), QString(qq::ProjectWorkspace::largestFile, QLatin1Char('x'))}});
        }) << QStringLiteral("too large") << untitled << QString();
        QTest::newRow("oversized by far") << Plant([](const Hostile &h) {
            QFile f(h.project.keptFile());
            QDir().mkpath(QFileInfo(f).absolutePath());
            QVERIFY(f.open(QIODevice::WriteOnly));
            QVERIFY(f.resize(qint64(1) << 28));  // 256 MB: nothing of it may be read
        }) << QStringLiteral("too large") << untitled << QString();
        QTest::newRow("unknown version") << Plant([](const Hostile &h) { h.plant(QJsonObject{{QStringLiteral("version"), 99}}); })
                                         << QStringLiteral("version") << untitled << QString();
        QTest::newRow("version as text") << Plant([](const Hostile &h) { h.plant(QJsonObject{{QStringLiteral("version"), QStringLiteral("1")}}); })
                                         << QStringLiteral("version") << untitled << QString();
        QTest::newRow("no version") << Plant([](const Hostile &h) {
            QJsonObject kept = h.good();
            kept.remove(QStringLiteral("version"));
            h.plant(QJsonDocument(kept).toJson());
        }) << QStringLiteral("version") << untitled << QString();
        QTest::newRow("written for another folder") << Plant([](const Hostile &h) {
            h.plant(QJsonObject{{QStringLiteral("project"), h.elsewhere.real()}});
        }) << QStringLiteral("another folder") << untitled << QString();
        QTest::newRow("no folder at all") << Plant([](const Hostile &h) {
            QJsonObject kept = h.good();
            kept.remove(QStringLiteral("project"));
            h.plant(QJsonDocument(kept).toJson());
        }) << QStringLiteral("another folder") << untitled << QString();
        QTest::newRow("a folder in the store's place") << Plant([](const Hostile &h) {
            QVERIFY(QDir().mkpath(h.project.keptFile()));
        }) << QStringLiteral("not a plain file") << untitled << QString();

        // Its paths: each is dropped alone, and the other still comes back.
        const auto climbing = [](const QString &key, const QString &path) {
            return Plant([key, path](const Hostile &h) { h.plant(QJsonObject{{key, path}}); });
        };
        QTest::newRow("a scene climbing out") << climbing(QStringLiteral("scene"), QStringLiteral("scenes/../../secret.qml"))
                                              << refused << untitled << good;
        QTest::newRow("a scene just out") << climbing(QStringLiteral("scene"), QStringLiteral("../secret.qml"))
                                          << refused << untitled << good;
        QTest::newRow("a scene through . and ..") << climbing(QStringLiteral("scene"), QStringLiteral("./scenes/./../good.qml"))
                                                  << refused << untitled << good;
        QTest::newRow("a scene as a URL") << climbing(QStringLiteral("scene"), QStringLiteral("file:///etc/passwd"))
                                          << refused << untitled << good;
        QTest::newRow("a scene percent-encoded") << climbing(QStringLiteral("scene"), QStringLiteral("scenes/%2e%2e%2f%2e%2e%2fsecret.qml"))
                                                 << refused << untitled << good;
        QTest::newRow("a scene by the other slash") << climbing(QStringLiteral("scene"), QStringLiteral("scenes\\..\\..\\secret.qml"))
                                                    << refused << untitled << good;
        QTest::newRow("a scene with a NUL") << climbing(QStringLiteral("scene"), QStringLiteral("scenes/good.qml") + QChar(0) + QStringLiteral(".txt"))
                                            << refused << untitled << good;
        QTest::newRow("a scene on a drive") << climbing(QStringLiteral("scene"), QStringLiteral("C:/Windows/secret.qml"))
                                            << refused << untitled << good;
        QTest::newRow("logic climbing out") << climbing(QStringLiteral("logic"), QStringLiteral("logic/../../secret-logic.qml"))
                                            << refused << good << QString();
        QTest::newRow("a path too long") << climbing(QStringLiteral("scene"), QStringLiteral("scenes/") + QString(5000, QLatin1Char('a')))
                                         << refused << untitled << good;
        QTest::newRow("a scene that is absolute") << Plant([](const Hostile &h) {
            h.plant(QJsonObject{{QStringLiteral("scene"), h.secret()}});
        }) << refused << untitled << good;
        QTest::newRow("logic that is absolute") << Plant([](const Hostile &h) {
            h.plant(QJsonObject{{QStringLiteral("logic"), h.elsewhere.file(QStringLiteral("secret-logic.qml"))}});
        }) << refused << good << QString();

        // In the right place, by name, and not what it claims to be.
        QTest::newRow("a scene in the wrong folder") << Plant([](const Hostile &h) {
            h.plant(QJsonObject{{QStringLiteral("scene"), QStringLiteral("logic/good.qml")}});
        }) << QStringLiteral("does not belong") << untitled << good;
        QTest::newRow("logic in the wrong folder") << Plant([](const Hostile &h) {
            h.plant(QJsonObject{{QStringLiteral("logic"), QStringLiteral("scenes/good.qml")}});
        }) << QStringLiteral("does not belong") << good << QString();
        QTest::newRow("a folder for a scene") << Plant([](const Hostile &h) {
            QVERIFY(QDir().mkpath(h.project.file(QStringLiteral("scenes/folder.qml"))));
            h.plant(QJsonObject{{QStringLiteral("scene"), QStringLiteral("scenes/folder.qml")}});
        }) << refused << untitled << good;

        // Links that leave. A link in a project is whatever its maker wanted.
        const auto linked = [](const QString &key, const QString &name, const QString &to) {
            return Plant([key, name, to](const Hostile &h) {
                if (!makeLink(h.elsewhere.file(to), h.project.file(name)))
                    QSKIP("This user cannot make symbolic links here.");
                h.plant(QJsonObject{{key, name}});
            });
        };
        QTest::newRow("a scene that is a link out") << linked(QStringLiteral("scene"), QStringLiteral("scenes/link.qml"), QStringLiteral("secret.qml"))
                                                    << refused << untitled << good;
        QTest::newRow("logic that is a link out") << linked(QStringLiteral("logic"), QStringLiteral("logic/link.qml"), QStringLiteral("secret-logic.qml"))
                                                  << refused << good << QString();
        QTest::newRow("a scene through a linked folder") << Plant([](const Hostile &h) {
            if (!makeLink(h.elsewhere.path(), h.project.file(QStringLiteral("scenes/away"))))
                QSKIP("This user cannot make symbolic links here.");
            h.plant(QJsonObject{{QStringLiteral("scene"), QStringLiteral("scenes/away/secret.qml")}});
        }) << refused << untitled << good;
        QTest::newRow("a dangling link") << Plant([](const Hostile &h) {
            if (!makeLink(QStringLiteral("/no/such/place.qml"), h.project.file(QStringLiteral("scenes/dangling.qml"))))
                QSKIP("This user cannot make symbolic links here.");
            h.plant(QJsonObject{{QStringLiteral("scene"), QStringLiteral("scenes/dangling.qml")}});
        }) << refused << untitled << good;
        QTest::newRow("the workspace itself a link") << Plant([](const Hostile &h) {
            const QString decoy = h.elsewhere.file(QStringLiteral("decoy.json"));
            QVERIFY(writeFile(decoy, QJsonDocument(h.good()).toJson()));
            if (!makeLink(decoy, h.project.keptFile()))
                QSKIP("This user cannot make symbolic links here.");
        }) << QStringLiteral("not a plain file") << untitled << QString();
#ifdef Q_OS_UNIX
        // A pipe: whoever opens it waits for ever for someone to write.
        QTest::newRow("a scene that is a pipe") << Plant([](const Hostile &h) {
            QVERIFY(::mkfifo(QFile::encodeName(h.project.file(QStringLiteral("scenes/pipe.qml"))).constData(), 0600) == 0);
            h.plant(QJsonObject{{QStringLiteral("scene"), QStringLiteral("scenes/pipe.qml")}});
        }) << refused << untitled << good;
        QTest::newRow("the workspace itself a pipe") << Plant([](const Hostile &h) {
            QDir().mkpath(QFileInfo(h.project.keptFile()).absolutePath());
            QVERIFY(::mkfifo(QFile::encodeName(h.project.keptFile()).constData(), 0600) == 0);
        }) << QStringLiteral("not a plain file") << untitled << QString();
#endif

        // Deleted since. The other still comes back.
        QTest::newRow("a scene deleted since") << Plant([](const Hostile &h) {
            QVERIFY(QFile::remove(h.project.file(QStringLiteral("scenes/good.qml"))));
            h.plant(QJsonObject{});
        }) << QStringLiteral("scenes/good.qml is no longer") << untitled << good;
        QTest::newRow("logic deleted since") << Plant([](const Hostile &h) {
            QVERIFY(QFile::remove(h.project.file(QStringLiteral("logic/good.qml"))));
            h.plant(QJsonObject{});
        }) << QStringLiteral("logic/good.qml is no longer") << good << QString();
        QTest::newRow("both deleted since") << Plant([](const Hostile &h) {
            QVERIFY(QFile::remove(h.project.file(QStringLiteral("scenes/good.qml"))));
            QVERIFY(QFile::remove(h.project.file(QStringLiteral("logic/good.qml"))));
            h.plant(QJsonObject{});
        }) << QStringLiteral("no longer") << untitled << QString();
        // There still, but not a scene any more.
        QTest::newRow("a scene that no longer builds") << Plant([](const Hostile &h) {
            QVERIFY(writeFile(h.project.file(QStringLiteral("scenes/good.qml")), "import QQ\nScene {\n"));
            h.plant(QJsonObject{});
        }) << QStringLiteral("good.qml") << untitled << good;
        QTest::newRow("a scene that is not a scene") << Plant([](const Hostile &h) {
            QVERIFY(writeFile(h.project.file(QStringLiteral("scenes/good.qml")), "import QtQml\nQtObject {}\n"));
            h.plant(QJsonObject{});
        }) << QStringLiteral("not a QQ scene") << untitled << good;

        // Fields of the wrong kind are ignored, each alone: nothing was lost that a person would need to hear about.
        QTest::newRow("paths of the wrong kind") << Plant([](const Hostile &h) {
            h.plant(QJsonObject{{QStringLiteral("scene"), 5}, {QStringLiteral("logic"), QJsonObject{}}});
        }) << QString() << untitled << QString();
        QTest::newRow("panels and window of the wrong kind") << Plant([](const Hostile &h) {
            h.plant(QJsonObject{{QStringLiteral("panels"), 5}, {QStringLiteral("window"), QStringLiteral("big")}});
        }) << QString() << good << good;
        QTest::newRow("panels that are not yes or no") << Plant([](const Hostile &h) {
            h.plant(QJsonObject{{QStringLiteral("panels"), QJsonObject{{QStringLiteral("stats"), QStringLiteral("true")},
                                                                       {QStringLiteral("agent"), 1}}}});
        }) << QString() << good << good;
    }

    void aHostileWorkspaceOpensSafely()
    {
        QFETCH(Plant, plant);
        QFETCH(QString, says);
        QFETCH(QString, opensScene);
        QFETCH(QString, opensLogic);

        Hostile h;
        plant(h);
        if (QTest::currentTestResolved())
            return;

        // It opens at all: no crash, and no hang on what it was handed.
        STUDIO_IN(h.project.url());
        openedSafely(s, h, says, opensScene, opensLogic);
    }

    // The same from a start, which is another way in to the same reading.
    void aHostileWorkspaceAtAStartOpensSafely_data()
    {
        QTest::addColumn<Plant>("plant");
        QTest::addColumn<QString>("says");
        QTest::addColumn<QString>("opensScene");
        QTest::addColumn<QString>("opensLogic");
        const QString refused = QStringLiteral("not a plain file inside this project");
        const QString untitled = QStringLiteral("untitled");
        QTest::newRow("not JSON") << Plant([](const Hostile &h) { h.plant(QByteArray("{ \"version\": 1, \"proj")); })
                                  << QStringLiteral("could not be read") << untitled << QString();
        QTest::newRow("oversized") << Plant([](const Hostile &h) {
            h.plant(QJsonObject{{QStringLiteral("padding"), QString(qq::ProjectWorkspace::largestFile, QLatin1Char('x'))}});
        }) << QStringLiteral("too large") << untitled << QString();
        QTest::newRow("a scene climbing out") << Plant([](const Hostile &h) {
            h.plant(QJsonObject{{QStringLiteral("scene"), QStringLiteral("scenes/../../secret.qml")}});
        }) << refused << untitled << QStringLiteral("good");
        QTest::newRow("a scene that is a link out") << Plant([](const Hostile &h) {
            if (!makeLink(h.secret(), h.project.file(QStringLiteral("scenes/link.qml"))))
                QSKIP("This user cannot make symbolic links here.");
            h.plant(QJsonObject{{QStringLiteral("scene"), QStringLiteral("scenes/link.qml")}});
        }) << refused << untitled << QStringLiteral("good");
    }

    void aHostileWorkspaceAtAStartOpensSafely()
    {
        QFETCH(Plant, plant);
        QFETCH(QString, says);
        QFETCH(QString, opensScene);
        QFETCH(QString, opensLogic);
        Hostile h;
        plant(h);
        if (QTest::currentTestResolved())
            return;
        Studio s;
        QVERIFY(s.open(h.project.url()));
        openedSafely(s, h, says, opensScene, opensLogic);
    }

    // After a hostile file the next save puts it right, and a link there is replaced, not written through.
    void theNextSaveMendsWhatWasThere()
    {
        Hostile h;
        h.plant(QByteArray("garbage"));
        {
            Studio s;
            QVERIFY(s.open(h.project.url()));
            QVERIFY(s.call("saveWorkspace").toBool());
        }
        QVERIFY(qq::ProjectWorkspace().load(h.project.url()).value(QStringLiteral("found")).toBool());
        QCOMPARE(qq::ProjectWorkspace().load(h.project.url()).value(QStringLiteral("problem")).toString(), QString());

        const QString decoy = h.elsewhere.file(QStringLiteral("decoy.json"));
        QVERIFY(writeFile(decoy, "the decoy"));
        QVERIFY(QFile::remove(h.project.keptFile()));
        if (!makeLink(decoy, h.project.keptFile()))
            QSKIP("This user cannot make symbolic links here.");
        Studio s;
        QVERIFY(s.open(h.project.url()));
        QVERIFY(s.call("saveWorkspace").toBool());
        QCOMPARE(readBytes(decoy), QByteArray("the decoy"));
        QVERIFY(!QFileInfo(h.project.keptFile()).isSymLink());
    }

    void aStoreThatCannotBeWrittenIsNoHarm()
    {
        Project p;
        trust(p.url());
        const QString store = qq::ProjectWorkspace::storeFolder();
        QVERIFY(QDir().mkpath(QFileInfo(store).absolutePath()));
        QDir(store).removeRecursively();
        QVERIFY(writeFile(store, "a file where the folder goes"));
        {
            Studio s;
            QVERIFY(s.open(p.url()));
            QVERIFY(!s.call("saveWorkspace").toBool());
            QCOMPARE(s.get("recentProjects").toList().size(), 0);
        }
        QVERIFY(QFile::remove(store));
    }

    void aHostileWindowIsBroughtOntoTheScreen_data()
    {
        QTest::addColumn<QJsonValue>("x");
        QTest::addColumn<QJsonValue>("y");
        QTest::addColumn<QJsonValue>("width");
        QTest::addColumn<QJsonValue>("height");
        QTest::addColumn<bool>("sizeIsUsed");

        QTest::newRow("far off to the left") << QJsonValue(-50000) << QJsonValue(-50000) << QJsonValue(1000) << QJsonValue(700) << true;
        QTest::newRow("far off to the right") << QJsonValue(90000) << QJsonValue(90000) << QJsonValue(1000) << QJsonValue(700) << true;
        QTest::newRow("huge") << QJsonValue(0) << QJsonValue(0) << QJsonValue(900000) << QJsonValue(900000) << true;
        QTest::newRow("beyond any number") << QJsonValue(1e300) << QJsonValue(-1e300) << QJsonValue(1e300) << QJsonValue(1e300) << false;
        QTest::newRow("tiny") << QJsonValue(0) << QJsonValue(0) << QJsonValue(1) << QJsonValue(1) << true;
        QTest::newRow("negative") << QJsonValue(0) << QJsonValue(0) << QJsonValue(-400) << QJsonValue(-300) << false;
        QTest::newRow("no size") << QJsonValue(0) << QJsonValue(0) << QJsonValue(0) << QJsonValue(0) << false;
        QTest::newRow("words") << QJsonValue(QStringLiteral("left")) << QJsonValue(QStringLiteral("top"))
                               << QJsonValue(QStringLiteral("wide")) << QJsonValue(QStringLiteral("tall")) << false;
        QTest::newRow("numbers as words") << QJsonValue(QStringLiteral("10")) << QJsonValue(QStringLiteral("10"))
                                          << QJsonValue(QStringLiteral("800")) << QJsonValue(QStringLiteral("600")) << false;
        QTest::newRow("nothing") << QJsonValue(QJsonValue::Null) << QJsonValue(QJsonValue::Null)
                                 << QJsonValue(QJsonValue::Null) << QJsonValue(QJsonValue::Null) << false;
        QTest::newRow("yes and no") << QJsonValue(true) << QJsonValue(false) << QJsonValue(true) << QJsonValue(true) << false;
    }

    void aHostileWindowIsBroughtOntoTheScreen()
    {
        QFETCH(QJsonValue, x);
        QFETCH(QJsonValue, y);
        QFETCH(QJsonValue, width);
        QFETCH(QJsonValue, height);
        QFETCH(bool, sizeIsUsed);

        Hostile h;
        h.plant(QJsonObject{{QStringLiteral("window"), QJsonObject{{QStringLiteral("x"), x}, {QStringLiteral("y"), y},
                                                                   {QStringLiteral("width"), width}, {QStringLiteral("height"), height},
                                                                   {QStringLiteral("maximized"), QStringLiteral("yes")}}}});
        QVERIFY(openSharedStudio());
        m_shared->window->setProperty("width", 1010);
        m_shared->window->setProperty("height", 710);
        QTRY_COMPARE(m_shared->window->width(), 1010);
        STUDIO_IN(h.project.url());
        s.settleFrames();
        // It is a window a person can get hold of.
        const QRect screen = s.window->screen()->availableGeometry();
        QVERIFY2(s.window->width() >= qMin(qq::ProjectWorkspace::smallestWidth, screen.width()), qPrintable(QString::number(s.window->width())));
        QVERIFY2(s.window->height() >= qMin(qq::ProjectWorkspace::smallestHeight, screen.height()), qPrintable(QString::number(s.window->height())));
        if (sizeIsUsed) {
            QVERIFY2(s.window->width() <= screen.width(), qPrintable(QString::number(s.window->width())));
            QVERIFY2(s.window->height() <= screen.height(), qPrintable(QString::number(s.window->height())));
            QVERIFY(s.window->width() != 1010);
        } else {
            // Nothing usable was kept, so it stays as it was.
            QCOMPARE(s.window->width(), 1010);
            QCOMPARE(s.window->height(), 710);
        }
        QVERIFY(s.window->visibility() != QWindow::Maximized);  // "yes" is not true
        // And the rest of the workspace came back all the same.
        QCOMPARE(s.sceneName(), QStringLiteral("good"));
    }

    void aWindowIsFittedToTheScreensThereAre_data()
    {
        QTest::addColumn<QVariantMap>("stored");
        QTest::addColumn<QRect>("expected");  // an invalid one: no geometry at all

        const auto window = [](int x, int y, int w, int h) {
            return QVariantMap{{QStringLiteral("x"), x}, {QStringLiteral("y"), y}, {QStringLiteral("width"), w}, {QStringLiteral("height"), h}};
        };
        // The screens are 1920 by 1080 at the origin, with a panel taking 40 at the top, and 2560 by 1440 to its right.
        QTest::newRow("as it was") << window(100, 100, 800, 600) << QRect(100, 100, 800, 600);
        QTest::newRow("on the second screen") << window(2500, 200, 1000, 800) << QRect(2500, 200, 1000, 800);
        QTest::newRow("off the left") << window(-5000, 200, 800, 600) << QRect(0, 200, 800, 600);
        QTest::newRow("off the far right") << window(9000, 200, 800, 600) << QRect(1120, 200, 800, 600);
        QTest::newRow("under the panel") << window(100, 0, 800, 600) << QRect(100, 40, 800, 600);
        QTest::newRow("overhanging the second") << window(4300, 1300, 800, 600) << QRect(3680, 840, 800, 600);
        QTest::newRow("larger than the screen it is on") << window(100, 100, 9000, 9000) << QRect(1920, 0, 2560, 1440);
        QTest::newRow("larger, and mostly on the first") << window(-8000, 100, 9000, 500) << QRect(0, 100, 1920, 500);
        QTest::newRow("a sliver") << window(100, 100, 1, 1) << QRect(100, 100, qq::ProjectWorkspace::smallestWidth, qq::ProjectWorkspace::smallestHeight);
        QTest::newRow("negative") << window(100, 100, -5, 600) << QRect();
        QTest::newRow("zero") << window(100, 100, 0, 0) << QRect();
        QTest::newRow("a word") << QVariantMap{{QStringLiteral("width"), QStringLiteral("800")}, {QStringLiteral("height"), 600}} << QRect();
        QTest::newRow("beyond a number") << QVariantMap{{QStringLiteral("width"), 1e300}, {QStringLiteral("height"), 600.0}} << QRect();
        QTest::newRow("infinite") << QVariantMap{{QStringLiteral("width"), qInf()}, {QStringLiteral("height"), 600.0}} << QRect();
        QTest::newRow("not a number") << QVariantMap{{QStringLiteral("width"), qQNaN()}, {QStringLiteral("height"), 600.0}} << QRect();
    }

    void aWindowIsFittedToTheScreensThereAre()
    {
        QFETCH(QVariantMap, stored);
        QFETCH(QRect, expected);
        const QList<QRect> screens{QRect(0, 40, 1920, 1040), QRect(1920, 0, 2560, 1440)};
        const QVariantMap fitted = qq::ProjectWorkspace::fitWindow(stored, screens);
        if (!expected.isValid()) {
            QVERIFY(!fitted.contains(QStringLiteral("width")));
            return;
        }
        QCOMPARE(QRect(fitted.value(QStringLiteral("x")).toInt(), fitted.value(QStringLiteral("y")).toInt(),
                       fitted.value(QStringLiteral("width")).toInt(), fitted.value(QStringLiteral("height")).toInt()),
                 expected);
    }

    void aWindowWithNoScreenToMeasureByStaysSensible()
    {
        const QVariantMap fitted = qq::ProjectWorkspace::fitWindow(
            {{QStringLiteral("width"), 999999}, {QStringLiteral("height"), 1}}, {});
        QCOMPARE(fitted.value(QStringLiteral("width")).toInt(), 16384);
        QCOMPARE(fitted.value(QStringLiteral("height")).toInt(), qq::ProjectWorkspace::smallestHeight);
    }

    void onlyPlainPathsCount_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("plain");
        QTest::newRow("a scene") << QStringLiteral("scenes/alpha.qml") << true;
        QTest::newRow("deeper") << QStringLiteral("logic/sub/spin-2.qml") << true;
        QTest::newRow("spaces inside") << QStringLiteral("logic/my file.qml") << true;
        QTest::newRow("empty") << QString() << false;
        QTest::newRow("absolute") << QStringLiteral("/etc/passwd") << false;
        QTest::newRow("up") << QStringLiteral("../x.qml") << false;
        QTest::newRow("up inside") << QStringLiteral("scenes/../x.qml") << false;
        QTest::newRow("here") << QStringLiteral("./x.qml") << false;
        QTest::newRow("trailing dots") << QStringLiteral("scenes/.. /x.qml") << false;
        QTest::newRow("an empty part") << QStringLiteral("scenes//x.qml") << false;
        QTest::newRow("a backslash") << QStringLiteral("scenes\\x.qml") << false;
        QTest::newRow("a drive") << QStringLiteral("C:/x.qml") << false;
        QTest::newRow("a stream") << QStringLiteral("scenes/x.qml:hidden") << false;
        QTest::newRow("a URL") << QStringLiteral("file:///x.qml") << false;
        QTest::newRow("percent") << QStringLiteral("scenes/%2e%2e/x.qml") << false;
        QTest::newRow("a fragment") << QStringLiteral("scenes/x.qml#y") << false;
        QTest::newRow("a query") << QStringLiteral("scenes/x.qml?y") << false;
        QTest::newRow("a newline") << QStringLiteral("scenes/x\n.qml") << false;
        QTest::newRow("a NUL") << QString(QStringLiteral("scenes/x") + QChar(0) + QStringLiteral(".qml")) << false;
        QTest::newRow("too long") << QString(2000, QLatin1Char('a')) << false;
    }

    void onlyPlainPathsCount()
    {
        QFETCH(QString, path);
        QFETCH(bool, plain);
        QCOMPARE(qq::ProjectWorkspace::plainRelative(path), plain);
    }

    void relativePathsAreOnlyFromInside()
    {
        Project p;
        Project elsewhere;
        p.scene(QStringLiteral("alpha"), QStringLiteral("x"));
        writeFile(elsewhere.file(QStringLiteral("out.qml")), "x");
        qq::ProjectWorkspace workspace;
        QCOMPARE(workspace.relativeInside(p.url(), QUrl::fromLocalFile(p.file(QStringLiteral("scenes/alpha.qml")))), QStringLiteral("scenes/alpha.qml"));
        // Not there, outside, or not a file at all: nothing to keep.
        QCOMPARE(workspace.relativeInside(p.url(), QUrl::fromLocalFile(p.file(QStringLiteral("scenes/none.qml")))), QString());
        QCOMPARE(workspace.relativeInside(p.url(), QUrl::fromLocalFile(elsewhere.file(QStringLiteral("out.qml")))), QString());
        QCOMPARE(workspace.relativeInside(p.url(), QUrl(QStringLiteral("qrc:/qq-untrusted/untitled.qml"))), QString());
        QCOMPARE(workspace.relativeInside(p.url(), QUrl()), QString());
        QCOMPARE(workspace.relativeInside(QUrl(), QUrl::fromLocalFile(p.file(QStringLiteral("scenes/alpha.qml")))), QString());
        // Saving will not write what loading would refuse.
        QVERIFY(workspace.save(p.url(), {{QStringLiteral("scene"), QStringLiteral("../../out.qml")},
                                         {QStringLiteral("logic"), elsewhere.file(QStringLiteral("out.qml"))}}));
        const QJsonObject kept = QJsonDocument::fromJson(readBytes(p.keptFile())).object();
        QCOMPARE(kept.value(QStringLiteral("scene")).toString(), QString());
        QCOMPARE(kept.value(QStringLiteral("logic")).toString(), QString());
    }

    // A project reached by a link is one project, wherever it is called from.
    void aProjectThroughALinkIsTheSameProject()
    {
        TwoProjects two;
        QTemporaryDir parent;
        const QString through = QDir(parent.path()).filePath(QStringLiteral("through"));
        if (!makeLink(two.a.path(), through))
            QSKIP("This user cannot make symbolic links here.");
        STUDIO_IN(two.a.url());
        s.call("openScene", QStringLiteral("alpha"));
        s.edit();
        QVERIFY(s.call("switchProject", QUrl::fromLocalFile(through)).toBool());  // the same: nothing asked, nothing lost
        QVERIFY(!s.asking());
        QVERIFY(s.get("dirty").toBool());
        QCOMPARE(qq::ProjectWorkspace::fileFor(qq::Confinement::realPath(through)), two.a.keptFile());
    }
};

QTEST_MAIN(TstWorkspace)

#include "tst_workspace.moc"
