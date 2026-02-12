import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0

AbstractButton {
    id: root

    property string configId: ""
    property string name: ""
    property string serverAddr: ""
    property string kcpBlock: ""
    property string kcpMode: ""
    property string group: ""
    property bool selected: false
    property bool isRunning: false

    property color accentColor: {
        if (isRunning) return window.successColor
        var g = group.length > 0 ? group : serverAddr
        var hash = 0
        for (var i = 0; i < g.length; i++)
            hash = ((hash << 5) - hash) + g.charCodeAt(i)
        return window.accentPalette[Math.abs(hash) % window.accentPalette.length]
    }

    implicitHeight: 110
    hoverEnabled: true
    leftPadding: 16
    rightPadding: 16
    topPadding: 18
    bottomPadding: 18

    background: Rectangle {
        radius: 6
        border.width: root.selected ? 2 : 1
        border.color: root.selected ? FluTheme.primaryColor : window.cardBorderColor
        color: root.selected ? window.selectedCardColor : window.cardColor
        opacity: root.pressed ? 0.9 : (root.hovered ? 1.0 : 0.95)
        scale: root.hovered ? 1.01 : 1.0

        Behavior on opacity { NumberAnimation { duration: 150 } }
        Behavior on border.color { ColorAnimation { duration: 150 } }
        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on scale { NumberAnimation { duration: 150 } }
    }

    contentItem: RowLayout {
        spacing: 14

        Rectangle {
            width: 6
            height: parent.height - 20
            radius: 3
            color: root.accentColor
            Layout.alignment: Qt.AlignVCenter

            Behavior on color { ColorAnimation { duration: 250 } }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            FluText {
                text: root.name || root.serverAddr
                font: FluTextStyle.BodyStrong
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Row {
                spacing: 8

                Rectangle {
                    width: tagEncLabel.implicitWidth + 20
                    height: 28
                    radius: 999
                    color: window.tagColor
                    visible: root.kcpBlock.length > 0

                    FluText {
                        id: tagEncLabel
                        anchors.centerIn: parent
                        text: root.kcpBlock
                        font: FluTextStyle.Caption
                        color: FluTheme.fontSecondaryColor
                    }
                }

                Rectangle {
                    width: tagModeLabel.implicitWidth + 20
                    height: 28
                    radius: 999
                    color: window.tagColor
                    visible: root.kcpMode.length > 0

                    FluText {
                        id: tagModeLabel
                        anchors.centerIn: parent
                        text: root.kcpMode
                        font: FluTextStyle.Caption
                        color: FluTheme.fontSecondaryColor
                    }
                }
            }
        }
    }
}
