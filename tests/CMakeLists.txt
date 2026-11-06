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

include(CTest)
include(Catch)
catch_discover_tests(intelidata_tests)
