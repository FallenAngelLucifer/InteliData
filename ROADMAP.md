# Stack Tecnológico y Roadmap de Implementación

Este documento define las dependencias del proyecto, la estructura de carpetas en C++20 y el plan de implementación por fases para **InteliData**.

---

## 1. Stack Tecnológico y Dependencias

### 1.1. Estándar y Compilador
* **Estándar C++:** C++20 (Uso intensivo de conceptos, `std::span`, `std::jthread`, `std::latch` y algoritmos de rangos).
* **Compilador:** GCC 11+, Clang 13+ o MSVC 2022+.
* **Sistema de Construcción:** CMake 3.22+.

### 1.2. Bibliotecas Externas (Sin dependencias pesadas de runtime)
Todas las dependencias se gestionan preferentemente mediante **CMake `FetchContent`** o submódulos Git:

| Biblioteca | Versión / Rama | Propósito | Tipo de Dependencia |
| :--- | :--- | :--- | :--- |
| **`Eigen 3`** | `3.4.0` | Matrices densas/dispersas, SVD básico, álgebra lineal SIMD | Header-only |
| **`Spectra`** | `1.0.1` | Descomposición espectral de matrices gigantes (Lanczos) | Header-only (usa Eigen) |
| **`DuckDB`** | `v1.1+` | Motor embebido de ingesta para Parquet, CSV y SQLite | C++ API (`libduckdb`) |
| **`Dear ImGui`** | `docking` | Interfaz gráfica inmediata de escritorio | Código fuente embebido |
| **`ImPlot`** | `v0.16+` | Gráficos estadísticos y curvas 2D en tiempo real | Código fuente embebido |
| **`GLFW`** | `3.3+` | Creación de ventanas y gestión de contexto OpenGL/Vulkan | Sistema o FetchContent |
| **`nlohmann_json`**| `v3.11+` | Serialización y deserialización del Semantic AST | Header-only |

---

## 2. Estructura de Directorios Recomendada

```
InteliData/
├── CMakeLists.txt
├── README.md
├── MATHEMATICAL_FOUNDATIONS.md
├── ARCHITECTURE.md
├── ROADMAP.md
├── cmake/
│   └── Dependencies.cmake        // Configuración de FetchContent
├── include/
│   └── intelidata/
│       ├── core/
│       │   ├── Benford.hpp
│       │   ├── Entropy.hpp
│       │   ├── Markov.hpp
│       │   ├── Spectral.hpp
│       │   ├── SVDLatent.hpp
│       │   └── StringMetrics.hpp
│       ├── ingestion/
│       │   ├── DataAdapter.hpp
│       │   ├── DuckDBAdapter.hpp
│       │   └── ReservoirSampler.hpp
│       ├── model/
│       │   └── SemanticAST.hpp
│       └── pipeline/
│           └── Engine.hpp
├── src/
│   ├── core/
│   │   ├── Benford.cpp
│   │   ├── Entropy.cpp
│   │   ├── Markov.cpp
│   │   ├── Spectral.cpp
│   │   ├── SVDLatent.cpp
│   │   └── StringMetrics.cpp
│   ├── ingestion/
│   │   ├── DuckDBAdapter.cpp
│   │   └── ReservoirSampler.cpp
│   ├── pipeline/
│   │   ├── Engine.cpp
│   │   └── DDLGenerator.cpp
│   ├── cli/
│   │   └── main_cli.cpp          // Binario Headless / Daemon
│   └── gui/
│       ├── main_gui.cpp          // Binario de Escritorio con ImGui
│       ├── GraphCanvas.cpp       // Simulación física del grafo
│       └── InspectorView.cpp     // Gráficos ImPlot
└── tests/
    ├── test_benford.cpp
    ├── test_entropy.cpp
    ├── test_markov.cpp
    └── test_spectral.cpp
```

---

## 3. Plan de Implementación por Fases

