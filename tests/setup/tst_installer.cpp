// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "installerstate.h"
#include "kit.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(QOR_KitPlugin)
Q_IMPORT_QML_PLUGIN(LumenPlugin)
Q_IMPORT_QML_PLUGIN(QOR_SetupPlugin)
using namespace Qt::StringLiterals;

class InstallerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        qor::Kit::initialize();
        QQuickStyle::setStyle(u"Lumen"_s);
    }
    void explicitReviewAndConfirmation()
    {
        qor::InstallerState state(QString::fromUtf8(QOR_INSTALL_TEST_CHILD), { u"good"_s });
        state.install(u"qor-0123456789abcdef"_s, true);
        QCOMPARE(state.phase(), u"idle");
        state.inspect();
        QTRY_COMPARE(state.phase(), u"choose");
        QVERIFY(state.target().contains(u"17179869184"));
        QVERIFY(!state.writing());
        state.review();
        QCOMPARE(state.phase(), u"review");
        state.install(state.serial(), false);
        QCOMPARE(state.phase(), u"review");
        state.install(u"/dev/sda"_s, true);
        QCOMPARE(state.phase(), u"review");
        state.install(state.serial(), true);
        QVERIFY(state.writing());
        QTRY_COMPARE(state.phase(), u"complete");
        QVERIFY(!state.writing());
    }
    void hostileOrFailedProtocol_data()
    {
        QTest::addColumn<QString>("mode");
        for (const auto &mode :
             { u"oversize"_s, u"invalid"_s, u"bad-inventory"_s, u"incomplete"_s, u"inventory-fail"_s })
            QTest::newRow(qPrintable(mode)) << mode;
    }
    void hostileOrFailedProtocol()
    {
        QFETCH(QString, mode);
        qor::InstallerState state(QString::fromUtf8(QOR_INSTALL_TEST_CHILD), { mode });
        state.inspect();
        QTRY_COMPARE(state.phase(), u"error");
        QVERIFY(state.serial().isEmpty());
        QVERIFY(!state.message().contains(u"<img"));
        QVERIFY(!state.message().contains(u"invalid.test"));
    }
    void incompleteTransactionCannotPassOrRetry_data()
    {
        QTest::addColumn<QString>("mode");
        for (const auto &mode : { u"complete-fail"_s, u"no-complete"_s, u"early-complete"_s })
            QTest::newRow(qPrintable(mode)) << mode;
    }
    void incompleteTransactionCannotPassOrRetry()
    {
        QFETCH(QString, mode);
        qor::InstallerState state(QString::fromUtf8(QOR_INSTALL_TEST_CHILD), { mode });
        state.inspect();
        QTRY_COMPARE(state.phase(), u"choose");
        state.review();
        state.install(state.serial(), true);
        QTRY_COMPARE(state.phase(), u"error");
        QTRY_VERIFY(!state.writing());
        QVERIFY(!state.retryAllowed());
        state.inspect();
        QCOMPARE(state.phase(), u"error");
    }
    void unavailableBackendCannotPass()
    {
        qor::InstallerState state(u"/nonexistent/qor-test-backend"_s, {});
        state.inspect();
        QTRY_COMPARE(state.phase(), u"error");
        QVERIFY(!state.writing());
        QVERIFY(state.serial().isEmpty());
    }
    void actualViewHasNoPreselectedDestructiveAction()
    {
        qor::InstallerState state(QString::fromUtf8(QOR_INSTALL_TEST_CHILD), { u"good"_s });
        QQmlApplicationEngine engine;
        engine.setInitialProperties({ { u"installer"_s, QVariant::fromValue(&state) } });
        engine.loadFromModule("QOR.Setup", "Installer");
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QTRY_COMPARE(state.phase(), u"choose");
        auto *erase = window->findChild<QQuickItem *>(u"eraseAcknowledged"_s);
        auto *begin = window->findChild<QQuickItem *>(u"beginInstall"_s);
        QVERIFY(erase && begin);
        QVERIFY(!erase->property("checked").toBool());
        QVERIFY(!begin->isEnabled());
        state.review();
        QTRY_VERIFY(erase->isVisible());
        QVERIFY(!erase->property("checked").toBool());
        QVERIFY(!begin->isEnabled());
    }
};

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    InstallerTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_installer.moc"
