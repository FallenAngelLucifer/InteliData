#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "intelidata/core/SVDLatent.hpp"

using namespace intelidata;
using namespace intelidata::core;
using Catch::Matchers::WithinRel;
using Catch::Matchers::WithinAbs;

TEST_CASE("SVDLatent: Cálculo de PPMI no negativo", "[svd]") {
    Eigen::MatrixXd cooc(3, 3);
    cooc << 10.0,  2.0,  0.0,
             2.0, 15.0,  1.0,
             0.0,  1.0,  8.0;

    const auto ppmi = compute_ppmi(cooc);

    // Todos los elementos de PPMI deben ser no negativos
    for (Eigen::Index i = 0; i < 3; ++i) {
        for (Eigen::Index j = 0; j < 3; ++j) {
            REQUIRE(ppmi(i, j) >= 0.0);
        }
    }

    // La diagonal principal exhibe una fuerte co-ocurrencia propia
    REQUIRE(ppmi(0, 0) > 0.0);
    REQUIRE(ppmi(1, 1) > 0.0);
    REQUIRE(ppmi(2, 2) > 0.0);
}

TEST_CASE("SVDLatent: Factorización Randomized SVD de rango bajo", "[svd]") {
    // Construir una matriz sintética de rango 2 exacto: M = A * B^T
    // A: 20 x 2, B: 15 x 2
    Eigen::MatrixXd a = Eigen::MatrixXd::Random(20, 2);
    Eigen::MatrixXd b = Eigen::MatrixXd::Random(15, 2);
    Eigen::MatrixXd m = a * b.transpose();

    // Aproximar con rango k = 2
    const auto svd_res = randomized_svd(m, 2, 5, 2, 1337);

    REQUIRE(svd_res.U.cols() == 2);
    REQUIRE(svd_res.S.size() == 2);
    REQUIRE(svd_res.V.cols() == 2);

    // Reconstrucción: M_approx = U * diag(S) * V^T
    const Eigen::MatrixXd m_approx = svd_res.U * svd_res.S.asDiagonal() * svd_res.V.transpose();

    // El error de reconstrucción en norma Frobenius debe ser casi cero
    const Float64 frobenius_error = (m - m_approx).norm() / m.norm();
    REQUIRE(frobenius_error < 1e-4);
}

TEST_CASE("SVDLatent: Similitud Coseno de vectores latentes", "[svd]") {
    Eigen::VectorXd v1(3);
    v1 << 1.0, 2.0, 3.0;

    Eigen::VectorXd v2(3);
    v2 << 2.0, 4.0, 6.0; // Paralelo a v1

    Eigen::VectorXd v3(3);
    v3 << -1.0, -2.0, -3.0; // Opuesto a v1

    Eigen::VectorXd v4(3);
    v4 << 0.0, 3.0, -2.0; // Ortogonal a v1 (1*0 + 2*3 + 3*(-2) = 0)

    REQUIRE_THAT(cosine_similarity(v1, v2), WithinAbs(1.0, 1e-12));
    REQUIRE_THAT(cosine_similarity(v1, v3), WithinAbs(-1.0, 1e-12));
    REQUIRE_THAT(cosine_similarity(v1, v4), WithinAbs(0.0, 1e-12));
}