### Fase 1: Núcleo Atómico y Muestreo (Semanas 1 - 2) [COMPLETADA]
* [x] Configuración del proyecto base en CMake con `Eigen 3` y testing con `Catch2` (Catch2 v3.5.2).
* [x] Implementación de `ReservoirSampler` para extracción aleatoria uniforme en una pasada (Algoritmo R).
* [x] Implementación de `Entropy` (Shannon normalizada para números y categorías, ratios de unicidad).
* [x] Implementación de `Benford` (histograma de primer dígito, prueba \(\chi^2\), cálculo de \(p\)-value analítico exacto y Nigrini MAD).
* [x] Implementación de algoritmo Welford en una pasada para media, varianza, asimetría y curtosis (Pébay 2008 con `combine`).
* [x] Pruebas unitarias con vectores sintéticos y bases de datos de prueba conocidas (100% de aprobación).

### Fase 2: Ingesta y Dependencias Relacionales (Semanas 3 - 4) [COMPLETADA]
* [x] Integración de `DuckDB C API` (`duckdb.h`) para lectura universal de archivos `.csv`, `.parquet` y `.sqlite`.
* [x] Implementación de `PIND` (Probabilistic Inclusion Dependencies) con resolución óptima de colisiones y filtro de cardinalidad.
* [x] Construcción del multigrafo de dependencias en memoria (`DependencyGraph`).
* [x] Implementación de `StringMetrics` (MinHash 64 semillas sobre $k$-shingles, esqueleto consonántico y subsecuencia monótona).

### Fase 3: Dinámica Estocástica y Álgebra Espectral (Semanas 5 - 6) [COMPLETADA]
* [x] Construcción de la matriz estocástica del esquema y cálculo de vector estacionario de **PageRank** con amortiguación ($\alpha = 0.85$).
* [x] Integración de álgebra espectral: construcción de **Matriz Laplaciana** normalizada del esquema, vector de Fiedler (\(\lambda_1\)) y corte normalizado 2-way.
* [x] Modelado de cadenas de Markov para columnas con estados discretos (detección de estados terminales absorbentes $P_{kk} \ge 0.98$).
* [x] Implementación de la matriz de co-ocurrencias PPMI y **Randomized SVD** (Halko, Martinsson, Tropp) para el espacio latente de entidades.

### Fase 4: Modelo Semántico y Generadores (Semana 7) [COMPLETADA]
* [x] Construcción de la estructura `SemanticAST` y serialización a JSON mediante `nlohmann_json`.
* [x] Implementación del motor de asignación de roles y nombres canónicos a partir del vector de firma unificado (`Engine`).
* [x] Implementación de `DDLGenerator`: generación de scripts SQL de vistas (`CREATE OR REPLACE VIEW`) y diccionario de datos en Markdown.
* [x] Binario CLI funcional: `intelidata-cli --input <file> --output-json <ast.json> --output-sql <views.sql> --output-docs <dict.md>`.

### Fase 5: Interfaz de Escritorio en Dear ImGui (Semanas 8 - 9) [COMPLETADA]
* [x] Inicialización de ventana GLFW + contexto OpenGL3 con Dear ImGui (rama `docking` v1.91+).
* [x] Integración de `ImPlot` para visualizar la curva teórica de Benford vs. histograma observado en vivo.
* [x] Implementación del canvas de dibujo (`ImDrawList`) para el **Grafo Ontológico**:
  * Simulación de fuerzas Fruchterman-Reingold en un hilo dedicado con amortiguación cinética.
  * Renderizado interactivo de nodos, colores por subdominio espectral y aristas dirigidas (curvas de Bézier cuadráticas).
* [x] Panel de inspección y visor de DDL con resaltado y botón de copiado rápido al portapapeles.
* [x] Soporte multiplataforma e integración continua (CI) en GitHub Actions para Linux y Windows (MSVC).

---

## 4. Guía Rápida de Compilación (Preview)

```bash
# 1. Clonar y preparar directorio de construcción
mkdir build && cd build

# 2. Configurar con CMake (Habilitando GUI y soporte DuckDB)
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=ON

# 3. Compilar en paralelo utilizando todos los núcleos del procesador
cmake --build . -j$(nproc)

# 4. Ejecutar la versión gráfica interactiva
./bin/intelidata-gui

# O ejecutar el análisis headless por línea de comandos
./bin/intelidata-cli --input /ruta/a/mi_base.sqlite --output ./resultado/
```
