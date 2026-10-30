set(FIXTURES ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)

qt_add_executable(tst_engine tst_engine.cpp)
target_link_libraries(tst_engine PRIVATE office-screens Qt6::Test)
target_compile_definitions(tst_engine PRIVATE FIXTURES="${FIXTURES}")
add_test(NAME engine COMMAND tst_engine)
# Every test runs with its own home, so the engine's profile and the recent
# list never touch a real person's.
set_tests_properties(engine PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;HOME=${CMAKE_CURRENT_BINARY_DIR}/home;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.local/share;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.config"
    TIMEOUT 180)

# Not a test: draws the screens into pictures for looking at them by hand.
qt_add_executable(office-snap snap.cpp)
target_link_libraries(office-snap PRIVATE office-screens office-screensplugin Qt6::Test Qt6::QuickControls2)
# Not a test: builds the ready-made sheets in templates/ (tools/make-templates.sh).
qt_add_executable(office-templates ../tools/make-templates.cpp)
target_link_libraries(office-templates PRIVATE office-screens Qt6::Test)
