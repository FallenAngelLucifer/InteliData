# Manual de Usuario: InteliData

**InteliData** es un motor analítico y ontológico determinista de bajo nivel escrito en **C++20 puro**, diseñado para inferir la estructura, tipos semánticos y dependencias relacionales de cualquier base de datos u origen tabular (.csv, .parquet, .sqlite), sin el uso de cajas negras ni modelos de lenguaje generativo (LLMs).

---

## 1. Requisitos del Sistema y Compilación

### 1.1. En Arch Linux

1. **Instalar paquetes base y dependencias del sistema:**
   ```bash
   sudo pacman -S --needed base-devel cmake gcc git glfw-x11 mesa
   ```
   *(Si usas Wayland puro, también es compatible mediante `glfw-wayland` o la capa XWayland).*

2. **Clonar y configurar el proyecto:**
   ```bash
   git clone <url-del-repositorio> InteliData
   cd InteliData
   cmake -B build -DCMAKE_BUILD_TYPE=Release -DINTELIDATA_BUILD_GUI=ON -DINTELIDATA_BUILD_TESTS=ON
   ```

3. **Compilar en paralelo:**
   ```bash
   cmake --build build -j$(nproc)
   ```

4. **Ejecutar la suite de pruebas automatizadas:**
   ```bash
   ctest --test-dir build --output-on-failure
   ```
   *(Debe reportar 100% tests passed out of 34).*

---

### 1.2. En Microsoft Windows (MSVC 2022+)

1. **Requisitos previos:**
   - Visual Studio 2022 (Community, Professional o Enterprise) con la carga de trabajo *"Desarrollo para el escritorio con C++"*.
   - CMake 3.22+ y Git instalados en el `PATH`.
   - Python 3.10+ (opcional, para generar los conjuntos de datos de prueba sintéticos).

2. **Configuración y compilación desde Developer Command Prompt o PowerShell:**
   ```powershell
   cmake -B build -DCMAKE_BUILD_TYPE=Release -DINTELIDATA_BUILD_GUI=ON -DINTELIDATA_BUILD_TESTS=ON
   cmake --build build --config Release -j
   ```

3. **Ejecutar las pruebas unitarias:**
   ```powershell
   ctest --test-dir build -C Release --output-on-failure
   ```

Los binarios ejecutables se generarán en:
- Linux: `build/intelidata-cli` e `build/intelidata-gui`.
- Windows: `build/Release/intelidata-cli.exe` e `build/Release/intelidata-gui.exe`.

---

## 2. Guía de Uso del CLI (`intelidata-cli`)

El binario de línea de comandos `intelidata-cli` está diseñado para pipelines de datos, integración continua (CI/CD) y entornos de servidor sin entorno gráfico (headless).

### 2.1. Sintaxis y Argumentos

```bash
./build/intelidata-cli [opciones]
```

| Parámetro | Requerido | Descripción | Valor por Defecto |
| :--- | :---: | :--- | :--- |
| `--input <ruta>` | **Sí** | Ruta al archivo de origen de datos (`.parquet`, `.csv`, `.sqlite`, `.db`). | - |
| `--output-json <ruta>` | No | Ruta donde se guardará el AST semántico en formato JSON. | `schema.json` |
| `--output-sql <ruta>` | No | Ruta para generar el script SQL DDL con las vistas limpias y desofuscadas. | Deshabilitado |
| `--output-docs <ruta>` | No | Ruta para generar el Diccionario de Datos en formato Markdown estructurado. | Deshabilitado |
| `--help`, `-h` | No | Muestra la ayuda en pantalla con la sintaxis y opciones. | - |

---

### 2.2. Ejemplos Prácticos de Ejecución

#### Ejemplo A: Análisis de una base SQLite multitabla ofuscada
```bash
# 1. Generar base de datos sintética de prueba (si no existe)
python3 samples/generate_sample_data.py

# 2. Ejecutar inferencia ontológica completa con exportación triple
./build/intelidata-cli \
  --input samples/ecommerce_obfuscated.db \
  --output-json samples/ecommerce_ast.json \
  --output-sql samples/ecommerce_views.sql \
  --output-docs samples/ecommerce_dictionary.md
```

