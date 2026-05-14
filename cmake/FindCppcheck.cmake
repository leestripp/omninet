# CMake module to find cppcheck
# Creates target Cppcheck if found

find_program(CPPCHECK_EXECUTABLE cppcheck
    DOC "cppcheck static analyzer"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Cppcheck
    REQUIRED_VARS CPPCHECK_EXECUTABLE
)

# Create imported target if cppcheck found
if(CPPCHECK_EXECUTABLE)
    if(NOT TARGET Cppcheck::Cppcheck)
        add_executable(Cppcheck::Cppcheck IMPORTED)
        set_target_properties(Cppcheck::Cppcheck PROPERTIES
            IMPORTED_LOCATION "${CPPCHECK_EXECUTABLE}"
        )
    endif()
    
    # Get cppcheck version
    execute_process(
        COMMAND ${CPPCHECK_EXECUTABLE} --version
        OUTPUT_VARIABLE CPPCHECK_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    message(STATUS "cppcheck found: ${CPPCHECK_EXECUTABLE} (${CPPCHECK_VERSION})")
endif()