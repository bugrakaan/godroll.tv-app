import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: searchWindow
    width: 700
    height: calculatedHeight
    radius: 14
    color: "#CC1a1a1a"  // Semi-transparent dark background (80% opacity)
    
    // Enable layer for proper clipping with radius on macOS
    layer.enabled: true
    layer.smooth: true

    signal close()
    signal refocusNeeded()  // Emitted after opening URL in background to refocus window

    // Loading state from parent
    property bool isLoading: false

    // Function to focus search input and select first item (used on initial show)
    function focusSearchInput() {
        searchInput.forceActiveFocus()
        // Auto-select first item when there are results
        if (resultsList.count > 0) {
            resultsList.currentIndex = 0
        }
        // Reset mouse tracking
        mouseHasMoved = false
        lastMousePos = Qt.point(-1, -1)
    }
    
    // Function to just refocus without resetting state (used after middle-click URL open)
    function refocusOnly() {
        searchInput.forceActiveFocus()
    }

    // Function to reset scroll position (called when window is hidden)
    function resetScrollPosition() {
        resultsList.positionViewAtBeginning()
        resultsList.currentIndex = -1
        mouseHasMoved = false
        lastMousePos = Qt.point(-1, -1)
    }

    // Track if mouse has moved since window became visible
    // This prevents hover from activating when window appears under cursor
    property bool mouseHasMoved: false

    // Font - Space Grotesk with Segoe UI fallback
    property string mainFont: "Space Grotesk"

    // Dynamic height calculation
    // Logo: 65 + 10 margin = 75
    // Search input: 60
    // Hint text: ~30 (13px font + padding)
    // Margins: 24*2 = 48
    // Spacing: 16*3 = 48
    // Fixed parts total: ~261
    property int itemHeight: 85
    property int itemSpacing: 6
    property int fixedHeight: 261
    property int maxVisibleItems: 5
    property int noResultsHeight: 60
    property int noResultsBottomMargin: 16
    
    property int calculatedHeight: {
        // Loading state - minimal height
        if (isLoading) {
            return fixedHeight + noResultsHeight
        }
        
        var itemCount = resultsList.count
        if (itemCount === 0) {
            if (searchInput.text.length > 0) {
                // No results for search query - show message
                return fixedHeight + noResultsHeight + noResultsBottomMargin
            } else {
                // Empty state - just fixed height (no message)
                return fixedHeight
            }
        } else {
            var visibleItems = Math.min(itemCount, maxVisibleItems)
            var listHeight = visibleItems * itemHeight + (visibleItems - 1) * itemSpacing
            return fixedHeight + listHeight
        }
    }

    // Simple shadow with border - only on non-macOS or use DropShadow
    Rectangle {
        visible: Qt.platform.os !== "osx"
        anchors.fill: parent
        anchors.margins: -4
        z: -1
        radius: 18
        color: "transparent"
        border.color: "#10000000"
        border.width: 4
    }
    
    // Track last mouse position to detect actual movement
    property point lastMousePos: Qt.point(-1, -1)
    
    // Function to check if mouse actually moved (not just window appeared under cursor)
    function checkMouseMoved(mouseX, mouseY) {
        if (!mouseHasMoved) {
            if (lastMousePos.x < 0) {
                // First position - just record it
                lastMousePos = Qt.point(mouseX, mouseY)
            } else if (Math.abs(mouseX - lastMousePos.x) > 3 || Math.abs(mouseY - lastMousePos.y) > 3) {
                // Mouse moved more than 3 pixels - consider it a real movement
                mouseHasMoved = true
            }
        }
        return mouseHasMoved
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        // Logo - Text based GODROLL TV with SVG icon
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 65
            Layout.bottomMargin: 10
            
            // Container for centering
            Item {
                id: logoContainer
                anchors.centerIn: parent
                width: logoRow.width
                height: logoRow.height
                
                Row {
                    id: logoRow
                    spacing: 0
                    
                    // Logo SVG icon
                    Image {
                        id: logoIcon
                        source: "qrc:/qt/qml/GodrollLauncher/resources/logo.svg"
                        width: 40
                        height: 40
                        fillMode: Image.PreserveAspectFit
                        anchors.verticalCenter: parent.verticalCenter
                        smooth: true
                        mipmap: true
                    }
                    
                    Item { width: 12; height: 1 }  // Spacer
                    
                    // GODROLL text
                    Text {
                        id: godrollText
                        text: "GODROLL"
                        font.family: searchWindow.mainFont
                        font.pixelSize: 36
                        font.weight: Font.Bold
                        font.letterSpacing: 2
                        color: "#ffffff"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    
                    // TV superscript - slightly below top of GODROLL text
                    Text {
                        text: "TV"
                        font.family: searchWindow.mainFont
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        color: "#09d7d0"
                        anchors.top: godrollText.top
                        anchors.topMargin: 6
                    }
                }
                
                MouseArea {
                    anchors.fill: logoRow
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    
                    onClicked: Qt.openUrlExternally("https://godroll.tv")
                    
                    ToolTip.visible: containsMouse
                    ToolTip.delay: 300
                    ToolTip.text: "Open Godroll.tv"
                }
            }
        }

        // Search input
        Rectangle {
            id: searchInputContainer
            Layout.fillWidth: true
            height: 60
            radius: 10
            color: "#B3232323"  // Semi-transparent background (70% opacity)
            border.color: searchInput.activeFocus ? "#09d7d0" : "#2d2d2d"
            border.width: 2
            clip: true  // Prevent text from overflowing

            // Search icon
            Image {
                id: searchIcon
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                height: 22
                source: "qrc:/qt/qml/GodrollLauncher/resources/search-icon.svg"
                opacity: isLoading ? 0.3 : 0.5
                smooth: true
                mipmap: true
            }

            // Helper function to check if text contains a flag character
            function hasFlag(text, flag) {
                var lower = text.toLowerCase()
                // Check for the flag in any -xxx combination (e.g., -h, -!h, -h!, -!*hae, etc.)
                var regex = new RegExp("-[!*hae]*" + flag + "[!*hae]*", "i")
                return regex.test(lower)
            }

            // Helper properties to detect flags (defined on the search input container)
            property bool hasHolofoilFlag: hasFlag(searchInput.text, "h") || 
                                           searchInput.text.toLowerCase().includes("holofoil") || 
                                           searchInput.text.toLowerCase().includes("holo")
            property bool hasUniqueFlag: hasFlag(searchInput.text, "!")
            property bool hasNoLimitFlag: hasFlag(searchInput.text, "\\*")
            property bool hasAdeptFlag: hasFlag(searchInput.text, "a") ||
                                        /\badept\b/i.test(searchInput.text)
            property bool hasExoticFlag: hasFlag(searchInput.text, "e") ||
                                         /\bexotic\b/i.test(searchInput.text)

            // Badges container wrapper (right-aligned inside search input)
            Item {
                id: badgesWrapper
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                width: badgesRow.width
                height: badgesRow.height
                visible: !isLoading && (searchInput.text.length > 0 || searchModel.showLatestSeason)
                z: 10  // Above the input area MouseArea
                
                // MouseArea to catch clicks on badges and prevent window hide
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.ArrowCursor
                    onClicked: function(mouse) {
                        searchInput.forceActiveFocus()
                        mouse.accepted = true
                    }
                }
                
                Row {
                    id: badgesRow
                    spacing: 6

                    // Holofoil flag badge with rainbow animation (same as WeaponItem)
                Rectangle {
                    id: holofoilFlagBadge
                    visible: searchInputContainer.hasHolofoilFlag
                    color: "#2d1f4e"  // Dark purple background
                    border.color: "#8b5cf6"
                    border.width: 1
                    radius: 6
                    width: holofoilFlagText.implicitWidth + 16
                    height: resultCountBadge.height
                    clip: true
                    
                    // Rainbow gradient property for animation
                    property real gradientOffset: 0
                    
                    NumberAnimation on gradientOffset {
                        from: 0
                        to: 1
                        duration: 5000
                        loops: Animation.Infinite
                        running: holofoilFlagBadge.visible
                    }
                    
                    // Animated rainbow gradient overlay
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        opacity: 0.3
                        
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Qt.hsla((holofoilFlagBadge.gradientOffset + 0.0) % 1.0, 0.8, 0.6, 1) }
                            GradientStop { position: 0.2; color: Qt.hsla((holofoilFlagBadge.gradientOffset + 0.2) % 1.0, 0.8, 0.6, 1) }
                            GradientStop { position: 0.4; color: Qt.hsla((holofoilFlagBadge.gradientOffset + 0.4) % 1.0, 0.8, 0.6, 1) }
                            GradientStop { position: 0.6; color: Qt.hsla((holofoilFlagBadge.gradientOffset + 0.6) % 1.0, 0.8, 0.6, 1) }
                            GradientStop { position: 0.8; color: Qt.hsla((holofoilFlagBadge.gradientOffset + 0.8) % 1.0, 0.8, 0.6, 1) }
                            GradientStop { position: 1.0; color: Qt.hsla((holofoilFlagBadge.gradientOffset + 1.0) % 1.0, 0.8, 0.6, 1) }
                        }
                    }
                    
                    Text {
                        id: holofoilFlagText
                        anchors.centerIn: parent
                        text: "HOLOFOIL"
                        font.family: searchWindow.mainFont
                        font.pixelSize: 11
                        font.weight: Font.Bold
                        color: "#ffffff"
                    }
                }

                // Unique flag badge
                Rectangle {
                    visible: searchInputContainer.hasUniqueFlag
                    width: uniqueText.width + 16
                    height: resultCountBadge.height
                    radius: 6
                    color: "#2a1a3a"
                    border.color: "#a855f7"
                    border.width: 1

                    Text {
                        id: uniqueText
                        anchors.centerIn: parent
                        text: "Unique"
                        font.family: searchWindow.mainFont
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        color: "#a855f7"
                    }
                }

                // No Limit flag badge
                Rectangle {
                    visible: searchInputContainer.hasNoLimitFlag
                    width: noLimitText.width + 16
                    height: resultCountBadge.height
                    radius: 6
                    color: "#3a2a1a"
                    border.color: "#f59e0b"
                    border.width: 1

                    Text {
                        id: noLimitText
                        anchors.centerIn: parent
                        text: "All"
                        font.family: searchWindow.mainFont
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        color: "#f59e0b"
                    }
                }

                // Adept flag badge
                Rectangle {
                    visible: searchInputContainer.hasAdeptFlag
                    width: adeptText.width + 16
                    height: resultCountBadge.height
                    radius: 6
                    color: "#1a2a1a"
                    border.color: "#22c55e"
                    border.width: 1

                    Text {
                        id: adeptText
                        anchors.centerIn: parent
                        text: "Adept"
                        font.family: searchWindow.mainFont
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        color: "#22c55e"
                    }
                }

                // Exotic flag badge
                Rectangle {
                    visible: searchInputContainer.hasExoticFlag
                    width: exoticText.width + 16
                    height: resultCountBadge.height
                    radius: 6
                    color: "#2a2510"
                    border.color: "#ffd700"
                    border.width: 1

                    Text {
                        id: exoticText
                        anchors.centerIn: parent
                        text: "Exotic"
                        font.family: searchWindow.mainFont
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        color: "#ffd700"
                    }
                }

                // Result count badge
                Rectangle {
                    id: resultCountBadge
                    width: resultCountRow.width + 16
                    height: 26
                    radius: 6
                    color: "#1a1a1a"

                    Row {
                        id: resultCountRow
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            text: resultsList.count
                            font.family: searchWindow.mainFont
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            color: "#888888"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: resultsList.count === 1 ? "result" : "results"
                            font.family: searchWindow.mainFont
                            font.pixelSize: 13
                            color: "#888888"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
                }  // Close Row (badgesRow)
            }  // Close Item (badgesWrapper)

            TextInput {
                id: searchInput
                anchors.fill: parent
                anchors.leftMargin: 52
                anchors.rightMargin: badgesWrapper.visible ? badgesRow.width + 24 : 20
                verticalAlignment: TextInput.AlignVCenter
                font.family: searchWindow.mainFont
                font.pixelSize: 22
                color: "#ffffff"
                opacity: isLoading ? 0.5 : 1.0  // Only text opacity affected by loading
                selectionColor: "#09d7d0"
                selectedTextColor: "#ffffff"
                selectByMouse: true
                focus: true
                clip: true  // Clip text within bounds
                enabled: !isLoading
                
                // Show I-beam cursor on hover
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.IBeamCursor
                    acceptedButtons: Qt.NoButton  // Don't intercept clicks
                }

                text: searchModel.searchQuery
                onTextChanged: {
                    searchModel.searchQuery = text
                    // Auto-select first item when there are results
                    if (resultsList.count > 0) {
                        resultsList.currentIndex = 0
                    } else {
                        resultsList.currentIndex = -1
                    }
                }

                // Keyboard navigation
                Keys.onUpPressed: {
                    // Enable showLatestSeason when pressing arrow keys without search query
                    if (searchModel.searchQuery.length === 0 && !searchModel.showLatestSeason) {
                        searchModel.showLatestSeason = true
                    }
                    if (resultsList.count > 0) {
                        if (resultsList.currentIndex <= 0) {
                            resultsList.currentIndex = resultsList.count - 1  // Wrap to end
                        } else {
                            resultsList.currentIndex--
                        }
                    }
                }

                Keys.onDownPressed: {
                    // Enable showLatestSeason when pressing arrow keys without search query
                    if (searchModel.searchQuery.length === 0 && !searchModel.showLatestSeason) {
                        searchModel.showLatestSeason = true
                    }
                    if (resultsList.count > 0) {
                        if (resultsList.currentIndex >= resultsList.count - 1) {
                            resultsList.currentIndex = 0  // Wrap to start
                        } else {
                            resultsList.currentIndex++
                        }
                    }
                }

                Keys.onReturnPressed: function(event) {
                    if (resultsList.currentIndex >= 0) {
                        searchModel.openWeapon(resultsList.currentIndex)
                        searchWindow.close()
                    } else if (resultsList.count > 0) {
                        // If nothing selected but there are results, open first one
                        searchModel.openWeapon(0)
                        searchWindow.close()
                    }
                    event.accepted = true
                }

                Keys.onEscapePressed: function(event) {
                    if (searchModel.searchQuery.length > 0) {
                        searchModel.clearSearch()
                    } else {
                        // Reset scroll position before closing
                        resultsList.positionViewAtBeginning()
                        searchWindow.close()
                    }
                    event.accepted = true
                }
                
                // F5 to reload weapon list
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_F5) {
                        weaponLoader.reload()
                        event.accepted = true
                    }
                }
            }
            
            // Placeholder text
            Text {
                anchors.left: searchIcon.right
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: "Search weapons..."
                font.family: searchWindow.mainFont
                font.pixelSize: 22
                color: "#666666"
                visible: searchInput.text.length === 0
            }
            
            // MouseArea to catch clicks on the entire input container (including badges)
            // This prevents the window from hiding when clicking anywhere inside the search input area
            MouseArea {
                anchors.fill: parent
                z: 1  // Below badges MouseArea
                // Let TextInput handle its own mouse events
                propagateComposedEvents: true
                // Show text cursor when hovering over input area (not over badges)
                cursorShape: Qt.IBeamCursor
                
                onClicked: function(mouse) {
                    searchInput.forceActiveFocus()
                    // Don't propagate to prevent window hide
                    mouse.accepted = true
                }
                
                onPressed: function(mouse) {
                    // Allow TextInput to receive the click for text selection
                    mouse.accepted = false
                }
            }
        }

        // Results list
        ListView {
            id: resultsList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: searchModel
            currentIndex: -1
            visible: !isLoading
            
            // Auto-select first item when list is populated
            onCountChanged: {
                if (count > 0) {
                    currentIndex = 0
                } else {
                    currentIndex = -1
                }
                // Reset mouse tracking when list changes
                searchWindow.mouseHasMoved = false
            }
            
            // Ensure selected item is visible
            highlightFollowsCurrentItem: true
            highlightMoveDuration: 100

            // macOS style overlay scrollbar
            ScrollBar.vertical: ScrollBar {
                id: scrollBar
                active: resultsList.moving || hovered || pressed
                policy: ScrollBar.AsNeeded
                parent: resultsList
                anchors.top: resultsList.top
                anchors.right: resultsList.right
                anchors.bottom: resultsList.bottom
                anchors.rightMargin: 2
                width: 12
                
                contentItem: Rectangle {
                    implicitWidth: 10
                    radius: 5
                    color: scrollBar.pressed ? "#09d7d0" : (scrollBar.hovered ? "#888888" : "#555555")
                    opacity: scrollBar.active ? 0.8 : 0.0
                    
                    Behavior on color { ColorAnimation { duration: 100 } }
                    Behavior on opacity { NumberAnimation { duration: 200 } }
                }
                
                background: Item {}
            }

            delegate: WeaponItem {
                width: resultsList.width
                highlighted: resultsList.currentIndex === index
                fontFamily: searchWindow.mainFont
                
                onClicked: {
                    searchModel.openWeapon(index)
                    searchWindow.close()
                }
                
                onMiddleClicked: {
                    // Open weapon and request refocus after browser takes focus
                    searchModel.openWeapon(index)
                    searchWindow.refocusNeeded()
                }
                
                onHoveredChanged: {
                    // Only respond to hover if mouse has moved since window appeared
                    if (hovered && searchWindow.mouseHasMoved) {
                        resultsList.currentIndex = index
                    }
                }
            }
        }

        // Loading message
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: searchWindow.noResultsHeight
            Layout.fillHeight: false
            visible: isLoading

            Row {
                anchors.centerIn: parent
                spacing: 12

                // Loading spinner
                Rectangle {
                    id: spinner
                    width: 20
                    height: 20
                    radius: 10
                    color: "transparent"
                    border.color: "#09d7d0"
                    border.width: 2
                    
                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        color: "#09d7d0"
                        anchors.top: parent.top
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.topMargin: 2
                    }
                    
                    RotationAnimation on rotation {
                        from: 0
                        to: 360
                        duration: 1000
                        loops: Animation.Infinite
                        running: isLoading
                    }
                }

                Text {
                    text: "Loading weapons..."
                    font.family: searchWindow.mainFont
                    font.pixelSize: 18
                    color: "#09d7d0"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // No results message
        Text {
            Layout.fillWidth: true
            Layout.preferredHeight: searchWindow.noResultsHeight
            Layout.fillHeight: false
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 16
            visible: !isLoading && resultsList.count === 0 && searchInput.text.length > 0
            text: "No weapons found for \"" + searchInput.text + "\""
            font.family: searchWindow.mainFont
            font.pixelSize: 18
            color: "#888888"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideMiddle  // Truncate long search terms
            maximumLineCount: 1
        }

        // Keyboard shortcut hint
        Text {
            Layout.fillWidth: true
            text: "Alt+G to toggle • ESC to close • ↑↓ to navigate • Enter to open"
            font.family: searchWindow.mainFont
            font.pixelSize: 13
            color: "#999999"
            horizontalAlignment: Text.AlignHCenter
        }
    }

    Behavior on height {
        NumberAnimation {
            duration: 150
            easing.type: Easing.OutQuad
        }
    }
}
