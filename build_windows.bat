@echo off
setlocal enabledelayedexpansion
title InteliData - Compilacion en Windows / Visual Studio 2022

echo ==============================================================================
echo  InteliData: Sistema de Inteligencia de Negocios y Modelado Multidimensional
echo  Script de Compilacion para Microsoft Windows / Visual Studio 2022 (x64)
echo ==============================================================================
echo.

:: 1. Verificar si CMake esta disponible
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake no fue encontrado en el PATH del sistema.
    echo Asegurese de tener instalado Visual Studio 2022 con herramientas CMake para C++.
    pause
    exit /b 1
)

:: 2. Configurar usando preset nativo de Ninja/MSVC o fallback
echo [1/3] Configurando proyecto con CMake...
cmake --preset windows-x64-release
if %ERRORLEVEL% neq 0 (
    echo [INFO] Fallo preset directo, configurando en out\build\windows-x64-release...
    cmake -B out\build\windows-x64-release ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_CXX_STANDARD=20 ^
        -DINTELIDATA_BUILD_TESTS=ON ^
        -DINTELIDATA_ENABLE_AVX2=ON ^
        -DINTELIDATA_ENABLE_DUCKDB=ON ^
        -DINTELIDATA_BUILD_GUI=ON
)

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo la configuracion de CMake.
    pause
    exit /b 1
)

:: 3. Compilar en configuracion Release
echo.
echo [2/3] Compilando solucion en modo Release...
cmake --build out\build\windows-x64-release --config Release --parallel

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Fallo la compilacion.
    pause
    exit /b 1
)

:: 4. Ejecutar suite de pruebas unitarias
echo.
echo [3/3] Ejecutando pruebas unitarias de validacion...
ctest --test-dir out\build\windows-x64-release --output-on-failure

echo.
echo ==============================================================================
echo  ¡Compilacion y pruebas completadas exitosamente en Windows!
echo  Binarios generados en: out\build\windows-x64-release\
echo    * intelidata-gui.exe  (Interfaz Grafica con Tablero BI)
echo    * intelidata-cli.exe  (Binario de consola para pipelines)
echo ==============================================================================
echo.
