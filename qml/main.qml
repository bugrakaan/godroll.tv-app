import QtQuick
import QtQuick.Window
import QtQuick.Layouts

Window {
    id: root
    width: 700
    height: searchWindowComponent.height + 20  // Dynamic height based on content
    visible: false  // Show after the window has completed initialization
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    color: "transparent"
    opacity: 0  // Start invisible, animate in

    // Track loading state
    property bool isLoading: true
    property bool bootComplete: false
    
    // Loading message text (changes for manifest reload)
    property string loadingMessage: "Loading weapons..."
    
    // Track if we're in the process of hiding (for animation)
    property bool isHiding: false
    // Prevent initial/deferred activation events from being mistaken for focus loss.
    property bool isShowing: false
    // Track if ESC was pressed (to do reset after hide animation)
    property bool escPressed: false
    
    // Track if update dialog is shown
    property bool updateDialogShown: false
    property bool blockingModalVisible: updateDialog.visible || shortcutDialog.visible
    
    // Track if this is a manual update check (should always show dialog)
    property bool manualUpdateCheck: false

    // Opacity animation for smooth show/hide
    Behavior on opacity {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutCubic
        }
    }
    
    NumberAnimation {
        id: hideAnimation
        target: root
        property: "opacity"
        to: 0
        duration: 200
        easing.type: Easing.OutCubic
        onStopped: {
            if (root.isHiding && root.opacity <= 0.001) {
                if (escPressed) {
                    // ESC was pressed, do reset
                    searchModel.clearSearch()
                    searchWindowComponent.resetScrollPosition()
                    escPressed = false
                }
                root.hide()
                isHiding = false
                root.isShowing = false
            }
        }
    }

    // Center on screen
    Component.onCompleted: {
        x = (Screen.width - width) / 2
        y = (Screen.height - height) / 3
    }

    function completeBoot() {
        if (root.bootComplete)
            return
        root.bootComplete = true

        if (!startHidden) {
            Qt.callLater(function() { root.forceShowWindow() })
        }

        if (!hotkey.registered) {
            Qt.callLater(function() {
                root.forceShowWindow()
                shortcutDialog.openDialog(true)
            })
        }

        // Background checks begin only after the main boot path completes.
        updateCheckTimer.start()
    }
    
    // Check for updates when window becomes visible (max once per 4 hours)
    onVisibleChanged: {
        if (visible && !isLoading) {
            updateChecker.checkForUpdatesIfNeeded()
        }
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
                    root.forceShowWindow()
                }
                updateDialog.show()
            }
        }
        
        function onJustUpdated(version) {
            // Show a notification that we just updated
            console.log("Just updated to version:", version)
            // Show window if hidden
            if (!root.visible) {
                root.forceShowWindow()
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
            root.loadingMessage = "Loading weapons..."
            searchWindowComponent.focusSearchInput()
        }
    }

    // Handle manifest checker
    Connections {
        target: manifestChecker
        function onManifestChanged() {
            root.loadingMessage = "Updating weapon database..."
        }
    }

    // Hide when focus is lost (no reset, just hide with animation)
    onActiveChanged: {
        if (active && !isLoading) {
            // Check manifest when app comes to foreground
            manifestChecker.checkIfNeeded()
        }
        if (active) {
            isShowing = false
        }
        if (!active && visible && !blockingModalVisible &&
                !ignoreFocusLoss && !isHiding && !isShowing) {
            hideWindow(false)
        }
    }

    Connections {
        target: hotkey
        function onActivated() {
            if (root.bootComplete)
                toggleWindow()
        }
    }

    Connections {
        target: trayIcon
        function onShowHideRequested() {
            if (root.bootComplete)
                toggleWindow()
        }
        function onForceShowRequested() {
            if (root.bootComplete)
                forceShowWindow()
        }
        function onHotkeyEditingStarted() {
            if (!root.bootComplete)
                return
            root.ignoreFocusLoss = true
            root.forceShowWindow()
        }
        function onHotkeyEditingFinished() {
            if (!root.bootComplete)
                return
            root.ignoreFocusLoss = false
            root.forceShowWindow()
        }
        function onHotkeyEditorRequested() {
            if (!root.bootComplete)
                return
            root.forceShowWindow()
            Qt.callLater(function() { shortcutDialog.openDialog(false) })
        }
        function onCheckForUpdatesRequested() {
            // Show window first if hidden
            if (!root.visible) {
                root.forceShowWindow()
            }
            // Reset the update dialog shown flag to allow showing again
            root.updateDialogShown = false
            root.manualUpdateCheck = true
            updateChecker.checkForUpdates()
        }
    }

    function toggleWindow() {
        if (root.blockingModalVisible) {
            forceShowWindow()
            return
        }
        // Treat stale visible-but-transparent/hiding states as hidden and recover them.
        if (!root.visible || root.opacity <= 0.001 || root.isHiding) {
            forceShowWindow()
        } else {
            hideWindow(false)
        }
    }

    function forceShowWindow() {
        hideAnimation.stop()
        root.isHiding = false
        root.isShowing = true
        root.show()
        root.opacity = 1
        root.raise()

        // Activation can be ignored if requested in the same event that creates/shows
        // a native tool window, so repeat it on the next event-loop turn.
        Qt.callLater(function() {
            root.show()
            root.opacity = 1
            root.raise()
            root.requestActivate()
            if (shortcutDialog.visible)
                shortcutDialog.focusInput()
            else
                searchWindowComponent.refocusOnly()
        })
    }

    function hideWindow(resetAfterHide) {
        if (!root.bootComplete || !root.visible || root.isHiding)
            return
        root.escPressed = resetAfterHide
        root.isShowing = false
        root.isHiding = true
        hideAnimation.restart()
    }

    // Click outside to close (no reset, just hide with animation)
    MouseArea {
        anchors.fill: parent
        onClicked: {
            root.hideWindow(false)
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
            root.forceShowWindow()
        }
    }

    SearchWindow {
        id: searchWindowComponent
        anchors.centerIn: parent
        isLoading: root.isLoading
        loadingMessage: root.loadingMessage
        onClose: {
            // ESC triggers reset + hide with animation
            root.hideWindow(true)
        }
        onRefocusNeeded: {
            // Set flag to ignore the focus loss that will happen
            root.ignoreFocusLoss = true
            refocusTimer.restart()
        }
        onChangeShortcutRequested: shortcutDialog.openDialog(!hotkey.registered)
    }
    
    // Update Dialog overlay - blocks mouse events when dialog is visible
    Rectangle {
        id: updateDialogOverlay
        anchors.fill: parent
        color: "transparent"
        visible: updateDialog.visible || shortcutDialog.visible
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

    ShortcutDialog {
        id: shortcutDialog
        anchors.centerIn: parent
        z: 100
        onClosed: searchWindowComponent.refocusOnly()
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
