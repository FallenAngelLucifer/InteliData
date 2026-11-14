cmake_minimum_required(VERSION 3.22)
project(InteliData VERSION 0.1.0 LANGUAGES CXX C)

# Configuración básica de C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Opciones de construcción
option(INTELIDATA_BUILD_TESTS "Construir suite de pruebas unitarias" ON)
option(INTELIDATA_ENABLE_AVX2 "Habilitar optimizaciones SIMD AVX2" ON)
option(INTELIDATA_ENABLE_DUCKDB "Habilitar soporte de ingesta DuckDB (Parquet, CSV, SQLite)" ON)
option(INTELIDATA_BUILD_GUI "Construir interfaz gráfica Dear ImGui" OFF)

# Configuración de carpetas jerárquicas para Visual Studio Solution Explorer
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

# Módulos auxiliares de CMake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(Dependencies)

# ==============================================================================
# Biblioteca Principal: intelidata_core
# ==============================================================================
set(INTELIDATA_CORE_HEADERS
    include/intelidata/common/Platform.hpp
    include/intelidata/common/PathUtils.hpp
    include/intelidata/common/Types.hpp
    include/intelidata/core/Welford.hpp
    include/intelidata/core/Entropy.hpp
    include/intelidata/core/Benford.hpp
    include/intelidata/core/StringMetrics.hpp
    include/intelidata/core/PIND.hpp
    include/intelidata/core/Markov.hpp
    include/intelidata/core/Spectral.hpp
    include/intelidata/core/SVDLatent.hpp
    include/intelidata/model/DependencyGraph.hpp
    include/intelidata/model/SemanticAST.hpp
    include/intelidata/ingestion/ReservoirSampler.hpp
    include/intelidata/ingestion/DataAdapter.hpp
    include/intelidata/ingestion/DuckDBAdapter.hpp
    include/intelidata/pipeline/Engine.hpp
    include/intelidata/pipeline/DDLGenerator.hpp
    include/intelidata/pipeline/DomainClassifier.hpp
)

set(INTELIDATA_CORE_SOURCES
    src/core/Welford.cpp
    src/core/Entropy.cpp
    src/core/Benford.cpp
    src/core/StringMetrics.cpp
    src/core/PIND.cpp
    src/core/Markov.cpp
    src/core/Spectral.cpp
    src/core/SVDLatent.cpp
    src/model/DependencyGraph.cpp
    src/model/SemanticAST.cpp
    src/ingestion/DuckDBAdapter.cpp
    src/pipeline/Engine.cpp
    src/pipeline/DDLGenerator.cpp
    src/pipeline/DomainClassifier.cpp
)

add_library(intelidata_core STATIC
    ${INTELIDATA_CORE_HEADERS}
    ${INTELIDATA_CORE_SOURCES}
)

add_library(intelidata::core ALIAS intelidata_core)

target_include_directories(intelidata_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(intelidata_core
    PUBLIC
        Eigen3::Eigen
        nlohmann_json::nlohmann_json
)

if(TARGET DuckDB::DuckDB)
    target_link_libraries(intelidata_core PUBLIC DuckDB::DuckDB)
endif()

target_apply_intelidata_compiler_flags(intelidata_core)

# ==============================================================================
# Binario CLI Ejecutable: intelidata-cli
# ==============================================================================
set(INTELIDATA_CLI_SOURCES
    src/cli/main_cli.cpp
)

add_executable(intelidata-cli
    ${INTELIDATA_CLI_SOURCES}
)

target_include_directories(intelidata-cli
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(intelidata-cli
    PRIVATE
        intelidata_core
)

if(TARGET DuckDB::DuckDB)
    target_link_libraries(intelidata-cli PRIVATE DuckDB::DuckDB)
endif()

target_apply_intelidata_compiler_flags(intelidata-cli)

if(WIN32 AND TARGET DuckDB::DuckDB)
    add_custom_command(TARGET intelidata-cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${duckdb_bin_SOURCE_DIR}/duckdb.dll"
            $<TARGET_FILE_DIR:intelidata-cli>
        COMMENT "[Windows] Copiando duckdb.dll al directorio del ejecutable intelidata-cli..."
    )
endif()

if(MSVC)
    set_target_properties(intelidata-cli PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()

# ==============================================================================
# Binario GUI Ejecutable: intelidata-gui (Dear ImGui + ImPlot + GLFW)
# ==============================================================================
set(INTELIDATA_GUI_HEADERS
    include/intelidata/gui/AnalyticsDashboardView.hpp
    include/intelidata/gui/DataHubView.hpp
    include/intelidata/gui/DataPreviewView.hpp
    include/intelidata/gui/GraphCanvas.hpp
    include/intelidata/gui/InspectorView.hpp
    include/intelidata/gui/MarkdownRenderer.hpp
)

set(INTELIDATA_GUI_SOURCES
    src/gui/AnalyticsDashboardView.cpp
    src/gui/DataHubView.cpp
    src/gui/DataPreviewView.cpp
    src/gui/GraphCanvas.cpp
    src/gui/InspectorView.cpp
    src/gui/main_gui.cpp
    src/gui/MarkdownRenderer.cpp
)

if(INTELIDATA_BUILD_GUI)
    add_executable(intelidata-gui
        ${INTELIDATA_GUI_HEADERS}
        ${INTELIDATA_GUI_SOURCES}
    )

    target_include_directories(intelidata-gui
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    target_link_libraries(intelidata-gui
        PRIVATE
            intelidata_core
            imgui
            implot
    )

    if(TARGET DuckDB::DuckDB)
        target_link_libraries(intelidata-gui PRIVATE DuckDB::DuckDB)
    endif()

    target_apply_intelidata_compiler_flags(intelidata-gui)

    if(WIN32 AND TARGET DuckDB::DuckDB)
        add_custom_command(TARGET intelidata-gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${duckdb_bin_SOURCE_DIR}/duckdb.dll"
                $<TARGET_FILE_DIR:intelidata-gui>
            COMMENT "[Windows] Copiando duckdb.dll al directorio del ejecutable intelidata-gui..."
        )
    endif()

    if(MSVC)
        set_target_properties(intelidata-gui PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endif()
endif()

# ==============================================================================
# Organización Jerárquica para Visual Studio Solution Explorer
# ==============================================================================
set(ALL_PROJECT_FILES
    ${INTELIDATA_CORE_HEADERS}
    ${INTELIDATA_CORE_SOURCES}
    ${INTELIDATA_CLI_SOURCES}
)
if(INTELIDATA_BUILD_GUI)
    list(APPEND ALL_PROJECT_FILES ${INTELIDATA_GUI_HEADERS} ${INTELIDATA_GUI_SOURCES})
endif()
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ALL_PROJECT_FILES})

if(MSVC)
    if(INTELIDATA_BUILD_GUI)
        set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT intelidata-gui)
    else()
        set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT intelidata-cli)
    endif()
endif()

# ==============================================================================
# Suite de Pruebas
# ==============================================================================
if(INTELIDATA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
