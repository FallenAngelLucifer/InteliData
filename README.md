# InteliData

> **Motor analítico y ontológico determinista para la inferencia, perfilado y reconstrucción de bases de datos mediante matemáticas avanzadas, sin modelos de caja negra (IA generativa/LLMs).**

---

## 1. Visión y Filosofía

**InteliData** nace de una premisa formal: *Una base de datos no es solo una colección arbitraria de bytes o nombres; es una variedad algebraica, un grafo topológico de dependencias y una colección de distribuciones de probabilidad estocásticas.*

Incluso si todos los nombres de tablas y columnas son ofuscados (e.g., `t1, t2`, `c_1, c_2`) o presentan abreviaciones y permutaciones caóticas (`usr_dir`, `dir_env_usuario`), la **materia** (los datos brutos) y la **topología** (las relaciones y flujos de información) revelan con exactitud matemática el rol, tipo y significado del sistema.

### Principios Rectores:
1. **Determinismo y Explicabilidad:** Cada etiqueta, rol y relación inferida cuenta con una justificación algebraica comprobable (valores $p$, entropías, vectores propios). Cero alucinaciones.
2. **Eficiencia Bare-Metal:** Desarrollado íntegramente en **C++20 puro**, optimizado con instrucciones SIMD (AVX2/AVX-512) y visualización inmediata en GPU mediante **Dear ImGui**.
3. **Fusión Multimodal (Materia + Contexto):** Combina el análisis atómico del dato bruto con el modelado espectral de co-ocurrencias (PPMI + Randomized SVD) y cadenas de Markov.
4. **Independencia de Nombre:** La inferencia no depende de etiquetas humanas, aunque es capaz de resolver variaciones y abreviaciones mediante álgebra de subsecuencias y MinHash.

---

## 2. Estructura de la Documentación

Esta carpeta contiene la especificación completa del sistema para su desarrollo en Antigravity:

* **[`MATHEMATICAL_FOUNDATIONS.md`](./MATHEMATICAL_FOUNDATIONS.md):** Formulación matemática rigurosa de las 5 capas del motor (Teoría de la Información, Álgebra Espectral, Cadenas de Markov, Benford, MinHash, SVD).
* **[`ARCHITECTURE.md`](./ARCHITECTURE.md):** Diseño del sistema, pipeline de datos, modos de ingesta (Streaming, Dumps, Zero-Knowledge), las tres salidas y diseño de la interfaz en Dear ImGui.
* **[`ROADMAP.md`](./ROADMAP.md):** Plan de ejecución por fases, estructura de carpetas C++, dependencias nativas y guía de compilación.

---

## 3. Resumen Rápido del Pipeline

```
[ Ingesta de Datos: Conexión Viva / Dumps / Stats de Catálogo ]
                                │
                                ▼
         [ Muestreador Adaptativo: Reservoir Sampling ]
                                │
                                ▼
         [ Motor Matemático Core (5 Capas en C++20) ]
       ┌────────────────────────┼────────────────────────┐
       ▼                        ▼                        ▼
[ Salida 1: API / AST ]  [ Salida 2: DDL / Docs ]  [ Salida 3: Grafo ImGui ]
  Modelo semántico JSON    Vistas SQL refactorizadas  Topología visual a 144 FPS
```
