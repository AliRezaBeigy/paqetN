import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0

Rectangle {
    id: root

    property var configData: ({})
    property bool isRunning: false
    property string proxyMode: "none"

    signal editRequested(string configId)
    signal deleteRequested(string configId)
    signal exportRequested(string configId)

    implicitWidth: 320
    color: FluTheme.dark ? Qt.rgba(30/255, 36/255, 48/255, 1) : window.cardColor

    property string cfgId: configData && configData.id ? configData.id : ""
    property string cfgName: configData && configData.name ? configData.name : ""
    property string cfgServerAddr: configData && configData.serverAddr ? configData.serverAddr : ""
    property string cfgGroup: configData && configData.group ? configData.group : ""
    property string cfgKcpBlock: configData && configData.kcpBlock ? configData.kcpBlock : ""
    property string cfgKcpMode: configData && configData.kcpMode ? configData.kcpMode : ""
    property int cfgMtu: configData && configData.mtu ? configData.mtu : 1350
    property int cfgConn: configData && configData.conn ? configData.conn : 1
    property string cfgSocksListen: configData && configData.socksListen ? configData.socksListen : ""

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: detailColumn.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: FluScrollBar {}

        ColumnLayout {
            id: detailColumn
            width: parent.width
            spacing: 16

            Item { Layout.preferredHeight: 4 }

            // Header
            FluText {
                text: qsTr("Host Details")
                font: FluTextStyle.Subtitle
                Layout.leftMargin: 16
                Layout.rightMargin: 16
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                color: FluTheme.dividerColor
            }

            // Address section
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 4

                FluText {
                    text: qsTr("Address")
                    font: FluTextStyle.BodyStrong
                    color: FluTheme.fontSecondaryColor
                }
                FluFrame {
                    Layout.fillWidth: true
                    implicitHeight: 40
                    padding: 12
                    FluText {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.cfgServerAddr
                        font: FluTextStyle.Body
                        elide: Text.ElideRight
                        width: parent.width - 24
                    }
                }
            }

            // General section
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 4

                FluText {
                    text: qsTr("General")
                    font: FluTextStyle.BodyStrong
                    color: FluTheme.fontSecondaryColor
                }
                FluFrame {
                    Layout.fillWidth: true
                    implicitHeight: 40
                    padding: 12
                    FluText {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.cfgName || root.cfgServerAddr
                        font: FluTextStyle.Body
                        elide: Text.ElideRight
                        width: parent.width - 24
                    }
                }

                // Tags
                Flow {
                    Layout.fillWidth: true
                    spacing: 6

                    Rectangle {
                        width: grpTagRow.implicitWidth + 16
                        height: 24
                        radius: 999
                        color: window.tagColor
                        visible: root.cfgGroup.length > 0

                        Row {
                            id: grpTagRow
                            anchors.centerIn: parent
                            spacing: 4
                            FluIcon { iconSource: FluentIcons.Folder; iconSize: 14; iconColor: FluTheme.fontSecondaryColor; anchors.verticalCenter: parent.verticalCenter }
                            FluText {
                                text: root.cfgGroup
                                font: FluTextStyle.Caption
                                color: FluTheme.fontSecondaryColor
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    Rectangle {
                        width: encTagLabel.implicitWidth + 16
                        height: 24
                        radius: 999
                        color: window.tagColor
                        visible: root.cfgKcpBlock.length > 0

                        FluText {
                            id: encTagLabel
                            anchors.centerIn: parent
                            text: root.cfgKcpBlock
                            font: FluTextStyle.Caption
                            color: FluTheme.fontSecondaryColor
                        }
                    }

                    Rectangle {
                        width: modeTagLabel.implicitWidth + 16
                        height: 24
                        radius: 999
                        color: window.tagColor
                        visible: root.cfgKcpMode.length > 0

                        FluText {
                            id: modeTagLabel
                            anchors.centerIn: parent
                            text: root.cfgKcpMode
                            font: FluTextStyle.Caption
                            color: FluTheme.fontSecondaryColor
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                color: FluTheme.dividerColor
            }

            // Connection Details
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 4

                FluText {
                    text: qsTr("Connection Details")
                    font: FluTextStyle.BodyStrong
                    color: FluTheme.fontSecondaryColor
                }

                GridLayout {
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 8
                    Layout.fillWidth: true

                    FluText { text: qsTr("SOCKS listen"); font: FluTextStyle.Caption; color: FluTheme.fontSecondaryColor }
                    FluText { text: root.cfgSocksListen; font: FluTextStyle.Body; Layout.fillWidth: true; elide: Text.ElideRight }

                    FluText { text: qsTr("Encryption"); font: FluTextStyle.Caption; color: FluTheme.fontSecondaryColor }
                    FluText { text: root.cfgKcpBlock; font: FluTextStyle.Body }

                    FluText { text: qsTr("KCP mode"); font: FluTextStyle.Caption; color: FluTheme.fontSecondaryColor }
                    FluText { text: root.cfgKcpMode; font: FluTextStyle.Body }

                    FluText { text: qsTr("MTU"); font: FluTextStyle.Caption; color: FluTheme.fontSecondaryColor }
                    FluText { text: String(root.cfgMtu); font: FluTextStyle.Body }

                    FluText { text: qsTr("Connections"); font: FluTextStyle.Caption; color: FluTheme.fontSecondaryColor }
                    FluText { text: String(root.cfgConn); font: FluTextStyle.Body }
                }
            }

            Item { Layout.fillHeight: true; Layout.minimumHeight: 16 }

            // Proxy mode badge
            Rectangle {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                visible: root.isRunning && root.proxyMode !== "none"
                width: proxyBadgeRow.implicitWidth + 16
                height: 24
                radius: 999
                color: root.proxyMode === "tun" ? window.accentPalette : window.tagColor

                Row {
                    id: proxyBadgeRow
                    anchors.centerIn: parent
                    spacing: 4
                    FluIcon {
                        iconSource: root.proxyMode === "tun" ? FluentIcons.Globe : FluentIcons.Settings
                        iconSize: 14
                        iconColor: root.proxyMode === "tun" ? "#FFFFFF" : FluTheme.fontSecondaryColor
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    FluText {
                        text: root.proxyMode === "tun" ? qsTr("TUN") : qsTr("System Proxy")
                        font: FluTextStyle.Caption
                        color: root.proxyMode === "tun" ? "#FFFFFF" : FluTheme.fontSecondaryColor
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // Action buttons
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 8

                FluIconButton {
                    iconSource: FluentIcons.Edit
                    text: qsTr("Edit")
                    display: Button.IconOnly
                    iconSize: 18
                    implicitWidth: 36
                    implicitHeight: 36
                    onClicked: root.editRequested(root.cfgId)
                }
                FluIconButton {
                    iconSource: FluentIcons.Delete
                    text: qsTr("Delete")
                    display: Button.IconOnly
                    iconSize: 18
                    implicitWidth: 36
                    implicitHeight: 36
                    onClicked: root.deleteRequested(root.cfgId)
                }
                FluIconButton {
                    iconSource: FluentIcons.Share
                    text: qsTr("Export")
                    display: Button.IconOnly
                    iconSize: 18
                    implicitWidth: 36
                    implicitHeight: 36
                    onClicked: root.exportRequested(root.cfgId)
                }
            }

            Item { Layout.preferredHeight: 16 }
        }
    }
}
