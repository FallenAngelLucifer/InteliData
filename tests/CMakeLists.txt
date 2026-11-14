# tests/CMakeLists.txt

add_executable(intelidata_tests
    test_welford.cpp
    test_entropy.cpp
    test_benford.cpp
    test_reservoir.cpp
    test_string_metrics.cpp
    test_pind.cpp
    test_dependency_graph.cpp
    test_data_adapter.cpp
    test_duckdb_adapter.cpp
    test_markov.cpp
    test_spectral.cpp
    test_svd.cpp
    test_ast.cpp
    test_engine.cpp
    test_domain_classifier.cpp
)

target_link_libraries(intelidata_tests
    PRIVATE
        intelidata_core
        Catch2::Catch2WithMain
)

target_apply_intelidata_compiler_flags(intelidata_tests)

if(TARGET imgui)
    target_sources(intelidata_tests PRIVATE
        test_markdown.cpp
        ../src/gui/MarkdownRenderer.cpp
    )
    target_link_libraries(intelidata_tests PRIVATE imgui)
endif()

if(WIN32 AND TARGET DuckDB::DuckDB)
    add_custom_command(TARGET intelidata_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${duckdb_bin_SOURCE_DIR}/duckdb.dll"
            $<TARGET_FILE_DIR:intelidata_tests>
        COMMENT "[Windows] Copiando duckdb.dll al directorio de intelidata_tests..."
    )
endif()

if(MSVC)
    set_target_properties(intelidata_tests PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
endif()

set(TEST_SRC_FILES
    test_welford.cpp
    test_entropy.cpp
    test_benford.cpp
    test_reservoir.cpp
    test_string_metrics.cpp
    test_pind.cpp
    test_dependency_graph.cpp
    test_data_adapter.cpp
    test_duckdb_adapter.cpp
    test_markov.cpp
    test_spectral.cpp
    test_svd.cpp
    test_ast.cpp
    test_engine.cpp
    test_domain_classifier.cpp
)
if(TARGET imgui)
    list(APPEND TEST_SRC_FILES test_markdown.cpp)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${TEST_SRC_FILES})

include(CTest)
include(Catch)
catch_discover_tests(intelidata_tests)
