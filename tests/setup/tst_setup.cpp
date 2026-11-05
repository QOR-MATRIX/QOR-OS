// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "kit.h"
#include "sessionwelcome.h"
#include "setupstate.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>
#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif

Q_IMPORT_QML_PLUGIN(QOR_KitPlugin)
Q_IMPORT_QML_PLUGIN(LumenPlugin)
Q_IMPORT_QML_PLUGIN(QOR_SetupPlugin)
using namespace Qt::StringLiterals;

class Desktop : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool locked MEMBER locked NOTIFY lockedChanged)
public:
    bool locked = true;
    void unlock()
    {
        locked = false;
        Q_EMIT lockedChanged();
    }
Q_SIGNALS:
    void lockedChanged();
};

class SetupTest : public QObject
{
    Q_OBJECT
    static QByteArray read(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return file.readAll();
    }
    static bool write(const QString &path, const QByteArray &data)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    }
    static QQuickWindow *load(QQmlApplicationEngine &engine, qor::SetupState &state)
    {
        engine.setInitialProperties({ { u"setupState"_s, QVariant::fromValue(&state) } });
        qor::applyWelcomeAppearance(engine.singletonInstance<QObject *>("QOR.Kit", "Theme"), {}, {});
        engine.loadFromModule("QOR.Setup", "Welcome");
        return engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    }