**Salida en consola:**
```text
[InteliData] Inicializando motor de ingesta universal DuckDB...
[InteliData] Cargando archivo: ecommerce_obfuscated.db...
[InteliData] Iniciando analisis ontologico determinista...
  [10%] Ingesta y Muestreo
  [17%] Perfilado Atómico
  [25%] Perfilado Atómico
  [32%] Perfilado Atómico
  [40%] Perfilado Atómico
  [45%] Descubrimiento de Dependencias PIND
  [70%] Álgebra Espectral y PageRank
  [90%] Síntesis Semántica
  [100%] Completado

[InteliData] Analisis concluido con exito:
  * Dominio Detectado: Gestion Operativa Relacional
  * Nivel de Confianza: 82%
  * Conectividad Fiedler: 0.5
  * Tablas Reconstruidas: 4
  * Relaciones Descubiertas: 3

[InteliData] Exportando JSON semantico a: samples/ecommerce_ast.json
[InteliData] Exportando vistas SQL DDL a: samples/ecommerce_views.sql
[InteliData] Exportando diccionario en Markdown a: samples/ecommerce_dictionary.md
[InteliData] Proceso completado exitosamente.
```

#### Ejemplo B: Análisis rápido de un archivo CSV plano
```bash
./build/intelidata-cli --input samples/sample.csv --output-docs diccionario_csv.md
```

#### Ejemplo C: Extracción en pipeline hacia JSON para consumo automatizado
```bash
./build/intelidata-cli --input telemetry.parquet --output-json ast_telemetria.json
```

---

## 3. Guía de Uso de la Interfaz Gráfica (`intelidata-gui`)

La aplicación de escritorio `intelidata-gui` ofrece un entorno visual de alta tasa de refresco (60-144 FPS) acelerado por hardware para la inspección interactiva del esquema reconstruido.

### 3.1. Lanzamiento

```bash
# Lanzamiento estándar con esquema de demostración en memoria
./build/intelidata-gui

# Lanzamiento directo abriendo un archivo específico
./build/intelidata-gui --input samples/ecommerce_obfuscated.db

# Modo verificación de frames para pruebas automatizadas (renderiza 10 frames y sale con código 0)
./build/intelidata-gui --input samples/ecommerce_obfuscated.db --test
```

---

### 3.2. Espacio de Trabajo Acoplable (ImGui Docking Space)

