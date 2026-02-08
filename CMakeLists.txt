cmake_minimum_required(VERSION 3.20)

# Allow APP_VERSION to override PROJECT_VERSION if provided
if(DEFINED APP_VERSION)
    # Parse version string (e.g., "0.1.0" -> major=0, minor=1, patch=0)
    string(REPLACE "." ";" VERSION_LIST ${APP_VERSION})
    list(LENGTH VERSION_LIST VERSION_LENGTH)
    if(VERSION_LENGTH GREATER_EQUAL 1)
        list(GET VERSION_LIST 0 VERSION_MAJOR)
    else()
        set(VERSION_MAJOR 0)
    endif()
    if(VERSION_LENGTH GREATER_EQUAL 2)
        list(GET VERSION_LIST 1 VERSION_MINOR)
    else()
        set(VERSION_MINOR 0)
    endif()
    if(VERSION_LENGTH GREATER_EQUAL 3)
        list(GET VERSION_LIST 2 VERSION_PATCH)
    else()
        set(VERSION_PATCH 0)
    endif()
    project(paqetN VERSION ${VERSION_MAJOR}.${VERSION_MINOR} LANGUAGES CXX)
    set(PROJECT_VERSION_FULL ${APP_VERSION})
else()
    project(paqetN VERSION 0.1 LANGUAGES CXX)
    set(PROJECT_VERSION_FULL ${PROJECT_VERSION})
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Network Widgets Svg PrintSupport Core5Compat)

qt_standard_project_setup(REQUIRES 6.8)

# FluentUI
set(FLUENTUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FLUENTUI_BUILD_STATIC_LIB OFF CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/FluentUI/src)

# QuaZip
set(QUAZIP_QT_MAJOR_VERSION 6 CACHE STRING "" FORCE)
set(QUAZIP_INSTALL OFF CACHE BOOL "" FORCE)
set(QUAZIP_BZIP2 OFF CACHE BOOL "" FORCE)

# Help find zlib in Strawberry Perl installation on Windows
if(WIN32 AND EXISTS "C:/Strawberry")
    set(ZLIB_ROOT "C:/Strawberry/c" CACHE PATH "Path to zlib root")
    set(ZLIB_INCLUDE_DIR "C:/Strawberry/c/include" CACHE PATH "Path to zlib headers")
    set(ZLIB_LIBRARY "C:/Strawberry/c/lib/libz.a" CACHE FILEPATH "Path to zlib library")
endif()

add_subdirectory(3rdparty/quazip)

set(PAQET_SOURCES
    src/PaqetConfig.cpp
    src/ConfigRepository.cpp
    src/ConfigListModel.cpp
    src/SettingsRepository.cpp
    src/LogBuffer.cpp
    src/PaqetRunner.cpp
    src/LatencyChecker.cpp
    src/SingleInstanceGuard.cpp
    src/UpdateManager.cpp
    src/NetworkInfoDetector.cpp
    src/PaqetController.cpp
)

qt_add_executable(apppaqetN
    main.cpp
    ${PAQET_SOURCES}
)

target_include_directories(apppaqetN PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

qt_add_qml_module(apppaqetN
    URI paqetN
    VERSION 1.0
    NO_RESOURCE_TARGET_PATH
    QML_FILES
        qml/Main.qml
        qml/pages/HostsPage.qml
        qml/pages/LogPage.qml
        qml/pages/UpdatesPage.qml
        qml/dialogs/ConfigEditorDialog.qml
        qml/dialogs/SettingsPage.qml
        qml/components/GroupCard.qml
        qml/components/HostCard.qml
        qml/components/DetailPanel.qml
        qml/components/CustomContentDialog.qml
)

# Add application icon resources
qt_add_resources(apppaqetN "app_assets"
    PREFIX "/assets"
    FILES
        assets/icons/app_icon.png
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(apppaqetN PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.apppaqetN
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(apppaqetN
    PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::Widgets Qt6::Svg Qt6::PrintSupport Qt6::Core5Compat fluentuiplugin QuaZip::QuaZip
)

# Define version macro for use in C++ code
target_compile_definitions(apppaqetN PRIVATE
    PAQETN_VERSION="${PROJECT_VERSION_FULL}"
    PAQETN_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    PAQETN_VERSION_MINOR=${PROJECT_VERSION_MINOR}
)


include(GNUInstallDirs)

# Install main executable
install(TARGETS apppaqetN
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install QuaZip library (needed on Windows for DLL)
# On Windows, shared libraries are RUNTIME targets
if(WIN32)
    install(TARGETS QuaZip
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}  # DLLs go with executable on Windows
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}  # Import libraries
    )
else()
    install(TARGETS QuaZip
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()
