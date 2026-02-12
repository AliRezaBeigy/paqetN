import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
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
    }

    property string activeGroupFilter: ""
    property var groupsModel: []

    readonly property var accentPalette: ["#5BA3F5", "#3DD68C", "#FFC947", "#FF6B6B", "#B794F6", "#F882D1", "#36D9EE", "#FF9A56"]
    readonly property color successColor: FluTheme.dark ? "#3DD68C" : "#22c55e"
    readonly property color warningColor: FluTheme.dark ? "#FFC947" : "#f59e0b"
    readonly property color tagColor: FluTheme.dark ? "#2D3B4E" : "#e2e8f0"
    readonly property color surfaceColor: FluTheme.dark ? Qt.rgba(26/255, 32/255, 44/255, 1) : Qt.rgba(1,1,1,1)
    readonly property color cardColor: FluTheme.dark ? Qt.rgba(35/255, 42/255, 56/255, 1) : Qt.rgba(1,1,1,1)
    readonly property color cardBorderColor: FluTheme.dark ? Qt.rgba(70/255, 80/255, 100/255, 1) : FluTheme.dividerColor
    readonly property color selectedCardColor: FluTheme.dark ? Qt.rgba(42/255, 52/255, 70/255, 1) : Qt.lighter(cardColor, 1.1)

    Component.onCompleted: {
        FluApp.windowIcon = "qrc:/assets/assets/icons/app_icon.png"
        var t = paqetController.getTheme()
        if (t === "dark") FluTheme.darkMode = FluThemeType.Dark
        else if (t === "light") FluTheme.darkMode = FluThemeType.Light
        else FluTheme.darkMode = FluThemeType.System
        groupsModel = paqetController.getGroups()
    }

    Connections {
        target: paqetController
        function onConfigsChanged() {
            groupsModel = paqetController.getGroups()
        }
    }

    FluNavigationView {
        id: nav_view
        anchors.fill: parent
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

    FluContentDialog {
        id: downloadPaqetPromptDialog
        title: qsTr("Paqet binary not found")
        message: qsTr("The paqet binary was not found on your system. Would you like to download it now?")
        buttonFlags: FluContentDialogType.NegativeButton | FluContentDialogType.PositiveButton
        negativeText: qsTr("Later")
        positiveText: qsTr("Download")
        onPositiveClicked: {
            paqetController.autoDownloadPaqetIfMissing()
        }
    }

    Connections {
        target: paqetController
        function onPaqetBinaryMissingPrompt() {
            downloadPaqetPromptDialog.open()
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
