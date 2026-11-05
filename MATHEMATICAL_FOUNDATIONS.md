# Fundamentos Matemáticos de InteliData

Este documento detalla formalmente los teoremas, métricas y algoritmos que componen el núcleo matemático de **InteliData**.

---

## 1. Capa 1: Profiling Atómico (Nivel de Columna)

El objetivo de esta capa es convertir cualquier vector de datos bruto \( X = (x_1, x_2, \dots, x_N) \) en una firma compacta e invariante \( \vec{v}_{\text{bruto}}(X) \in \mathbb{R}^{d_1} \).

### 1.1. Entropía Normalizada de Shannon
Para medir el grado de incertidumbre, unicidad y dispersión del contenido:
\[
\hat{H}(X) = -\frac{1}{\log_2 |D|} \sum_{x \in D} P(x) \log_2 P(x)
\]
donde \( D \) es el conjunto de valores distintos observados y \( P(x) \) es la frecuencia empírica relativa.
* **\(\hat{H}(X) \approx 1.0\) con \(|D| = N\):** Identificador unívoco estricto (candidato a Llave Primaria, UUID, Hash).
* **\(\hat{H}(X) \ll 0.3\) con \(|D| \ll N\):** Variable de baja cardinalidad (Flags binarios, Códigos de estado, Tipos enumerados).

### 1.2. Estadístico de Bondad de Ajuste a la Ley de Benford
La Ley de Benford establece que en transacciones cuantitativas naturales (contabilidad, precios, inventarios, poblaciones), el primer dígito significativo \( d \in \{1, \dots, 9\} \) sigue la distribución:
\[
P(d) = \log_{10}\left(1 + \frac{1}{d}\right)
\]
Calculamos el estadístico de prueba \( \chi^2 \):
\[
\chi^2_{\text{Benford}} = \sum_{d=1}^{9} \frac{(O_d - E_d)^2}{E_d}
\]
donde \( O_d \) es el conteo observado y \( E_d = N \cdot P(d) \) es el conteo esperado.
* Mediante el valor \( p \) derivado de una distribución \( \chi^2 \) con 8 grados de libertad, si \( p > 0.05 \), clasificamos la columna con alta confianza como **Magnitud Financiera, Económica o de Inventario**, descartando identificadores secuenciales, alturas o fechas.

### 1.3. Momentos Estadísticos Estandarizados (Varianza en una pasada vía Welford)
Para datos numéricos continuos, calculamos los momentos centrales:
* Media: \( \mu = \mathbb{E}[X] \)
* Varianza: \( \sigma^2 = \mathbb{E}[(X - \mu)^2] \)
* Asimetría (*Skewness*): \( \gamma_1 = \mathbb{E}\left[\left(\frac{X - \mu}{\sigma}\right)^3\right] \)
* Curtosis (*Kurtosis*): \( \beta_2 = \mathbb{E}\left[\left(\frac{X - \mu}{\sigma}\right)^4\right] \)
* **Interpretación:** Distribuciones de colas pesadas (\(\beta_2 \gg 3\) y \(\gamma_1 > 0\)) corresponden a leyes de potencia / Pareto, típicas de métricas de uso, balance de cuentas o accesos.

### 1.4. Modelo de Markov de Caracteres (Nivel Léxico)
Para clasificar cadenas sin procesar texto con IA, modelamos la secuencia de caracteres como una cadena de Markov de orden 1 sobre el alfabeto \( \Sigma \):
\[
P(c_{k+1} = j \mid c_k = i) = \frac{N(i \to j)}{\sum_m N(i \to m)}
\]
* La matriz de transición \( T \in \mathbb{R}^{|\Sigma| \times |\Sigma|} \) posee firmas fonotácticas universales:
  * El lenguaje natural humano presenta patrones vocálicos/consonánticos restringidos (baja entropía de transición).
  * Los hashes y UUIDs presentan matrices de transición cuasi-uniformes (máxima entropía).
  * Correos y URLs presentan picos deterministas en separadores (`@`, `.`, `/`).

---

## 2. Capa 2: Relaciones y Dependencias (Nivel Inter-Columna)

### 2.1. Dependencias de Inclusión Probabilísticas (PINDs)
Para inferir llaves foráneas sin declaraciones de catálogo:
\[
S(c_a \subseteq c_b) = \frac{|V(c_a) \cap V(c_b)|}{|V(c_a)|}
\]
donde \( V(c) \) es el conjunto de valores distintos observados en la muestra.
* Si \( S(c_a \subseteq c_b) \ge 1.0 - \epsilon \) y \( \hat{H}(c_b) \approx 1.0 \) (unicidad en la tabla destino), inferimos formalmente una arista dirigida \( c_a \xrightarrow{\text{FK}} c_b \).

### 2.2. Información Mutua Normalizada (NMI)
Para detectar dependencias funcionales ocultas y jerarquías:
\[
\text{NMI}(X; Y) = \frac{2 \cdot I(X; Y)}{H(X) + H(Y)}
\]
donde \( I(X; Y) = H(X) + H(Y) - H(X, Y) \).
* Si \( \text{NMI}(X; Y) \to 1 \), existe una biyección o codependencia funcional entre atributos (ej. `Código Postal` y `Ciudad`).

---

## 3. Capa 3: Dinámica Estocástica y Cadenas de Markov (Nivel Procesos)

