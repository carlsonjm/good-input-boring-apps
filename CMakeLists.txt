cmake_minimum_required(VERSION 3.22)

project(good-input-office VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(GNUInstallDirs)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)
qt_standard_project_setup()

# LibreOfficeKit is header-only: the program finds LibreOffice at run time.
find_path(LIBREOFFICEKIT_INCLUDE_DIR LibreOfficeKit/LibreOfficeKit.hxx
    PATHS /usr/include /usr/include/libreoffice /usr/lib/libreoffice/sdk/include
    REQUIRED)

# The engine (one LibreOffice per process, its own thread), the page view and
# the screens, shared by the program and the tests.
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
# The screens sit at the module's root, so each one sees Theme as the module's
# singleton rather than as a plain file beside it.
file(GLOB OFFICE_QML RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS qml/*.qml qml/icons/*.svg qml/icons/ink/*.svg)
foreach(file ${OFFICE_QML})
    string(REGEX REPLACE "^qml/" "" alias ${file})
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${alias})
endforeach()
# Qt 6.4's compiled bindings loop when they look a singleton up; older Qt
# reads the screens as written instead.
set(OFFICE_QML_CACHE "")
if(Qt6_VERSION VERSION_LESS 6.5)
    set(OFFICE_QML_CACHE NO_CACHEGEN)
endif()
qt_add_library(office-screens STATIC)
qt_add_qml_module(office-screens
    URI co.goodinput.office
    VERSION 1.0
    ${OFFICE_QML_CACHE}
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/co/goodinput/office
    SOURCES
        src/engine/Engine.cpp src/engine/Engine.h
        src/engine/OfficeDocument.cpp src/engine/OfficeDocument.h
        src/view/PageView.cpp src/view/PageView.h
        src/app/Recent.cpp src/app/Recent.h
    QML_FILES
        qml/Theme.qml
        qml/Pill.qml
        qml/IconPill.qml
        qml/Paper.qml
        qml/DocsWindow.qml
        qml/TopLine.qml
        qml/SelectionBar.qml
        qml/StyleSheet.qml
        qml/HomeView.qml
        qml/FindBar.qml
    RESOURCES
        qml/icons/chevron-left.svg
        qml/icons/undo-2.svg
        qml/icons/redo-2.svg
        qml/icons/ellipsis.svg
        qml/icons/highlighter.svg
        qml/icons/link.svg
        qml/icons/message-circle.svg
        qml/icons/list.svg
        qml/icons/list-ordered.svg
        qml/icons/list-checks.svg
        qml/icons/align-left.svg
        qml/icons/align-center.svg
        qml/icons/search.svg
        qml/icons/plus.svg
        qml/icons/file-text.svg
        qml/icons/chevron-right.svg
        qml/icons/x.svg
        qml/icons/ink/chevron-left.svg
        qml/icons/ink/undo-2.svg
        qml/icons/ink/redo-2.svg
        qml/icons/ink/ellipsis.svg
        qml/icons/ink/highlighter.svg
        qml/icons/ink/link.svg
        qml/icons/ink/message-circle.svg
        qml/icons/ink/list.svg
        qml/icons/ink/list-ordered.svg
        qml/icons/ink/list-checks.svg
        qml/icons/ink/align-left.svg
        qml/icons/ink/align-center.svg
        qml/icons/ink/search.svg
        qml/icons/ink/plus.svg
        qml/icons/ink/file-text.svg
        qml/icons/ink/chevron-right.svg
        qml/icons/ink/x.svg
)
target_include_directories(office-screens PUBLIC src/engine src/view src/app ${LIBREOFFICEKIT_INCLUDE_DIR})
target_link_libraries(office-screens PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick ${CMAKE_DL_LIBS})

qt_add_executable(good-input-office src/app/main.cpp)
target_link_libraries(good-input-office PRIVATE office-screens office-screensplugin Qt6::QuickControls2)
install(TARGETS good-input-office RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES
    data/co.goodinput.Docs.desktop
    data/co.goodinput.Sheets.desktop
    data/co.goodinput.Slides.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
