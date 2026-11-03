// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QOR.Kit
import QOR.Services
import QOR.Store

// Store: themes and widgets for QOR OS today; apps, games, tools and music
// with the developer program. Every item is signed by its developer and
// verified against the QOR OS Store's signed catalogue before it installs
// (docs/STORE.md).
//
// A demonstration build also has the two storefronts (`Storefronts`), the Player Store and the Creator Store. Every list of
// theirs here asks of one view by name, and nothing here merges them or shows them with what `Store` lists. Whatever comes
// from a catalogue is someone else's text: its Text sets textFormat: Text.PlainText.
Item {
    id: app

    property real topInset: 40
    property var launchArgs: ({})
    // discover | themes | widgets | apps | games | tools | music | creator | library
    property string section: "discover"
    property string selectedId: ""
    property string query: ""
    // The last failed install, shown as a banner.
    property string notice: ""
    // The storefront the open listing was found in, or empty when what is open is a theme or a widget.
    property string selectedView: ""
    // The storefront this section is, or empty: Games is the Player Store only in a build that has one.
    readonly property string sectionView: !Storefronts.available ? ""
                                          : section === "games" ? "player"
                                          : section === "creator" && Storefronts.creatorFeatures ? "creator" : ""
    // Empty if that listing is not in that view, or has just stopped being.
    readonly property var selectedListing: selectedView.length > 0 && Storefronts.listingsRevision >= 0
                                           ? Storefronts.item(selectedView, selectedId) : ({})

    // What the Library counts, of both engines. The second adds nothing in a build without one.
    readonly property int updates: Store.updateCount + Storefronts.updateCount
    readonly property var themeItems: Store.items.filter(item => item.kind === "theme")
    readonly property var widgetItems: Store.items.filter(item => item.kind === "widget")
    readonly property var featuredTheme: themeItems.find(item => item.featured) ?? themeItems[0] ?? null
    readonly property var selected: selectedView.length === 0 && selectedId.length > 0 && Store.items.length >= 0
                                    ? Store.item(selectedId) : null

    function open(id: string) {
        selectedView = "";
        selectedId = id;
        scroller.contentY = 0;
    }

    function openListing(view: string, id: string) {
        selectedView = view;
        selectedId = id;
        scroller.contentY = 0;
    }

    function closeItem() {
        selectedId = "";
        selectedView = "";
    }

    function show(name: string) {
        section = name === "creator" && !Storefronts.creatorFeatures ? "discover" : name;
        closeItem();
        query = "";
        search.text = "";
        scroller.contentY = 0;
    }

    onLaunchArgsChanged: {
        if (!launchArgs)
            return;
        if (launchArgs.page)
            show(launchArgs.page);
        if (launchArgs.item)
            open(launchArgs.item);
    }

    Connections {
        target: Store
        function onFinished(id: string, ok: bool, message: string) {
            if (ok)
                return;
            const item = Store.item(id);
            app.notice = qsTr("%1 could not be installed. %2").arg(item.name ?? id).arg(message);
        }
    }

    Connections {
        target: Storefronts
        function onFinished(id: string, ok: bool, message: string) {
            if (!ok)
                app.notice = qsTr("%1: %2").arg(Storefronts.status(id).name ?? id).arg(message);
        }
        // The Creator Store goes, and what was installed from it stays.
        function onCreatorFeaturesChanged() {
            if (!Storefronts.creatorFeatures && (app.section === "creator" || app.selectedView === "creator"))
                app.show(app.section === "creator" ? "discover" : app.section);
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.surface
    }

    FrostSidebar {
        id: sidebar
        width: 220
        height: parent.height
        topInset: app.topInset

        SearchField {
            id: search
            objectName: "storeSearch"
            width: 200
            placeholderText: app.sectionView === "player" ? qsTr("Search the Player Store")
                             : app.sectionView === "creator" ? qsTr("Search the Creator Store")
                             : qsTr("Search the Store")
            onTextChanged: {
                app.query = text;
                if (text.length > 0)
                    app.closeItem();
            }
        }
        Item { width: 1; height: 8 }
        SidebarItem {
            objectName: "storeDiscover"
            width: 200; icon: "sparkles"; text: qsTr("Discover")
            selected: app.section === "discover" && app.query.length === 0
            onClicked: app.show("discover")
        }
        SidebarItem {
            objectName: "storeThemes"
            width: 200; icon: "palette"; text: qsTr("Themes"); badge: String(app.themeItems.length)
            selected: app.section === "themes" && app.query.length === 0
            onClicked: app.show("themes")
        }
        SidebarItem {
            objectName: "storeWidgets"
            width: 200; icon: "layout-grid"; text: qsTr("Widgets"); badge: String(app.widgetItems.length)
            selected: app.section === "widgets" && app.query.length === 0
            onClicked: app.show("widgets")
        }
        SidebarItem {
            objectName: "storeApps"
            width: 200; icon: "app-window"; text: qsTr("Apps")
            selected: app.section === "apps" && app.query.length === 0
            onClicked: app.show("apps")
        }
        SidebarItem {
            objectName: "storeGames"
            width: 200; icon: "gamepad-2"; text: qsTr("Games")
            badge: Storefronts.available && Storefronts.listingsRevision >= 0 ? String(Storefronts.listings("player").length) : ""
            selected: app.section === "games" && app.query.length === 0
            onClicked: app.show("games")
        }
        SidebarItem {
            objectName: "storeTools"
            width: 200; icon: "wrench"; text: qsTr("Tools")
            selected: app.section === "tools" && app.query.length === 0
            onClicked: app.show("tools")
        }
        SidebarItem {
            objectName: "storeMusic"
            width: 200; icon: "music"; text: qsTr("Music")
            selected: app.section === "music" && app.query.length === 0
            onClicked: app.show("music")
        }
        // A preference's courtesy. Behind it, the engine refuses (docs/STORE.md).
        SectionHeader {
            visible: Storefronts.creatorFeatures
            text: qsTr("For creators")
        }
        SidebarItem {
            objectName: "storeCreator"
            visible: Storefronts.creatorFeatures
            width: 200; icon: "layers"; text: qsTr("Creator Store")
            badge: Storefronts.listingsRevision >= 0 ? String(Storefronts.listings("creator").length) : ""
            selected: app.section === "creator" && app.query.length === 0
            onClicked: app.show("creator")
        }
        SectionHeader { text: qsTr("You") }
        SidebarItem {
            objectName: "storeLibrary"
            width: 200; icon: "download"; text: qsTr("Library")
            badge: app.updates > 0 ? qsTr("%n update(s)", "", app.updates) : String(Store.library.length + Storefronts.library.length)
            selected: app.section === "library" && app.query.length === 0
            onClicked: app.show("library")
        }
    }

    Flickable {
        id: scroller
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: app.topInset
        contentHeight: page.height + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { }

        Column {
            id: page
            x: 32
            y: 12
            width: scroller.width - 64
            spacing: 22

            // A failed install.
            Surface {
                width: parent.width
                height: noticeText.implicitHeight + 24
                visible: app.notice.length > 0
                radius: Tokens.radius.l
                color: Oklch.alpha(Theme.critical, 0.12)
                strokeColor: Oklch.alpha(Theme.critical, 0.4)
                Text {
                    id: noticeText
                    objectName: "storeNotice"
                    x: 16
                    width: parent.width - 60
                    anchors.verticalCenter: parent.verticalCenter
                    text: app.notice
                    textFormat: Text.PlainText
                    color: Theme.textPrimary
                    font: Theme.body
                    wrapMode: Text.Wrap
                }
                IconButton {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "x"
                    onClicked: app.notice = ""
                }
            }

            // The catalogue could not be loaded.
            Card {
                width: parent.width
                visible: !Store.ready && !Store.loading
                Text {
                    width: parent.width
                    text: qsTr("The Store is unavailable")
                    color: Theme.textPrimary
                    font: Theme.headline
                }
                Text {
                    width: parent.width
                    text: Store.error
                    textFormat: Text.PlainText
                    color: Theme.textSecondary
                    font: Theme.body
                    wrapMode: Text.Wrap
                }
                Button {
                    text: qsTr("Try Again")
                    onClicked: Store.refresh()
                }
            }

            Loader {
                width: parent.width
                sourceComponent: app.selectedListing.id !== undefined ? listingPage
                               : app.selected ? itemPage
                               : app.query.length > 0 ? (app.sectionView.length > 0 ? storefrontSearchPage : searchPage)
                               : app.sectionView.length > 0 ? storefrontPage
                               : app.section === "discover" ? discoverPage
                               : app.section === "themes" ? themesPage
                               : app.section === "widgets" ? widgetsPage
                               : app.section === "apps" ? appsPage
                               : app.section === "library" ? libraryPage
                               : comingPage
            }
        }
    }

    // ── Pages ───────────────────────────────────────────────────────────────

    Component {
        id: discoverPage
        Column {
            spacing: 26

            PageTitle {
                text: qsTr("Discover")
                subtitle: qsTr("Themes and widgets for QOR OS, signed by their developers and verified by the Store.")
            }

            // The featured theme.
            Surface {
                id: hero
                objectName: "storeHero"
                width: parent.width
                height: 260
                visible: app.featuredTheme !== null
                radius: Tokens.radius.xl
                color: Theme.surfaceRaised
                strokeColor: Theme.windowStroke
                elevation: 2

                ThemePreview {
                    x: 16
                    y: 16
                    width: (parent.height - 32) * 1.6
                    height: parent.height - 32
                    theme: app.featuredTheme ? app.featuredTheme.preview : ({})
                    radius: Tokens.radius.l
                }
                Column {
                    x: (parent.height - 32) * 1.6 + 44
                    width: parent.width - x - 24
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                    Text {
                        text: qsTr("FEATURED THEME")
                        color: Theme.accentText
                        font: Theme.caption
                    }
                    Text {
                        width: parent.width
                        text: app.featuredTheme ? app.featuredTheme.name : ""
                        textFormat: Text.PlainText
                        color: Theme.textPrimary
                        font: Theme.display
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: app.featuredTheme ? app.featuredTheme.summary : ""
                        textFormat: Text.PlainText
                        color: Theme.textSecondary
                        font: Theme.callout
                        wrapMode: Text.Wrap
                    }
                    Item { width: 1; height: 6 }
                    Row {
                        spacing: 10
                        ActionButton { item: app.featuredTheme ?? ({}) }
                        Button {
                            text: qsTr("Details")
                            flat: true
                            onClicked: app.open(app.featuredTheme.id)
                        }
                    }
                }
            }

            Shelf {
                title: qsTr("Themes")
                items: app.themeItems
                onSeeAll: app.show("themes")
            }
            Shelf {
                title: qsTr("Widgets for your desktop")
                items: app.widgetItems
                onSeeAll: app.show("widgets")
            }

            // What comes next, plainly.
            Surface {
                width: parent.width
                height: program.implicitHeight + 40
                radius: Tokens.radius.xl
                color: Oklch.alpha(Theme.accent, 0.08)
                strokeColor: Oklch.alpha(Theme.accent, 0.3)
                Row {
                    id: program
                    x: 20
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 16
                    AppIcon {
                        glyph: "code-xml"
                        hue: Theme.hue
                        size: 52
                    }
                    Column {
                        width: page.width - 140
                        spacing: 4
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            text: qsTr("Build for QOR OS")
                            color: Theme.textPrimary
                            font: Theme.headline
                        }
                        Text {
                            width: parent.width
                            text: qsTr("The developer program opens with the QOR OS SDK: themes, widgets, apps and games from developers everywhere, each signed and reviewed before it reaches the Store.")
                            color: Theme.textSecondary
                            font: Theme.body
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }
    }

    Component {
        id: themesPage
        Column {
            spacing: 20
            PageTitle {
                text: qsTr("Themes")
                subtitle: qsTr("A theme sets the accent, the tint of every surface and the character of the wallpaper. You can still change the accent and appearance afterwards.")
            }
            ItemGrid { items: app.themeItems }
        }
    }

    Component {
        id: widgetsPage
        Column {
            spacing: 20
            PageTitle {
                text: qsTr("Widgets")
                subtitle: qsTr("Glanceable information on your desktop. Add them from here, or right-click the desktop and choose Edit Widgets.")
            }
            ItemGrid { items: app.widgetItems }
        }
    }

    Component {
        id: searchPage
        Column {
            id: searchColumn
            readonly property var results: Store.items.length >= 0 ? Store.search(app.query) : []
            spacing: 20
            PageTitle {
                text: qsTr("Results for “%1”").arg(app.query)
                subtitle: searchColumn.results.length === 0
                          ? qsTr("Nothing matches. Try a colour, a mood or a kind of widget.")
                          : qsTr("%n item(s)", "", searchColumn.results.length)
            }
            ItemGrid { items: searchColumn.results }
        }
    }

    Component {
        id: appsPage
        Column {
            spacing: 20
            PageTitle {
                text: qsTr("Apps")
                subtitle: qsTr("Apps from other developers arrive with the developer program, each running in its own sandbox. These come with QOR OS.")
            }
            Card {
                width: parent.width
                padding: 0
                spacing: 0
                Repeater {
                    model: AppRegistry.apps.filter(a => a.id !== "store")
                    delegate: Item {
                        id: row
                        required property var modelData
                        required property int index
                        width: parent.width
                        height: 68
                        AppIcon {
                            x: 16
                            anchors.verticalCenter: parent.verticalCenter
                            glyph: row.modelData.glyph
                            hue: row.modelData.hue
                            chroma: row.modelData.chroma
                            size: 40
                        }
                        Column {
                            x: 70
                            width: parent.width - 180
                            anchors.verticalCenter: parent.verticalCenter
                            Text {
                                text: row.modelData.name
                                textFormat: Text.PlainText
                                color: Theme.textPrimary
                                font: Theme.bodyStrong
                            }
                            Text {
                                width: parent.width
                                text: row.modelData.description
                                color: Theme.textSecondary
                                font: Theme.footnote
                                elide: Text.ElideRight
                            }
                        }
                        Button {
                            anchors.right: parent.right
                            anchors.rightMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Open")
                            onClicked: Intents.openApp(row.modelData.id)
                        }
                        Divider {
                            visible: row.index > 0
                            x: 70
                            width: parent.width - 70
                        }
                    }
                }
            }
        }
    }

    Component {
        id: comingPage
        Column {
            id: coming
            readonly property var what: ({
                games: { icon: "gamepad-2", title: qsTr("Games are on their way"),
                         text: qsTr("Games arrive with the developer program and the app sandbox, controller support included.") },
                tools: { icon: "wrench", title: qsTr("Tools are on their way"),
                         text: qsTr("Utilities and developer tools arrive with the developer program and the app sandbox.") },
                music: { icon: "music", title: qsTr("Music is on its way"),
                         text: qsTr("Sound packs and music from the artists who own them arrive with the developer program.") }
            })[app.section] ?? { icon: "sparkles", title: "", text: "" }
            spacing: 14
            topPadding: 80
            Icon {
                anchors.horizontalCenter: parent.horizontalCenter
                name: coming.what.icon
                size: 44
                color: Theme.textTertiary
            }
            Text {
                objectName: "storeComingTitle"
                anchors.horizontalCenter: parent.horizontalCenter
                text: coming.what.title
                color: Theme.textPrimary
                font: Theme.title2
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(460, page.width)
                horizontalAlignment: Text.AlignHCenter
                text: coming.what.text
                color: Theme.textSecondary
                font: Theme.body
                wrapMode: Text.Wrap
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Browse Themes")
                onClicked: app.show("themes")
            }
        }
    }

    Component {
        id: libraryPage
        Column {
            spacing: 20
            Item {
                width: parent.width
                height: libraryTitle.height
                PageTitle {
                    id: libraryTitle
                    width: parent.width - 200
                    text: qsTr("Library")
                    subtitle: Store.library.length + Storefronts.library.length === 0 ? qsTr("What you get from the Store appears here.")
                              : app.updates > 0 ? qsTr("%n update(s) available", "", app.updates)
                              : Storefronts.waitingUpdateCount > 0 ? qsTr("%n update(s) waiting until creator features are on", "", Storefronts.waitingUpdateCount)
                              : qsTr("Everything is up to date.")
                }
                Button {
                    objectName: "storeRefresh"
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    text: Store.loading ? qsTr("Checking…") : qsTr("Check for Updates")
                    enabled: !Store.loading
                    onClicked: {
                        Store.refresh();
                        Storefronts.refresh();
                    }
                }
            }
            Card {
                width: parent.width
                visible: Store.library.length > 0
                padding: 0
                spacing: 0
                Repeater {
                    model: Store.library
                    delegate: Item {
                        id: entry
                        required property var modelData
                        required property int index
                        objectName: "libraryItem:" + modelData.id
                        width: parent.width
                        height: 72
                        Component {
                            id: smallTheme
                            ThemePreview {
                                theme: entry.modelData.preview ?? ({})
                                radius: 8
                            }
                        }
                        Component {
                            id: smallWidget
                            WidgetTile {
                                item: entry.modelData
                                compact: true
                            }
                        }
                        Loader {
                            x: 14
                            width: 72
                            height: 46
                            anchors.verticalCenter: parent.verticalCenter
                            sourceComponent: entry.modelData.kind === "theme" ? smallTheme : smallWidget
                        }
                        Column {
                            x: 102
                            width: parent.width - 102 - actions.width - 24
                            anchors.verticalCenter: parent.verticalCenter
                            Text {
                                width: parent.width
                                text: entry.modelData.name
                                textFormat: Text.PlainText
                                color: Theme.textPrimary
                                font: Theme.bodyStrong
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: (entry.modelData.kind === "theme" ? qsTr("Theme") : qsTr("Widget")) + " · "
                                      + (entry.modelData.updateAvailable
                                         ? qsTr("%1 → %2").arg(entry.modelData.installedVersion).arg(entry.modelData.version)
                                         : entry.modelData.installedVersion)
                                color: Theme.textSecondary
                                font: Theme.footnote
                            }
                        }
                        Row {
                            id: actions
                            anchors.right: parent.right
                            anchors.rightMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8
                            ActionButton {
                                objectName: "libraryAction:" + entry.modelData.id
                                item: entry.modelData
                            }
                            Button {
                                objectName: "libraryRemove:" + entry.modelData.id
                                text: qsTr("Remove")
                                flat: true
                                enabled: !entry.modelData.busy
                                onClicked: Store.remove(entry.modelData.id)
                            }
                        }
                        Divider {
                            visible: entry.index > 0
                            x: 102
                            width: parent.width - 102
                        }
                    }
                }
            }

            // What was installed from the storefronts, whichever store it came from and whether or not that store can be
            // seen now: turning creator features off removes nothing.
            DemonstrationNote { visible: Storefronts.available }
            Card {
                objectName: "storefrontLibrary"
                width: parent.width
                visible: Storefronts.library.length > 0
                padding: 0
                spacing: 0
                Repeater {
                    model: Storefronts.library
                    delegate: Item {
                        id: installedEntry
                        required property var modelData
                        required property int index
                        readonly property var status: Storefronts.statusRevision >= 0 ? Storefronts.status(modelData.id) : ({})
                        objectName: "libraryItem:" + modelData.id
                        width: parent.width
                        height: 72
                        ListingArt {
                            objectName: "libraryArt:" + installedEntry.modelData.id
                            x: 14
                            width: 72
                            height: 46
                            anchors.verticalCenter: parent.verticalCenter
                            item: installedEntry.modelData
                            compact: true
                        }
                        Column {
                            x: 102
                            width: parent.width - 102 - installedActions.width - 24
                            anchors.verticalCenter: parent.verticalCenter
                            Text {
                                width: parent.width
                                text: installedEntry.modelData.name
                                textFormat: Text.PlainText
                                color: Theme.textPrimary
                                font: Theme.bodyStrong
                                elide: Text.ElideRight
                            }
                            Text {
                                objectName: "libraryVersion:" + installedEntry.modelData.id
                                width: parent.width
                                text: app.kindName(installedEntry.modelData.kind) + " · "
                                      + (installedEntry.status.updateAvailable
                                         ? qsTr("%1 → %2").arg(installedEntry.status.installedVersion).arg(installedEntry.status.version)
                                         : installedEntry.status.updateWaits
                                           ? qsTr("%1. Its update waits until creator features are on.").arg(installedEntry.status.installedVersion)
                                           : installedEntry.status.installedVersion)
                                textFormat: Text.PlainText
                                color: Theme.textSecondary
                                font: Theme.footnote
                                elide: Text.ElideRight
                            }
                        }
                        Row {
                            id: installedActions
                            anchors.right: parent.right
                            anchors.rightMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8
                            ListingAction {
                                objectName: "libraryAction:" + installedEntry.modelData.id
                                itemId: installedEntry.modelData.id
                            }
                            Button {
                                objectName: "libraryRemove:" + installedEntry.modelData.id
                                text: qsTr("Remove")
                                flat: true
                                enabled: !installedEntry.status.busy
                                onClicked: Storefronts.remove(installedEntry.modelData.id)
                            }
                        }
                        Divider {
                            visible: installedEntry.index > 0
                            x: 102
                            width: parent.width - 102
                        }
                    }
                }
            }
        }
    }

    // ── The storefronts ─────────────────────────────────────────────────────

    // One storefront, of the view this section is. Its search, its featured row and its counts all ask of that view alone.
    Component {
        id: storefrontPage
        Column {
            id: front
            readonly property string view: app.sectionView
            // The category shown, or empty for all of them.
            property string category: ""
            readonly property var counts: Storefronts.listingsRevision >= 0 ? Storefronts.categoryCounts(view) : ({})
            readonly property var featured: Storefronts.listingsRevision >= 0 ? Storefronts.featured(view) : []
            readonly property var listings: Storefronts.listingsRevision >= 0 ? Storefronts.listings(view) : []
            readonly property var shown: category.length === 0 ? listings
                                         : listings.filter(listing => listing.categories.indexOf(category) >= 0)
            onViewChanged: category = ""
            spacing: 20

            PageTitle {
                objectName: "storefrontTitle"
                text: front.view === "player" ? qsTr("Player Store") : qsTr("Creator Store")
                subtitle: front.view === "player"
                          ? qsTr("Finished games, each signed by its maker and verified by the Store.")
                          : qsTr("Assets for making games, with what they may be used for and where they came from.")
            }
            DemonstrationNote { }

            Flow {
                width: parent.width
                spacing: 8
                visible: front.listings.length > 0
                CategoryChip {
                    objectName: "storeCategoryAll"
                    text: qsTr("All") + " · " + front.listings.length
                    selected: front.category.length === 0
                    onClicked: front.category = ""
                }
                Repeater {
                    model: Object.keys(front.counts)
                    delegate: CategoryChip {
                        required property string modelData
                        objectName: "storeCategory:" + modelData
                        text: modelData + " · " + front.counts[modelData]
                        selected: front.category === modelData
                        onClicked: front.category = modelData
                    }
                }
            }

            SectionTitle {
                visible: front.featured.length > 0
                text: qsTr("Featured")
            }
            Flow {
                objectName: "storefrontFeatured"
                width: parent.width
                spacing: 18
                visible: front.featured.length > 0
                Repeater {
                    model: front.featured
                    delegate: ListingCard {
                        required property var modelData
                        objectName: "storeFeatured:" + modelData.id
                        view: front.view
                        item: modelData
                    }
                }
            }

            SectionTitle {
                visible: front.featured.length > 0
                text: qsTr("Everything")
            }
            ListingGrid {
                view: front.view
                items: front.shown
            }
            Text {
                visible: Storefronts.ready && front.listings.length === 0
                width: parent.width
                text: qsTr("Nothing is listed here yet.")
                color: Theme.textSecondary
                font: Theme.body
            }
        }
    }

    Component {
        id: storefrontSearchPage
        Column {
            id: frontSearch
            readonly property string view: app.sectionView
            readonly property var results: Storefronts.listingsRevision >= 0 ? Storefronts.search(view, app.query) : []
            spacing: 20
            PageTitle {
                text: qsTr("Results for “%1”").arg(app.query)
                subtitle: frontSearch.results.length === 0
                          ? (frontSearch.view === "player" ? qsTr("Nothing in the Player Store matches.")
                                                           : qsTr("Nothing in the Creator Store matches."))
                          : qsTr("%n item(s)", "", frontSearch.results.length)
            }
            ListingGrid {
                view: frontSearch.view
                items: frontSearch.results
            }
        }
    }

    // A listing: what it is, what it may be used for, and where it came from, all of which the signed catalogue says.
    Component {
        id: listingPage
        Column {
            id: listing
            readonly property var item: app.selectedListing
            readonly property var status: Storefronts.statusRevision >= 0 ? Storefronts.status(item.id) : ({})
            readonly property var licence: Storefronts.licenceRevision >= 0 ? Storefronts.licence(item.id) : ({})
            readonly property var provenance: item.provenance ?? ({})
            readonly property real artWidth: Math.min(420, width * 0.5)
            spacing: 24

            Button {
                objectName: "storeBack"
                flat: true
                text: "‹  " + (app.query.length > 0 ? qsTr("Results")
                               : app.selectedView === "player" ? qsTr("Player Store") : qsTr("Creator Store"))
                onClicked: app.closeItem()
            }

            Row {
                width: parent.width
                spacing: 28
                ListingArt {
                    objectName: "storeListingArt"
                    width: listing.artWidth
                    height: width * 0.62
                    item: listing.item
                }
                Column {
                    width: parent.width - listing.artWidth - 28
                    spacing: 8
                    Text {
                        objectName: "storeItemName"
                        width: parent.width
                        text: listing.item.name ?? ""
                        textFormat: Text.PlainText
                        color: Theme.textPrimary
                        font: Theme.title1
                        wrapMode: Text.Wrap
                    }
                    Row {
                        spacing: 6
                        Text {
                            text: listing.item.developerName ?? ""
                            textFormat: Text.PlainText
                            color: Theme.accentText
                            font: Theme.body
                        }
                        Icon {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: listing.item.verified === true
                            name: "badge-check"
                            size: 14
                            color: Theme.accentText
                        }
                    }
                    Text {
                        width: parent.width
                        text: listing.item.summary ?? ""
                        textFormat: Text.PlainText
                        color: Theme.textSecondary
                        font: Theme.callout
                        wrapMode: Text.Wrap
                    }
                    Item { width: 1; height: 10 }
                    Row {
                        spacing: 10
                        ListingAction {
                            objectName: "storeAction"
                            itemId: listing.item.id ?? ""
                        }
                        Button {
                            objectName: "storeRemove"
                            visible: listing.status.installed === true
                            text: qsTr("Remove")
                            flat: true
                            enabled: !listing.status.busy
                            onClicked: Storefronts.remove(listing.item.id)
                        }
                    }
                    ProgressBar {
                        visible: listing.status.busy === true
                        width: Math.min(parent.width, 240)
                        value: listing.status.progress ?? 0
                    }
                    Text {
                        text: listing.status.installed === true ? qsTr("Installed: %1").arg(listing.status.installedVersion)
                                                                : qsTr("Free")
                        textFormat: Text.PlainText
                        color: Theme.textTertiary
                        font: Theme.footnote
                    }
                }
            }

            SectionTitle { text: qsTr("About") }
            Text {
                objectName: "storeDescription"
                width: Math.min(parent.width, 680)
                text: listing.item.description ?? ""
                textFormat: Text.PlainText
                color: Theme.textPrimary
                font: Theme.body
                wrapMode: Text.Wrap
                lineHeight: 1.25
            }

            SectionTitle { text: qsTr("What's New") }
            Text {
                width: Math.min(parent.width, 680)
                text: qsTr("Version %1").arg(listing.item.version) + "  ·  " + (listing.item.whatsNew ?? "")
                textFormat: Text.PlainText
                color: Theme.textSecondary
                font: Theme.body
                wrapMode: Text.Wrap
            }

            SectionTitle { text: qsTr("Licence") }
            Text {
                objectName: "storeLicence"
                width: Math.min(parent.width, 680)
                text: listing.item.licence ? listing.item.licence.spdx : ""
                textFormat: Text.PlainText
                color: Theme.textPrimary
                font: Theme.bodyStrong
                wrapMode: Text.Wrap
            }
            Button {
                objectName: "storeReadLicence"
                visible: listing.licence.text === undefined
                text: listing.licence.asked === true && listing.licence.error === undefined
                      ? qsTr("Fetching…") : qsTr("Read the Licence")
                enabled: !(listing.licence.asked === true && listing.licence.error === undefined)
                onClicked: Storefronts.fetchLicence(listing.item.id)
            }
            Text {
                objectName: "storeLicenceRefused"
                width: Math.min(parent.width, 680)
                visible: listing.licence.error !== undefined
                text: listing.licence.error ?? ""
                textFormat: Text.PlainText
                color: Theme.critical
                font: Theme.body
                wrapMode: Text.Wrap
            }
            Text {
                objectName: "storeLicenceText"
                width: Math.min(parent.width, 680)
                visible: listing.licence.text !== undefined
                text: listing.licence.text ?? ""
                textFormat: Text.PlainText
                color: Theme.textSecondary
                font: Theme.footnote
                wrapMode: Text.Wrap
            }

            SectionTitle { text: qsTr("Where it came from") }
            FactGrid {
                objectName: "storeProvenance"
                facts: [].concat(
                    [qsTr("Origin"), listing.provenance.origin],
                    [qsTr("Author"), listing.provenance.author],
                    [qsTr("Source"), listing.provenance.source],
                    [qsTr("Permission"), listing.provenance.permission])
            }

            SectionTitle { text: qsTr("Information") }
            FactGrid {
                objectName: "storeInformation"
                facts: [].concat(
                    [qsTr("Developer"), listing.item.developerName],
                    [qsTr("Kind"), app.kindName(listing.item.kind)],
                    [qsTr("Category"), (listing.item.categories ?? []).join(", ")],
                    [qsTr("Version"), listing.item.version],
                    [qsTr("Download"), app.sizeText(listing.item.size)],
                    [qsTr("Unpacked"), app.sizeText(listing.item.unpacked)],
                    [qsTr("Released"), listing.item.released],
                    [qsTr("Platforms"), (listing.item.platforms ?? []).join(", ")],
                    [qsTr("Made for"), (listing.item.engines ?? []).map(range => qsTr("%1 %2 to below %3").arg(range.id).arg(range.min).arg(range.below)).join(", ")],
                    [qsTr("Price"), qsTr("Free")])
            }

            Row {
                spacing: 10
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "shield-check"
                    size: 18
                    color: Theme.positive
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(page.width - 40, 640)
                    text: qsTr("Signed by %1 and verified against the signed catalogue before it installs.")
                          .arg(listing.item.developerName)
                    textFormat: Text.PlainText
                    color: Theme.textSecondary
                    font: Theme.footnote
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    Component {
        id: itemPage
        Column {
            id: details
            readonly property bool isTheme: app.selected.kind === "theme"
            readonly property real artWidth: Math.min(420, width * 0.5)
            spacing: 24

            Component {
                id: bigTheme
                ThemePreview {
                    theme: app.selected.preview ?? ({})
                    radius: Tokens.radius.xl
                }
            }
            Component {
                id: bigWidget
                Item {
                    id: widgetArt
                    // Installed widgets show themselves; others their tile.
                    readonly property var live: Store.widgets.length >= 0 ? Store.widget(app.selectedId) : ({})
                    readonly property bool isLive: live.source !== undefined
                    WidgetTile {
                        anchors.fill: parent
                        visible: !widgetArt.isLive
                        item: app.selected
                    }
                    Glass {
                        visible: widgetArt.isLive
                        anchors.centerIn: parent
                        width: widgetArt.live.size === "small" ? 152 : 320
                        height: widgetArt.live.size === "large" ? 320 : 152
                        scale: Math.min(1, widgetArt.height / height)
                        backdrop: Materials.desktopFor(Window.window)
                        radius: Tokens.radius.xl
                        Loader {
                            anchors.fill: parent
                            source: widgetArt.isLive ? widgetArt.live.source : ""
                        }
                    }
                }
            }

            Button {
                objectName: "storeBack"
                flat: true
                text: "‹  " + (app.query.length > 0 ? qsTr("Results")
                               : app.section === "discover" ? qsTr("Discover")
                               : app.section === "library" ? qsTr("Library")
                               : details.isTheme ? qsTr("Themes") : qsTr("Widgets"))
                onClicked: app.selectedId = ""
            }

            Row {
                width: parent.width
                spacing: 28

                Loader {
                    width: details.artWidth
                    height: width * 0.62
                    sourceComponent: details.isTheme ? bigTheme : bigWidget
                }

                Column {
                    width: parent.width - details.artWidth - 28
                    spacing: 8
                    Text {
                        objectName: "storeItemName"
                        width: parent.width
                        text: app.selected.name
                        textFormat: Text.PlainText
                        color: Theme.textPrimary
                        font: Theme.title1
                        wrapMode: Text.Wrap
                    }
                    Row {
                        spacing: 6
                        Text {
                            text: app.selected.developerName
                            textFormat: Text.PlainText
                            color: Theme.accentText
                            font: Theme.body
                        }
                        Icon {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: app.selected.verified === true
                            name: "badge-check"
                            size: 14
                            color: Theme.accentText
                        }
                    }
                    Text {
                        width: parent.width
                        text: app.selected.summary
                        textFormat: Text.PlainText
                        color: Theme.textSecondary
                        font: Theme.callout
                        wrapMode: Text.Wrap
                    }
                    Item { width: 1; height: 10 }
                    Row {
                        spacing: 10
                        ActionButton {
                            objectName: "storeAction"
                            item: app.selected
                        }
                        Button {
                            objectName: "storeRemove"
                            visible: app.selected.installed === true
                            text: qsTr("Remove")
                            flat: true
                            enabled: !app.selected.busy
                            onClicked: Store.remove(app.selectedId)
                        }
                    }
                    Text {
                        visible: app.selected.price === 0
                        text: qsTr("Free")
                        color: Theme.textTertiary
                        font: Theme.footnote
                    }
                }
            }

            SectionTitle { text: qsTr("About") }
            Text {
                width: Math.min(parent.width, 680)
                text: app.selected.description ?? ""
                textFormat: Text.PlainText
                color: Theme.textPrimary
                font: Theme.body
                wrapMode: Text.Wrap
                lineHeight: 1.25
            }

            SectionTitle { text: qsTr("What's New") }
            Text {
                width: Math.min(parent.width, 680)
                text: qsTr("Version %1").arg(app.selected.version) + "  ·  " + (app.selected.whatsNew ?? "")
                textFormat: Text.PlainText
                color: Theme.textSecondary
                font: Theme.body
                wrapMode: Text.Wrap
            }

            SectionTitle { text: qsTr("Information") }
            Grid {
                columns: 2
                columnSpacing: 40
                rowSpacing: 10
                Repeater {
                    model: [].concat(
                        [qsTr("Developer"), app.selected.developerName],
                        [qsTr("Kind"), app.selected.kind === "theme" ? qsTr("Theme")
                            : qsTr("Widget · %1").arg(app.selected.preview && app.selected.preview.size === "small" ? qsTr("small")
                                                     : app.selected.preview && app.selected.preview.size === "large" ? qsTr("large") : qsTr("medium"))],
                        [qsTr("Version"), app.selected.version],
                        [qsTr("Size"), qsTr("%1 KB").arg(Math.max(1, Math.round((app.selected.size ?? 0) / 1024)))],
                        [qsTr("Released"), app.selected.released ?? ""],
                        [qsTr("Price"), app.selected.price === 0 ? qsTr("Free") : "$" + app.selected.price])
                    delegate: Text {
                        required property var modelData
                        required property int index
                        text: modelData ?? ""
                        textFormat: Text.PlainText
                        color: index % 2 === 0 ? Theme.textTertiary : Theme.textPrimary
                        font: Theme.body
                    }
                }
            }

            Row {
                spacing: 10
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "shield-check"
                    size: 18
                    color: Theme.positive
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(page.width - 40, 640)
                    text: qsTr("Signed by %1 and verified against the QOR OS Store's signed catalogue before it installs.")
                          .arg(app.selected.developerName)
                    textFormat: Text.PlainText
                    color: Theme.textSecondary
                    font: Theme.footnote
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    // ── Pieces ──────────────────────────────────────────────────────────────

    function kindName(kind: string): string {
        return kind === "game" ? qsTr("Game") : kind === "asset-pack" ? qsTr("Asset pack") : kind;
    }

    function sizeText(bytes: var): string {
        return qsTr("%1 KB").arg(Math.max(1, Math.round((bytes ?? 0) / 1024)));
    }

    // What a search types, and a catalogue's name, reach these.
    component PageTitle: Column {
        property alias text: heading.text
        property alias subtitle: sub.text
        width: parent ? parent.width : 400
        spacing: 4
        Text {
            id: heading
            textFormat: Text.PlainText
            color: Theme.textPrimary
            font: Theme.title1
        }
        Text {
            id: sub
            width: parent.width
            visible: text.length > 0
            textFormat: Text.PlainText
            color: Theme.textSecondary
            font: Theme.body
            wrapMode: Text.Wrap
        }
    }

    component SectionTitle: Text {
        color: Theme.textPrimary
        font: Theme.title3
    }

    // A horizontal row of items with "See All".
    component Shelf: Column {
        id: shelf
        property string title
        property var items: []
        signal seeAll()
        width: parent ? parent.width : 400
        spacing: 12
        Item {
            width: parent.width
            height: shelfTitle.height
            SectionTitle {
                id: shelfTitle
                text: shelf.title
            }
            Button {
                anchors.right: parent.right
                anchors.verticalCenter: shelfTitle.verticalCenter
                flat: true
                text: qsTr("See All")
                onClicked: shelf.seeAll()
            }
        }
        ListView {
            width: parent.width
            height: 238
            orientation: ListView.Horizontal
            spacing: 18
            clip: true
            model: shelf.items
            delegate: ItemCard {
                required property var modelData
                item: modelData
            }
        }
    }

    component ItemGrid: Flow {
        id: grid
        property var items: []
        width: parent ? parent.width : 400
        spacing: 18
        Repeater {
            model: grid.items
            delegate: ItemCard {
                required property var modelData
                item: modelData
            }
        }
    }

    component ItemCard: Item {
        id: card
        property var item: ({})
        objectName: "storeItem:" + (item.id ?? "")
        width: 240
        height: 238

        Component {
            id: cardTheme
            ThemePreview { theme: card.item.preview ?? ({}) }
        }
        Component {
            id: cardWidget
            WidgetTile { item: card.item }
        }
        Loader {
            id: art
            width: parent.width
            height: 150
            sourceComponent: card.item.kind === "theme" ? cardTheme : cardWidget
            scale: hover.hovered ? 1.02 : 1
            Behavior on scale {
                enabled: !Theme.reduceMotion
                NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
            }
        }
        Column {
            y: art.height + 10
            width: parent.width - actionButton.width - 8
            Text {
                width: parent.width
                text: card.item.name ?? ""
                textFormat: Text.PlainText
                color: Theme.textPrimary
                font: Theme.bodyStrong
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: card.item.summary ?? ""
                textFormat: Text.PlainText
                color: Theme.textSecondary
                font: Theme.footnote
                elide: Text.ElideRight
            }
            Text {
                text: card.item.developerName ?? ""
                textFormat: Text.PlainText
                color: Theme.textTertiary
                font: Theme.caption
            }
        }
        ActionButton {
            id: actionButton
            objectName: "cardAction:" + (card.item.id ?? "")
            anchors.right: parent.right
            y: art.height + 12
            item: card.item
            compact: true
        }
        HoverHandler { id: hover }
        TapHandler {
            // Opening the page; the button handles its own taps.
            onTapped: eventPoint => {
                if (!actionButton.contains(actionButton.mapFromItem(card, eventPoint.position)))
                    app.open(card.item.id);
            }
        }
    }

    // Says plainly what this is, on every page of a storefront: nothing in it is a real store's.
    component DemonstrationNote: Surface {
        id: note
        width: parent ? parent.width : 400
        height: Math.max(noteText.implicitHeight, nextCatalogue.height) + 24
        radius: Tokens.radius.l
        color: Oklch.alpha(Theme.accent, 0.08)
        strokeColor: Oklch.alpha(Theme.accent, 0.3)
        Text {
            id: noteText
            objectName: "storeDemonstrationNote"
            x: 16
            width: parent.width - 32 - (nextCatalogue.visible ? nextCatalogue.width + 12 : 0)
            anchors.verticalCenter: parent.verticalCenter
            text: Storefronts.ready
                  ? qsTr("A demonstration: “%1”, signed with a throwaway key and held only by this development build. Nothing here is a real store's.").arg(Storefronts.name)
                  : Storefronts.loading ? qsTr("Reading the demonstration's catalogue…")
                  : qsTr("The demonstration's catalogue was refused. %1").arg(Storefronts.error)
            textFormat: Text.PlainText
            color: Storefronts.ready || Storefronts.loading ? Theme.textPrimary : Theme.critical
            font: Theme.footnote
            wrapMode: Text.Wrap
        }
        Button {
            id: nextCatalogue
            objectName: "storeNextCatalogue"
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            visible: Storefronts.hasNextCatalogue
            text: qsTr("Next Catalogue")
            onClicked: Storefronts.nextCatalogue()
        }
    }

    component CategoryChip: Surface {
        id: chip
        property string text
        property bool selected: false
        signal clicked()
        width: chipLabel.implicitWidth + 24
        height: 28
        radius: 14
        exponent: 2
        color: selected ? Theme.accent : chipHover.hovered ? Theme.fillControlHover : Theme.fillControl
        strokeColor: selected ? "transparent" : Theme.windowStroke
        Accessible.role: Accessible.Button
        Accessible.name: text
        Text {
            id: chipLabel
            anchors.centerIn: parent
            text: chip.text
            textFormat: Text.PlainText
            font: Theme.footnote
            color: chip.selected ? Theme.onAccent : Theme.textPrimary
        }
        HoverHandler { id: chipHover }
        TapHandler { onTapped: chip.clicked() }
    }

    // Pairs of a label and a value, of which only the value is the catalogue's. One with nothing to say is left out.
    component FactGrid: Grid {
        id: factGrid
        property var facts: []
        readonly property var said: {
            const pairs = [];
            for (let i = 0; i + 1 < facts.length; i += 2) {
                if (facts[i + 1])
                    pairs.push(facts[i], facts[i + 1]);
            }
            return pairs;
        }
        columns: 2
        columnSpacing: 40
        rowSpacing: 10
        Repeater {
            model: factGrid.said
            delegate: Text {
                required property var modelData
                required property int index
                objectName: factGrid.objectName + ":" + index
                width: index % 2 === 0 ? implicitWidth : Math.min(implicitWidth, 520)
                text: modelData ?? ""
                textFormat: Text.PlainText
                color: index % 2 === 0 ? Theme.textTertiary : Theme.textPrimary
                font: Theme.body
                wrapMode: Text.Wrap
            }
        }
    }

    component ListingGrid: Flow {
        id: listingGrid
        property string view
        property var items: []
        width: parent ? parent.width : 400
        spacing: 18
        Repeater {
            model: listingGrid.items
            delegate: ListingCard {
                required property var modelData
                objectName: "storeListing:" + modelData.id
                view: listingGrid.view
                item: modelData
            }
        }
    }

    component ListingCard: Item {
        id: listingCard
        property string view
        property var item: ({})
        width: 240
        height: 238

        ListingArt {
            id: listingCardArt
            width: parent.width
            height: 150
            item: listingCard.item
        }
        Column {
            y: listingCardArt.height + 10
            width: parent.width - listingCardAction.width - 8
            Text {
                width: parent.width
                text: listingCard.item.name ?? ""
                textFormat: Text.PlainText
                color: Theme.textPrimary
                font: Theme.bodyStrong
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: listingCard.item.summary ?? ""
                textFormat: Text.PlainText
                color: Theme.textSecondary
                font: Theme.footnote
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: listingCard.item.developerName ?? ""
                textFormat: Text.PlainText
                color: Theme.textTertiary
                font: Theme.caption
                elide: Text.ElideRight
            }
        }
        ListingAction {
            id: listingCardAction
            objectName: "cardAction:" + (listingCard.item.id ?? "")
            anchors.right: parent.right
            y: listingCardArt.height + 12
            itemId: listingCard.item.id ?? ""
        }
        TapHandler {
            // Opening the page; the button handles its own taps.
            onTapped: eventPoint => {
                if (!listingCardAction.contains(listingCardAction.mapFromItem(listingCard, eventPoint.position)))
                    app.openListing(listingCard.view, listingCard.item.id);
            }
        }
    }

    // A listing's first preview once the Store has checked it, or until then, or if it was refused, its kind's glyph. The
    // picture is only ever the bytes the Store checked: nothing here has a path a catalogue chose.
    component ListingArt: Surface {
        id: listingArt
        property var item: ({})
        property bool compact: false
        readonly property url picture: item.id !== undefined && Storefronts.previewRevision >= 0
                                       ? Storefronts.preview(item.id, 0) : ""
        function fetch() {
            if (item.id !== undefined && (item.previewCount ?? 0) > 0)
                Storefronts.fetchPreview(item.id, 0);
        }
        onItemChanged: fetch()
        Component.onCompleted: fetch()
        Connections {
            target: Storefronts
            function onPreviewsForgotten() {
                listingArt.fetch();
            }
        }
        radius: compact ? 8 : Tokens.radius.l
        color: Theme.surfaceRaised
        strokeColor: Theme.windowStroke
        Icon {
            anchors.centerIn: parent
            visible: artPicture.status !== Image.Ready
            name: listingArt.item.kind === "game" ? "gamepad-2" : "images"
            size: listingArt.compact ? 18 : 46
            color: Theme.textTertiary
        }
        Image {
            id: artPicture
            anchors.fill: parent
            anchors.margins: listingArt.compact ? 4 : 12
            source: listingArt.picture
            asynchronous: true
            cache: false
            fillMode: Image.PreserveAspectFit
            sourceSize.width: 840
        }
    }

    // Get, Installing…, Update or Installed. A press that is refused says why, through `finished`.
    component ListingAction: Button {
        property string itemId
        readonly property var status: Storefronts.statusRevision >= 0 && itemId.length > 0 ? Storefronts.status(itemId) : ({})
        text: status.busy === true ? (status.progress > 0 ? qsTr("Installing… %1%").arg(Math.round(status.progress * 100)) : qsTr("Installing…"))
              : status.installed !== true ? qsTr("Get")
              : status.updateAvailable === true ? qsTr("Update")
              : qsTr("Installed")
        highlighted: status.installed !== true || status.updateAvailable === true
        enabled: itemId.length > 0 && status.busy !== true && (status.installed !== true || status.updateAvailable === true)
        onClicked: Storefronts.install(itemId)
    }

    // A widget's tile until it is installed: its glyph on its hue.
    component WidgetTile: Surface {
        id: tile
        property var item: ({})
        property bool compact: false
        readonly property real hue: item.preview ? item.preview.hue : 274
        radius: compact ? 8 : Tokens.radius.l
        color: Oklch.color(Theme.darkness > 0.5 ? 0.34 : 0.9, 0.06, hue)
        gradientTo: Oklch.color(Theme.darkness > 0.5 ? 0.24 : 0.82, 0.08, hue + 30)
        strokeColor: Theme.windowStroke
        Icon {
            anchors.centerIn: parent
            name: tile.item.preview ? tile.item.preview.glyph : "layout-grid"
            size: tile.compact ? 18 : 46
            color: Oklch.color(Theme.darkness > 0.5 ? 0.86 : 0.42, 0.12, tile.hue)
        }
        Text {
            visible: !tile.compact
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            text: !tile.item.preview ? "" : tile.item.preview.size === "small" ? qsTr("Small · 1×1")
                  : tile.item.preview.size === "large" ? qsTr("Large · 2×2") : qsTr("Medium · 2×1")
            color: Theme.textSecondary
            font: Theme.caption
        }
    }

    // Get, Installing…, Update, Apply / In Use (themes) or Add to Desktop
    // (widgets).
    component ActionButton: Button {
        id: action
        property var item: ({})
        property bool compact: false
        readonly property bool activeTheme: item.kind === "theme" && item.installed === true && Themes.activeId === item.id
        text: item.busy ? qsTr("Installing…")
              : !item.installed ? qsTr("Get")
              : item.updateAvailable ? qsTr("Update")
              : item.kind === "theme" ? (activeTheme ? qsTr("In Use") : qsTr("Apply"))
              : compact ? qsTr("Add") : qsTr("Add to Desktop")
        highlighted: !item.installed || item.updateAvailable === true
        enabled: item.id !== undefined && !item.busy && !activeTheme
        onClicked: {
            if (!item.installed || item.updateAvailable)
                Store.install(item.id);
            else if (item.kind === "theme")
                Themes.apply(item.id);
            else
                Intents.request("widgets.add", { id: item.id });
        }
    }
}