### 3.1. PageRank del Esquema (Gravedad Relacional de Entidades)
Modelamos la base de datos completa como un multigrafo dirigido donde los nodos son las tablas y las aristas son las dependencias de inclusión.
Construimos la matriz estocástica de adyacencia normalizada \( P \):
\[
P_{ij} = \frac{A_{ij}}{\sum_k A_{ik}}
\]
Incorporando un factor de amortiguamiento \( \alpha = 0.85 \):
\[
\mathbf{G} = \alpha P + \frac{1 - \alpha}{|V|} \mathbf{1} \mathbf{1}^T
\]
Calculamos el vector propio estacionario \( \pi \) que satisface:
\[
\pi \mathbf{G} = \pi, \quad \sum_i \pi_i = 1
\]
* **Interpretación:** La probabilidad estacionaria \( \pi_i \) cuantifica la **gravedad de la entidad**. Las tablas con mayor \( \pi_i \) constituyen el núcleo del negocio (ej. `Pacientes` en salud, `Cuentas` en banca, `Órdenes` en comercio).

### 3.2. Cadenas de Markov de Estados de Transacción
Para cualquier tabla que contenga un campo temporal y un código de estado categórico, ordenamos las transiciones por clave de entidad y tiempo \( t \):
\[
P_{ij} = P(S_{t+1} = j \mid S_t = i)
\]
* **Estados Absorbentes:** Un estado \( k \) con \( P_{kk} = 1 \) identifica el cierre del ciclo de vida (ej. `FACTURADO`, `CANCELADO`, `ENTREGADO`).
* La matriz de transición es una huella estocástica invariante del proceso de negocio.

---

## 4. Capa 4: Álgebra Lineal Espectral (Topología Global del Esquema)

Dada la matriz de adyacencia no dirigida del esquema \( A \) y la matriz de grados \( D_{ii} = \sum_j A_{ij} \), definimos la **Matriz Laplaciana**:
\[
L = D - A
\]
Y su versión normalizada:
\[
L_{\text{norm}} = D^{-1/2} L D^{-1/2} = I - D^{-1/2} A D^{-1/2}
\]
Calculamos su descomposición espectral mediante el algoritmo de Lanczos (vía `Spectra`):
\[
0 = \lambda_0 \le \lambda_1 \le \lambda_2 \le \dots \le \lambda_{n-1}
\]
1. **Multiplicidad de \( \lambda = 0 \):** Número exacto de componentes conexas independientes (sub-sistemas desacoplados).
2. **Valor de Fiedler (\( \lambda_1 \)):** Conectividad algebraica del grafo. Valores cercanos a cero indican cuellos de botella estructurales donde el esquema puede escindirse limpiamente.
3. **Clustering Espectral:** Los componentes del vector de Fiedler (vector propio correspondiente a \( \lambda_1 \)) dividen las tablas en dos subdominios óptimos minimizando el corte de aristas (*Normalized Cut*).

---

## 5. Capa 5: Espacio Latente de Co-ocurrencias (SVD y PPMI)

Para capturar la afinidad semántica entre entidades sin recurrir a procesamiento de lenguaje natural:

### 5.1. Matriz de Co-ocurrencias y PPMI
Definimos la matriz \( M \) de co-ocurrencias de atributos y valores. Ponderamos cada par \( (i, j) \) mediante la Información Mutua Puntual Positiva:
\[
\text{PPMI}(i, j) = \max\left(\log_2 \frac{P(i, j)}{P(i) P(j)}, 0\right)
\]

### 5.2. Randomized SVD (Halko, Martinsson, Tropp)
Para factorizar matrices dispersas de alta dimensión \( M \in \mathbb{R}^{m \times n} \) en un subespacio de rango bajo \( k \):
1. Muestreo Gaussiano aleatorio: \( \Omega \in \mathbb{R}^{n \times (k + p)} \).
2. Proyección y ortogonalización QR: \( Y = M \Omega \), \( Q R = Y \).
3. Reducción a matriz pequeña: \( B = Q^T M \).
4. SVD estándar sobre matriz reducida: \( B = \tilde{U} \Sigma V^T \).
5. Reconstrucción de la base izquierda: \( U = Q \tilde{U} \).

Cada entidad o atributo obtiene un vector denso continuo \( \vec{u} \in \mathbb{R}^k \) donde la similitud del coseno:
\[
\text{Sim}(a, b) = \frac{\vec{u}_a \cdot \vec{u}_b}{\|\vec{u}_a\| \|\vec{u}_b\|}
\]
revela sinónimos y afinidad contextual funcional.

---

## 6. Tratamiento de Variaciones Léxicas: Abreviaciones y Permutaciones

### 6.1. Min-Wise Independent Permutations (MinHash)
Para ser invariantes ante permutaciones del orden de palabras (`usr_dir_envio` vs `envio_dir_usr`):
1. Se extraen los \( k \)-shingles (subcadenas de tamaño \( k \)).
2. Con \( m \) funciones hash independientes \( h_i \), se calcula la firma:
   \[
   s_i(A) = \min_{x \in A} h_i(x)
   \]
3. El estimador no sesgado del Índice de Jaccard es la tasa de colisión:
   \[
   P(s_i(A) = s_i(B)) = \frac{|A \cap B|}{|A \cup B|}
   \]

### 6.2. Métrica de Contracción de Subsecuencia Monótona
Para validar si una cadena \( A \) es una abreviación de \( B \):
\[
\text{Score}_{\text{Abrev}}(A, B) = \frac{\text{LCS}(\phi(A), \phi(B))}{\text{len}(\phi(A))}
\]
donde \( \phi(s) \) es el operador de proyección a **esqueleto consonántico** (eliminación de vocales y caracteres especiales) y \( \text{LCS} \) es la Longest Common Subsequence calculada en \( O(|A| \cdot |B|) \).
* Si el score es \( 1.0 \), \( A \) es una contracción determinista válida de \( B \).
