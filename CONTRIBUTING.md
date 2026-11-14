# Guía de Contribución a InteliData

Gracias por tu interés en contribuir a **InteliData**, la plataforma de alto rendimiento en C++20 para modelado multidimensional, minería de datos y análisis estadístico.

Este documento establece las pautas técnicas, convenciones de código y el flujo de trabajo para colaborar en el proyecto.

---

## 1. Código de Conducta

* Mantener un trato profesional, constructivo e inclusivo.
* Enfocarse en la excelencia técnica, reproducibilidad y estabilidad de los algoritmos.
* Cero referencias académicas, docentes o personales en el código fuente, pruebas o documentación técnica. El proyecto es una herramienta analítica independiente de código abierto.

---

## 2. Requisitos de Desarrollo

Para compilar y desarrollar en InteliData necesitas:

### Linux
* **Compilador C++20:** GCC 11+ o Clang 14+.
* **CMake:** Versión 3.22 o superior.
* **Sistema de Construcción:** Ninja (recomendado) o GNU Make.
* **Librerías del Sistema:**
  ```bash
  sudo apt update && sudo apt install -y build-essential cmake ninja-build \
      libglfw3-dev libgl1-mesa-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libx11-dev
  ```

### Windows
* **IDE / Compilador:** Microsoft Visual Studio 2022 (MSVC v143+) con el paquete *Desktop development with C++*.
* **CMake:** Versión 3.22+ (incluido con Visual Studio).
* **Script de Compilación Automatizado:** `build_windows.bat`.

---

## 3. Flujo de Trabajo para el Desarrollo

1. **Clonar el Repositorio:**
   ```bash
   git clone https://github.com/FallenAngelLucifer/InteliData.git
   cd InteliData
   ```

2. **Crear una Rama de Trabajo:**
   Utiliza ramas semánticas descriptivas:
   ```bash
   git checkout -b feat/nueva-funcionalidad
   # o
   git checkout -b fix/correccion-algoritmo
   ```

3. **Configurar y Compilar:**
   ```bash
   cmake -B build -G Ninja \
     -DCMAKE_BUILD_TYPE=Release \
     -DINTELIDATA_BUILD_TESTS=ON \
     -DINTELIDATA_BUILD_GUI=ON

   cmake --build build -j$(nproc)
   ```

4. **Ejecutar la Suite de Pruebas Automatizadas:**
   **Toda contribución debe mantener el 100% de pruebas pasando.**
   ```bash
   ctest --test-dir build --output-on-failure
   ```

---

## 4. Estándares y Convenciones de Código (C++20)

* **Estándar:** C++20 estricto (`-std=c++20` / `/std:c++20`).
* **Memoria y Punteros:**
  * Cero `raw new` / `delete`. Usar `std::unique_ptr` y `std::shared_ptr`.
  * Aprovechar `std::span` para vistas de vectores sin copias de memoria.
  * Usar semántica de movimiento (`std::move`) y `const auto&` para accesos de solo lectura.
* **Algoritmos Estadísticos:**
  * Algoritmos en streaming de una sola pasada (p. ej., Welford para varianza, Reservoir Sampling para flujos continuos).
  * Evitar dependencias de aprendizaje profundo o cajas negras; los modelos deben ser deterministas y explicables.
* **Manejo de Errores:**
  * Uso de excepciones estándar o tipos de resultado explícitos (`std::optional`, `std::expected`).
  * Validación estricta de límites de matrices y rangos numéricos.
* **Formato:**
  * Sangría de 4 espacios.
  * Nombres de clases y estructuras en `PascalCase`.
  * Métodos y funciones en `snake_case` o `camelCase` consistente con el módulo.
  * Constantes en `UPPER_SNAKE_CASE`.

---

## 5. Adición de Nuevas Pruebas Unitarias

Si agregas una nueva funcionalidad o corriges un error:
1. Agrega el caso de prueba correspondiente en `tests/` utilizando **Catch2 v3**.
2. Registra el ejecutable o caso en `tests/CMakeLists.txt`.
3. Verifica que la suite completa pase antes de enviar tu Pull Request:
   ```bash
   ctest --test-dir build --output-on-failure
   ```

---

## 6. Envío de Pull Requests

1. Asegúrate de que el código compila sin advertencias (`-Wall -Wextra`).
2. Verifica que las 44 pruebas unitarias (o más si agregaste nuevas) pasen al 100%.
3. Escribe mensajes de commit convencionales:
   * `feat: agregar nuevo algoritmo de clustering`
   * `fix: corregir cálculo de varianza en streaming`
   * `docs: actualizar guía de inicio rápido`
   * `test: agregar prueba para descomposición SVD`
4. Abre el Pull Request en GitHub describiendo detalladamente los cambios realizados.
