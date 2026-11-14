# Dependencies.cmake - Gestión de dependencias externas con FetchContent

include(FetchContent)

# 1. Eigen 3 (Header-only para álgebra lineal)
if(NOT TARGET Eigen3::Eigen)
    message(STATUS "Configurando Eigen 3.4.0...")
    FetchContent_Declare(
        eigen
        URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(BUILD_TESTING OFF CACHE INTERNAL "")
    set(EIGEN_BUILD_PKGCONFIG OFF CACHE INTERNAL "")
    set(EIGEN_BUILD_DOC OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(eigen)
endif()

# 2. nlohmann_json (Header-only para serialización AST)
if(NOT TARGET nlohmann_json::nlohmann_json)
    message(STATUS "Configurando nlohmann_json 3.11.3...")
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    set(JSON_Install OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# 3. Catch2 v3 (Framework de testing moderno C++20)
if(INTELIDATA_BUILD_TESTS AND NOT TARGET Catch2::Catch2WithMain)
    message(STATUS "Configurando Catch2 v3.5.2...")
    FetchContent_Declare(
        Catch2
        URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.5.2.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(CATCH_BUILD_TESTING OFF CACHE INTERNAL "")
    set(CATCH_INSTALL_DOCS OFF CACHE INTERNAL "")
    set(CATCH_INSTALL_EXTRAS OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(Catch2)
endif()

# 4. DuckDB (C++ API precompilada para Parquet, CSV, SQLite)
if(INTELIDATA_ENABLE_DUCKDB AND NOT TARGET DuckDB::DuckDB)
    message(STATUS "Configurando DuckDB precompilado v1.1.3...")
    if(WIN32)
        set(DUCKDB_BIN_URL "https://github.com/duckdb/duckdb/releases/download/v1.1.3/libduckdb-windows-amd64.zip")
    else()
        set(DUCKDB_BIN_URL "https://github.com/duckdb/duckdb/releases/download/v1.1.3/libduckdb-linux-amd64.zip")
    endif()

    FetchContent_Declare(
        duckdb_bin
        URL ${DUCKDB_BIN_URL}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(duckdb_bin)

    add_library(duckdb_imported SHARED IMPORTED GLOBAL)
    set_target_properties(duckdb_imported PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${duckdb_bin_SOURCE_DIR}"
    )

    if(WIN32)
        set_target_properties(duckdb_imported PROPERTIES
            IMPORTED_IMPLIB "${duckdb_bin_SOURCE_DIR}/duckdb.lib"
            IMPORTED_LOCATION "${duckdb_bin_SOURCE_DIR}/duckdb.dll"
        )
    else()
        set_target_properties(duckdb_imported PROPERTIES
            IMPORTED_LOCATION "${duckdb_bin_SOURCE_DIR}/libduckdb.so"
        )
    endif()

    add_library(DuckDB::DuckDB ALIAS duckdb_imported)
    add_compile_definitions(INTELIDATA_HAS_DUCKDB=1)
endif()

# 5. Spectra (Header-only para descomposición espectral Lanczos sobre Eigen)
if(NOT TARGET Spectra::Spectra)
    message(STATUS "Configurando Spectra v1.0.1...")
    FetchContent_Declare(
        spectra
        URL https://github.com/yixuan/spectra/archive/refs/tags/v1.0.1.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(spectra)
endif()

# 6. Dear ImGui + ImPlot + GLFW (para INTELIDATA_BUILD_GUI)
if(INTELIDATA_BUILD_GUI)
    find_package(OpenGL REQUIRED)

    find_package(glfw3 3.3 QUIET)
    if(NOT glfw3_FOUND)
        message(STATUS "Configurando GLFW via FetchContent...")
        FetchContent_Declare(
            glfw
            URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "")
        set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "")
        set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "")
        FetchContent_MakeAvailable(glfw)
    endif()

    if(NOT TARGET imgui)
        message(STATUS "Configurando Dear ImGui (docking)...")
        FetchContent_Declare(
            imgui
            URL https://github.com/ocornut/imgui/archive/refs/heads/docking.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(imgui)

        add_library(imgui STATIC
            "${imgui_SOURCE_DIR}/imgui.cpp"
            "${imgui_SOURCE_DIR}/imgui_draw.cpp"
            "${imgui_SOURCE_DIR}/imgui_tables.cpp"
            "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
            "${imgui_SOURCE_DIR}/imgui_demo.cpp"
            "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
            "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
        )
        target_include_directories(imgui PUBLIC
            "${imgui_SOURCE_DIR}"
            "${imgui_SOURCE_DIR}/backends"
        )
        target_link_libraries(imgui PUBLIC OpenGL::GL)
        if(TARGET glfw)
            target_link_libraries(imgui PUBLIC glfw)
        else()
            target_link_libraries(imgui PUBLIC glfw)
        endif()
    endif()

    if(NOT TARGET implot)
        message(STATUS "Configurando ImPlot (master compatible con ImGui docking)...")
        FetchContent_Declare(
            implot
            URL https://github.com/epezent/implot/archive/refs/heads/master.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(implot)

        add_library(implot STATIC
            "${implot_SOURCE_DIR}/implot.cpp"
            "${implot_SOURCE_DIR}/implot_items.cpp"
        )
        target_include_directories(implot PUBLIC
            "${implot_SOURCE_DIR}"
        )
        target_compile_definitions(implot PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
        target_link_libraries(implot PUBLIC imgui)
    endif()
endif()



