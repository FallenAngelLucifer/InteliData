#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "intelidata/core/Spectral.hpp"

using namespace intelidata;
using namespace intelidata::core;
using Catch::Matchers::WithinRel;
using Catch::Matchers::WithinAbs;

TEST_CASE("Spectral: Grafo desconectado en dos componentes conexas", "[spectral]") {
    // Grafo de 4 nodos con 2 componentes completamente aisladas:
    // Componente A: {0, 1} con arista entre 0 y 1
    // Componente B: {2, 3} con arista entre 2 y 3
    Eigen::MatrixXd adj = Eigen::MatrixXd::Zero(4, 4);
    adj(0, 1) = 1.0;
    adj(1, 0) = 1.0;
    adj(2, 3) = 1.0;
    adj(3, 2) = 1.0;

    const auto res = compute_spectral_analysis(adj);

    // Multiplicidad de autovalores cero = 2 componentes
    REQUIRE(res.connected_components == 2);
    REQUIRE_THAT(res.fiedler_value, WithinAbs(0.0, 1e-4));
}

TEST_CASE("Spectral: Grafo tipo Barbell y particionamiento espectral óptimo", "[spectral]") {
    // 6 nodos: {0, 1, 2} cluster izquierdo, {3, 4, 5} cluster derecho
    // Un puente débil entre el nodo 2 y el nodo 3
    Eigen::MatrixXd adj = Eigen::MatrixXd::Zero(6, 6);

    // Clique 1: {0, 1, 2}
    adj(0, 1) = 1.0; adj(1, 0) = 1.0;
    adj(1, 2) = 1.0; adj(2, 1) = 1.0;
    adj(0, 2) = 1.0; adj(2, 0) = 1.0;

    // Clique 2: {3, 4, 5}
    adj(3, 4) = 1.0; adj(4, 3) = 1.0;
    adj(4, 5) = 1.0; adj(5, 4) = 1.0;
    adj(3, 5) = 1.0; adj(5, 3) = 1.0;

    // Puente único
    adj(2, 3) = 0.5; adj(3, 2) = 0.5;

    const auto res = compute_spectral_analysis(adj);

    // Grafo conexo único
    REQUIRE(res.connected_components == 1);
    // Conectividad algebraica positiva
    REQUIRE(res.fiedler_value > 0.0);

    // Los nodos {0, 1, 2} deben pertenecer al mismo cluster espectral
    REQUIRE(res.spectral_clusters[0] == res.spectral_clusters[1]);
    REQUIRE(res.spectral_clusters[1] == res.spectral_clusters[2]);

    // Los nodos {3, 4, 5} deben pertenecer al mismo cluster espectral
    REQUIRE(res.spectral_clusters[3] == res.spectral_clusters[4]);
    REQUIRE(res.spectral_clusters[4] == res.spectral_clusters[5]);

    // Ambos clusters deben ser distintos entre sí (bipartición 0 vs 1)
    REQUIRE(res.spectral_clusters[0] != res.spectral_clusters[3]);
}
