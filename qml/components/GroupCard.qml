import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0

AbstractButton {
    id: root

    property string groupName: ""
    property int hostCount: 0
    property bool selected: false

    signal renameRequested(string groupName)

    property color accentColor: {
        var hash = 0
        for (var i = 0; i < groupName.length; i++)
            hash = ((hash << 5) - hash) + groupName.charCodeAt(i)
        return window.accentPalette[Math.abs(hash) % window.accentPalette.length]
    }

    implicitHeight: 88
    hoverEnabled: true
    leftPadding: 12
    rightPadding: 12
    topPadding: 16
    bottomPadding: 16

    background: Rectangle {
        radius: 4
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
        spacing: 12

        Rectangle {
            width: 40
            height: 40
            radius: 20
            color: root.accentColor

            FluText {
                anchors.centerIn: parent
                text: root.groupName.length > 0 ? root.groupName.charAt(0).toUpperCase() : "?"
                font: FluTextStyle.BodyStrong
                color: "#ffffff"
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            FluText {
                text: root.groupName
                font: FluTextStyle.BodyStrong
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            FluText {
                text: qsTr("%1 Hosts").arg(root.hostCount)
                font: FluTextStyle.Caption
                color: FluTheme.fontSecondaryColor
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                contextMenu.popup()
            }
        }
    }

    FluMenu {
        id: contextMenu

        FluMenuItem {
            text: qsTr("Rename group")
            onClicked: root.renameRequested(root.groupName)
        }
    }
}
