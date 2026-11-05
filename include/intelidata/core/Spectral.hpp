#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include <vector>
#include <Eigen/Dense>

namespace intelidata::core {

struct SpectralDecompositionResult {
    Eigen::VectorXd eigenvalues;           // 0 = \lambda_0 <= \lambda_1 <= ... <= \lambda_{n-1}
    Eigen::MatrixXd eigenvectors;          // Columnas: vectores propios ortonormales
    Float64 fiedler_value{0.0};            // \lambda_1 (conectividad algebraica)
    Eigen::VectorXd fiedler_vector;        // Vector propio v_1 correspondiente a \lambda_1
    std::vector<Int32> spectral_clusters;  // Partición óptima de corte normalizado (0 o 1)
    Size connected_components{1};          // Multiplicidad de valores propios \approx 0
    Float64 algebraic_connectivity{0.0};   // Sinónimo formal de fiedler_value
};

/**
 * @brief Simetriza una matriz de adyacencia dirigida A: A_sym = 0.5 * (A + A^T).
 */
[[nodiscard]] Eigen::MatrixXd symmetrize_adjacency(const Eigen::MatrixXd& adj);

/**
 * @brief Construye la Matriz Laplaciana Normalizada L_norm = I - D^(-1/2) * A_sym * D^(-1/2).
 */
[[nodiscard]] Eigen::MatrixXd build_normalized_laplacian(const Eigen::MatrixXd& symmetric_adj);

/**
 * @brief Realiza el análisis espectral completo del grafo del esquema.
 * Identifica componentes conexas, valor y vector de Fiedler, y clustering espectral (Normalized Cut).
 */
[[nodiscard]] SpectralDecompositionResult compute_spectral_analysis(const Eigen::MatrixXd& adjacency_matrix);

} // namespace intelidata::core
