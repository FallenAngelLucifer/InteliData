#include "intelidata/core/SVDLatent.hpp"
#include <cmath>
#include <random>
#include <algorithm>

namespace intelidata::core {

Eigen::MatrixXd compute_ppmi(const Eigen::MatrixXd& cooccurrence_matrix) {
    const auto m = cooccurrence_matrix.rows();
    const auto n = cooccurrence_matrix.cols();

    if (m == 0 || n == 0) {
        return Eigen::MatrixXd();
    }

    const Float64 total_sum = cooccurrence_matrix.sum();
    if (total_sum <= 0.0) {
        return Eigen::MatrixXd::Zero(m, n);
    }

    const Eigen::VectorXd row_sums = cooccurrence_matrix.rowwise().sum();
    const Eigen::VectorXd col_sums = cooccurrence_matrix.colwise().sum();

    Eigen::MatrixXd ppmi = Eigen::MatrixXd::Zero(m, n);

    for (Eigen::Index i = 0; i < m; ++i) {
        const Float64 row_val = row_sums(i);
        if (row_val <= 0.0) continue;

        for (Eigen::Index j = 0; j < n; ++j) {
            const Float64 count = cooccurrence_matrix(i, j);
            const Float64 col_val = col_sums(j);

            if (count > 0.0 && col_val > 0.0) {
                // P(i, j) / (P(i) * P(j)) = (count * total_sum) / (row_val * col_val)
                const Float64 ratio = (count * total_sum) / (row_val * col_val);
                if (ratio > 1.0) {
                    ppmi(i, j) = std::log2(ratio);
                }
            }
        }
    }

    return ppmi;
}

SVDResult randomized_svd(
    const Eigen::MatrixXd& matrix,
    Size target_rank,
    Size oversampling,
    Size power_iterations,
    UInt64 seed) {
    SVDResult result;
    const auto m = matrix.rows();
    const auto n = matrix.cols();

    if (m == 0 || n == 0 || target_rank == 0) {
        return result;
    }

    const auto min_dim = std::min(m, n);
    const Size l_size = std::min<Size>(static_cast<Size>(min_dim), target_rank + oversampling);
    const Size k_size = std::min<Size>(target_rank, l_size);
    const auto l = static_cast<Eigen::Index>(l_size);
    const auto k = static_cast<Eigen::Index>(k_size);

    // 1. Muestreo Gaussiano aleatorio: Omega (n x l)
    std::mt19937_64 rng(seed);
    std::normal_distribution<Float64> dist(0.0, 1.0);

    Eigen::MatrixXd omega(n, l);
    for (Eigen::Index j = 0; j < l; ++j) {
        for (Eigen::Index i = 0; i < n; ++i) {
            omega(i, j) = dist(rng);
        }
    }

    // 2. Iteraciones de potencia para mejorar la separación de valores singulares
    // Y = (M * M^T)^q * M * Omega
    Eigen::MatrixXd y = matrix * omega;
    for (Size it = 0; it < power_iterations; ++it) {
        // Ortogonalización intermedia para estabilidad numérica
        Eigen::HouseholderQR<Eigen::MatrixXd> qr_sub(y);
        y = qr_sub.householderQ() * Eigen::MatrixXd::Identity(m, l);
        y = matrix * (matrix.transpose() * y);
    }

    // 3. Ortogonalización QR final: Y = Q * R
    Eigen::HouseholderQR<Eigen::MatrixXd> qr(y);
    Eigen::MatrixXd q = qr.householderQ() * Eigen::MatrixXd::Identity(m, l);

    // 4. Proyección a matriz compacta B = Q^T * M (l x n)
    Eigen::MatrixXd b = q.transpose() * matrix;

    // 5. SVD estándar sobre la matriz pequeña B
    Eigen::BDCSVD<Eigen::MatrixXd> svd(b, Eigen::ComputeThinU | Eigen::ComputeThinV);

    // 6. Reconstrucción de la base izquierda U = Q * U_tilde
    const Eigen::MatrixXd u_full = q * svd.matrixU();
    const Eigen::VectorXd s_full = svd.singularValues();
    const Eigen::MatrixXd v_full = svd.matrixV();

    // 7. Truncar al rango k deseado
    result.U = u_full.leftCols(k);
    result.S = s_full.head(k);
    result.V = v_full.leftCols(k);

    return result;
}

Float64 cosine_similarity(
    const Eigen::VectorXd& vec_a,
    const Eigen::VectorXd& vec_b) noexcept {
    const Float64 norm_a = vec_a.norm();
    const Float64 norm_b = vec_b.norm();

    if (norm_a <= 1e-12 || norm_b <= 1e-12) {
        return 0.0;
    }

    const Float64 cos_sim = vec_a.dot(vec_b) / (norm_a * norm_b);
    return std::clamp(cos_sim, -1.0, 1.0);
}

} // namespace intelidata::core
