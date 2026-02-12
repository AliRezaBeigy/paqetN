import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import FluentUI

FluPopup {
    id: control
    property string title: ""
    property string message: ""
    property string negativeText: qsTr("Cancel")
    property string positiveText: qsTr("OK")

    signal negativeClicked
    signal positiveClicked

    implicitWidth: 400
    // Let height be determined by content
    height: contentColumn.height
    focus: true

    ColumnLayout {
        id: contentColumn
        width: parent.width
        spacing: 0

        // Title
        FluText {
            font: FluTextStyle.Title
            text: control.title
            Layout.fillWidth: true
            Layout.topMargin: 20
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            wrapMode: Text.WordWrap
        }

        // Message
        FluText {
            font: FluTextStyle.Body
            text: control.message
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 20
            wrapMode: Text.WordWrap
        }

        // Button area
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            radius: 5
            color: FluTheme.dark ? Qt.rgba(32/255, 32/255, 35/255, 1) : Qt.rgba(243/255, 243/255, 243/255, 1)

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    FluButton {
                        text: control.negativeText
                        width: parent.width
                        anchors.centerIn: parent
                        onClicked: {
                            control.negativeClicked()
                            control.close()
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    FluFilledButton {
                        text: control.positiveText
                        width: parent.width
                        anchors.centerIn: parent
                        onClicked: {
                            control.positiveClicked()
                            control.close()
                        }
                    }
                }
            }
        }
    }
}
