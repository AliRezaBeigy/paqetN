import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0

FluPage {
    id: updatesPage
    title: ""
    padding: 0

    property bool paqetUpdateAvailable: false
    property string paqetLatestVersion: ""
    property string paqetDownloadUrl: ""

    property bool paqetnUpdateAvailable: false
    property string paqetnLatestVersion: ""
    property string paqetnDownloadUrl: ""

    Component.onCompleted: {
        // Auto-check on page load if enabled
        if (paqetController.getAutoCheckUpdates()) {
            paqetController.checkPaqetUpdate()
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.height + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: FluScrollBar {}

        ColumnLayout {
            id: contentColumn
            width: parent.width
            spacing: 0

            // Header
            FluText {
                text: qsTr("Updates")
                font: FluTextStyle.Title
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.topMargin: 16
                Layout.bottomMargin: 16
            }

            // PaqetN GUI Section
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                spacing: 8

                FluText {
                    text: qsTr("PaqetN GUI")
                    font: FluTextStyle.BodyStrong
                    color: FluTheme.fontSecondaryColor
                }

                FluFrame {
                    Layout.fillWidth: true
                    padding: 16

                    ColumnLayout {
                        width: parent.width - 32
                        spacing: 12

                        RowLayout {
                            spacing: 8
                            FluText {
                                text: qsTr("Current version:")
                                font: FluTextStyle.Body
                            }
                            FluText {
                                text: paqetController.getPaqetNVersion()
                                font: FluTextStyle.BodyStrong
                                color: FluTheme.primaryColor
                            }
                        }

                        RowLayout {
                            spacing: 8
                            FluButton {
                                text: qsTr("Check for Updates")
                                enabled: !paqetController.updateCheckInProgress
                                onClicked: {
                                    paqetnUpdateAvailable = false
                                    paqetController.checkPaqetNUpdate()
                                }
                            }

                            FluProgressRing {
                                visible: paqetController.updateCheckInProgress
                                width: 20
                                height: 20
                            }
                        }

                        // Update available section for PaqetN
                        FluFrame {
                            Layout.fillWidth: true
                            visible: paqetnUpdateAvailable
                            border.color: window.successColor
                            border.width: 1
                            padding: 12

                            ColumnLayout {
                                width: parent.width - 24
                                spacing: 8

                                FluText {
                                    text: qsTr("Update Available!")
                                    font: FluTextStyle.BodyStrong
                                    color: window.successColor
                                }

                                FluText {
                                    text: qsTr("Version %1 is available for download.").arg(paqetnLatestVersion)
                                    font: FluTextStyle.Body
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }

                                FluText {
                                    text: qsTr("⚠ App will restart after download to apply the update.")
                                    font: FluTextStyle.Caption
                                    color: window.warningColor
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }

                                RowLayout {
                                    spacing: 8
                                    FluFilledButton {
                                        text: qsTr("Download & Restart")
                                        enabled: !paqetController.paqetnDownloadInProgress
                                        onClicked: {
                                            paqetController.downloadPaqetNUpdate(paqetnLatestVersion, paqetnDownloadUrl)
                                        }
                                    }
                                    FluButton {
                                        text: qsTr("Cancel")
                                        enabled: paqetController.paqetnDownloadInProgress
                                        onClicked: paqetController.cancelUpdate()
                                    }
                                }

                                // Download progress for PaqetN
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    visible: paqetController.paqetnDownloadInProgress
                                    spacing: 4

                                    FluProgressBar {
                                        Layout.fillWidth: true
                                        value: paqetController.paqetnDownloadProgress
                                        indeterminate: paqetController.paqetnDownloadProgress === 0
                                    }

                                    FluText {
                                        text: qsTr("Downloading... %1%").arg(paqetController.paqetnDownloadProgress)
                                        font: FluTextStyle.Caption
                                        color: FluTheme.fontTertiaryColor
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 16 }

            // Paqet Binary Section
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                spacing: 8

                FluText {
                    text: qsTr("Paqet Binary")
                    font: FluTextStyle.BodyStrong
                    color: FluTheme.fontSecondaryColor
                }

                FluFrame {
                    Layout.fillWidth: true
                    padding: 16

                    ColumnLayout {
                        width: parent.width - 32
                        spacing: 12

                        // Status indicator
                        RowLayout {
                            spacing: 8
                            Rectangle {
                                width: 12
                                height: 12
                                radius: 6
                                color: paqetController.isPaqetBinaryAvailable()
                                    ? window.successColor
                                    : window.warningColor
                            }
                            FluText {
                                text: paqetController.isPaqetBinaryAvailable()
                                    ? qsTr("Binary installed")
                                    : qsTr("Binary not found")
                                font: FluTextStyle.Body
                            }
                        }

                        // Current version info
                        RowLayout {
                            spacing: 8
                            FluText {
                                text: qsTr("Installed version:")
                                font: FluTextStyle.Body
                            }
                            FluText {
                                text: paqetController.installedPaqetVersion
                                font: FluTextStyle.BodyStrong
                                color: FluTheme.primaryColor
                            }
                        }

                        // Check for updates button
                        RowLayout {
                            spacing: 8
                            FluButton {
                                text: qsTr("Check for Updates")
                                enabled: !paqetController.updateCheckInProgress
                                onClicked: {
                                    paqetUpdateAvailable = false
                                    paqetController.checkPaqetUpdate()
                                }
                            }

                            FluProgressRing {
                                visible: paqetController.updateCheckInProgress
                                width: 20
                                height: 20
                            }
                        }

                        // Status message
                        FluText {
                            text: paqetController.updateStatusMessage
                            font: FluTextStyle.Caption
                            color: FluTheme.fontSecondaryColor
                            visible: text.length > 0
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        // Update available section for paqet
                        FluFrame {
                            Layout.fillWidth: true
                            visible: paqetUpdateAvailable
                            border.color: window.successColor
                            border.width: 1
                            padding: 12

                            ColumnLayout {
                                width: parent.width - 24
                                spacing: 8

                                FluText {
                                    text: qsTr("Update Available!")
                                    font: FluTextStyle.BodyStrong
                                    color: window.successColor
                                }

                                FluText {
                                    text: qsTr("Version %1 is available for download.").arg(paqetLatestVersion)
                                    font: FluTextStyle.Body
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }

                                RowLayout {
                                    spacing: 8
                                    FluFilledButton {
                                        text: qsTr("Download & Install")
                                        enabled: !paqetController.paqetDownloadInProgress
                                        onClicked: {
                                            paqetController.downloadPaqet(paqetLatestVersion, paqetDownloadUrl)
                                        }
                                    }
                                    FluButton {
                                        text: qsTr("Cancel")
                                        enabled: paqetController.paqetDownloadInProgress
                                        onClicked: paqetController.cancelUpdate()
                                    }
                                }

                                // Download progress for paqet
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    visible: paqetController.paqetDownloadInProgress
                                    spacing: 4

                                    FluProgressBar {
                                        Layout.fillWidth: true
                                        value: paqetController.paqetDownloadProgress
                                        indeterminate: paqetController.paqetDownloadProgress === 0
                                    }

                                    FluText {
                                        text: qsTr("Downloading... %1%").arg(paqetController.paqetDownloadProgress)
                                        font: FluTextStyle.Caption
                                        color: FluTheme.fontTertiaryColor
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 16 }

            // Settings Section
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                spacing: 8

                FluText {
                    text: qsTr("Update Settings")
                    font: FluTextStyle.BodyStrong
                    color: FluTheme.fontSecondaryColor
                }

                FluFrame {
                    Layout.fillWidth: true
                    padding: 16

                    ColumnLayout {
                        width: parent.width - 32
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            FluText {
                                text: qsTr("Auto-download paqet binary if missing")
                                font: FluTextStyle.Body
                                Layout.fillWidth: true
                            }
                            FluCheckBox {
                                id: autoDownloadCheck
                                checked: paqetController.getAutoDownloadPaqet()
                                onClicked: paqetController.setAutoDownloadPaqet(checked)
                            }
                        }

                        FluText {
                            text: qsTr("Automatically downloads the paqet binary on first launch if not found.")
                            font: FluTextStyle.Caption
                            color: FluTheme.fontTertiaryColor
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            Layout.leftMargin: 0
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: FluTheme.dividerColor
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            FluText {
                                text: qsTr("Auto-check for updates on startup")
                                font: FluTextStyle.Body
                                Layout.fillWidth: true
                            }
                            FluCheckBox {
                                id: autoCheckUpdatesCheck
                                checked: paqetController.getAutoCheckUpdates()
                                onClicked: paqetController.setAutoCheckUpdates(checked)
                            }
                        }

                        FluText {
                            text: qsTr("Checks for both PaqetN and paqet updates when the app starts.")
                            font: FluTextStyle.Caption
                            color: FluTheme.fontTertiaryColor
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            Layout.leftMargin: 0
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: FluTheme.dividerColor
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            FluText {
                                text: qsTr("Auto-update PaqetN GUI (requires restart)")
                                font: FluTextStyle.Body
                                Layout.fillWidth: true
                            }
                            FluCheckBox {
                                id: autoUpdatePaqetNCheck
                                checked: paqetController.getAutoUpdatePaqetN()
                                onClicked: paqetController.setAutoUpdatePaqetN(checked)
                            }
                        }

                        FluText {
                            text: qsTr("Automatically downloads and installs PaqetN GUI updates. The app will restart to apply updates.")
                            font: FluTextStyle.Caption
                            color: window.warningColor
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            Layout.leftMargin: 0
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 20 }
        }
    }

    // Connections
    Connections {
        target: paqetController

        function onPaqetUpdateAvailable(version, url) {
            paqetUpdateAvailable = true
            paqetLatestVersion = version
            paqetDownloadUrl = url
        }

        function onPaqetNUpdateAvailable(version, url) {
            paqetnUpdateAvailable = true
            paqetnLatestVersion = version
            paqetnDownloadUrl = url
        }

        function onPaqetDownloadComplete(path) {
            paqetUpdateAvailable = false
            // Show success notification
            showSuccess(qsTr("Paqet binary updated successfully!"), 3000)
        }

        function onPaqetnDownloadComplete() {
            // App will close and restart
            Qt.quit()
        }
    }

    function showSuccess(message, duration) {
        // Use FluInfoBar if available
        if (typeof FluInfoBar !== 'undefined') {
            FluInfoBar.showSuccess(message, duration)
        }
    }
}