private Q_SLOTS:
    void initTestCase()
    {
        qor::Kit::initialize();
        QQuickStyle::setStyle(u"Lumen"_s);
    }
    void completionIsAtomicAndIdempotent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        qor::SetupState state(dir.path());
        QSignalSpy done(&state, &qor::SetupState::completedChanged);
        QVERIFY(!state.completed());
        QVERIFY(state.error().isEmpty());
        QVERIFY2(state.finish(), qPrintable(state.error()));
        QVERIFY(state.completed());
        QCOMPARE(done.count(), 1);
        QVERIFY(state.finish());
        QCOMPARE(done.count(), 1);
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot),
                 QStringList { u"completed.json"_s });
        qor::SetupState next(dir.path());
        QVERIFY(next.completed());
    }
    void malformedRecordsArePreserved_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("truncated") << QByteArray("{\"schema\":1");
        QTest::newRow("oversized") << QByteArray(2048, 'x');
        QTest::newRow("duplicate") << QByteArray("{\"schema\":1,\"completed\":false,\"completed\":true}\n");
        QTest::newRow("outside-text") << QByteArray("<img src='https://invalid.test/not-loaded'>");
    }
    void malformedRecordsArePreserved()
    {
        QFETCH(QByteArray, bytes);
        QTemporaryDir dir;
        const QString path = dir.filePath(u"completed.json"_s);
        QVERIFY(write(path, bytes));
        qor::SetupState state(dir.path());
        QVERIFY(!state.completed());
        QVERIFY(!state.error().isEmpty());
        QVERIFY(!state.finish());
        QVERIFY(read(path) == bytes);
        QVERIFY(!state.error().contains(u"img"_s));
    }
    void changedRecordIsNotOverwritten()
    {
        QTemporaryDir dir;
        qor::SetupState state(dir.path());
        const QString path = dir.filePath(u"completed.json"_s);
        QVERIFY(write(path, "unfinished external change"));
        QVERIFY(!state.finish());
        QVERIFY(!state.completed());
        QVERIFY(read(path) == QByteArray("unfinished external change"));
    }
    void failedWriteDoesNotComplete()
    {
        QTemporaryDir dir;
        qor::SetupState state(dir.path());
        QVERIFY(QDir().mkpath(dir.filePath(u"completed.json"_s)));
        QVERIFY(!state.finish());
        QVERIFY(!state.completed());
        QVERIFY(!state.error().isEmpty());
        QVERIFY(QFileInfo(dir.filePath(u"completed.json"_s)).isDir());
    }
    void linkedRecordIsRefused()
    {
        QTemporaryDir dir;
        const QString target = dir.filePath(u"original"_s);
        QVERIFY(write(target, "preserve"));
        const QString link = dir.filePath(u"completed.json"_s);
#ifdef Q_OS_WIN
        if (!CreateSymbolicLinkW(reinterpret_cast<LPCWSTR>(link.utf16()), reinterpret_cast<LPCWSTR>(target.utf16()),
                                 SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE))
#else
        if (!QFile::link(target, link))
#endif
            QSKIP("Host does not allow a test link");
        qor::SetupState state(dir.path());
        QVERIFY(!state.finish());
        QVERIFY(read(target) == QByteArray("preserve"));
    }
    void keyboardWelcomeCompletes()
    {
        QTemporaryDir dir;
        qor::SetupState state(dir.path());
        QQmlApplicationEngine engine;
        QQuickWindow *window = load(engine, state);
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto *next = window->findChild<QQuickItem *>(u"setupNext"_s);
        QVERIFY(next);
        next->forceActiveFocus();
        QTRY_VERIFY(next->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_COMPARE(window->property("page").toInt(), 1);
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_COMPARE(window->property("page").toInt(), 2);
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_VERIFY2(state.completed(), qPrintable(state.error()));
        QVERIFY(!state.error().size());
    }
    void closeKeepsWelcomePending()
    {
        QTemporaryDir dir;
        qor::SetupState state(dir.path());
        QQmlApplicationEngine engine;
        QQuickWindow *window = load(engine, state);
        QVERIFY(window);
        window->close();
        QVERIFY(!state.completed());
        QVERIFY(!QFileInfo::exists(dir.filePath(u"completed.json"_s)));
    }
    void errorIsPlainTextAndCompletionBlocked()
    {
        QTemporaryDir dir;
        QVERIFY(write(dir.filePath(u"completed.json"_s), "<img src='https://invalid.test'>"));
        qor::SetupState state(dir.path());
        QQmlApplicationEngine engine;
        QQuickWindow *window = load(engine, state);
        QVERIFY(window);
        auto *error = window->findChild<QQuickItem *>(u"setupError"_s);
        QVERIFY(error);
        QCOMPARE(error->property("textFormat").toInt(), int(Qt::PlainText));
        QVERIFY(!window->findChild<QQuickItem *>(u"setupNext"_s)->isEnabled());
        QVERIFY(!state.completed());
    }
    void layout_data()
    {
        QTest::addColumn<QSize>("size");
        QTest::newRow("small") << QSize(640, 540);
        QTest::newRow("hd") << QSize(1920, 1080);
        QTest::newRow("uhd") << QSize(3840, 2160);
    }
    void layout()
    {
        QFETCH(QSize, size);
        QTemporaryDir dir;
        qor::SetupState state(dir.path());
        QQmlApplicationEngine engine;
        auto *window = load(engine, state);
        QVERIFY(window);
        window->resize(size);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto *button = window->findChild<QQuickItem *>(u"setupNext"_s);
        QVERIFY(button);
        QTRY_VERIFY(button->mapToScene(QPointF()).y() + button->height() <= window->height());
        QVERIFY(button->mapToScene(QPointF()).x() + button->width() <= window->width());
    }
    void welcomeWaitsForUnlockAndRunsOnce()
    {
        QTemporaryDir dir;
        const QByteArray probe = QFile::encodeName(dir.filePath(u"probe"_s));
        qputenv("QOR_SETUP_TEST_PROBE", probe);
        Desktop desktop;
        QObject appearance;
        appearance.setProperty("reduceMotion", false);
        appearance.setProperty("increaseContrast", true);
        qor::SessionWelcome launcher(&desktop, QStringLiteral(QOR_SETUP_TEST_CHILD), &appearance);
        QSignalSpy attempted(&launcher, &qor::SessionWelcome::launchAttempted);
        QVERIFY(!QFileInfo::exists(QString::fromLocal8Bit(probe)));
        desktop.unlock();
        QTRY_COMPARE(attempted.count(), 1);
        QVERIFY(attempted.first().first().toBool());
        QTRY_VERIFY(read(QString::fromLocal8Bit(probe)) == QByteArray("--first-run\n0\n1\n"));
        desktop.locked = true;
        Q_EMIT desktop.lockedChanged();
        desktop.unlock();
        QCOMPARE(attempted.count(), 1);
        qunsetenv("QOR_SETUP_TEST_PROBE");
    }
    void launchFailureDoesNotRetryInALoop()
    {
        Desktop desktop;
        qor::SessionWelcome launcher(&desktop, u"/nonexistent-qor-test-program"_s);
        QSignalSpy attempted(&launcher, &qor::SessionWelcome::launchAttempted);
        desktop.unlock();
        QCOMPARE(attempted.count(), 1);
        QVERIFY(!attempted.first().first().toBool());
        desktop.unlock();
        QCOMPARE(attempted.count(), 1);
    }
    void alreadyUnlockedFailureIsObservable()
    {
        Desktop desktop;
        desktop.locked = false;
        qor::SessionWelcome launcher(&desktop, u"/nonexistent-qor-test-program"_s);
        QSignalSpy attempted(&launcher, &qor::SessionWelcome::launchAttempted);
        QTRY_COMPARE(attempted.count(), 1);
        QVERIFY(!attempted.first().first().toBool());
        desktop.unlock();
        QCOMPARE(attempted.count(), 1);
    }
    void accessibilityHintsReachActualTheme()
    {
        QQmlApplicationEngine engine;
        auto *theme = engine.singletonInstance<QObject *>("QOR.Kit", "Theme");
        QVERIFY(theme);
        qor::applyWelcomeAppearance(theme, "1", "1");
        QVERIFY(theme->property("reduceMotion").toBool());
        QVERIFY(theme->property("increaseContrast").toBool());
        qor::applyWelcomeAppearance(theme, "0", "0");
        QVERIFY(!theme->property("reduceMotion").toBool());
        QVERIFY(!theme->property("increaseContrast").toBool());
        qor::applyWelcomeAppearance(theme, "unknown", "unknown");
        QVERIFY(theme->property("reduceMotion").toBool());
        QVERIFY(!theme->property("increaseContrast").toBool());
    }
};
QTEST_MAIN(SetupTest)
#include "tst_setup.moc"
