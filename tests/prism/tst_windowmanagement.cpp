// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "windowmanagement.h"

#include <QtCore/QMetaObject>
#include <QtQuick/QQuickItem>
#include <QtTest/QTest>

class TestWindowManagement : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void configureAckedRejectsNonToplevels_data();
    void configureAckedRejectsNonToplevels();
};

void TestWindowManagement::configureAckedRejectsNonToplevels_data()
{
    QTest::addColumn<int>("kind");
    QTest::newRow("null") << 0;
    QTest::newRow("plain-object") << 1;
    QTest::newRow("quick-item") << 2;
    QTest::newRow("window-manager") << 3;
}

void TestWindowManagement::configureAckedRejectsNonToplevels()
{
    QFETCH(int, kind);
    qor::prism::WindowManagement manager;
    QObject object;
    QQuickItem item;
    const QList<QObject *> inputs { nullptr, &object, &item, &manager };
    QObject *input = inputs.at(kind);
    QVERIFY(!manager.configureAcked(input));

    // Exercise the same invokable signature used by QML as well as the C++ call.
    bool acked = true;
    QVERIFY(QMetaObject::invokeMethod(&manager, "configureAcked", Qt::DirectConnection, Q_RETURN_ARG(bool, acked),
                                      Q_ARG(QObject *, input)));
    QVERIFY(!acked);
}

QTEST_MAIN(TestWindowManagement)
#include "tst_windowmanagement.moc"
