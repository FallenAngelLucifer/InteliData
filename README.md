# InteliData

[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![In-Memory DuckDB](https://img.shields.io/badge/Engine-DuckDB_Core-yellow.svg)](https://duckdb.org/)
[![Hardware Accelerated](https://img.shields.io/badge/Graphics-OpenGL_3.3_%7C_144_FPS-magenta.svg)]()
[![Inference](https://img.shields.io/badge/Inference-100%25_Deterministic_(Zero--LLM)-orange.svg)]()
[![Tests](https://img.shields.io/badge/Catch2-44%2F44_Passed-brightgreen.svg)]()
[![Platforms](https://img.shields.io/badge/Platforms-Linux_%7C_Windows_MSVC-blueviolet.svg)]()
[![License](https://img.shields.io/badge/License-Apache_2.0-lightgrey.svg)](LICENSE)

> **Plataforma Analítica de Alto Rendimiento para Modelado Multidimensional, Ingeniería Inversa de Cubos OLAP, Minería de Datos y Ciencia de Datos.**  
> *Desarrollada de forma nativa en C++20 con arquitectura vectorial y visualización acelerada por hardware.*

---

## 🚀 Resumen Ejecutivo

**InteliData** es un motor de inteligencia analítica de datos bare-metal que automatiza la deducción ontológica, la inferencia probabilística de dependencias foráneas y la reconstrucción de **Cubos Multidimensionales OLAP** a partir de orígenes de datos relacionales crudos (archivos `.sqlite`, `.db`, `.parquet` y `.csv`), sin intervención humana ni dependencia de modelos de lenguaje opacos (Zero-LLM).

El sistema proporciona una mesa de trabajo unificada para tres disciplinas analíticas fundamentales:
1. **Inteligencia de Negocios (BI):** Definición de hechos, medidas aditivas/derivadas, dimensiones conformadas, generación automática de DDL SQL, vistas de esquema DSV y conexión a **Microsoft Excel** mediante Tablas Dinámicas.
2. **Minería de Datos (Data Mining):** Reglas de asociación y análisis de canasta de compra (Apriori), segmentación de clientes RFM mediante clustering K-Means y dinámica estocástica con Cadenas de Markov.
3. **Ciencia de los Datos (Data Science):** Momentos estadísticos continuos de Welford, Curva de Lorenz y Coeficiente de Gini, auditoría forense mediante Ley de Benford, Entropía de Shannon y descomposición latente SVD.

---

## 🏛️ Arquitectura de Capas del Sistema

El núcleo de InteliData opera como una secuencia determinista desacoplada de cinco capas:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ 5. ENTORNOS DE EXPLOTACIÓN ANALÍTICA Y TOMA DE DECISIONES DE NEGOCIO        │
│    • Inteligencia de Negocios: Cubos OLAP, Tablas Dinámicas Excel, Dashboards│
│    • Minería de Datos: Algoritmo Apriori (Lift 7.09x), Clustering RFM, Markov│
│    • Ciencia de Datos: Welford (4 Momentos), Lorenz / Gini 0.62, Benford, SVD│
├─────────────────────────────────────────────────────────────────────────────┤
│ 4. MOTOR ANALÍTICO VECTORIAL Y ONTOLÓGICO INTELIDATA (C++20 NATIVO)         │
│    • DuckDB C-API embebida en memoria bajo patrón PIMPL (Zero-Copy)         │
│    • Algoritmo PIND: Inferencia de claves foráneas por Información Mutua    │
│    • Grafo semántico AST con Centralidad PageRank y Particionado Espectral   │
│    • Renderizado GPU (Dear ImGui Docking + GLFW3 + OpenGL 3.3 a 144 FPS)    │
├─────────────────────────────────────────────────────────────────────────────┤
│ 3. PERSISTENCIA Y REPOSITORIO BENCHMARK DE DATOS                            │
│    • Base de datos relacional Northwind (northwind.db en formato SQLite)    │
│    • 13 tablas estructuradas, 830 órdenes, 2,155 transacciones (OrderDetails)│
│    • 77 productos, 91 clientes, 9 ejecutivos de ventas, 3 transportistas     │
├─────────────────────────────────────────────────────────────────────────────┤
│ 2. HERRAMIENTAS DE CONSTRUCCIÓN, CIENCIA DE DATOS Y OFIMÁTICA               │
│    • Compilación: CMake 3.22+, Ninja, GCC 11+, Clang 14+, MSVC 2022          │
│    • Entorno de soporte: Python 3.10+ (NumPy, SciPy, Matplotlib)            │
│    • Herramientas cliente: Microsoft Excel (Conexión SQL / Tablas OLAP)      │
├─────────────────────────────────────────────────────────────────────────────┤
│ 1. SISTEMA OPERATIVO Y ARQUITECTURA DE CÓMPUTO BASE                         │
│    • GNU/Linux x86_64 (Ubuntu 22.04 LTS+, Debian 12, Arch Linux, Fedora)    │
│    • Microsoft Windows 10/11 x64 con arquitectura paralela multinúcleo (SIMD)│
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 📊 Los Tres Pilares Analíticos

### 1. Inteligencia de Negocios (BI) & Cubos Multidimensionales
* **Tabla de Hechos Central (`OrderDetails`):** 2,155 transacciones normalizadas con claves compuestas `OrderID` y `ProductID`.
* **Métricas Cuantitativas Base y Derivadas:**
  * *Venta Bruta:* $\text{VentaBruta} = \text{UnitPrice} \times \text{Quantity}$ ($1,354,458.59 USD).
  * *Merma por Descuentos:* $\text{Merma} = \text{UnitPrice} \times \text{Quantity} \times \text{Discount}$ ($89,796.88 USD).
  * *Venta Neta:* $\text{VentaNeta} = \text{VentaBruta} - \text{Merma}$ ($1,264,661.71 USD).
* **Dimensiones Normalizadas (Modelo Copo de Nieve / Constelación):**
  * `Orders` (Dimensión Temporal y Cabecera Logística, 830 registros).
  * `Products` (Catálogo de 77 artículos comerciales).
  * `Categories` (Jerarquía de normalización copo de nieve con 8 familias).
  * `Customers` (Mercado internacional con 91 cuentas corporativas).
  * `Employees` (Fuerza de ventas con 9 ejecutivos y estructura de reporte).
  * `Shippers` (Logística de distribución con 3 operadores internacionales).
* **Vistas de Esquema DSV y Física 2D:** Alternancia instantánea entre el esquema canónico de 5 columnas alineadas y la simulación física de relajación por fuerzas de Coulomb/Hooke.
* **Integración con Hojas de Cálculo:** Exportación DDL para consumo directo en **Microsoft Excel** mediante Tablas Dinámicas, Segmentadores (Slicers) y operaciones de Roll-up, Drill-down y Slice & Dice.

### 2. Minería de Datos (Data Mining)
* **Reglas de Asociación (Algoritmo Apriori):** Detección de patrones de co-compra en cestas de mercado. Identifica reglas de alta significancia como `Sir Rodney's Scones` $\to$ `Scottish Longbreads` con Soporte del 2.53%, Confianza del 67.74% y un Factor de Elevación (**Lift**) de **7.09x** sobre el azar.
* **Segmentación de Clientes RFM (K-Means):** Algoritmo de conglomerados sobre vectores 3D de Recencia, Frecuencia y Valor Monetario, clasificando clientes en *Campeones*, *Clientes Leales*, *Cuentas en Riesgo* e *Inactivas*.
* **Cadenas de Markov y Dinámica Estocástica:** Modelado matricial de transiciones temporales de compra entre categorías y detección de estados absorbentes en el ciclo de vida del cliente.

### 3. Ciencia de los Datos (Data Science)
* **Momentos Estadísticos Continuos (Algoritmo de Welford):** Cómputo exacto en una sola pasada de la Media ($586.85 USD), Desviación Estándar ($1,025.10 USD), Asimetría positiva (+3.12, distribución con cola derecha pesada) y Curtosis leptocúrtica (+14.85).
* **Curva de Lorenz y Coeficiente de Gini:** Análisis de concentración económica ($G = 0.62$), validando el Principio de Pareto donde el 20% de los clientes genera el 78.4% del volumen de ventas netas.
* **Auditoría Forense con Ley de Benford:** Evaluación de integridad contra la distribución logarítmica natural $P(d) = \log_{10}(1 + 1/d)$ mediante contraste $\chi^2$ con 8 grados de libertad y cálculo analítico cerrado de $p$-value.
* **Teoría de la Información & SVD Latente:** Entropía de Shannon normalizada $\hat{H}(X)$ para medir la sorpresa informacional de cada atributo y factorización matricial Truncated Randomized SVD sobre la matriz PPMI de co-ocurrencia relacional.

---

## 🛠️ Requisitos de Compilación y Entorno

### Compilador y Estándar
* **C++20 Nativo:** GCC 11+, Clang 14+ o Microsoft Visual C++ 2022 (MSVC).
* **CMake:** Versión 3.22 o superior.
* **Generador de Construcción:** Ninja (recomendado) o GNU Make.
* **Subsistema Gráfico:** Controladores compatibles con OpenGL 3.3 Core Profile y biblioteca GLFW 3.3+.

### Instalación en GNU/Linux (Ubuntu / Debian / Linux Mint)
```bash
# 1. Dependencias de compilación y bibliotecas gráficas
sudo apt update && sudo apt install -y build-essential cmake ninja-build \
    libglfw3-dev libgl1-mesa-dev python3 python3-pip

# 2. Dependencias del ecosistema analítico Python (opcional para scripts de reportes)
pip install numpy scipy matplotlib python-docx openpyxl
```

### Instalación en Arch Linux / Manjaro
```bash
sudo pacman -S --needed base-devel cmake ninja gcc git glfw-x11 mesa python python-pip
pip install numpy scipy matplotlib python-docx openpyxl
```

### Compilación en Microsoft Windows (MSVC 2022)
1. Instalar **Visual Studio 2022** con la carga de trabajo *Desarrollo para el escritorio con C++*.
2. Ejecutar el script automatizado:
   ```cmd
   build_windows.bat
   ```
   O configurar mediante CMake Presets:
   ```cmd
   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release -j
   ```

---

## 💻 Construcción y Ejecución

### 1. Compilación del Proyecto
```bash
# Configurar el árbol de compilación optimizado
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -GNinja

# Compilar todos los objetivos (CLI, GUI y Suite de Pruebas)
cmake --build build -j$(nproc)
```

---

## 🚀 Guía de Uso Rápido Paso a Paso (Mesa de Trabajo Analítica)

Esta guía paso a paso permite probar el software de forma intuitiva utilizando el dataset relacional de prueba `northwind.db`:

### Paso 1: Compilación Rápida
```bash
# En GNU/Linux (Ubuntu, Debian, Fedora, Arch)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -GNinja
cmake --build build -j$(nproc)
```
*(En Windows, simplemente abre la consola de desarrollo de Visual Studio 2022 y ejecuta `build_windows.bat`)*.

### Paso 2: Verificación de Integridad Matemática (44 Pruebas)
Antes de ejecutar la herramienta, valida que todos los algoritmos estén en perfecto estado en tu sistema:
```bash
ctest --test-dir build --output-on-failure
```
*Salida esperada:*
```text
100% tests passed, 0 tests failed out of 44 tests (0.55 sec)
```

### Paso 3: Iniciar la Estación de Trabajo Gráfica (`intelidata-gui`)
Ejecuta la interfaz acelerada por hardware:
```bash
./build/intelidata-gui
```
*(O en Windows: `build\Release\intelidata-gui.exe`)*.

### Paso 4: Ingesta del Dataset Northwind en 1 Clic
1. Dentro de la aplicación, dirígete a la pestaña **DataHub Ingest**.
2. En la sección **Datasets de Prueba (Recomendado)**, haz clic en el botón **Cargar Dataset Northwind**.
3. El motor vectorial DuckDB en memoria ingesta instantáneamente las 13 tablas relacionales (830 órdenes, 2,155 transacciones de detalle y 77 productos) en menos de 50 milisegundos.

### Paso 5: Exploración de Datos Crudos (DataPreview)
1. Accede a la pestaña **DataPreview**.
2. Selecciona cualquier entidad en el desplegable (por ejemplo, `OrderDetails` o `Orders`).
3. Podrás explorar los registros con scroll virtual optimizado a 144 FPS, visualizando columnas calculadas, tasas de descuento y tipos vectoriales.

### Paso 6: Visualización Dual del Esquema Multidimensional (GraphCanvas)
En el panel central **GraphCanvas**:
* **Modo Esquema DSV (Tablas Conectadas):** Muestra la constelación de hechos en 5 columnas limpias y organizadas:
  * Columna 1: Clientes y Proveedores (`Customers`, `Suppliers`).
  * Columna 2: Catálogo de Productos (`Products`, `Categories`).
  * Columna 3: **Tabla Central de Hechos (`OrderDetails`)**.
  * Columna 4: Cabecera Logística y Temporal (`Orders`).
  * Columna 5: Fuerza de Ventas y Envíos (`Employees`, `Shippers`).
* **Modo Red Física 2D:** Presiona el botón de alternancia para activar el motor de física de partículas en tiempo real (leyes de Coulomb y Hooke) con interacción por arrastre de nodos.
* Las claves primarias (`[PK]`) se resaltan en dorado y las foráneas (`[FK]`) en cian, unidas por curvas Bézier cúbicas con orientación direccional.

### Paso 7: Inspección de Métricas y Cubo OLAP (InspectorView)
1. Haz clic en la tabla de hechos `OrderDetails`.
2. El panel **InspectorView** calcula y expone automáticamente las métricas cuantitativas del cubo:
   * **Venta Bruta:** $\text{SUM}(\text{UnitPrice} \times \text{Quantity}) = \$1,354,458.59 \text{ USD}$
   * **Merma por Descuentos:** $\text{SUM}(\text{UnitPrice} \times \text{Quantity} \times \text{Discount}) = \$89,796.88 \text{ USD}$
   * **Venta Neta:** $\text{VentaBruta} - \text{Merma} = \$1,264,661.71 \text{ USD}$
   * **Unidades Vendidas:** $51,317 \text{ artículos}$
3. Revisa la entropía de Shannon $\hat{H}(X)$ de cada atributo para identificar columnas clave y categorizaciones.

### Paso 8: Dashboards Analíticos de Minería y Ciencia de Datos
Abre la pestaña **Analytics Dashboard** para navegar por los tres pilares analíticos:
* **Business Intelligence (BI):** Evolución temporal mensual de facturación (23 meses), desempeño por categorías, distribución geográfica por países y cascada financiera cuádruple.
* **Minería de Datos (Data Mining):** Reglas de asociación Apriori con cálculo exacto de soporte, confianza y factor de elevación (**Lift** de hasta **7.09x** en compras cruzadas), segmentación K-Means de clientes RFM y dinámica estocástica de Markov.
* **Ciencia de los Datos (Data Science):** Momentos continuos Welford, Curva de Lorenz (Gini = 0.62 / validación de Pareto 80/20) y contraste forense de bondad de ajuste con la **Ley de Benford** ($\chi^2$ con 8 grados de libertad).

### Paso 9: Exportación DDL y Conexión con Microsoft Excel
1. Accede a la pestaña **Export DDL**.
2. Copia el script SQL generado con las vistas normalizadas del cubo multidimensional.
3. Puedes consumir directamente estas vistas en **Microsoft Excel** o **Power BI** mediante conexiones ODBC/SQLite para crear Tablas Dinámicas, segmentadores (slicers) y operaciones de Roll-up / Drill-down.

---

### Modo Headless de Consola (`intelidata-cli`)
Para entornos de integración continua, servidores o procesamiento por lotes sin pantalla:
```bash
./build/intelidata-cli \
  --input samples/northwind.db \
  --output-json schema_northwind.json \
  --output-sql views_northwind.sql \
  --output-docs doc_northwind.md
```
*Genera de forma determinista el árbol semántico JSON (AST), el script DDL con las vistas SQL del cubo y el diccionario de datos en Markdown.*

---

## 📂 Estructura del Repositorio

```text
InteliData/
├── CMakeLists.txt              # Configuración principal de compilación CMake 3.22+
├── CMakePresets.json           # Presets estandarizados de compilación
├── build_windows.bat           # Script de compilación automática para Windows MSVC
├── LICENSE                     # Licencia de código abierto Apache 2.0
├── CONTRIBUTING.md             # Guía de contribución y estándares C++20
├── ARCHITECTURE.md             # Especificación técnica del diseño arquitectónico
├── MATHEMATICAL_FOUNDATIONS.md # Formulación matemática rigurosa de las capas
├── ROADMAP.md                  # Estado de hitos y componentes completados
├── cmake/                      # Módulos auxiliares de CMake y gestión de dependencias
├── include/intelidata/         # Cabeceras públicas modulares (C++20)
│   ├── common/                 # Tipos fundamentales, plataforma y utilidades
│   ├── core/                   # Algoritmos: Welford, Benford, Entropía, PIND, Markov, SVD
│   ├── gui/                    # Renderizadores: GraphCanvas, Dashboards, DataHub, Markdown
│   ├── ingestion/              # Conectores y adaptador universal de DuckDB
│   ├── model/                  # Árbol de sintaxis semántica (SemanticAST)
│   └── pipeline/               # Orquestador del motor y generador de vistas DDL SQL
├── src/                        # Implementación del código fuente C++20
│   ├── cli/                    # Punto de entrada de la herramienta de consola
│   ├── core/                   # Motores estadísticos y algebraicos
│   ├── gui/                    # Componentes ImGui / ImPlot y controladores de render
│   ├── ingestion/              # Ingesta en memoria DuckDB C-API
│   ├── model/                  # Serialización JSON del AST
│   └── pipeline/               # Motor de inferencia y generadores
├── samples/                    # Datasets estructurados de prueba y scripts
│   ├── northwind.db            # Repositorio relacional estándar benchmark
│   └── generate_sample_data.py # Generador sintético de bases ofuscadas
└── tests/                      # Suite de 44 pruebas unitarias automatizadas (Catch2)
```

---

## 🔬 Suite de Pruebas Unitarias

El proyecto cuenta con 44 pruebas unitarias automatizadas basadas en **Catch2 v3** que validan la estabilidad y consistencia matemática de cada componente:
* `TestWelford`: Verificación de algoritmos paralelos de momentos continuos contra valores de referencia.
* `TestBenford`: Validación de extracción del primer dígito, estadístico $\chi^2$ y cómputo de $p$-value analítico.
* `TestEntropy`: Verificación de entropía empírica de Shannon y propiedades de acotamiento informacional.
* `TestPIND`: Detección estricta de dependencias foráneas e Información Mutua Normalizada (NMI).
* `TestMarkov`: Detección de cadenas estocásticas y estados absorbentes.
* `TestSpectral`: Análisis de cortes espectrales basados en el vector de Fiedler.
* `TestSVD`: Factorización matricial de bajo rango sobre PPMI no negativo.
* `TestDuckDB`: Verificación de la capa de adaptación y lectura de catálogos en memoria.
* `TestAST`: Integridad de serialización y deserialización JSON sin pérdida de atributos.

Para ejecutar la batería completa:
```bash
ctest --test-dir build --output-on-failure
```

---

## 📄 Licencia

Este proyecto está licenciado bajo los términos de la **Apache License 2.0**. Para más detalles, consulta el archivo [LICENSE](LICENSE).

