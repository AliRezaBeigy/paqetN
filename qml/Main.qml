import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Qt.labs.platform as Platform
import FluentUI 1.0

FluWindow {
    id: window
    title: qsTr("paqetN")
    width: 1100
    height: 700
    minimumWidth: 800
    minimumHeight: 500
    visible: true
    fitsAppBarWindows: true
    showDark: true
    launchMode: FluWindowType.SingleTask

    appBar: FluAppBar {
        height: 30
        showDark: true
        darkClickListener: (button) => { handleDarkChanged(button) }
        showMinimize: true
        showMaximize: true
        showClose: true
        z: 7
        closeClickListener: function() {
            if (paqetController.getCloseToTray()) {
                window.hide()
            } else {
                // Hide window immediately so user doesn't see the cleanup freeze
                window.hide()
                Qt.callLater(Qt.quit)
            }
        }
    }

    // System Tray Icon
    Platform.SystemTrayIcon {
        id: systemTray
        visible: true
        icon.source: "qrc:/assets/assets/icons/app_icon.png"
        tooltip: qsTr("paqetN")

        onActivated: function(reason) {
            if (reason === Platform.SystemTrayIcon.Trigger ||
                reason === Platform.SystemTrayIcon.DoubleClick) {
                window.show()
                window.raise()
                window.requestActivate()
            } else if (reason === Platform.SystemTrayIcon.Context) {
                systemTray.menu.open()
            }
        }

        menu: Platform.Menu {
            Platform.MenuItem {
                text: qsTr("Hosts")
                onTriggered: {
                    window.show()
                    window.raise()
                    window.requestActivate()
                    nav_view.setCurrentIndex(0)
                    nav_view.push(Qt.resolvedUrl("pages/HostsPage.qml"))
                }
            }
            Platform.MenuItem {
                text: qsTr("Log")
                onTriggered: {
                    window.show()
                    window.raise()
                    window.requestActivate()
                    nav_view.setCurrentIndex(1)
                    nav_view.push(Qt.resolvedUrl("pages/LogPage.qml"))
                }
            }
            Platform.MenuItem {
                text: qsTr("Updates")
                onTriggered: {
                    window.show()
                    window.raise()
                    window.requestActivate()
                    nav_view.setCurrentIndex(2)
                    nav_view.push(Qt.resolvedUrl("pages/UpdatesPage.qml"))
                }
            }
            Platform.MenuItem {
                text: qsTr("Settings")
                onTriggered: {
                    window.show()
                    window.raise()
                    window.requestActivate()
                    nav_view.push(Qt.resolvedUrl("dialogs/SettingsPage.qml"))
                }
            }
            Platform.MenuSeparator {}
            Platform.MenuItem {
                text: qsTr("Exit")
                onTriggered: {
                    // Hide window immediately so user doesn't see the cleanup freeze
                    window.hide()
                    // Small delay to let the hide complete, then quit
                    Qt.callLater(Qt.quit)
                }
            }
        }
    }

    property string activeGroupFilter: ""
    property var groupsModel: []
    property var networkAdapters: []
    property string selectedNetworkInterface: ""

    // Modern Dark Theme - Slate/Charcoal palette with vibrant accents
    readonly property var accentPalette: ["#818CF8", "#34D399", "#FBBF24", "#F87171", "#C084FC", "#F472B6", "#22D3EE", "#FB923C"]
    readonly property color successColor: FluTheme.dark ? "#34D399" : "#22c55e"
    readonly property color warningColor: FluTheme.dark ? "#FBBF24" : "#f59e0b"
    readonly property color errorColor: FluTheme.dark ? "#F87171" : "#ef4444"
    readonly property color tagColor: FluTheme.dark ? "#3F3F46" : "#e2e8f0"
    readonly property color surfaceColor: FluTheme.dark ? Qt.rgba(9/255, 9/255, 11/255, 1) : Qt.rgba(1,1,1,1)
    readonly property color cardColor: FluTheme.dark ? Qt.rgba(39/255, 39/255, 42/255, 1) : Qt.rgba(1,1,1,1)
    readonly property color cardBorderColor: FluTheme.dark ? Qt.rgba(63/255, 63/255, 70/255, 1) : FluTheme.dividerColor
    readonly property color selectedCardColor: FluTheme.dark ? Qt.rgba(52/255, 52/255, 56/255, 1) : Qt.lighter(cardColor, 1.1)
    readonly property color hoverCardColor: FluTheme.dark ? Qt.rgba(46/255, 46/255, 50/255, 1) : Qt.lighter(cardColor, 0.98)

    Component.onCompleted: {
        FluApp.windowIcon = "qrc:/assets/assets/icons/app_icon.png"
        var t = paqetController.getTheme()
        if (t === "dark") FluTheme.darkMode = FluThemeType.Dark
        else if (t === "light") FluTheme.darkMode = FluThemeType.Light
        else FluTheme.darkMode = FluThemeType.System
        groupsModel = paqetController.getGroups()
        
        // Load network adapters and selected interface
        refreshNetworkAdapters()
        
        // Start monitoring for network changes
        paqetController.startNetworkMonitoring()

        // Auto hide on startup (only when start on boot is enabled)
        if (paqetController.getStartOnBoot() && paqetController.getAutoHideOnStartup()) {
            window.hide()
        }
    }
    
    function refreshNetworkAdapters() {
        networkAdapters = paqetController.getAcceptableNetworkAdapters()
        selectedNetworkInterface = paqetController.getSelectedNetworkInterface()
        
        // Validate that the selected interface still exists
        if (selectedNetworkInterface) {
            var found = false
            for (var i = 0; i < networkAdapters.length; i++) {
                if (networkAdapters[i].guid === selectedNetworkInterface) {
                    found = true
                    break
                }
            }
            // If selected interface no longer exists, reset to Auto
            if (!found) {
                selectedNetworkInterface = ""
                paqetController.setSelectedNetworkInterface("")
            }
        }
    }

    Connections {
        target: paqetController
        function onConfigsChanged() {
            groupsModel = paqetController.getGroups()
        }
        function onNetworkAdaptersChanged() {
            refreshNetworkAdapters()
        }
    }

    // Bottom bar: active profile (left) + proxy mode (right)
    Rectangle {
        id: bottomBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 48
        color: window.surfaceColor
        border.color: FluTheme.dividerColor
        border.width: 1
        z: 5

        // Check if any download is in progress
        property bool downloadInProgress: paqetController.paqetDownloadInProgress || paqetController.tunAssetsDownloadInProgress
        property int downloadProgress: {
            if (paqetController.paqetDownloadInProgress) return paqetController.paqetDownloadProgress
            if (paqetController.tunAssetsDownloadInProgress) return paqetController.tunAssetsDownloadProgress
            return 0
        }
        property string downloadReason: {
            if (paqetController.paqetDownloadInProgress) return qsTr("Downloading paqet binary...")
            if (paqetController.tunAssetsDownloadInProgress) return qsTr("Downloading TUN assets...")
            return ""
        }

        // Thin blue progress bar at the top
        Rectangle {
            id: progressBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 3
            color: "transparent"
            visible: bottomBar.downloadInProgress

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * (bottomBar.downloadProgress / 100.0)
                color: FluTheme.primaryColor
                Behavior on width { NumberAnimation { duration: 150 } }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            anchors.topMargin: bottomBar.downloadInProgress ? 3 : 0
            spacing: 12

            // Profile name | status - latency (click latency icon to test)
            RowLayout {
                Layout.preferredWidth: 320
                spacing: 4
                FluText {
                    text: (paqetController.selectedConfigName || qsTr("No profile"))
                        + " | "
                        + (paqetController.isRunning ? qsTr("connected") : qsTr("Not connected"))
                        + (paqetController.latencyTesting ? " - " + qsTr("Testing…")
                            : (paqetController.isRunning ? " - " + (paqetController.latencyMs >= 0 ? qsTr("Connection took %1 ms").arg(paqetController.latencyMs) : qsTr("Connection took %1 ms").arg(-1))
                                : ""))
                    font: FluTextStyle.Body
                    color: FluTheme.fontPrimaryColor
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                FluIconButton {
                    iconSource: FluentIcons.SpeedHigh
                    text: qsTr("Test latency")
                    display: Button.IconOnly
                    iconSize: 18
                    implicitWidth: 36
                    implicitHeight: 36
                    enabled: !!paqetController.selectedConfigId && paqetController.isRunning
                    onClicked: paqetController.testLatency()
                }
            }

            // Center: download progress text when active, or interface selector
            Item { Layout.fillWidth: true }
            FluText {
                visible: bottomBar.downloadInProgress
                text: bottomBar.downloadReason + " " + bottomBar.downloadProgress + "%"
                font: FluTextStyle.Body
                color: FluTheme.primaryColor
            }
            
            // Network interface selector (only show if more than one adapter)
            FluComboBox {
                id: interfaceCombo
                visible: !bottomBar.downloadInProgress && window.networkAdapters.length > 1
                Layout.preferredWidth: 180
                property var adapterGuids: {
                    var guids = [""]  // First item is "Auto" with empty guid
                    for (var i = 0; i < window.networkAdapters.length; i++) {
                        guids.push(window.networkAdapters[i].guid)
                    }
                    return guids
                }
                model: {
                    var names = [qsTr("Auto")]
                    for (var i = 0; i < window.networkAdapters.length; i++) {
                        var adapter = window.networkAdapters[i]
                        // Show name with IP for clarity
                        var ip = adapter.ipv4Address.replace(":0", "")
                        names.push(adapter.name + " (" + ip + ")")
                    }
                    return names
                }
                currentIndex: {
                    if (!window.selectedNetworkInterface) return 0
                    for (var i = 0; i < window.networkAdapters.length; i++) {
                        if (window.networkAdapters[i].guid === window.selectedNetworkInterface) {
                            return i + 1  // +1 because first item is "Auto"
                        }
                    }
                    return 0
                }
                onActivated: function(index) {
                    var guid = adapterGuids[index] || ""
                    window.selectedNetworkInterface = guid
                    paqetController.setSelectedNetworkInterface(guid)
                }
            }
            
            Item { Layout.fillWidth: true }

            FluComboBox {
                id: proxyModeCombo
                Layout.preferredWidth: 160
                Layout.alignment: Qt.AlignRight
                property var proxyModeValues: ["none", "system", "tun"]
                model: [qsTr("Socks only"), qsTr("System proxy"), qsTr("Tun mode")]
                currentIndex: {
                    var m = paqetController.proxyMode
                    var i = proxyModeValues.indexOf(m)
                    return i >= 0 ? i : 0
                }
                onActivated: function(index) {
                    if (index >= 0 && index < proxyModeValues.length)
                        paqetController.setProxyMode(proxyModeValues[index])
                }
            }
            FluIconButton {
                iconSource: FluentIcons.Sync
                text: qsTr("Restart")
                display: Button.IconOnly
                iconSize: 18
                implicitWidth: 36
                implicitHeight: 36
                enabled: !!paqetController.selectedConfigId
                onClicked: paqetController.restart()
            }
        }
    }

    FluNavigationView {
        id: nav_view
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: bottomBar.top
        pageMode: FluNavigationViewType.NoStack
        displayMode: FluNavigationViewType.Compact
        navCompactWidth: 56
        title: "paqetN"
        topPadding: 0

        items: FluObject {
            FluPaneItem {
                title: qsTr("Hosts")
                icon: FluentIcons.GridView
                url: Qt.resolvedUrl("pages/HostsPage.qml")
                onTap: nav_view.push(url)
            }
            FluPaneItem {
                title: qsTr("Log")
                icon: FluentIcons.CommandPrompt
                url: Qt.resolvedUrl("pages/LogPage.qml")
                onTap: nav_view.push(url)
            }
            FluPaneItem {
                title: qsTr("Updates")
                icon: FluentIcons.UpdateRestore
                url: Qt.resolvedUrl("pages/UpdatesPage.qml")
                onTap: nav_view.push(url)
            }
        }

        footerItems: FluObject {
            FluPaneItemSeparator {}
            FluPaneItem {
                title: qsTr("Settings")
                icon: FluentIcons.Settings
                url: Qt.resolvedUrl("dialogs/SettingsPage.qml")
                onTap: nav_view.push(url)
            }
        }

        Component.onCompleted: {
            nav_view.buttonBack.visible = false
            setCurrentIndex(0)
            push(Qt.resolvedUrl("pages/HostsPage.qml"))
            window.setHitTestVisible(nav_view.buttonMenu)
        }
    }

    // ── Dialogs ──

    ConfigEditorDialog {
        id: configEditorDialog
        onSaved: function(c) {
            paqetController.saveConfig(c)
            configEditorDialog.close()
        }
    }

    FluContentDialog {
        id: deleteConfirmDialog
        property string configId: ""
        property string configName: ""
        title: qsTr("Delete profile")
        message: qsTr("Are you sure you want to delete \"%1\"?").arg(deleteConfirmDialog.configName || deleteConfirmDialog.configId)
        buttonFlags: FluContentDialogType.NegativeButton | FluContentDialogType.PositiveButton
        negativeText: qsTr("Cancel")
        positiveText: qsTr("Delete")
        onPositiveClicked: {
            if (configId) paqetController.deleteConfig(configId)
            configId = ""
            configName = ""
        }
    }

    FluContentDialog {
        id: renameGroupDialog
        property string oldGroupName: ""
        property string newGroupName: ""
        title: qsTr("Rename group")
        message: qsTr("Enter a new name for group \"%1\":").arg(renameGroupDialog.oldGroupName)
        buttonFlags: FluContentDialogType.NegativeButton | FluContentDialogType.PositiveButton
        negativeText: qsTr("Cancel")
        positiveText: qsTr("Rename")

        contentDelegate: Component {
            FluTextBox {
                Layout.fillWidth: true
                Layout.preferredWidth: 300
                placeholderText: qsTr("New group name")
                text: renameGroupDialog.oldGroupName
                selectByMouse: true
                onTextChanged: renameGroupDialog.newGroupName = text
                Component.onCompleted: {
                    forceActiveFocus()
                    selectAll()
                }
            }
        }

        onPositiveClicked: {
            var trimmed = newGroupName.trim()
            if (trimmed && trimmed !== oldGroupName) {
                paqetController.renameGroup(oldGroupName, trimmed)
                if (window.activeGroupFilter === oldGroupName) {
                    window.activeGroupFilter = trimmed
                }
            }
            oldGroupName = ""
            newGroupName = ""
        }
        onNegativeClicked: {
            oldGroupName = ""
            newGroupName = ""
        }
    }

    FileDialog {
        id: exportLogDialog
        title: qsTr("Export log")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("Text files (*.txt)")]
        onAccepted: {
            var path = selectedFile.toString()
            if (path.indexOf("file:///") === 0) path = path.substring(8)
            else if (path.indexOf("file://") === 0) path = path.substring(7)
            paqetController.writeFile(path, paqetController.logText)
        }
    }

    FileDialog {
        id: importFileDialog
        title: qsTr("Import config")
        nameFilters: [qsTr("JSON or text (*.json *.txt *.conf)")]
        fileMode: FileDialog.OpenFile
        onAccepted: {
            var path = selectedFile.toString()
            if (path.indexOf("file:///") === 0)
                path = path.substring(path.indexOf("file:///") + 8)
            else if (path.indexOf("file://") === 0)
                path = path.substring(7)
            var content = paqetController.readFile(path)
            if (content)
                paqetController.addConfigFromImport(content)
        }
    }

    CustomContentDialog {
        id: downloadPaqetPromptDialog
        title: qsTr("Paqet binary not found")
        message: qsTr("The paqet binary was not found on your system. Would you like to download it now?")
        negativeText: qsTr("Later")
        positiveText: qsTr("Download")
        onPositiveClicked: {
            paqetController.autoDownloadPaqetIfMissing()
        }
    }

    CustomContentDialog {
        id: downloadTunAssetsPromptDialog
        title: qsTr("TUN mode assets not found")
        message: qsTr("TUN mode requires hev-socks5-tunnel and (on Windows) wintun.dll. They were not found.\n\nWould you like to download them now?")
        negativeText: qsTr("Later")
        positiveText: qsTr("Download")
        onPositiveClicked: {
            paqetController.autoDownloadTunAssetsIfMissing()
        }
    }

    CustomContentDialog {
        id: adminPrivilegeDialog
        title: qsTr("Administrator Privileges Required")
        message: qsTr("TUN mode requires administrator privileges to create a virtual network adapter.\n\nWould you like to restart the application with administrator privileges?")
        negativeText: qsTr("Cancel")
        positiveText: qsTr("Restart as Admin")
        onPositiveClicked: {
            paqetController.restartAsAdmin()
        }
    }

    Connections {
        target: paqetController
        function onPaqetBinaryMissingPrompt() {
            downloadPaqetPromptDialog.open()
        }
        function onPaqetBinaryMissing() {
            downloadPaqetPromptDialog.open()
        }
        function onTunAssetsMissingPrompt() {
            downloadTunAssetsPromptDialog.open()
        }
        function onAdminPrivilegeRequired() {
            adminPrivilegeDialog.open()
        }
    }

    // ── Helper functions ──

    function openConfigEditor(id) {
        configEditorDialog.configId = id
        configEditorDialog.config = paqetController.getConfigForEdit(id)
        configEditorDialog.open()
    }

    function openRenameGroupDialog(groupName) {
        renameGroupDialog.oldGroupName = groupName
        renameGroupDialog.open()
    }

    function handleDarkChanged(button) {
        changeDark()
    }

    function changeDark() {
        if (FluTheme.dark) {
            FluTheme.darkMode = FluThemeType.Light
            paqetController.setTheme("light")
        } else {
            FluTheme.darkMode = FluThemeType.Dark
            paqetController.setTheme("dark")
        }
    }
}
