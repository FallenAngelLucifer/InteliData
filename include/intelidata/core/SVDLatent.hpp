#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include <Eigen/Dense>

namespace intelidata::core {

struct SVDResult {
    Eigen::MatrixXd U; // m x k (vectores latentes de fila/entidad)
    Eigen::VectorXd S; // k (valores singulares decrecientes)
    Eigen::MatrixXd V; // n x k (vectores latentes de columna)
};

/**
 * @brief Transforma una matriz de co-ocurrencias brutas en PPMI (Positive Pointwise Mutual Information).
 * PPMI(i, j) = max(log2( P(i, j) / (P(i) * P(j)) ), 0.0)
 */
[[nodiscard]] Eigen::MatrixXd compute_ppmi(const Eigen::MatrixXd& cooccurrence_matrix);

/**
 * @brief Randomized SVD (Halko, Martinsson, Tropp 2011).
 * Factoriza eficientemente matrices de alta dimensión en un subespacio de rango bajo k.
 */
[[nodiscard]] SVDResult randomized_svd(
    const Eigen::MatrixXd& matrix,
    Size target_rank,
    Size oversampling = 5,
    Size power_iterations = 2,
    UInt64 seed = 42ULL);

/**
 * @brief Calcula la similitud de coseno entre dos vectores densos en el espacio latente.
 */
[[nodiscard]] Float64 cosine_similarity(
    const Eigen::VectorXd& vec_a,
    const Eigen::VectorXd& vec_b) noexcept;

} // namespace intelidata::core
