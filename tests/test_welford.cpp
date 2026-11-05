#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "intelidata/core/Welford.hpp"
#include <vector>
#include <random>

using namespace intelidata::core;
using Catch::Matchers::WithinRel;
using Catch::Matchers::WithinAbs;

TEST_CASE("WelfordAccumulator: Comportamiento inicial y un solo elemento", "[welford]") {
    WelfordAccumulator acc;
    REQUIRE(acc.count() == 0);
    REQUIRE(acc.mean() == 0.0);
    REQUIRE(acc.variance() == 0.0);
    REQUIRE(acc.standard_deviation() == 0.0);
    REQUIRE(acc.skewness() == 0.0);
    REQUIRE(acc.kurtosis() == 0.0);

    acc.update(42.0);
    REQUIRE(acc.count() == 1);
    REQUIRE(acc.mean() == 42.0);
    REQUIRE(acc.variance() == 0.0);
    REQUIRE(acc.standard_deviation() == 0.0);
    REQUIRE(acc.min() == 42.0);
    REQUIRE(acc.max() == 42.0);
}

TEST_CASE("WelfordAccumulator: Vector con valores conocidos", "[welford]") {
    // Vector de prueba clásico: {2, 4, 4, 4, 5, 5, 7, 9}
    const std::vector<double> data = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    WelfordAccumulator acc;
    acc.update_batch<double>(data);

    REQUIRE(acc.count() == 8);
    REQUIRE_THAT(acc.mean(), WithinAbs(5.0, 1e-12));
    REQUIRE_THAT(acc.variance(true), WithinRel(32.0 / 7.0, 1e-12));
    REQUIRE_THAT(acc.variance(false), WithinRel(4.0, 1e-12));
    REQUIRE_THAT(acc.standard_deviation(false), WithinRel(2.0, 1e-12));
    REQUIRE(acc.min() == 2.0);
    REQUIRE(acc.max() == 9.0);
}

TEST_CASE("WelfordAccumulator: Asimetría cero para distribución simétrica", "[welford]") {
    const std::vector<double> symmetric = {-3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0};
    WelfordAccumulator acc;
    acc.update_batch<double>(symmetric);

    REQUIRE(acc.count() == 7);
    REQUIRE_THAT(acc.mean(), WithinAbs(0.0, 1e-12));
    REQUIRE_THAT(acc.skewness(), WithinAbs(0.0, 1e-12));
}

TEST_CASE("WelfordAccumulator: Fusión paralela asociativa (combine)", "[welford]") {
    std::mt19937_64 rng(42);
    std::normal_distribution<double> dist(100.0, 15.0);

    std::vector<double> all_data(10000);
    for (auto& val : all_data) {
        val = dist(rng);
    }

    // 1. Acumulación secuencial completa
    WelfordAccumulator sequential_acc;
    sequential_acc.update_batch<double>(all_data);

    // 2. Acumulación dividida en 4 particiones concurrentes simuladas
    WelfordAccumulator p1, p2, p3, p4;
    p1.update_batch(std::span<const double>(all_data.data(), 2500));
    p2.update_batch(std::span<const double>(all_data.data() + 2500, 2500));
    p3.update_batch(std::span<const double>(all_data.data() + 5000, 2500));
    p4.update_batch(std::span<const double>(all_data.data() + 7500, 2500));

    // Fusión
    WelfordAccumulator merged_acc = p1 + p2 + p3 + p4;

    REQUIRE(merged_acc.count() == sequential_acc.count());
    REQUIRE_THAT(merged_acc.mean(), WithinRel(sequential_acc.mean(), 1e-12));
    REQUIRE_THAT(merged_acc.variance(true), WithinRel(sequential_acc.variance(true), 1e-12));
    REQUIRE_THAT(merged_acc.skewness(), WithinRel(sequential_acc.skewness(), 1e-10));
    REQUIRE_THAT(merged_acc.kurtosis(), WithinRel(sequential_acc.kurtosis(), 1e-10));
    REQUIRE(merged_acc.min() == sequential_acc.min());
    REQUIRE(merged_acc.max() == sequential_acc.max());
}
