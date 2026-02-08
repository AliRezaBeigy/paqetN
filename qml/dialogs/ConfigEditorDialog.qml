import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0

FluPopup {
    id: root
    implicitWidth: Math.min(580, window.width - 80)
    implicitHeight: Math.min(layout_root.height, window.height - 100)
    property string configId: ""
    property var config: ({})

    signal saved(var configMap)

    property var kcpBlockOptions: ["aes", "aes-128", "aes-128-gcm", "aes-192", "salsa20", "blowfish", "twofish", "cast5", "3des", "tea", "xtea", "xor", "sm4", "none"]
    property var kcpModeOptions: ["normal", "fast", "fast2", "fast3", "manual"]

    onVisibleChanged: {
        if (visible) populateFields()
    }

    // Helper function to safely convert flag value to array, handling Qt's QVariantList conversion quirks
    // Qt may convert QVariantList to JavaScript arrays, strings, or array-like objects
    function flagValueToArray(flagValue) {
        if (flagValue === undefined || flagValue === null) {
            return []
        }
        // Handle proper JavaScript arrays - most common case
        if (Array.isArray(flagValue)) {
            return flagValue.map(function(v) { return String(v).trim() }).filter(function(v) { return v.length > 0 })
        }
        // Handle strings - Qt might convert single-element QVariantList ["PA"] to string "PA"
        if (typeof flagValue === "string") {
            var trimmed = flagValue.trim()
            return trimmed ? [trimmed] : []
        }
        // Handle array-like objects (QVariantList converted to object with numeric indices)
        // This handles cases where Qt converts to an object instead of a proper array
        try {
            var result = []
            // Check for length property (but not strings, already handled above)
            if (typeof flagValue.length === "number") {
                // Convert to proper array using Array.from or manual iteration
                for (var i = 0; i < flagValue.length; i++) {
                    var val = flagValue[i]
                    if (val !== undefined && val !== null) {
                        var strVal = String(val).trim()
                        if (strVal.length > 0) {
                            result.push(strVal)
                        }
                    }
                }
                if (result.length > 0) return result
            }
            // Try Object.keys for objects with numeric string keys like {"0": "PA"}
            var keys = Object.keys(flagValue).filter(function(k) {
                var num = parseInt(k)
                return !isNaN(num) && num >= 0
            }).sort(function(a, b) { return parseInt(a) - parseInt(b) })
            if (keys.length > 0) {
                for (var j = 0; j < keys.length; j++) {
                    var val = flagValue[keys[j]]
                    if (val !== undefined && val !== null) {
                        var strVal = String(val).trim()
                        if (strVal.length > 0) {
                            result.push(strVal)
                        }
                    }
                }
                if (result.length > 0) return result
            }
        } catch(e) {
            // Silently handle errors
        }
        // Final fallback: convert entire value to string
        var str = String(flagValue).trim()
        return str ? [str] : []
    }

    function populateFields() {
        if (config.name !== undefined) nameField.text = config.name; else nameField.text = ""
        if (config.group !== undefined) groupField.text = config.group; else groupField.text = ""
        if (config.serverAddr !== undefined) serverAddrField.text = config.serverAddr; else serverAddrField.text = ""
        if (config.kcpKey !== undefined) kcpKeyField.text = config.kcpKey; else kcpKeyField.text = ""
        if (config.kcpBlock !== undefined) {
            var bi = kcpBlockOptions.indexOf(config.kcpBlock)
            kcpBlockCombo.currentIndex = bi >= 0 ? bi : 0
        } else kcpBlockCombo.currentIndex = 0
        if (config.socksListen !== undefined) socksListenField.text = config.socksListen; else socksListenField.text = "127.0.0.1:1284"
        if (config.conn !== undefined) connField.text = String(config.conn); else connField.text = "1"
        if (config.kcpMode !== undefined) {
            var mi = kcpModeOptions.indexOf(config.kcpMode)
            kcpModeCombo.currentIndex = mi >= 0 ? mi : 1
        } else kcpModeCombo.currentIndex = 1
        if (config.mtu !== undefined) mtuField.text = String(config.mtu); else mtuField.text = "1350"
        // Use helper function to safely convert flags to arrays
        var localFlagArray = flagValueToArray(config.localFlag)
        localFlagField.text = localFlagArray.length > 0 ? localFlagArray.join(", ") : (config.localFlag === undefined ? "PA" : "")

        var remoteFlagArray = flagValueToArray(config.remoteFlag)
        remoteFlagField.text = remoteFlagArray.length > 0 ? remoteFlagArray.join(", ") : (config.remoteFlag === undefined ? "PA" : "")
    }

    function collectAndSave() {
        var c = {}
        c.id = configId
        c.name = nameField.text
        c.group = groupField.text.trim()
        c.serverAddr = serverAddrField.text
        c.kcpKey = kcpKeyField.text
        c.kcpBlock = (kcpBlockCombo.currentIndex >= 0 ? kcpBlockOptions[kcpBlockCombo.currentIndex] : "aes")
        c.socksListen = socksListenField.text
        c.conn = parseInt(connField.text) || 1
        c.kcpMode = (kcpModeCombo.currentIndex >= 0 ? kcpModeOptions[kcpModeCombo.currentIndex] : "fast")
        c.mtu = parseInt(mtuField.text) || 1350
        // Parse comma-separated TCP flags into arrays
        // Allow empty arrays - don't force default "PA" here, let withDefaults() handle it only for new configs
        var localText = localFlagField.text.trim()
        c.localFlag = localText ? localText.split(',').map(function(s) { return s.trim() }).filter(function(s) { return s.length > 0 }) : []
        var remoteText = remoteFlagField.text.trim()
        c.remoteFlag = remoteText ? remoteText.split(',').map(function(s) { return s.trim() }).filter(function(s) { return s.length > 0 }) : []
        // Network settings are auto-detected at connect time, not stored in config
        saved(c)
    }

    ColumnLayout {
        id: layout_root
        width: parent.width
        spacing: 0

        // Title
        FluText {
            text: root.configId ? qsTr("Edit config") : qsTr("Add config")
            font: FluTextStyle.Title
            topPadding: 20
            leftPadding: 20
            rightPadding: 20
            bottomPadding: 8
        }

        // Scrollable form
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: Math.min(contentHeight, window.height - 200)
            contentHeight: formLayout.height
            contentWidth: width
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: FluScrollBar {}

            ColumnLayout {
                id: formLayout
                width: parent.width - 40
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12

                // Connection section
                FluText {
                    text: qsTr("Connection")
                    font: FluTextStyle.BodyStrong
                    color: FluTheme.fontSecondaryColor
                    topPadding: 4
                    bottomPadding: 4
                }

                GridLayout {
                    columns: 2
                    rowSpacing: 12
                    columnSpacing: 12
                    Layout.fillWidth: true

                    FluText { text: qsTr("Name"); font: FluTextStyle.Body }
                    FluTextBox { id: nameField; Layout.fillWidth: true; placeholderText: qsTr("Optional display name") }
                    FluText { text: qsTr("Group"); font: FluTextStyle.Body }
                    FluTextBox { id: groupField; Layout.fillWidth: true; placeholderText: qsTr("Optional group name") }
                    FluText { text: qsTr("Server address"); font: FluTextStyle.Body }
                    FluTextBox { id: serverAddrField; Layout.fillWidth: true; placeholderText: "host:port" }
                    FluText { text: qsTr("Encryption"); font: FluTextStyle.Body }
                    FluComboBox {
                        id: kcpBlockCombo
                        Layout.fillWidth: true
                        model: kcpBlockOptions
                    }
                }

                // KCP section
                Rectangle { Layout.fillWidth: true; height: 1; color: FluTheme.dividerColor; Layout.topMargin: 8 }
                FluText {
                    text: qsTr("KCP")
                    font: FluTextStyle.BodyStrong
                    color: FluTheme.fontSecondaryColor
                    topPadding: 8
                    bottomPadding: 4
                }

                GridLayout {
                    columns: 2
                    rowSpacing: 12
                    columnSpacing: 12
                    Layout.fillWidth: true

                    FluText { text: qsTr("KCP key"); font: FluTextStyle.Body }
                    FluPasswordBox { id: kcpKeyField; Layout.fillWidth: true }
                    FluText { text: qsTr("KCP mode"); font: FluTextStyle.Body }
                    FluComboBox {
                        id: kcpModeCombo
                        Layout.fillWidth: true
                        model: kcpModeOptions
                    }
                    FluText { text: qsTr("MTU"); font: FluTextStyle.Body }
                    FluTextBox { id: mtuField; Layout.fillWidth: true; placeholderText: "1350"; validator: IntValidator { bottom: 50; top: 1500 } }
                }

                // Advanced section
                Rectangle { Layout.fillWidth: true; height: 1; color: FluTheme.dividerColor; Layout.topMargin: 8 }
                FluText {
                    text: qsTr("Advanced")
                    font: FluTextStyle.BodyStrong
                    color: FluTheme.fontSecondaryColor
                    topPadding: 8
                    bottomPadding: 4
                }

                GridLayout {
                    columns: 2
                    rowSpacing: 12
                    columnSpacing: 12
                    Layout.fillWidth: true

                    FluText { text: qsTr("SOCKS listen"); font: FluTextStyle.Body }
                    FluTextBox { id: socksListenField; Layout.fillWidth: true; placeholderText: "127.0.0.1:1284" }
                    FluText { text: qsTr("Connections"); font: FluTextStyle.Body }
                    FluTextBox { id: connField; Layout.fillWidth: true; placeholderText: "1"; validator: IntValidator { bottom: 1; top: 256 } }
                    FluText { text: qsTr("Local TCP flags"); font: FluTextStyle.Body }
                    FluTextBox { id: localFlagField; Layout.fillWidth: true; placeholderText: "PA, S" }
                    FluText { text: qsTr("Remote TCP flags"); font: FluTextStyle.Body }
                    FluTextBox { id: remoteFlagField; Layout.fillWidth: true; placeholderText: "PA, S" }
                }

                Item { Layout.preferredHeight: 8 }
            }
        }

        // Footer buttons
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            radius: 5
            color: FluTheme.dark ? Qt.lighter(window.cardColor, 1.05) : Qt.darker(window.cardColor, 1.02)

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                Item { Layout.fillWidth: true; Layout.fillHeight: true }
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    FluButton {
                        text: qsTr("Cancel")
                        width: parent.width
                        anchors.centerIn: parent
                        onClicked: root.close()
                    }
                }
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    FluFilledButton {
                        text: qsTr("OK")
                        textColor: "#FFFFFF"
                        width: parent.width
                        anchors.centerIn: parent
                        onClicked: root.collectAndSave()
                    }
                }
            }
        }
    }
}
