cmake_minimum_required(VERSION 3.22)

project(good-input-pdf VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

set(GOODINPUT_PDF_APP_ID "co.goodinput.PDF")

include(CTest)
include(GNUInstallDirs)

# Plain Qt and Poppler, nothing from Plasma: the app is built for Flathub
# first and must run the same on any desktop.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Quick Svg Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER REQUIRED IMPORTED_TARGET poppler-qt6>=24.02)

# The core: opening, drawing, finding text, forms, marks, signatures and
# saving. It knows nothing of the screen, so the look is built on it alone.
add_library(goodinput-pdf-core STATIC
    src/core/PdfFile.cpp src/core/PdfFile.h
    src/core/FormFields.cpp src/core/FormFields.h
    src/core/Marks.cpp src/core/Marks.h
    src/core/Signature.cpp src/core/Signature.h
    src/core/Saver.cpp src/core/Saver.h
    src/core/ReadingPlace.cpp src/core/ReadingPlace.h
)
target_include_directories(goodinput-pdf-core PUBLIC src/core)
target_link_libraries(goodinput-pdf-core PUBLIC Qt6::Core Qt6::Gui PkgConfig::POPPLER)
target_compile_options(goodinput-pdf-core PRIVATE -Wall -Wextra)

# What the look talks to: the open PDF in page-normalised numbers, and page
# pictures for QML. The look itself (QML) follows once its mock-up is approved.
add_library(goodinput-pdf-session STATIC
    src/app/Session.cpp src/app/Session.h
    src/app/PageImages.cpp src/app/PageImages.h
    src/app/Glyphs.cpp src/app/Glyphs.h
    src/app/Look.cpp src/app/Look.h
)
target_include_directories(goodinput-pdf-session PUBLIC src/app)
target_link_libraries(goodinput-pdf-session PUBLIC goodinput-pdf-core Qt6::Gui Qt6::Quick Qt6::Svg)
target_compile_options(goodinput-pdf-session PRIVATE -Wall -Wextra)

# The app: the approved look, in QML, over the session.
qt_add_executable(good-input-pdf src/main.cpp)
target_compile_definitions(good-input-pdf PRIVATE
    GOODINPUT_PDF_APP_ID="${GOODINPUT_PDF_APP_ID}"
    GOODINPUT_PDF_VERSION="${PROJECT_VERSION}")
set(GOODINPUT_PDF_QML
    qml/Main.qml qml/IconButton.qml qml/Pill.qml qml/Bar.qml
    qml/PageView.qml qml/PageItem.qml qml/SelectionMenu.qml qml/AllPages.qml
    qml/SignSheet.qml qml/PasswordSheet.qml qml/MoreMenu.qml qml/Corners.qml qml/Shadow.qml)
file(GLOB GOODINPUT_PDF_ICONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} qml/icons/*.svg)
qt_add_resources(good-input-pdf ui
    PREFIX /qt/qml/GoodInputPdf
    FILES ${GOODINPUT_PDF_QML})
qt_add_resources(good-input-pdf icons
    PREFIX /qt/qml/GoodInputPdf/icons
    BASE qml/icons
    FILES ${GOODINPUT_PDF_ICONS})
target_link_libraries(good-input-pdf PRIVATE goodinput-pdf-session Qt6::Quick)
install(TARGETS good-input-pdf RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/${GOODINPUT_PDF_APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/${GOODINPUT_PDF_APP_ID}.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES data/${GOODINPUT_PDF_APP_ID}.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

if(BUILD_TESTING)
    add_executable(test-core tests/test_core.cpp)
    target_link_libraries(test-core PRIVATE goodinput-pdf-core Qt6::Test)
    target_compile_definitions(test-core PRIVATE
        FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME core COMMAND test-core)
    set_tests_properties(core PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(test-session tests/test_session.cpp)
    target_link_libraries(test-session PRIVATE goodinput-pdf-session Qt6::Test)
    target_compile_definitions(test-session PRIVATE
        FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME session COMMAND test-session)
    set_tests_properties(session PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_test(NAME screens COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke.sh
        $<TARGET_FILE:good-input-pdf> ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
        ${CMAKE_CURRENT_BINARY_DIR}/screens)
endif()

