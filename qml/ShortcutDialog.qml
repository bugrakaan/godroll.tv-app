import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: shortcutDialog
    width: 450
    height: contentColumn.height + 48
    radius: 14
    color: "#1a1a1a"
    visible: false

    property string mainFont: "Space Grotesk"
    property bool unavailableMode: false
    property string selectedShortcut: ""
    property bool selectionValid: false
    property string validationMessage: ""
    signal closed()

    function validateSelection() {
        if (selectedShortcut.length === 0) {
            selectionValid = false
            validationMessage = "Press a shortcut to continue."
            return
        }
        const result = hotkey.validateShortcutForUi(selectedShortcut)
        selectionValid = result.valid
        validationMessage = result.message
    }

    function openDialog(isUnavailable) {
        unavailableMode = isUnavailable
        hotkey.suspendForShortcutCapture()
        selectedShortcut = hotkey.shortcutText
        validateSelection()
        visible = true
        opacity = 1
        Qt.callLater(function() { shortcutField.forceActiveFocus() })
    }

    function focusInput() {
        shortcutField.forceActiveFocus()
    }

    function closeDialog() {
        visible = false
        hotkey.resumeAfterShortcutCapture()
        closed()
    }

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 24
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Image {
                width: 48
                height: 48
                source: "qrc:/qt/qml/GodrollLauncher/resources/logo.svg"
                fillMode: Image.PreserveAspectFit
                sourceSize: Qt.size(48, 48)
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    Layout.fillWidth: true
                    text: shortcutDialog.unavailableMode
                        ? "Global Shortcut Unavailable"
                        : "Change Global Shortcut"
                    font.family: shortcutDialog.mainFont
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    color: "white"
                }

                Text {
                    Layout.fillWidth: true
                    text: shortcutDialog.unavailableMode
                        ? "Your saved shortcut is already being used. You can choose a new one below."
                        : "Choose the shortcut that toggles Godroll TV."
                    font.family: shortcutDialog.mainFont
                    font.pixelSize: 13
                    color: "#aaaaaa"
                    wrapMode: Text.WordWrap
                }
            }
        }

        TextField {
            id: shortcutField
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            text: shortcutDialog.selectedShortcut
            readOnly: false
            selectByMouse: false
            cursorVisible: false
            horizontalAlignment: TextInput.AlignHCenter
            verticalAlignment: TextInput.AlignVCenter
            font.family: shortcutDialog.mainFont
            font.pixelSize: 16
            font.weight: Font.DemiBold
            color: "white"
            placeholderText: "Press a shortcut"
            placeholderTextColor: "#777777"

            background: Rectangle {
                color: "#252525"
                radius: 8
                border.width: 2
                border.color: shortcutField.activeFocus
                    ? (shortcutDialog.selectionValid ? "#09d7d0" : "#ef6464")
                    : "#3a3a3a"
            }

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Escape) {
                    event.accepted = true
                    shortcutDialog.closeDialog()
                    return
                }
                if (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab) {
                    event.accepted = false
                    return
                }
                event.accepted = true
                const captured = hotkey.shortcutFromKey(event.key, event.modifiers)
                if (captured.length === 0)
                    return
                shortcutDialog.selectedShortcut = captured
                shortcutDialog.validateSelection()
            }
            Keys.onReleased: function(event) { event.accepted = true }
        }

        Text {
            Layout.fillWidth: true
            text: shortcutDialog.validationMessage
            font.family: shortcutDialog.mainFont
            font.pixelSize: 12
            color: shortcutDialog.selectionValid ? "#09d7d0" : "#ef6464"
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            text: "Letters and numbers require Alt, Ctrl, Shift, or Windows. Available function keys can be used alone."
            font.family: shortcutDialog.mainFont
            font.pixelSize: 12
            color: "#888888"
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Item { Layout.fillWidth: true }

            Rectangle {
                id: cancelButton
                Layout.preferredWidth: shortcutDialog.unavailableMode ? 100 : 90
                Layout.preferredHeight: 40
                activeFocusOnTab: true
                color: cancelMouse.containsMouse ? "#333333" : "#252525"
                border.color: activeFocus ? "#09d7d0" : "#555555"
                border.width: activeFocus ? 2 : 1
                radius: 8

                Keys.onEscapePressed: shortcutDialog.closeDialog()
                Keys.onReturnPressed: shortcutDialog.closeDialog()
                Keys.onEnterPressed: shortcutDialog.closeDialog()
                Keys.onSpacePressed: shortcutDialog.closeDialog()

                Text {
                    anchors.centerIn: parent
                    text: shortcutDialog.unavailableMode ? "Not Now" : "Cancel"
                    font.family: shortcutDialog.mainFont
                    font.pixelSize: 14
                    color: "white"
                }

                MouseArea {
                    id: cancelMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: shortcutDialog.closeDialog()
                }
            }

            Rectangle {
                id: saveButton
                Layout.preferredWidth: 120
                Layout.preferredHeight: 40
                activeFocusOnTab: shortcutDialog.selectionValid
                color: !shortcutDialog.selectionValid
                    ? "#555555"
                    : (saveMouse.containsMouse ? "#0bc5bf" : "#09d7d0")
                border.width: activeFocus ? 2 : 0
                border.color: activeFocus ? "white" : "transparent"
                radius: 8

                function saveShortcut() {
                    if (hotkey.setShortcut(shortcutDialog.selectedShortcut)) {
                        shortcutDialog.closeDialog()
                    } else {
                        shortcutDialog.validateSelection()
                    }
                }

                Keys.onEscapePressed: shortcutDialog.closeDialog()
                Keys.onReturnPressed: saveButton.saveShortcut()
                Keys.onEnterPressed: saveButton.saveShortcut()
                Keys.onSpacePressed: saveButton.saveShortcut()

                Text {
                    anchors.centerIn: parent
                    text: "Save"
                    font.family: shortcutDialog.mainFont
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    color: shortcutDialog.selectionValid ? "#1a1a1a" : "#888888"
                }

                MouseArea {
                    id: saveMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: shortcutDialog.selectionValid
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: saveButton.saveShortcut()
                }
            }
        }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        width: 28
        height: 28
        radius: 14
        color: closeMouse.containsMouse ? "#333333" : "transparent"

        Text {
            anchors.centerIn: parent
            text: "✕"
            font.pixelSize: 14
            color: closeMouse.containsMouse ? "white" : "#666666"
        }

        MouseArea {
            id: closeMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: shortcutDialog.closeDialog()
        }
    }
}
