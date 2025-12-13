import QtQuick
import QtQuick.Window
import QtQuick.Layouts

Window {
    id: root
    width: 700
    height: searchWindowComponent.height + 20  // Dynamic height based on content
    visible: !startHidden  // Start hidden if --hidden flag was passed
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | (trayIcon.showInDock ? Qt.Window : Qt.Tool)
    color: "transparent"
    opacity: 0  // Start invisible, animate in

    // Track loading state
    property bool isLoading: true
    
    // Track if we're in the process of hiding (for animation)
    property bool isHiding: false
    // Track if ESC was pressed (to do reset after hide animation)
    property bool escPressed: false
    
    // Track if update dialog is shown
    property bool updateDialogShown: false
    
    // Track if this is a manual update check (should always show dialog)
    property bool manualUpdateCheck: false

    // Opacity animation for smooth show/hide
    Behavior on opacity {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutCubic
        }
    }
    
    // When opacity animation finishes and we're hiding, actually hide the window
    onOpacityChanged: {
        if (opacity === 0 && isHiding) {
            if (escPressed) {
                // ESC was pressed, do reset
                searchModel.clearSearch()
                searchWindowComponent.resetScrollPosition()
                escPressed = false
            }
            root.hide()
            isHiding = false
        }
    }

    // Center on screen
    Component.onCompleted: {
        x = (Screen.width - width) / 2
        y = (Screen.height - height) / 3
        if (!startHidden) {
            root.opacity = 1  // Animate in on first show
            root.raise()
            root.requestActivate()
        }
        
        // Check for updates after a short delay
        updateCheckTimer.start()
    }
    
    // Timer to check for updates after app starts
    Timer {
        id: updateCheckTimer
        interval: 2000  // 2 seconds delay after startup
        onTriggered: {
            root.manualUpdateCheck = false
            updateChecker.checkForUpdates()
        }
    }
    
    // Handle update check result
    Connections {
        target: updateChecker
        function onUpdateCheckComplete(updateAvailable) {
            // Show dialog when: update available OR manual check
            if ((updateAvailable || root.manualUpdateCheck) && !root.updateDialogShown) {
                root.updateDialogShown = true
                // Show window if hidden
                if (!root.visible) {
                    root.show()
                    root.raise()
                    root.requestActivate()
                    root.opacity = 1
                }
                updateDialog.show()
            }
        }
        
        function onJustUpdated(version) {
            // Show a notification that we just updated
            console.log("Just updated to version:", version)
            // Show window if hidden
            if (!root.visible) {
                root.show()
                root.raise()
                root.requestActivate()
                root.opacity = 1
            }
            // Show the update success notification
            updateSuccessNotification.newVersion = version
            updateSuccessNotification.visible = true
        }
    }

    // Keep window vertically centered when height changes
    onHeightChanged: {
        y = (Screen.height - height) / 3
    }

    // Update loading state when weapons are loaded
    Connections {
        target: searchModel
        function onWeaponsLoaded() {
            root.isLoading = false
            searchWindowComponent.focusSearchInput()
        }
    }
    
    // Handle weapon reload (F5)
    Connections {
        target: weaponLoader
        function onReloadStarted() {
            root.isLoading = true
        }
        function onWeaponsLoaded() {
            root.isLoading = false
            searchWindowComponent.focusSearchInput()
        }
    }

    // Hide when focus is lost (no reset, just hide with animation)
    onActiveChanged: {
        if (!active && visible && !ignoreFocusLoss && !isHiding) {
            isHiding = true
            root.opacity = 0
        }
    }

    Connections {
        target: hotkey
        function onActivated() {
            toggleWindow()
        }
    }

    Connections {
        target: trayIcon
        function onShowHideRequested() {
            toggleWindow()
        }
        function onCheckForUpdatesRequested() {
            // Show window first if hidden
            if (!root.visible) {
                root.show()
                root.raise()
                root.requestActivate()
                root.opacity = 1
            }
            // Reset the update dialog shown flag to allow showing again
            root.updateDialogShown = false
            root.manualUpdateCheck = true
            updateChecker.checkForUpdates()
        }
    }

    function toggleWindow() {
        if (root.visible && !isHiding) {
            // Just hide with animation, no reset (reset only happens on ESC)
            isHiding = true
            root.opacity = 0
        } else if (!root.visible) {
            root.show()
            root.raise()
            root.requestActivate()
            searchWindowComponent.refocusOnly()
            root.opacity = 1
        }
    }

    // Click outside to close (no reset, just hide with animation)
    MouseArea {
        anchors.fill: parent
        onClicked: {
            if (!root.isHiding) {
                root.isHiding = true
                root.opacity = 0
            }
        }
    }
    
    // Temporarily ignore focus loss when middle-clicking to open URLs
    property bool ignoreFocusLoss: false
    
    // Timer to refocus window after browser opens
    Timer {
        id: refocusTimer
        interval: 150  // Short delay to let browser open
        onTriggered: {
            root.ignoreFocusLoss = false
            root.show()
            root.raise()
            root.requestActivate()
            searchWindowComponent.refocusOnly()  // Just refocus, don't reset scroll/selection
        }
    }

    SearchWindow {
        id: searchWindowComponent
        anchors.centerIn: parent
        isLoading: root.isLoading
        onClose: {
            // ESC triggers reset + hide with animation
            if (!root.isHiding) {
                root.escPressed = true
                root.isHiding = true
                root.opacity = 0
            }
        }
        onRefocusNeeded: {
            // Set flag to ignore the focus loss that will happen
            root.ignoreFocusLoss = true
            refocusTimer.restart()
        }
    }
    
    // Update Dialog overlay - blocks mouse events when dialog is visible
    Rectangle {
        id: updateDialogOverlay
        anchors.fill: parent
        color: "transparent"
        visible: updateDialog.visible
        z: 99
        
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            onClicked: (mouse) => { mouse.accepted = true }
            onPressed: (mouse) => { mouse.accepted = true }
            onReleased: (mouse) => { mouse.accepted = true }
            onWheel: (wheel) => { wheel.accepted = true }
        }
    }
    
    // Update Dialog - centered overlay
    UpdateDialog {
        id: updateDialog
        anchors.centerIn: parent
        z: 100
        
        onAccepted: {
            console.log("Update download started")
        }
        onRejected: {
            console.log("Update postponed")
        }
        onSkipped: {
            console.log("Update skipped")
        }
    }
    
    // Update Success Notification - shows after app is updated
    Rectangle {
        id: updateSuccessNotification
        property string newVersion: ""
        
        anchors.centerIn: parent
        width: 350
        height: successColumn.height + 40
        radius: 14
        color: "#1a1a1a"
        visible: false
        z: 100
        
        ColumnLayout {
            id: successColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 12
            
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                
                // Checkmark icon
                Rectangle {
                    width: 40
                    height: 40
                    radius: 20
                    color: "#09d7d0"
                    
                    Text {
                        anchors.centerIn: parent
                        text: "✓"
                        font.pixelSize: 20
                        font.bold: true
                        color: "#1a1a1a"
                    }
                }
                
                ColumnLayout {
                    spacing: 2
                    
                    Text {
                        text: "Update Complete!"
                        font.family: "Space Grotesk"
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        color: "white"
                    }
                    
                    Text {
                        text: "Now running version " + updateSuccessNotification.newVersion
                        font.family: "Space Grotesk"
                        font.pixelSize: 13
                        color: "#09d7d0"
                    }
                }
            }
            
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                Layout.topMargin: 8
                color: successOkMouse.containsMouse ? "#0bc5bf" : "#09d7d0"
                radius: 8
                
                Text {
                    anchors.centerIn: parent
                    text: "OK"
                    font.family: "Space Grotesk"
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    color: "#1a1a1a"
                }
                
                MouseArea {
                    id: successOkMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        updateSuccessNotification.visible = false
                        updateChecker.clearUpdateNotification()
                    }
                }
            }
        }
    }

}
