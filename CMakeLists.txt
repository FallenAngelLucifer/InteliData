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

# Módulos auxiliares de CMake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(Dependencies)

# ==============================================================================
# Biblioteca Principal: intelidata_core
# ==============================================================================
set(INTELIDATA_CORE_HEADERS
    include/intelidata/common/Platform.hpp
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
add_executable(intelidata-cli
    src/cli/main_cli.cpp
)

target_link_libraries(intelidata-cli
    PRIVATE
        intelidata_core
)

if(TARGET DuckDB::DuckDB)
    target_link_libraries(intelidata-cli PRIVATE DuckDB::DuckDB)
endif()

target_apply_intelidata_compiler_flags(intelidata-cli)

# ==============================================================================
# Binario GUI Ejecutable: intelidata-gui (Dear ImGui + ImPlot + GLFW)
# ==============================================================================
if(INTELIDATA_BUILD_GUI)
    add_executable(intelidata-gui
        src/gui/main_gui.cpp
        src/gui/GraphCanvas.cpp
        src/gui/InspectorView.cpp
        src/gui/DataHubView.cpp
        src/gui/DataPreviewView.cpp
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
endif()

# ==============================================================================
# Suite de Pruebas
# ==============================================================================
if(INTELIDATA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
