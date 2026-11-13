// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
import QtQuick
import QtTest
import QOR.Kit

TestCase {
    id: tests
    name: "Startup"
    when: windowShown
    width: 1280
    height: 720
    visible: true
    property bool previousMotion: false
    Component { id: component; StartupSplash {} }
    SignalSpy { id: completion; signalName: "finished" }

    function init() {
        failOnWarning(/.*/);
        previousMotion = Theme.reduceMotion;
        Theme.reduceMotion = false;
    }
    function cleanup() {
        completion.target = null;
        completion.clear();
        Theme.reduceMotion = previousMotion;
    }
    function make(properties) {
        const item = createTemporaryObject(component, tests, properties);
        verify(item !== null);
        completion.target = item;
        return item;
    }
    function test_sizes_data() {
        return [{tag: "small", width: 640, height: 480}, {tag: "hd", width: 1920, height: 1080},
                {tag: "uhd", width: 3840, height: 2160}, {tag: "portrait", width: 720, height: 1280}];
    }
    function test_sizes(row) {
        const item = make({width: row.width, height: row.height});
        const identity = findChild(item, "startupIdentity");
        const image = findChild(item, "startupMark");
        verify(identity !== null && image !== null);
        compare(identity.width, identity.height);
        verify(identity.width >= 72 && identity.width <= 160);
        compare(image.fillMode, Image.PreserveAspectFit);
        tryCompare(image, "status", Image.Ready);
        verify(item.color.r < 0.2 && item.color.g < 0.2 && item.color.b < 0.2);
        item.dismiss();
    }
    function test_readyReleasesWithoutWaitingForWatchdog() {
        const item = make({width: 1280, height: 720, timeoutMs: 1500});
        verify(item.active);
        item.ready = true;
        tryCompare(item, "settled", true, 1000);
        compare(completion.count, 1);
        verify(!item.visible);
        item.dismiss();
        compare(completion.count, 1);
    }
    function test_readyAtConstructionDoesNotReplay() {
        const item = make({width: 1280, height: 720, ready: true});
        verify(item.settled);
        verify(!item.visible);
    }
    function test_zeroExtentDoesNotDrawOutsideItsOutput() {
        const item = make({width: 0, height: 0});
        verify(!item.visible);
        item.width = 1280;
        item.height = 800;
        verify(item.visible);
        const identity = findChild(item, "startupIdentity");
        compare(identity.x + identity.width / 2, item.width / 2);
        compare(identity.y + identity.height / 2, item.height / 2);
        item.dismiss();
    }
    function test_noReadySignalFallsThrough() {
        const item = make({width: 1280, height: 720, timeoutMs: 100});
        tryCompare(item, "settled", true, 500);
        compare(completion.count, 1);
    }
    function test_reducedMotionAndTextFallback() {
        Theme.reduceMotion = true;
        const item = make({width: 1280, height: 720, graphicsEnabled: false});
        const fallback = findChild(item, "startupTextFallback");
        verify(fallback.visible);
        compare(fallback.text, "QOR OS");
        compare(fallback.textFormat, Text.PlainText);
        item.ready = true;
        verify(item.settled);
        compare(completion.count, 1);
    }
    function test_diagnosticsStayLiteralAndDismissImmediately() {
        const item = make({width: 1280, height: 720, statusText: "<img src='https://invalid.test/probe.png'>"});
        const status = findChild(item, "startupStatus");
        compare(status.text, item.statusText);
        compare(status.textFormat, Text.PlainText);
        item.dismiss();
        verify(item.settled);
        verify(!item.visible);
    }
}