La interfaz se divide en 3 paneles principales acoplables que puedes arrastrar, redimensionar o desacoplar en ventanas flotantes:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ Archivo | Ayuda                                [Listo: 4 tablas analizadas] │
├────────────────────────────────────────┬────────────────────────────────────┤
│                                        │ 2. INSPECTOR ESTADÍSTICO (ImPlot)  │
│                                        │   • Histograma de 1er Dígito       │
│                                        │   • Curva Teórica de Benford       │
│ 1. GRAFO ONTOLÓGICO (FÍSICA GPU)       │   • Chi-cuadrado & p-value         │
│   • Simulación Fruchterman-Reingold    │   • Estados Absorbentes Markov     │
│   • Nodos coloreados por Cluster       ├────────────────────────────────────┤
│   • Aristas con curvas Bézier dirigidas│ 3. VISOR DDL / DOCUMENTACIÓN       │
│   • Selección mediante clic            │   • CREATE OR REPLACE VIEW vistas  │
│                                        │   • Diccionario de Datos Markdown  │
│                                        │   • [Copiar al Portapapeles]       │
└────────────────────────────────────────┴────────────────────────────────────┘
```

#### 1. Panel de Grafo Ontológico
- **Simulación Física en Tiempo Real:** Los nodos representan tablas del esquema. Se aplican fuerzas de repulsión electrostática (Coulomb) y atracción por resortes (Hooke) entre tablas vinculadas por claves foráneas, con amortiguación cinética continua.
- **Colores de Nodos por Subdominio Espectral:** Los nodos se colorean automáticamente según el cluster asignado por el **Corte Normalizado de Fiedler** de la matriz Laplaciana.
- **Interacción:** Haz clic sobre cualquier nodo para seleccionarlo e inspeccionar todas sus columnas, distribuciones y métricas en el panel lateral.

#### 2. Panel del Inspector Estadístico (ImPlot)
- **Ajuste a la Ley de Benford:** Muestra el histograma de barras con la frecuencia observada de los primeros dígitos significativos (1 al 9) contrastado con la curva analítica logarítmica de Benford $P(d) = \log_{10}(1 + 1/d)$.
- **Diagnóstico Forense:** Despliega el estadístico $\chi^2$, el $p$-value analítico en forma cerrada y el veredicto sobre si la columna modela transacciones monetarias legítimas o posibles anomalías.
- **Máquinas de Estados y Cadenas de Markov:** Si la columna es de estados finitos, lista las transacciones de estado y destaca los **estados absorbentes terminales** ($P_{kk} \ge 0.98$).

#### 3. Panel de Visor DDL / Markdown
- **Pestaña SQL DDL:** Genera en vivo las sentencias `CREATE OR REPLACE VIEW` con los identificadores limpios, resolviendo colisiones y estandarizando claves primarias y foráneas.
- **Pestaña Diccionario Markdown:** Presenta la matriz de entidades, roles inferidos, niveles de entropía y razones de confianza.
- **Botón "Copiar al Portapapeles":** Permite copiar directamente el código SQL o Markdown generado para pegarlo en tu gestor de base de datos o documentación de proyecto.

---

### 3.3. Carga Dinámica de Archivos desde la Interfaz

1. En la barra de menú superior, selecciona **Archivo $\to$ Abrir Origen de Datos... (Ctrl+O)**.
2. Escribe o pega la ruta local de tu archivo (por ejemplo: `samples/ecommerce_obfuscated.db` o `datos/ventas.parquet`).
3. Haz clic en **"Cargar y Analizar"**.
4. El motor ejecutará el pipeline completo de 5 capas en background y actualizará instantáneamente el grafo, el inspector y el generador de vistas DDL.

---

## 4. Interpretación de los Resultados Generados

### 4.1. Vistas SQL DDL (`views.sql`)
Cada tabla original recibe una vista desofuscada con la convención canónica:
```sql
CREATE OR REPLACE VIEW v_tbl_002_ord AS
SELECT
    "fld_oid" AS "id_tbl_002_ord",          -- Surrogate Primary Key
    "fld_usr_ref" AS "id_tbl_001_usr",      -- Foreign Key Reference descubierta hacia tbl_001_usr
    "fld_amt" AS "fld_amt",                  -- Métrica numérica
    "fld_st_seq" AS "estado_tbl_002_ord",    -- Máquina de estados discretos
    "fld_auth_tok" AS "token_fld_auth_tok",  -- Token / Hash de alta entropía
    "fld_notes" AS "descripcion_fld_notes"   -- Texto en lenguaje natural
FROM "tbl_002_ord";
```

### 4.2. Diccionario de Datos (`dictionary.md`)
Resume los descubrimientos matemáticos del esquema:
- **Gravedad de Entidades (PageRank):** Cuantifica qué tan central es una tabla en la topología relacional. Las tablas con mayor PageRank corresponden a dimensiones raíz o maestros de datos (`Dimension_Master_Entity`), mientras que las de menor valor corresponden a transacciones dependientes o tablas puente.
- **Conectividad de Fiedler ($\lambda_1$):** El segundo valor propio más pequeño del Laplaciano normalizado. Si $\lambda_1 = 0$, el esquema contiene componentes completamente aisladas o desconectadas; si $\lambda_1 > 0$, indica la robustez y acoplamiento estructural del modelo relacional.
