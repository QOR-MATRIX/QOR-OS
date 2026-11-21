// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
import QtQuick
import QtTest
import "../../apps/qonsole" as Qonsole

Item {
    id: container
    width: 960
    height: 640

    Component {
        id: launcherComponent
        Qonsole.Launcher { width: 960; height: 640 }
    }
    SignalSpy { id: quitSpy; signalName: "quitRequested" }
    TestCase {
        name: "QonsoleNavigation"
        when: windowShown
        property var launcher

        function init() {
            launcher = createTemporaryObject(launcherComponent, container);
            verify(launcher !== null);
            quitSpy.target = launcher;
            quitSpy.clear();
            tryVerify(() => findChild(launcher, "openSample").activeFocus);
        }

        function cleanup() {
            quitSpy.target = null;
        }

        function test_initial_focus() {
            compare(launcher.page, "home");
            compare(launcher.homeIndex, 0);
            verify(findChild(launcher, "openSample").activeFocus);
        }

        function test_arrows_move_focus_and_clamp() {
            keyClick(Qt.Key_Left);
            compare(launcher.homeIndex, 0);
            keyClick(Qt.Key_Right);
            compare(launcher.homeIndex, 1);
            verify(findChild(launcher, "exitLauncher").activeFocus);
            keyClick(Qt.Key_Down);
            compare(launcher.homeIndex, 1);
            keyClick(Qt.Key_Up);
            compare(launcher.homeIndex, 0);
            verify(findChild(launcher, "openSample").activeFocus);
        }

        function test_confirm_sample_and_back_restores_focus() {
            keyClick(Qt.Key_Return);
            compare(launcher.page, "sample");
            verify(findChild(launcher, "samplePage").activeFocus);
            keyClick(Qt.Key_Escape);
            compare(launcher.page, "home");
            verify(findChild(launcher, "openSample").activeFocus);
            compare(quitSpy.count, 0);
        }

        function test_sample_interaction_and_bounds() {
            keyClick(Qt.Key_Return);
            for (let i = 0; i < 4; ++i) {
                keyClick(Qt.Key_Right);
                keyClick(Qt.Key_Up);
            }
            compare(launcher.sampleX, 2);
            compare(launcher.sampleY, 0);
            keyClick(Qt.Key_Space);
            compare(launcher.interactions, 1);
            keyClick(Qt.Key_Enter);
            compare(launcher.interactions, 2);
        }

        function test_repeated_returns_keep_focus() {
            for (let i = 0; i < 5; ++i) {
                keyClick(Qt.Key_Return);
                compare(launcher.page, "sample");
                keyClick(Qt.Key_Escape);
                compare(launcher.page, "home");
                verify(findChild(launcher, "openSample").activeFocus);
            }
        }

        function test_exit_is_explicit_and_home_back_is_safe() {
            keyClick(Qt.Key_Escape);
            compare(launcher.page, "home");
            compare(quitSpy.count, 0);
            keyClick(Qt.Key_Right);
            keyClick(Qt.Key_Return);
            compare(quitSpy.count, 1);
        }

        function test_adapter_actions_and_unknown_input() {
            compare(launcher.dispatch("unmapped"), false);
            compare(launcher.homeIndex, 0);
            compare(launcher.dispatch("down"), true);
            verify(findChild(launcher, "exitLauncher").activeFocus);
            launcher.dispatch("left");
            launcher.dispatch("confirm");
            compare(launcher.page, "sample");
            compare(launcher.dispatch("unmapped"), false);
            launcher.dispatch("back");
            verify(findChild(launcher, "openSample").activeFocus);
        }
    }
}
