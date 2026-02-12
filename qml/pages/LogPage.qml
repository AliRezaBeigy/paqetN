import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0

FluPage {
    id: logPage
    title: qsTr("Log")
    padding: 0
    background: Rectangle {
        color: FluTheme.dark ? Qt.rgba(18/255, 18/255, 20/255, 1) : FluTheme.windowBackgroundColor
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Top action bar
        FluFrame {
            Layout.fillWidth: true
            padding: 12

            RowLayout {
                anchors.fill: parent
                spacing: 12

                FluIconButton {
                    iconSource: FluentIcons.Copy
                    text: qsTr("Copy all")
                    display: Button.TextBesideIcon
                    onClicked: paqetController.copyToClipboard(paqetController.logText)
                }
                FluIconButton {
                    iconSource: FluentIcons.Export
                    text: qsTr("Export")
                    display: Button.TextBesideIcon
                    onClicked: exportLogDialog.open()
                }
                FluIconButton {
                    iconSource: FluentIcons.Clear
                    text: qsTr("Clear")
                    display: Button.TextBesideIcon
                    onClicked: paqetController.clearLog()
                }
                FluCheckBox {
                    id: logWordWrap
                    text: qsTr("Word wrap")
                    checked: true
                }
                FluCheckBox {
                    id: autoScrollCheck
                    text: qsTr("Auto scroll")
                    checked: true
                }
                Item { Layout.fillWidth: true }
            }
        }

        // Log text area
        FluFrame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            padding: 0

            Flickable {
                id: logFlickable
                anchors.fill: parent
                anchors.margins: 4
                clip: true
                contentWidth: logText.width
                contentHeight: logText.height
                boundsBehavior: Flickable.StopAtBounds
                
                ScrollBar.vertical: FluScrollBar {
                    id: verticalScrollBar
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: 0
                    policy: ScrollBar.AsNeeded
                }

                TextEdit {
                    id: logText
                    width: logFlickable.width
                    text: paqetController.logText
                    wrapMode: logWordWrap.checked ? TextEdit.Wrap : TextEdit.NoWrap
                    readOnly: true
                    selectByMouse: true
                    font.family: "Consolas"
                    font.pixelSize: 14
                    color: FluTheme.fontPrimaryColor
                    
                    // Update Flickable content height when text changes
                    onTextChanged: {
                        Qt.callLater(function() {
                            logFlickable.contentHeight = logText.implicitHeight
                            // Auto-scroll to bottom when enabled and user is near bottom
                            if (autoScrollCheck.checked) {
                                var scrollPos = logFlickable.contentY / Math.max(1, logFlickable.contentHeight - logFlickable.height)
                                if (scrollPos > 0.95 || scrollPos === 0 || logFlickable.contentHeight <= logFlickable.height) {
                                    logFlickable.contentY = Math.max(0, logText.implicitHeight - logFlickable.height)
                                }
                            }
                        })
                    }
                }
            }
        }
    }
}
