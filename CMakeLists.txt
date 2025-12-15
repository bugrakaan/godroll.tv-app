cmake_minimum_required(VERSION 3.16)

project(GodrollLauncher VERSION 1.1.13 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt packages
find_package(Qt6 REQUIRED COMPONENTS Core Quick Network Widgets)

qt_standard_project_setup()

# Pass version to C++ code
add_definitions(-DAPP_VERSION="${PROJECT_VERSION}")

# Set Qt policies
if(POLICY CMP0071)
  cmake_policy(SET CMP0071 NEW)
endif()

qt_policy(SET QTP0001 NEW)

# Source files
set(SOURCES
    src/main.cpp
    src/weaponsearchmodel.cpp
    src/globalhotkey.cpp
    src/weaponloader.cpp
    src/trayicon.cpp
    src/updatechecker.cpp
)

set(HEADERS
    src/weaponsearchmodel.h
    src/globalhotkey.h
    src/weaponloader.h
    src/trayicon.h
    src/seasonmapping.h
    src/updatechecker.h
)

# Windows icon resource
if(WIN32)
    set(APP_ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
endif()

qt_add_executable(GodrollLauncher
    ${SOURCES}
    ${HEADERS}
    ${APP_ICON_RESOURCE}
)

qt_add_qml_module(GodrollLauncher
    URI GodrollLauncher
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/SearchWindow.qml
        qml/WeaponItem.qml
        qml/UpdateDialog.qml
    RESOURCES
        resources/logo.svg
        resources/search-icon.svg
        resources/fonts/SpaceGrotesk.ttf
)

target_link_libraries(GodrollLauncher PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Network
    Qt6::Widgets
)

# macOS specific - link Carbon framework for global hotkeys
if(APPLE)
    target_link_libraries(GodrollLauncher PRIVATE
        "-framework Carbon"
        "-framework ApplicationServices"
    )
endif()

# Windows specific
if(WIN32)
    set_target_properties(GodrollLauncher PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# macOS specific
if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE AppIcon.icns)
    set(APP_ICON_MACOS "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppIcon.icns")
    set_source_files_properties(${APP_ICON_MACOS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(GodrollLauncher PRIVATE ${APP_ICON_MACOS})
    
    set_target_properties(GodrollLauncher PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "tv.godroll.launcher"
        MACOSX_BUNDLE_BUNDLE_NAME "Godroll Launcher"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in"
        MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
    )
endif()

# Install
install(TARGETS GodrollLauncher
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
