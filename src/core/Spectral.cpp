#include "intelidata/core/Spectral.hpp"
#include <cmath>
#include <algorithm>

namespace intelidata::core {

Eigen::MatrixXd symmetrize_adjacency(const Eigen::MatrixXd& adj) {
    const auto n = adj.rows();
    Eigen::MatrixXd sym = 0.5 * (adj + adj.transpose());
    // Eliminar auto-bucles en la diagonal
    for (Eigen::Index i = 0; i < n; ++i) {
        sym(i, i) = 0.0;
    }
    return sym;
}

Eigen::MatrixXd build_normalized_laplacian(const Eigen::MatrixXd& symmetric_adj) {
    const auto n = symmetric_adj.rows();
    if (n == 0) return Eigen::MatrixXd();

    Eigen::VectorXd deg = symmetric_adj.rowwise().sum();
    Eigen::VectorXd inv_sqrt_deg(n);

    for (Eigen::Index i = 0; i < n; ++i) {
        if (deg(i) > 1e-12) {
            inv_sqrt_deg(i) = 1.0 / std::sqrt(deg(i));
        } else {
            inv_sqrt_deg(i) = 0.0;
        }
    }

    // L_norm = I - D^(-1/2) * A * D^(-1/2)
    Eigen::MatrixXd d_inv = inv_sqrt_deg.asDiagonal();
    Eigen::MatrixXd dad = d_inv * symmetric_adj * d_inv;
    Eigen::MatrixXd l_norm = Eigen::MatrixXd::Identity(n, n) - dad;

    // Para nodos aislados (grado 0), la diagonal en Laplaciano normalizado se fija en 0
    for (Eigen::Index i = 0; i < n; ++i) {
        if (deg(i) <= 1e-12) {
            l_norm(i, i) = 0.0;
        }
    }

    return l_norm;
}

SpectralDecompositionResult compute_spectral_analysis(const Eigen::MatrixXd& adjacency_matrix) {
    SpectralDecompositionResult result;
    const auto n = adjacency_matrix.rows();

    if (n == 0) {
        return result;
    }

    if (n == 1) {
        result.eigenvalues = Eigen::VectorXd::Zero(1);
        result.eigenvectors = Eigen::MatrixXd::Identity(1, 1);
        result.fiedler_value = 0.0;
        result.algebraic_connectivity = 0.0;
        result.fiedler_vector = Eigen::VectorXd::Zero(1);
        result.spectral_clusters = {0};
        result.connected_components = 1;
        return result;
    }

    const Eigen::MatrixXd sym_adj = symmetrize_adjacency(adjacency_matrix);
    const Eigen::MatrixXd l_norm = build_normalized_laplacian(sym_adj);

    // Descomposición espectral exacta autoadjunta
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> solver(l_norm);
    if (solver.info() != Eigen::Success) {
        return result;
    }

    result.eigenvalues = solver.eigenvalues();
    result.eigenvectors = solver.eigenvectors();

    // Contar componentes conexas (multiplicidad de autovalores cercanos a 0)
    Size zero_count = 0;
    for (Eigen::Index i = 0; i < n; ++i) {
        if (std::abs(result.eigenvalues(i)) < 1e-4) {
            zero_count++;
        }
    }
    result.connected_components = std::max<Size>(1, zero_count);

    // Vector y valor de Fiedler (segundo autovalor más pequeño lambda_1)
    result.fiedler_value = std::max(0.0, result.eigenvalues(1));
    result.algebraic_connectivity = result.fiedler_value;
    result.fiedler_vector = result.eigenvectors.col(1);

    // Clustering espectral (partición por signo en el vector de Fiedler)
    result.spectral_clusters.resize(static_cast<Size>(n));
    for (Eigen::Index i = 0; i < n; ++i) {
        result.spectral_clusters[static_cast<Size>(i)] = (result.fiedler_vector(i) >= 0.0) ? 1 : 0;
    }

    return result;
}

} // namespace intelidata::core
