// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
// Private in-memory reports only. No virtual/system controller or native reader.
#include "controlleractions.h"
#include "controllerregistration.h"

#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest/QTest>

#include <memory>

using namespace qor::input;
using namespace qor::qonsole;

namespace {
QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name)
        return root;
    for (QQuickItem *child : root->childItems()) {
        if (auto *found = findItem(child, name))
            return found;
    }
    return nullptr;
}

QByteArray report(quint8 face = 0, quint8 hat = 8)
{
    QByteArray data(64, '\0');
    data[0] = 1;
    for (int i = 1; i <= 4; ++i)
        data[i] = char(-128); // QByteArray's signed byte representation of 0x80.
    data[5] = char(face | hat);
    return data;
}

struct LauncherView
{
    QQmlEngine engine;
    QQuickWindow window;
    QQmlComponent component { &engine, QUrl::fromLocalFile(QStringLiteral(QOR_QONSOLE_SOURCE_DIR "/Launcher.qml")) };
    std::unique_ptr<QObject> object { component.create() };

    QQuickItem *item()
    {
        auto *root = qobject_cast<QQuickItem *>(object.get());
        if (root) {
            root->setParentItem(window.contentItem());
            root->setWidth(960);
            root->setHeight(640);
            window.resize(960, 640);
            window.show();
        }
        return root;
    }
};
} // namespace

class QonsoleControllerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void exactRegistrationAndUnrelatedUsages()
    {
        const ControllerRegistration request { 1, 5, 0x2000, 123 };
        QVERIFY(registrationMatchesRequest({ request }, request, true));
        QVERIFY(registrationMatchesRequest({ { 1, 2, 0, 456 }, request }, request, true));
    }

    void missingFailedOrConflictingRegistrationCannotPass()
    {
        const ControllerRegistration request { 1, 5, 0x2000, 123 };
        QVERIFY(!registrationMatchesRequest({}, request, true));
        QVERIFY(!registrationMatchesRequest({ request }, request, false));
        QVERIFY(!registrationMatchesRequest({ { 1, 5, 0x2000, 0 } }, { 1, 5, 0x2000, 0 }, true));
        for (const auto &wrong : QList<ControllerRegistration> {
                 { 1, 4, 0x2000, 123 }, { 2, 5, 0x2000, 123 }, { 1, 5, 0x2100, 123 }, { 1, 5, 0x2000, 456 } })
            QVERIFY(!registrationMatchesRequest({ wrong }, request, true));
    }

    void duplicateRegistrationCannotEstablishOneTarget()
    {
        const ControllerRegistration request { 1, 5, 0x2000, 123 };
        QVERIFY(!registrationMatchesRequest({ request, request }, request, true));
        const ControllerRegistration other { 1, 5, 0x2000, 456 };
        QVERIFY(!registrationMatchesRequest({ request, other }, request, true));
        QVERIFY(!registrationMatchesRequest({ other, request }, request, true));
    }

    void ds4FixtureUsesExistingCrossCircleMapping()
    {
        ControllerActions actions;
        QList<ControllerAction> emitted;
        const auto consume = [&](const GamepadState &state) {
            const auto action = actions.update(state.buttons, true);
            if (action != ControllerAction::None)
                emitted.append(action);
        };
        const QByteArray data = report() + report(0x20) + report(0x20) + report() + report(0x40);
        QVERIFY(visitWiredDs4Reports(data, 64, 5, consume));
        QCOMPARE(emitted, QList<ControllerAction>({ ControllerAction::Confirm, ControllerAction::Back }));
    }

    void heldReleaseAndFreshPressHaveOneEdgeEach()
    {
        ControllerActions actions;
        QCOMPARE(actions.update(0, true), ControllerAction::None);
        QCOMPARE(actions.update(bit(Button::South), true), ControllerAction::Confirm);
        for (int i = 0; i < 20; ++i)
            QCOMPARE(actions.update(bit(Button::South), true), ControllerAction::None);
        QCOMPARE(actions.update(0, true), ControllerAction::None);
        QCOMPARE(actions.update(bit(Button::South), true), ControllerAction::Confirm);
        QCOMPARE(actions.update(0, true), ControllerAction::None);
    }

    void inactiveAndRefocusRequireNeutral()
    {
        ControllerActions actions;
        QCOMPARE(actions.update(bit(Button::South), false), ControllerAction::None);
        QCOMPARE(actions.update(bit(Button::South), true), ControllerAction::None);
        QCOMPARE(actions.update(0, true), ControllerAction::None);
        QCOMPARE(actions.update(bit(Button::South), true), ControllerAction::Confirm);
        actions.reset();
        QCOMPARE(actions.update(bit(Button::East), true), ControllerAction::None);
        QCOMPARE(actions.update(0, true), ControllerAction::None);
        QCOMPARE(actions.update(bit(Button::East), true), ControllerAction::Back);
    }

    void directionsAndUnknownButtons()
    {
        ControllerActions actions;
        const Button buttons[] = { Button::DPadUp, Button::DPadDown, Button::DPadLeft, Button::DPadRight };
        const ControllerAction expected[] = { ControllerAction::Up, ControllerAction::Down, ControllerAction::Left,
                                              ControllerAction::Right };
        for (int i = 0; i < 4; ++i) {
            QCOMPARE(actions.update(0, true), ControllerAction::None);
            QCOMPARE(actions.update(bit(buttons[i]), true), expected[i]);
            QCOMPARE(actions.update(bit(buttons[i]), true), ControllerAction::None);
        }
        QCOMPARE(actions.update(bit(Button::North) | bit(Button::Guide), true), ControllerAction::None);
    }

    void simultaneousCancelWinsWithoutDoubleActivation()
    {
        ControllerActions actions;
        actions.update(0, true);
        const quint32 both = bit(Button::South) | bit(Button::East);
        QCOMPARE(actions.update(both, true), ControllerAction::Back);
        QCOMPARE(actions.update(both, true), ControllerAction::None);
        QCOMPARE(actions.update(0, true), ControllerAction::None);
        QCOMPARE(actions.update(bit(Button::South) | bit(Button::DPadRight), true), ControllerAction::Confirm);
    }

    void hostileBatchBoundsAreCheckedBeforeCallbacks()
    {
        int callbacks = 0;
        const auto consume = [&](const GamepadState &) { ++callbacks; };
        QVERIFY(!visitWiredDs4Reports(QByteArray(), 64, 0, consume));
        QVERIFY(!visitWiredDs4Reports(report().left(63), 64, 1, consume));
        QVERIFY(!visitWiredDs4Reports(report(), 63, 1, consume));
        QVERIFY(!visitWiredDs4Reports(report(), 64, 65, consume));
        QVERIFY(!visitWiredDs4Reports(report(), 64, quint32(-1), consume));
        QByteArray invalid = report() + report();
        invalid[64] = 0x11; // Bluetooth-shaped/unknown report is outside this adapter.
        QVERIFY(!visitWiredDs4Reports(invalid, 64, 2, consume));
        QCOMPARE(callbacks, 0);
        QVERIFY(visitWiredDs4Reports(report(), 64, 1, consume));
        QCOMPARE(callbacks, 1);
    }

    void semanticDispatchReachesActualQmlAndRestoresFocus()
    {
        LauncherView view;
        QQuickItem *launcher = view.item();
        QVERIFY2(launcher, qPrintable(view.component.errorString()));
        QQuickItem *home = nullptr;
        QTRY_VERIFY((home = findItem(launcher, QStringLiteral("openSample"))) != nullptr);
        QTRY_VERIFY(home->property("activeFocus").toBool());
        ControllerActions actions;
        actions.update(0, true);
        QVERIFY(dispatchControllerAction(launcher, actions.update(bit(Button::South), true)));
        QCOMPARE(launcher->property("page").toString(), QStringLiteral("sample"));
        QVERIFY(!dispatchControllerAction(launcher, actions.update(bit(Button::South), true)));
        QCOMPARE(launcher->property("interactions").toInt(), 0);
        actions.update(0, true);
        QVERIFY(dispatchControllerAction(launcher, actions.update(bit(Button::East), true)));
        QCOMPARE(launcher->property("page").toString(), QStringLiteral("home"));
        QCOMPARE(findItem(launcher, QStringLiteral("openSample")), home);
        QVERIFY(home->property("activeFocus").toBool());
    }

    void keyboardBehaviorRemainsIndependent()
    {
        LauncherView view;
        QQuickItem *launcher = view.item();
        QVERIFY2(launcher, qPrintable(view.component.errorString()));
        QQuickItem *home = nullptr;
        QTRY_VERIFY((home = findItem(launcher, QStringLiteral("openSample"))) != nullptr);
        QTRY_VERIFY(home->property("activeFocus").toBool());
        QTest::keyClick(&view.window, Qt::Key_Return);
        QCOMPARE(launcher->property("page").toString(), QStringLiteral("sample"));
        QTest::keyClick(&view.window, Qt::Key_Space);
        QCOMPARE(launcher->property("interactions").toInt(), 1);
        QTest::keyClick(&view.window, Qt::Key_Escape);
        QCOMPARE(launcher->property("page").toString(), QStringLiteral("home"));
        QVERIFY(home->property("activeFocus").toBool());
    }

    void noActionOrMissingTargetDoesNotDispatch()
    {
        QVERIFY(!dispatchControllerAction(nullptr, ControllerAction::Confirm));
        QObject target;
        QVERIFY(!dispatchControllerAction(&target, ControllerAction::None));
    }
};

QTEST_MAIN(QonsoleControllerTest)
#include "tst_controller.moc"
