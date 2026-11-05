# Arquitectura del Sistema: InteliData

Este documento especifica la arquitectura de software, los flujos de datos, la gestión de concurrencia y el diseño de la interfaz de usuario en C++20 y Dear ImGui.

---

## 1. Visión General del Sistema

**InteliData** está diseñado bajo una arquitectura desacoplada de alto rendimiento:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                             MODOS DE INGESTA                                │
│   [ Live DB Connection ]        [ Flat Files / Dumps ]     [ Zero-Knowledge ]
│   (Postgres, MySQL, etc.)       (Parquet, SQLite, CSV)     (pg_stats / sys) 
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                    ADAPTADOR & MUESTREO EN STREAMING                        │
│   • Reservoir Sampling (Streaming en una pasada)                            │
│   • Límites de Error de Hoeffding (K = 5,000 registros representativos)     │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        MOTOR MATEMÁTICO CORE (C++20)                        │
│   • Multi-threading con Worker Pool (`std::jthread`)                        │
│   • Profiling Atómico (Entropía, Benford, Welford)                          │
│   • MinHash / LCS / Shingling                                               │
│   • PPMI + Randomized SVD (`Eigen 3` + `Spectra`)                           │
│   • Cadenas de Markov (Transiciones & PageRank del Esquema)                 │
│   • Análisis Espectral Laplaciano (Fiedler Vector)                          │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
                 ┌─────────────────────┼─────────────────────┐
                 ▼                     ▼                     ▼
