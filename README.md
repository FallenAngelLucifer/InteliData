# InteliData

[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platforms](https://img.shields.io/badge/Platforms-Arch_Linux_%7C_Windows_MSVC-brightgreen.svg)]()
[![No ML / No LLMs](https://img.shields.io/badge/Inference-100%25_Deterministic-orange.svg)]()
[![License](https://img.shields.io/badge/License-Apache_2.0-lightgrey.svg)]()
[![Tests](https://img.shields.io/badge/Catch2-34%2F34_Passed-success.svg)]()

> **Motor ontológico y analítico determinista de bajo nivel para la reconstrucción, inferencia de tipos y descubrimiento de dependencias relacionales en bases de datos mediante matemáticas puras, sin modelos de caja negra (IA generativa/LLMs).**

---

## 1. Visión y Filosofía

**InteliData** se basa en un principio formal:
*Una base de datos no es una colección arbitraria de bytes o nombres; es una variedad algebraica, un grafo topológico de dependencias y una colección de distribuciones de probabilidad estocásticas.*

Aun cuando todos los nombres de tablas y columnas estén completamente ofuscados (e.g. `tbl_001_usr`, `fld_usr_ref`) o presenten abreviaciones caóticas, la **materia** (los datos brutos) y la **topología** (el flujo y la inclusión de conjuntos) revelan con certeza matemática la semántica, el tipo y las dependencias del sistema.

### Principios Fundamentales:
1. **Determinismo y Explicabilidad:** Cada etiqueta, relación foránea y rol inferido posee justificación comprobable (valores $p$, entropías normalizadas, vectores propios de Fiedler). Cero alucinaciones.
2. **Eficiencia Bare-Metal:** Escrito en **C++20 puro**, optimizado para localidad de caché y SIMD (AVX2/AVX-512), compatible con **Arch Linux** y **Windows (MSVC)**.
3. **Aislamiento Binario Universal (ABI):** Ingesta universal mediante la C API de **DuckDB** (`duckdb.h`) bajo patrón PIMPL, permitiendo lectura transparente de archivos `.parquet`, `.csv` y bases `.sqlite`.
4. **Visualización Inmediata en GPU:** Interfaz de escritorio interactiva construida en **Dear ImGui (Docking)** e **ImPlot** con simulación física de grafos Fruchterman-Reingold.

---

## 2. Las 5 Capas Matemáticas del Motor

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ 1. PERFILADO ATÓMICO (Capa 1)                                               │
│    • Acumulador de Welford paralelo (Pébay 2008): Media, Varianza, Skew,    │
│      Kurtosis en 1 pasada asociativa.                                       │
│    • Entropía de Shannon normalizada Ĥ(X) y ratios de unicidad estricta.     │
│    • Ley de Benford con p-value analítico cerrado Q(4, χ²/2) exacto.        │
│    • Muestreo uniforme en streaming mediante Algorithm R Reservoir.         │
├─────────────────────────────────────────────────────────────────────────────┤
│ 2. DEPENDENCIAS RELACIONALES & INCLUSIÓN (Capa 2)                           │
│    • Probabilistic Inclusion Dependencies (PIND) S(ca ⊆ cb).                │
│    • Información Mutua Normalizada (NMI).                                   │
│    • MinHash 64 semillas sobre k-shingles (invariante a permutación).       │
│    • Esqueleto consonántico φ(s) y métrica de contracción monótona.         │
├─────────────────────────────────────────────────────────────────────────────┤
│ 3. TOPOLOGÍA Y DINÁMICA ESTOCÁSTICA (Capa 3)                                │
│    • Matriz Laplaciana normalizada L_norm = I - D^(-1/2) W D^(-1/2).        │
│    • Conectividad de Fiedler (λ1) y particionamiento 2-way Normalized Cut.  │
│    • Gravedad de entidades relacionales mediante PageRank amortiguado.      │
│    • Cadenas de Markov de estados finitos (detección de estados absorbentes)│
│    • Matriz de co-ocurrencia PPMI y Randomized SVD latente.                 │
├─────────────────────────────────────────────────────────────────────────────┤
│ 4. MODELO SEMÁNTICO Y GENERADORES (Capa 4)                                  │
│    • SemanticAST canónico con serialización y deserialización JSON.         │
│    • Generador de vistas SQL limpias (CREATE OR REPLACE VIEW).              │
│    • Diccionario de datos ejecutivo en Markdown estructurado.               │
├─────────────────────────────────────────────────────────────────────────────┤
│ 5. INTERFAZ GRÁFICA DE ESCRITORIO (Capa 5)                                  │
│    • Dear ImGui Docking Space + ImPlot a 144 FPS con aceleración OpenGL3.   │
│    • Grafo de física interactivo Fruchterman-Reingold con curvas Bézier.    │
│    • Inspector de curvas de Benford y visor DDL con copiado al portapapeles.│
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Inicio Rápido y Compilación

### 3.1. Requisitos
- **Compilador C++20:** GCC 11+, Clang 13+ o Microsoft Visual C++ 2022+ (MSVC).
- **CMake:** Versión 3.22 o superior.
- **Gráficos:** GLFW 3.3+ y drivers de OpenGL 3.3+.

### 3.2. Compilación en Arch Linux
```bash
# 1. Instalar dependencias del sistema
sudo pacman -S --needed base-devel cmake gcc git glfw-x11 mesa

# 2. Configurar y compilar en Release
cmake -B build -DCMAKE_BUILD_TYPE=Release -DINTELIDATA_BUILD_GUI=ON -DINTELIDATA_BUILD_TESTS=ON
cmake --build build -j$(nproc)

# 3. Ejecutar pruebas automatizadas
ctest --test-dir build --output-on-failure
```

### 3.3. Compilación en Windows (MSVC)
```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release -DINTELIDATA_BUILD_GUI=ON -DINTELIDATA_BUILD_TESTS=ON
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
```

---

## 4. Ejecución del Sistema

### 4.1. CLI Headless (`intelidata-cli`)
Analiza archivos `.csv`, `.parquet` o bases `.sqlite` y genera los tres artefactos de salida:
```bash
# Generar base de datos de prueba multitabla sintética
python3 samples/generate_sample_data.py

# Analizar la base de datos completa
./build/intelidata-cli \
  --input samples/ecommerce_obfuscated.db \
  --output-json schema.json \
  --output-sql vistas.sql \
  --output-docs diccionario.md
```

### 4.2. GUI de Escritorio (`intelidata-gui`)
Lanza el entorno gráfico interactivo para inspeccionar el grafo ontológico y las curvas estadísticas:
```bash
# Abrir directamente un origen de datos en la GUI
./build/intelidata-gui --input samples/ecommerce_obfuscated.db

# O lanzar en modo verificación de frames automatizada (CI)
./build/intelidata-gui --test
```

---

## 5. Demostración en Dataset Ofuscado

Al ejecutar `intelidata-cli` sobre una base de datos relacional sintética con tablas ofuscadas (`tbl_001_usr`, `tbl_002_ord`, `tbl_003_itm`, `tbl_004_log`):

1. **Claves Foráneas Descubiertas (100% de Precisión):**
   - `tbl_002_ord.fld_usr_ref` $\to$ `tbl_001_usr.fld_uid` (Inclusividad: 100%, Confianza: 90.0%)
   - `tbl_003_itm.fld_ord_ref` $\to$ `tbl_002_ord.fld_oid` (Inclusividad: 100%, Confianza: 90.0%)
   - `tbl_004_log.fld_usr_ref` $\to$ `tbl_001_usr.fld_uid` (Inclusividad: 100%, Confianza: 90.0%)
2. **Gravedad de Entidades (PageRank):**
   - `tbl_001_usr`: **0.4706** (Dimensión Raíz / Master Entity)
   - `tbl_002_ord`: **0.2544** (Hecho Transaccional / Fact Hub)
3. **Particionamiento Espectral (Corte Normalizado de Fiedler):**
   - **Cluster 0:** `tbl_001_usr`, `tbl_004_log` (Usuarios y registros de auditoría)
   - **Cluster 1:** `tbl_002_ord`, `tbl_003_itm` (Órdenes de compra y detalle de ítems)
4. **Vistas SQL Generadas:**
   Estandariza identificadores, elimina duplicados y expone vistas limpias `v_dim_tbl_001_usr`, `v_tbl_002_ord`, `v_tbl_003_itm`, `v_tbl_004_log`.

---

## 6. Índice de Documentación Completa

- [**Manual de Usuario** (`docs/MANUAL_DE_USUARIO.md`)](./docs/MANUAL_DE_USUARIO.md): Guía detallada paso a paso de compilación, uso de flags del CLI y controles de la GUI.
- [**Guía Técnica del Desarrollador** (`docs/GUIA_DESARROLLADOR.md`)](./docs/GUIA_DESARROLLADOR.md): Patrones C++20, arquitectura PIMPL de DuckDB, ecuaciones de combinación de Pébay, resolución PIND y cómo extender el sistema.
- [**Fundamentos Matemáticos** (`MATHEMATICAL_FOUNDATIONS.md`)](./MATHEMATICAL_FOUNDATIONS.md): Formulación matemática rigurosa de las 5 capas del motor.
- [**Arquitectura de Software** (`ARCHITECTURE.md`)](./ARCHITECTURE.md): Diseño de componentes, concurrencia, doble buffer y flujos de datos.
- [**Roadmap de Implementación** (`ROADMAP.md`)](./ROADMAP.md): Registro de fases completadas y dependencias.
- [**Integración Continua** (`.github/workflows/ci.yml`)](./.github/workflows/ci.yml): Configuración de CI multiplataforma para Ubuntu (GCC, Clang) y Windows (MSVC).

---

## 7. Licencia

Desarrollado bajo licencia Apache 2.0. Consulta el archivo de licencia para más información.
