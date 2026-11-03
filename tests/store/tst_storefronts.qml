// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
import QtQuick
import QtTest
import QOR.Apps
import QOR.Services
import QOR.Store

// The Store's real pages (apps/StoreApp.qml, apps/SettingsApp.qml) over a catalogue tst_storefronts_qml.cpp signs: markup in every
// field, a game for players, and a pack for creators that is featured and worded to be found by a search for what the game is.
// In order: a person who only plays, then one who turns creator features on, then off again.
Item {
    id: root
    width: 1280
    height: 1000

    readonly property string gameId: "org.example.game.one"
    readonly property string packId: "org.example.pack.art"

    StoreApp {
        id: storeApp
        anchors.fill: parent
        topInset: 0
    }

    Component {
        id: settingsComponent
        SettingsApp {
            width: 900
            height: 600
            topInset: 0
            page: "store"
        }
    }

    SignalSpy {
        id: finished
        target: Storefronts
        signalName: "finished"
    }

    TestCase {
        id: testCase
        name: "Storefronts"
        when: windowShown

        function named(item, name) {
            if (!item)
                return null;
            if (item.objectName === name)
                return item;
            for (let i = 0; i < item.children.length; ++i) {
                const found = named(item.children[i], name);
                if (found)
                    return found;
            }
            return null;
        }

        function find(name) {
            return named(storeApp, name);
        }

        function collect(item, into, test) {
            if (test(item))
                into.push(item);
            for (let i = 0; i < item.children.length; ++i)
                collect(item.children[i], into, test);
        }

        // Every Text on the page holding this, shown or not, which must be plain, and there must be one.
        function textsHolding(item, marker) {
            const found = [];
            collect(item, found, candidate => candidate.textFormat !== undefined
                                 && String(candidate.text).indexOf(marker) >= 0);
            return found;
        }

        function expectPlain(item, marker) {
            const found = textsHolding(item, marker);
            verify(found.length > 0, marker + " is on no page");
            for (const text of found)
                compare(text.textFormat, Text.PlainText, marker + " in " + text);
        }

        function setCreatorFeatures(on) {
            Services.settings.creatorFeatures = on;
            tryCompare(Storefronts, "creatorFeatures", on);
        }

        function cleanup() {
            finished.clear();
            storeApp.notice = "";
        }

        // A binding that throws leaves its property without a value and the test passing. Here it fails the test.
        function init() {
            failOnWarning(/StoreApp\.qml|SettingsApp\.qml|AppRegistry\.qml/);
        }

        function initTestCase() {
            tryCompare(Storefronts, "ready", true, 10000);
            verify(Storefronts.available);
        }

        // ── A person who only plays ──────────────────────────────────────────

        function test_a_gamesIsThePlayerStore() {
            verify(!Storefronts.creatorFeatures);
            storeApp.show("games");
            compare(storeApp.sectionView, "player");
            tryVerify(() => find("storeListing:" + gameId) !== null);
            compare(find("storefrontTitle").text, "Player Store");
            verify(find("storeFeatured:" + gameId) !== null);
            verify(find("storeCategory:<b>Platformers</b>") !== null);
            // Nothing of the creator store: not in its list, its featured row or its categories.
            compare(find("storeListing:" + packId), null);
            compare(find("storeFeatured:" + packId), null);
            compare(find("storeCategory:<b>2D art</b>"), null);
            compare(find("storeCreator").visible, false);
            verify(find("storeDemonstrationNote").visible);
            for (const marker of ["<b>game name</b>", "<b>game summary</b>", "<b>Platformers</b>"])
                expectPlain(storeApp, marker);
        }

        function test_b_searchFromThePlayerStoreAsksOfItAlone() {
            storeApp.show("games");
            const search = find("storeSearch");
            search.text = "one";
            tryVerify(() => find("storeListing:" + gameId) !== null);
            compare(find("storeListing:" + packId), null);
            // What only the pack is tagged with finds nothing here, and says so.
            search.text = "zebra";
            tryVerify(() => find("storeListing:" + gameId) === null);
            compare(find("storeListing:" + packId), null);
            search.text = "";
            tryVerify(() => find("storeListing:" + gameId) !== null);
        }

        function test_c_theCreatorStoreIsRefusedBehindThePage() {
            verify(!Storefronts.install(packId));
            compare(finished.count, 1);
            compare(finished.signalArguments[0][1], false);
            verify(String(finished.signalArguments[0][2]).indexOf("creator features are off") >= 0);
            // The refusal is said, in plain words and plain text.
            verify(storeApp.notice.indexOf("creator features are off") >= 0);
            compare(find("storeNotice").textFormat, Text.PlainText);
            compare(find("storeNotice").text, storeApp.notice);
            compare(Storefronts.status(packId).installed, false);
            // Nor can it be opened by being asked for by name.
            storeApp.openListing("creator", packId);
            compare(storeApp.selectedListing.id, undefined);
            storeApp.show("creator");
            compare(storeApp.section, "discover");
            compare(storeApp.sectionView, "");
            storeApp.section = "creator";
            compare(storeApp.sectionView, "");
            compare(find("storefrontTitle"), null);
            storeApp.show("games");
        }

        function test_d_aListingShowsItsLicenceAndWhereItCameFromAsPlainText() {
            storeApp.show("games");
            tryVerify(() => find("storeListing:" + gameId) !== null);
            mouseClick(find("storeListing:" + gameId), 20, 20);
            tryVerify(() => find("storeItemName") !== null);
            compare(storeApp.selectedView, "player");
            compare(find("storeItemName").textFormat, Text.PlainText);
            compare(find("storeLicence").text, "Apache-2.0");

            for (const field of ["name", "summary", "description", "whatsNew", "origin", "author", "source", "permission"])
                expectPlain(storeApp, "<b>game " + field + "</b>");
            expectPlain(storeApp, "<b>Platformers</b>");

            // The licence's own text, from the package, and only when asked for.
            compare(find("storeLicenceText").visible, false);
            const read = find("storeReadLicence");
            verify(read.visible);
            read.clicked();
            tryVerify(() => find("storeLicenceText").visible);
            compare(read.visible, false);
            compare(find("storeLicenceText").textFormat, Text.PlainText);
            compare(find("storeLicenceText").text, "Copyright Someone <someone@example.org>.\nFree to use.\n");

            // Its picture is only the bytes the Store checked.
            const art = find("storeListingArt");
            tryVerify(() => String(art.picture).indexOf("data:image/png;base64,") === 0);
            verify(String(art.picture).indexOf("hero.png") < 0);

            find("storeBack").clicked();
            compare(storeApp.selectedId, "");
            compare(storeApp.selectedView, "");
            tryVerify(() => find("storefrontTitle") !== null);
        }

        function test_e_aPlayerGetsTheGame() {
            storeApp.show("games");
            tryVerify(() => find("cardAction:" + gameId) !== null);
            // Two cards, as it is featured too. Either will do.
            const action = find("cardAction:" + gameId);
            compare(action.text, "Get");
            mouseClick(action);
            tryCompare(finished, "count", 1, 10000);
            verify(finished.signalArguments[0][1], String(finished.signalArguments[0][2]));
            compare(Storefronts.status(gameId).installedVersion, "1.0.0");
            tryCompare(action, "text", "Installed");
            compare(action.enabled, false);
        }

        // ── A person who makes games ─────────────────────────────────────────

        function test_f_settingsHasTheSwitchAndItIsOff() {
            verify(AppRegistry.settingsPages.some(page => page.id === "store"));
            const settings = createTemporaryObject(settingsComponent, root);
            verify(settings);
            let toggle = null;
            tryVerify(() => {
                toggle = named(settings, "creatorFeaturesSwitch");
                return toggle !== null;
            });
            compare(toggle.checked, false);
            compare(Services.settings.creatorFeatures, false);
            compare(Storefronts.creatorFeatures, false);

            mouseClick(toggle);
            tryCompare(Services.settings, "creatorFeatures", true);
            tryCompare(Storefronts, "creatorFeatures", true);
            compare(find("storeCreator").visible, true);
        }

        function test_g_theCreatorStoreOpens() {
            verify(Storefronts.creatorFeatures);
            find("storeCreator").clicked();
            compare(storeApp.section, "creator");
            compare(storeApp.sectionView, "creator");
            tryVerify(() => find("storeListing:" + packId) !== null);
            compare(find("storefrontTitle").text, "Creator Store");
            verify(find("storeFeatured:" + packId) !== null);
            verify(find("storeCategory:<b>2D art</b>") !== null);
            // And nothing of the player store's.
            compare(find("storeListing:" + gameId), null);
            compare(find("storeFeatured:" + gameId), null);
            compare(find("storeCategory:<b>Platformers</b>"), null);
            for (const marker of ["<b>pack name</b>", "<b>pack summary</b>", "<b>2D art</b>"])
                expectPlain(storeApp, marker);
            // A person with creator features on still has the player store as players do.
            storeApp.show("games");
            tryVerify(() => find("storeListing:" + gameId) !== null);
            compare(find("storeListing:" + packId), null);
            compare(find("storeFeatured:" + packId), null);
        }

        function test_h_aMakerGetsThePackWithItsTextPlain() {
            storeApp.show("creator");
            tryVerify(() => find("storeListing:" + packId) !== null);
            mouseClick(find("storeListing:" + packId), 20, 20);
            tryVerify(() => find("storeItemName") !== null);
            compare(storeApp.selectedView, "creator");
            for (const field of ["summary", "description", "whatsNew", "origin", "author", "source", "permission"])
                expectPlain(storeApp, "<b>pack " + field + "</b>");
            expectPlain(storeApp, "<b>2D art</b>");
            expectPlain(storeApp, "<b>pack name</b>");

            const action = find("storeAction");
            compare(action.text, "Get");
            action.clicked();
            tryCompare(finished, "count", 1, 10000);
            verify(finished.signalArguments[0][1], String(finished.signalArguments[0][2]));
            tryCompare(action, "text", "Installed");
            compare(find("storeRemove").visible, true);
            compare(Storefronts.status(packId).installedVersion, "1.0.0");
            find("storeBack").clicked();
        }

        function test_i_theLibraryHoldsBothAndNamesThemPlainly() {
            storeApp.show("library");
            tryVerify(() => find("libraryItem:" + packId) !== null);
            verify(find("libraryItem:" + gameId) !== null);
            expectPlain(find("storefrontLibrary"), "<b>pack name</b>");
            expectPlain(find("storefrontLibrary"), "<b>game name</b>");
        }

        function test_j_turningItOffKeepsWhatIsInstalledAndItsUpdateWaits() {
            storeApp.show("creator");
            tryVerify(() => find("storeListing:" + packId) !== null);
            setCreatorFeatures(false);
            // The page goes, and the sidebar's way to it.
            compare(storeApp.section, "discover");
            compare(find("storeCreator").visible, false);
            compare(storeApp.sectionView, "");
            tryVerify(() => find("storeListing:" + packId) === null);

            // The catalogue moves on, with a newer pack.
            verify(Storefronts.hasNextCatalogue);
            Storefronts.nextCatalogue();
            tryCompare(Storefronts, "ready", true, 10000);
            compare(Storefronts.status(packId).installedVersion, "1.0.0");
            compare(Storefronts.status(packId).updateAvailable, false);
            compare(Storefronts.status(packId).updateWaits, true);

            storeApp.show("library");
            tryVerify(() => find("libraryItem:" + packId) !== null);
            verify(find("libraryVersion:" + packId).text.indexOf("waits") >= 0);
            compare(Storefronts.waitingUpdateCount, 1);
            compare(Storefronts.updateCount, 0);
            compare(find("libraryAction:" + packId).text, "Installed");
            compare(find("libraryAction:" + packId).enabled, false);

            // And a press that reached it anyway is refused from behind the page.
            verify(!Storefronts.install(packId));
            verify(String(finished.signalArguments[0][2]).indexOf("creator features are off") >= 0);
            compare(Storefronts.status(packId).installedVersion, "1.0.0");
        }

        function test_k_turnedOnAgainItUpdates() {
            storeApp.show("library");
            setCreatorFeatures(true);
            tryVerify(() => find("libraryAction:" + packId) !== null);
            const action = find("libraryAction:" + packId);
            tryCompare(action, "text", "Update");
            compare(Storefronts.updateCount, 1);
            verify(find("libraryVersion:" + packId).text.indexOf("1.0.0") >= 0);
            verify(find("libraryVersion:" + packId).text.indexOf("1.1.0") >= 0);
            action.clicked();
            tryCompare(finished, "count", 1, 10000);
            verify(finished.signalArguments[0][1], String(finished.signalArguments[0][2]));
            compare(Storefronts.status(packId).installedVersion, "1.1.0");
            compare(Storefronts.updateCount, 0);
            tryCompare(action, "text", "Installed");
        }

        function test_l_turnedOffForGoodThePackStaysAndCanBeRemoved() {
            storeApp.show("library");
            setCreatorFeatures(false);
            tryVerify(() => find("libraryItem:" + packId) !== null);
            compare(Storefronts.status(packId).installed, true);
            compare(Storefronts.listings("creator").length, 0);
            // What was fetched for it is gone with the switch.
            compare(String(Storefronts.preview(packId, 0)), "");
            find("libraryRemove:" + packId).clicked();
            compare(Storefronts.status(packId).installed, false);
            tryVerify(() => find("libraryItem:" + packId) === null);
            verify(find("libraryItem:" + gameId) !== null);
        }

        // A row that stays when the switch goes has lost nothing it may still show. The Library's rows stay: its list is the
        // same after as before.
        function test_ll_aPictureOnAPageThatStaysComesBack() {
            storeApp.show("library");
            tryVerify(() => find("libraryArt:" + gameId) !== null);
            const art = find("libraryArt:" + gameId);
            tryVerify(() => String(art.picture).length > 0);
            setCreatorFeatures(true);
            setCreatorFeatures(false);
            // The same row, and its picture forgotten as the switch went and fetched again, since a game is the player store's.
            compare(find("libraryArt:" + gameId), art);
            tryVerify(() => String(art.picture).indexOf("data:image/png;base64,") === 0);
        }

        // Decoded at the size it is drawn, both ways, whatever size the picture says it is.
        function test_lm_aPictureIsDecodedAtTheSizeItIsDrawn() {
            storeApp.show("library");
            tryVerify(() => find("libraryArt:" + gameId) !== null);
            const art = find("libraryArt:" + gameId);
            tryVerify(() => String(art.picture).length > 0);
            const image = named(art, "listingArtPicture");
            verify(image !== null);
            tryCompare(image, "status", Image.Ready);
            verify(image.width > 0 && image.height > 0);
            const ratio = Screen.devicePixelRatio;
            verify(image.sourceSize.width > 0 && image.sourceSize.width <= Math.ceil(image.width * ratio),
                   "sourceSize.width " + image.sourceSize.width + " for " + image.width);
            verify(image.sourceSize.height > 0 && image.sourceSize.height <= Math.ceil(image.height * ratio),
                   "sourceSize.height " + image.sourceSize.height + " for " + image.height);
        }

        // "Check for Updates" asks both stores, and waits for both.
        function test_ln_checkingForUpdatesWaitsForTheStorefrontsToo() {
            storeApp.show("library");
            tryVerify(() => find("storeRefresh") !== null);
            const button = find("storeRefresh");
            tryCompare(Store, "loading", false, 10000);
            tryCompare(button, "enabled", true);
            Storefronts.refresh();
            verify(Storefronts.loading);
            verify(!Store.loading);
            compare(button.enabled, false);
            compare(button.text, "Checking…");
            tryCompare(button, "enabled", true, 10000);
        }

        // Themes and widgets carry on beside it, on their own engine.
        function test_m_themesAndWidgetsAreNotListedAsStorefrontsNorTheOtherWayRound() {
            tryCompare(Store, "ready", true, 10000);
            verify(Store.items.length > 0);
            for (const item of Store.items) {
                verify(item.id !== gameId);
                verify(item.id !== packId);
            }
            compare(Store.item(gameId).id, undefined);
            compare(Storefronts.status("dev.qorsync.theme.ember").installed, false);
            compare(Storefronts.listings("player").map(item => item.id).indexOf("dev.qorsync.theme.ember"), -1);
        }
    }
}
