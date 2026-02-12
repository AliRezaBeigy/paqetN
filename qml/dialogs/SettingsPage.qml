import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0

FluPage {
    id: settingsPage
    title: ""
    padding: 0
    background: Rectangle {
        color: FluTheme.dark ? Qt.rgba(18/255, 18/255, 20/255, 1) : FluTheme.windowBackgroundColor
    }

    Component.onCompleted: {
        socksPortField.text = String(paqetController.getSocksPort())
        allowLocalLanCheck.checked = paqetController.getAllowLocalLan()
        connectionCheckUrlField.text = paqetController.getConnectionCheckUrl()
        timeoutField.text = String(paqetController.getConnectionCheckTimeoutSeconds())
        showLatencyCheck.checked = paqetController.getShowLatencyInUi()
        var levels = paqetController.getLogLevels()
        var logIdx = levels.indexOf(paqetController.getLogLevel())
        logLevelCombo.currentIndex = logIdx >= 0 ? logIdx : levels.indexOf("fatal")
        paqetPathField.text = paqetController.getPaqetBinaryPath()
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: outerColumn.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: FluScrollBar {}

        ColumnLayout {
            id: outerColumn
            width: parent.width
            spacing: 0

            // Header
            FluText {
                text: qsTr("Settings")
                font: FluTextStyle.Title
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 20
                Layout.bottomMargin: 20
            }

            // ── Startup ──
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                spacing: 10

                FluText {
                    text: qsTr("Startup")
                    font: FluTextStyle.BodyStrong
                    color: FluTheme.fontSecondaryColor
                }

                FluFrame {
                    Layout.fillWidth: true
                    padding: 16

                    GridLayout {
                        columns: 2
                        rowSpacing: 12
                        columnSpacing: 12
                        width: parent.width - 32

                        FluText { text: qsTr("Start on boot"); font: FluTextStyle.Body }
                        FluCheckBox {
                            id: startOnBootCheck
                            checked: paqetController.getStartOnBoot()
                            onClicked: paqetController.setStartOnBoot(checked)
                        }

                        FluText {
                            text: qsTr("Auto hide on startup")
                            font: FluTextStyle.Body
                            opacity: startOnBootCheck.checked ? 1.0 : 0.5
                        }
                        FluCheckBox {
                            id: autoHideCheck
                            checked: paqetController.getAutoHideOnStartup()
                            enabled: startOnBootCheck.checked
                            onClicked: paqetController.setAutoHideOnStartup(checked)
                        }

                        FluText { text: qsTr("Close to tray"); font: FluTextStyle.Body }
                        FluCheckBox {
                            id: closeToTrayCheck
                            checked: paqetController.getCloseToTray()
                            onClicked: paqetController.setCloseToTray(checked)
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 16 }

            // ── Appearance ──
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                spacing: 10

                FluText {
                    text: qsTr("Appearance")
                    font: FluTextStyle.BodyStrong
                    color: FluTheme.fontSecondaryColor
                }

                FluFrame {
                    Layout.fillWidth: true
                    padding: 16

                    ColumnLayout {
                        width: parent.width - 32
                        spacing: 8

                        FluText {
                            text: qsTr("Theme")
                            font: FluTextStyle.Body
                        }
                        Repeater {
                            model: [
                                {title: qsTr("Light"), mode: FluThemeType.Light},
                                {title: qsTr("Dark"),  mode: FluThemeType.Dark},
                                {title: qsTr("System"), mode: FluThemeType.System}
                            ]
                            delegate: FluRadioButton {
                                checked: FluTheme.darkMode === modelData.mode
                                text: modelData.title
                                clickListener: function() {
                                    FluTheme.darkMode = modelData.mode
                                    var str = "system"
                                    if (modelData.mode === FluThemeType.Light) str = "light"
                                    else if (modelData.mode === FluThemeType.Dark) str = "dark"
                                    paqetController.setTheme(str)
                                }
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 16 }

            // ── Connection ──
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                spacing: 8

                FluText {
                    text: qsTr("Connection")
                    font: FluTextStyle.BodyStrong
                    color: FluTheme.fontSecondaryColor
                }

                FluFrame {
                    Layout.fillWidth: true
                    padding: 16

                    GridLayout {
                        columns: 2
                        rowSpacing: 12
                        columnSpacing: 12
                        width: parent.width - 32

                        FluText { text: qsTr("Default SOCKS port"); font: FluTextStyle.Body }
                        FluTextBox {
                            id: socksPortField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            text: "1284"
                            validator: IntValidator { bottom: 1; top: 65535 }
                            onEditingFinished: paqetController.setSocksPort(parseInt(text) || 1284)
                        }

                        FluText { text: qsTr("Allow Local LAN"); font: FluTextStyle.Body }
                        FluCheckBox {
                            id: allowLocalLanCheck
                            checked: false
                            onClicked: paqetController.setAllowLocalLan(checked)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Bind to 0.0.0.0 instead of 127.0.0.1 to allow connections from other devices on your local network")
                        }

                        FluText { text: qsTr("Connection check URL"); font: FluTextStyle.Body }
                        FluTextBox {
                            id: connectionCheckUrlField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            placeholderText: "https://www.gstatic.com/generate_204"
                            onEditingFinished: paqetController.setConnectionCheckUrl(text.trim() || "https://www.gstatic.com/generate_204")
                        }

                        FluText { text: qsTr("Check timeout (s)"); font: FluTextStyle.Body }
                        FluTextBox {
                            id: timeoutField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            text: "10"
                            validator: IntValidator { bottom: 3; top: 60 }
                            onEditingFinished: paqetController.setConnectionCheckTimeoutSeconds(parseInt(text) || 10)
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 16 }

            // ── UI & Logging ──
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                spacing: 10

                FluText {
                    text: qsTr("UI & Logging")
                    font: FluTextStyle.BodyStrong
                    color: FluTheme.fontSecondaryColor
                }

                FluFrame {
                    Layout.fillWidth: true
                    padding: 16

                    GridLayout {
                        columns: 2
                        rowSpacing: 12
                        columnSpacing: 12
                        width: parent.width - 32

                        FluText { text: qsTr("Show latency in UI"); font: FluTextStyle.Body }
                        FluCheckBox {
                            id: showLatencyCheck
                            checked: true
                            onClicked: paqetController.setShowLatencyInUi(checked)
                        }

                        FluText { text: qsTr("Log level"); font: FluTextStyle.Body }
                        FluComboBox {
                            id: logLevelCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            model: paqetController.getLogLevels()
                            onActivated: function(index) {
                                var levels = paqetController.getLogLevels()
                                if (index >= 0 && index < levels.length)
                                    paqetController.setLogLevel(levels[index])
                            }
                        }

                        FluText { text: qsTr("paqet binary path"); font: FluTextStyle.Body }
                        FluTextBox {
                            id: paqetPathField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            placeholderText: qsTr("Leave empty for default")
                            onEditingFinished: paqetController.setPaqetBinaryPath(text.trim())
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 20 }
        }
    }
}
