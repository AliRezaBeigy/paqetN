import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0

FluPage {
    id: hostsPage
    title: ""
    padding: 0

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Center: scrollable card grid
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Top bar
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.topMargin: 16
                Layout.bottomMargin: 12

                FluText {
                    text: qsTr("Hosts")
                    font: FluTextStyle.Title
                }
                Item { Layout.fillWidth: true }
                FluFilledButton {
                    id: addConfigBtn
                    text: qsTr("Add config")
                    textColor: "#FFFFFF"
                    onClicked: addConfigMenu.popup(addConfigBtn, 0, addConfigBtn.height + 4)
                }
            }

            // Scrollable grid
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Flickable {
                    id: flickArea
                    anchors.fill: parent
                    contentWidth: width
                    contentHeight: cardColumn.height
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: FluScrollBar {}
                    visible: paqetController.configs.rowCount() > 0

                    ColumnLayout {
                        id: cardColumn
                        width: flickArea.width
                        spacing: 20

                        // Groups section
                        FluText {
                            text: qsTr("Groups")
                            font: FluTextStyle.BodyStrong
                            color: FluTheme.fontPrimaryColor
                            Layout.leftMargin: 20
                            visible: window.groupsModel.length > 0
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.leftMargin: 20
                            Layout.rightMargin: 20
                            spacing: 12
                            visible: window.groupsModel.length > 0

                            Repeater {
                                model: window.groupsModel

                                GroupCard {
                                    groupName: modelData.name
                                    hostCount: modelData.count
                                    selected: window.activeGroupFilter === modelData.name
                                    width: Math.min(
                                        (cardColumn.width - 40 - 12) / 2,
                                        400
                                    )
                                    onClicked: {
                                        window.activeGroupFilter = (window.activeGroupFilter === modelData.name) ? "" : modelData.name
                                    }
                                    onRenameRequested: function(groupName) {
                                        window.openRenameGroupDialog(groupName)
                                    }
                                }
                            }
                        }

                        // Hosts section
                        FluText {
                            text: window.activeGroupFilter.length > 0 ? window.activeGroupFilter : qsTr("Hosts")
                            font: FluTextStyle.BodyStrong
                            color: FluTheme.fontPrimaryColor
                            Layout.leftMargin: 20
                            visible: window.groupsModel.length > 0
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.leftMargin: 20
                            Layout.rightMargin: 20
                            spacing: 12

                            Repeater {
                                model: paqetController.configs

                                HostCard {
                                    visible: window.activeGroupFilter === "" || model.group === window.activeGroupFilter
                                    width: visible ? Math.min(
                                        (cardColumn.width - 40 - 12) / 2,
                                        400
                                    ) : 0
                                    height: visible ? implicitHeight : 0
                                    configId: model.configId
                                    name: model.name
                                    serverAddr: model.serverAddr
                                    kcpBlock: model.kcpBlock
                                    kcpMode: model.kcpMode
                                    group: model.group
                                    selected: model.configId === paqetController.selectedConfigId
                                    isRunning: paqetController.isRunning && paqetController.selectedConfigId === model.configId
                                    onClicked: paqetController.selectedConfigId = model.configId
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 20 }
                    }
                }

                // Empty state
                ColumnLayout {
                    anchors.centerIn: parent
                    visible: paqetController.configs.rowCount() === 0
                    spacing: 8

                    FluIcon {
                        iconSource: FluentIcons.Folder
                        iconSize: 48
                        iconColor: FluTheme.fontSecondaryColor
                        Layout.alignment: Qt.AlignHCenter
                    }
                    FluText {
                        text: qsTr("No configurations yet")
                        font: FluTextStyle.Subtitle
                        color: FluTheme.fontSecondaryColor
                        Layout.alignment: Qt.AlignHCenter
                    }
                    FluText {
                        text: qsTr("Click 'Add config' to get started")
                        font: FluTextStyle.Caption
                        color: FluTheme.fontSecondaryColor
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }
        }

        // Separator
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: FluTheme.dividerColor
            visible: detailPanel.visible
        }

        // Right: Detail Panel
        DetailPanel {
            id: detailPanel
            Layout.fillHeight: true
            Layout.preferredWidth: 320
            visible: !!paqetController.selectedConfigId
            configData: paqetController.selectedConfigData
            isRunning: paqetController.isRunning
            latencyMs: paqetController.latencyMs
            latencyTesting: paqetController.latencyTesting
            onEditRequested: function(id) { window.openConfigEditor(id) }
            onDeleteRequested: function(id) {
                deleteConfirmDialog.configId = id
                deleteConfirmDialog.configName = paqetController.selectedConfigName
                deleteConfirmDialog.open()
            }
            onExportRequested: function(id) { exportMenu.popup() }
            onConnectRequested: paqetController.connectToSelected()
            onDisconnectRequested: paqetController.disconnect()
            onTestLatencyRequested: paqetController.testLatency()
        }
    }

    // ── Menus ──

    FluMenu {
        id: addConfigMenu
        width: 180

        FluMenuItem {
            text: qsTr("From clipboard")
            font: FluTextStyle.Caption
            onClicked: {
                if (paqetController.addConfigFromImport(paqetController.getClipboardText()))
                    addConfigMenu.close()
                else
                    showError(qsTr("Invalid or empty clipboard"))
            }
        }
        FluMenuItem {
            text: qsTr("Add manually")
            font: FluTextStyle.Caption
            onClicked: window.openConfigEditor("")
        }
        FluMenuItem {
            text: qsTr("Import from file")
            font: FluTextStyle.Caption
            onClicked: importFileDialog.open()
        }
    }

    FluMenu {
        id: exportMenu
        width: 200

        FluMenuItem {
            text: qsTr("Copy paqet:// link")
            font: FluTextStyle.Caption
            onClicked: {
                if (paqetController.selectedConfigId)
                    paqetController.copyToClipboard(paqetController.exportPaqetUri(paqetController.selectedConfigId))
            }
        }
        FluMenuItem {
            text: qsTr("Copy full YAML")
            font: FluTextStyle.Caption
            onClicked: {
                if (paqetController.selectedConfigId)
                    paqetController.copyToClipboard(paqetController.exportYaml(paqetController.selectedConfigId))
            }
        }
    }

    // ── Paqet Binary Missing Dialog ──
    CustomContentDialog {
        id: paqetMissingDialog
        title: qsTr("Paqet Binary Not Found")
        message: qsTr("The paqet binary is required to establish connections but was not found on your system.\n\nWould you like to download it now?")
        negativeText: qsTr("Cancel")
        positiveText: qsTr("Download")

        onPositiveClicked: {
            // Navigate to Updates page
            nav_view.push(Qt.resolvedUrl("UpdatesPage.qml"))
            // Trigger auto-download
            paqetController.autoDownloadPaqetIfMissing()
        }
    }

    // ── Connections ──
    Connections {
        target: paqetController

        function onPaqetBinaryMissing() {
            paqetMissingDialog.open()
        }
    }
}
