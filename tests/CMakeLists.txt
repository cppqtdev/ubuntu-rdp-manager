set(URDM_FIXTURE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

function(urdm_add_test name)
    cmake_parse_arguments(T "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${T_SOURCES})
    target_link_libraries(${name} PRIVATE urdm_core Qt6::Test ${T_LIBS})
    target_compile_definitions(${name} PRIVATE URDM_FIXTURE_DIR="${URDM_FIXTURE_DIR}")
    urdm_set_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

add_subdirectory(unit)
