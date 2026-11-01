# Runs the program from outside the build, with no QML import path, and
# checks it drew its first screen into a picture.
file(REMOVE ${PICTURE})
get_filename_component(elsewhere ${PICTURE} DIRECTORY)
execute_process(COMMAND ${CMAKE_COMMAND} -E env --unset=QML_IMPORT_PATH --unset=QML2_IMPORT_PATH
                    GOODINPUT_OFFICE_SNAPSHOT=${PICTURE} ${PROGRAM} --sheets
                WORKING_DIRECTORY /
                RESULT_VARIABLE result)
if(NOT result EQUAL 0 OR NOT EXISTS ${PICTURE})
    message(FATAL_ERROR "the program did not start (exit ${result})")
endif()
