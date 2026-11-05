#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "intelidata/ingestion/ReservoirSampler.hpp"
#include <vector>
#include <numeric>

using namespace intelidata::ingestion;
using Catch::Matchers::WithinAbs;

TEST_CASE("ReservoirSampler: Flujo menor que la capacidad", "[reservoir]") {
    ReservoirSampler<int> sampler(10);
    REQUIRE(sampler.capacity() == 10);
    REQUIRE(sampler.size() == 0);
    REQUIRE(sampler.total_seen() == 0);
    REQUIRE_FALSE(sampler.is_full());

    for (int i = 0; i < 5; ++i) {
        sampler.update(i * 10);
    }

    REQUIRE(sampler.size() == 5);
    REQUIRE(sampler.total_seen() == 5);
    REQUIRE_FALSE(sampler.is_full());

    const auto sample = sampler.sample();
    for (size_t i = 0; i < 5; ++i) {
        REQUIRE(sample[i] == static_cast<int>(i * 10));
    }
}

TEST_CASE("ReservoirSampler: Flujo masivo mayor que la capacidad", "[reservoir]") {
    const size_t K = 50;
    const size_t N = 10000;
    ReservoirSampler<int> sampler(K, 424242);

    for (size_t i = 0; i < N; ++i) {
        sampler.update(static_cast<int>(i));
    }

    REQUIRE(sampler.size() == K);
    REQUIRE(sampler.total_seen() == N);
    REQUIRE(sampler.is_full());

    // La media de los elementos muestreados de 0..(N-1) debe aproximar la media teórica (N-1)/2
    const auto sample = sampler.sample();
    double sum = 0.0;
    for (int val : sample) {
        sum += val;
    }
    const double sample_mean = sum / static_cast<double>(K);
    const double theoretical_mean = static_cast<double>(N - 1) / 2.0; // 4999.5

    // Con K=50, el error estándar es aprox sigma / sqrt(K) ~ 2886 / 7 ~ 400
    REQUIRE_THAT(sample_mean, WithinAbs(theoretical_mean, 1000.0));
}

TEST_CASE("ReservoirSampler: Reset y reinicialización", "[reservoir]") {
    ReservoirSampler<std::string> sampler(3);
    sampler.update("A");
    sampler.update("B");
    sampler.update("C");
    sampler.update("D");
    REQUIRE(sampler.size() == 3);

    sampler.reset();
    REQUIRE(sampler.size() == 0);
    REQUIRE(sampler.total_seen() == 0);

    sampler.update("X");
    REQUIRE(sampler.size() == 1);
    REQUIRE(sampler.sample()[0] == "X");
}