┌─────────────────────────┐ ┌─────────────────────┐ ┌─────────────────────────┐
│     SALIDA 1: API       │ │  SALIDA 2: DDL/DOCS │ │     SALIDA 3: IMGUI     │
│   Semantic AST (JSON)   │ │  Vistas SQL Limpias │ │  Grafo de Física (GPU)  │
│   para pipelines & SDK  │ │  Diccionario .md    │ │  Plots de Benford 144FPS│
└─────────────────────────┘ └─────────────────────┘ └─────────────────────────┘
```

---

## 2. Estrategia de Ingesta de Datos

### 2.1. Conexión Viva (Live DB URI)
* **Objetivo:** Analizar bases de datos de producción masivas (Terabytes) sin saturar la red ni bloquear tablas.
* **Mecanismo:** Consultas de muestreo probabilístico a nivel de páginas de disco gestionadas por el propio motor relacional:
  ```sql
  SELECT * FROM tabla TABLESAMPLE SYSTEM (1) LIMIT 5000;
  ```
* Los datos se consumen en streaming a través de cursores hacia acumuladores de estadísticas, descartando los registros de RAM inmediatamente.

### 2.2. Volcados y Archivos Planos (DuckDB C++ API)
* **Objetivo:** Soporte universal offline para formatos analíticos modernos.
* **Mecanismo:** Uso embebido de la biblioteca C++ de DuckDB (`duckdb.hpp`). DuckDB lee directamente archivos `.parquet`, `.csv` comprimidos y bases `.sqlite` mediante escaneo vectorizado multihilo y *zero-copy*.

### 2.3. Modo Zero-Knowledge (Preservación de Privacidad / GDPR)
* **Objetivo:** Inferencia en entornos bancarios, médicos o confidenciales donde la regulación prohíbe leer tuplas reales.
* **Mecanismo:** Extracción de metadatos estadísticos calculados por los optimizadores del RDBMS:
  * PostgreSQL: `pg_stats` (histogramas de frecuencia, null_frac, n_distinct, most_common_vals).
  * SQL Server: `sys.dm_db_stats_histogram`.
  * Oracle / MySQL: Tablas de estadísticas de optimizador de costos.
* El motor reconstruye las firmas atómicas a partir de los histogramas pre-computados.

---

## 3. Modelo de Concurrencia y Procesamiento Multihilo

Para garantizar que la interfaz gráfica (ImGui) permanezca a 60-144 FPS sin congelamientos:

1. **Hilo Principal (Main/UI Thread):**
   * Gestiona el bucle de eventos GLFW / SDL.
   * Renderiza los frames de Dear ImGui y despacha comandos de dibujo a OpenGL/Vulkan.
   * Ejecuta la simulación de física de fuerzas para los nodos del grafo visible.
2. **Worker Pool (Background Processing via `std::jthread`):**
   * Un pool de hilos de trabajo independientes procesa la ingesta y el cálculo matemático.
   * La asignación de columnas y tablas es puramente paralela (*embarrassingly parallel*).
   * **Sincronización:** Uso de `std::atomic<float>` para reportar progreso porcentual y `std::latch` para barreras de sincronización entre fases del pipeline.
   * **Intercambio de Datos:** Una estructura inmutable de doble buffer (*double-buffered state*) donde el motor publica el `SemanticAST` final una vez concluido el cálculo espectral.

---

## 4. Las Tres Salidas del Sistema

### 4.1. Salida 1: API / Semantic AST (Headless Core)
El motor puede compilarse como biblioteca estática/dinámica (`libintelidata.so` / `.dll`) o ejecutarse en modo CLI. Su resultado canónico es un árbol semántico estructurado en JSON:

```json
{
  "system_summary": {
    "detected_domain": "E-Commerce / Logística de Pedidos",
    "confidence": 0.948,
    "fiedler_connectivity": 0.28,
    "total_tables": 18,
    "total_relationships_inferred": 24
  },
  "tables": [
    {
      "original_identifier": "t_04",
      "inferred_role": "Fact_Transaction",
      "recommended_name": "ordenes_compra",
      "pagerank_gravity": 0.385,
      "columns": [
        {
          "original_identifier": "c_1",
          "inferred_type": "Surrogate_Primary_Key",
          "recommended_name": "id_orden",
          "entropy": 1.0,
          "is_unique": true
        },
        {
          "original_identifier": "c_9",
          "inferred_type": "Currency_Monetary_Amount",
          "recommended_name": "total_orden",
          "benford_p_value": 0.988,
          "distribution": "log-normal"
        },
        {
          "original_identifier": "st",
          "inferred_type": "Discrete_State_Machine",
          "recommended_name": "estado_orden",
          "markov_absorbing_states": ["COMPLETED", "CANCELED"]
        }
      ]
    }
  ]
}
```

### 4.2. Salida 2: Generador de DDL y Documentación
Transforma automáticamente el `SemanticAST` en artefactos de base de datos directamente ejecutables:
1. **Capa de Vistas de Desofuscación (SQL DDL):**
   Crea vistas de alto nivel sin alterar las tablas físicas originales, resolviendo los nombres mediante las inferencias del sistema:
   ```sql
   -- Generado automáticamente por InteliData
   CREATE VIEW v_ordenes_compra AS
   SELECT 
       c_1 AS id_orden,
       c_9 AS total_orden,
       st  AS estado_orden
   FROM t_04;
   ```
2. **Data Dictionary en Markdown:**
   Un informe técnico completo que incluye para cada tabla su rol en el grafo, justificación matemática de cada tipo inferido y matrices de transición de estado.

### 4.3. Salida 3: Interfaz Gráfica Nativa (Dear ImGui)
Estructurada con el sistema de acoplamiento de ventanas (*ImGui Docking*):

1. **Canvas del Grafo Topológico (`ImDrawList`):**
   * **Nodos:** Círculos interactivos arrastrables representando tablas.
   * **Física:** Simulación en tiempo real basada en Fruchterman-Reingold con amortiguación cinética.
   * **Radio del Nodo:** Escala logarítmica proporcional a su probabilidad estacionaria en la cadena de Markov (PageRank).
   * **Color del Nodo:** Mapeado al vector de particionamiento espectral (Laplaciano).
   * **Aristas:** Curvas de Bézier dirigidas indicando relaciones PK/FK inferidas por inclusión probabilística.
2. **Panel Inspector Estadístico (`ImPlot`):**
   * Al seleccionar cualquier columna, muestra el histograma de frecuencias reales contrastado con la curva teórica de la Ley de Benford.
   * Visualización matricial tipo mapa de calor para las probabilidades de transición de Markov \( P(S_{t+1} \mid S_t) \).
3. **Visor de Código DDL en Vivo:**
   * Área de texto integrada con sintaxis SQL resaltada, permitiendo copiar o exportar el script de vistas y modelos generados con un solo clic.
