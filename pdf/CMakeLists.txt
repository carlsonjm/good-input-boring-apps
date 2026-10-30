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
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Quick Test)
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
)
target_include_directories(goodinput-pdf-session PUBLIC src/app)
target_link_libraries(goodinput-pdf-session PUBLIC goodinput-pdf-core Qt6::Gui Qt6::Quick)
target_compile_options(goodinput-pdf-session PRIVATE -Wall -Wextra)

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
endif()

# The desktop entry and metainfo install with the app itself, which follows
# the look; data/ holds them already so Flathub's checks run from the start.
