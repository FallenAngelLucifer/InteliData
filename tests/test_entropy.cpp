#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "intelidata/core/Entropy.hpp"
#include <string>
#include <vector>

using namespace intelidata::core;
using Catch::Matchers::WithinRel;
using Catch::Matchers::WithinAbs;

TEST_CASE("Entropy: Columna constante (|D| = 1)", "[entropy]") {
    CategoricalEntropyTracker tracker;
    for (int i = 0; i < 50; ++i) {
        tracker.update("CONSTANT_STATUS");
    }

    const auto result = tracker.compute();
    REQUIRE(result.total_count == 50);
    REQUIRE(result.distinct_count == 1);
    REQUIRE_THAT(result.raw_entropy_bits, WithinAbs(0.0, 1e-12));
    REQUIRE_THAT(result.normalized_entropy, WithinAbs(0.0, 1e-12));
    REQUIRE_FALSE(result.is_candidate_key());
    REQUIRE(result.is_low_cardinality());
}

TEST_CASE("Entropy: Llave unívoca estricta (|D| = N)", "[entropy]") {
    CategoricalEntropyTracker tracker;
    const int N = 128;
    for (int i = 0; i < N; ++i) {
        tracker.update("ID_" + std::to_string(i));
    }

    const auto result = tracker.compute();
    REQUIRE(result.total_count == N);
    REQUIRE(result.distinct_count == N);
    REQUIRE_THAT(result.raw_entropy_bits, WithinAbs(7.0, 1e-12)); // log2(128) = 7
    REQUIRE_THAT(result.normalized_entropy, WithinAbs(1.0, 1e-12));
    REQUIRE(result.is_candidate_key());
}

TEST_CASE("Entropy: Variable binaria 50/50", "[entropy]") {
    CategoricalEntropyTracker tracker;
    for (int i = 0; i < 100; ++i) {
        tracker.update(i < 50 ? "TRUE" : "FALSE");
    }

    const auto result = tracker.compute();
    REQUIRE(result.total_count == 100);
    REQUIRE(result.distinct_count == 2);
    REQUIRE_THAT(result.raw_entropy_bits, WithinAbs(1.0, 1e-12));
    REQUIRE_THAT(result.normalized_entropy, WithinAbs(1.0, 1e-12));
    REQUIRE_FALSE(result.is_candidate_key());
    REQUIRE(result.is_low_cardinality());
}

TEST_CASE("Entropy: NumericEntropyTracker con enteros y combinación", "[entropy]") {
    NumericEntropyTracker<int> t1;
    NumericEntropyTracker<int> t2;

    for (int i = 0; i < 50; ++i) {
        t1.update(i);
        t2.update(i + 50);
    }

    t1.combine(t2);
    const auto result = t1.compute();

    REQUIRE(result.total_count == 100);
    REQUIRE(result.distinct_count == 100);
    REQUIRE(result.is_candidate_key());
}
