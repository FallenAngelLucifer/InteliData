#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "intelidata/core/StringMetrics.hpp"

using namespace intelidata;
using namespace intelidata::core;
using Catch::Matchers::WithinRel;
using Catch::Matchers::WithinAbs;

TEST_CASE("StringMetrics: Extracción de esqueleto consonántico", "[string_metrics]") {
    REQUIRE(extract_consonant_skeleton("usr_dir_envio") == "srdrnv");
    REQUIRE(extract_consonant_skeleton("dir_env_usuario") == "drnvsr");
    REQUIRE(extract_consonant_skeleton("id") == "d");
    REQUIRE(extract_consonant_skeleton("UUID") == "d");
    REQUIRE(extract_consonant_skeleton("io") == "io"); // Fallback alfanumérico
}

TEST_CASE("StringMetrics: Longest Common Subsequence (LCS)", "[string_metrics]") {
    REQUIRE(longest_common_subsequence("abcde", "ace") == 3);
    REQUIRE(longest_common_subsequence("srdr", "srdrnv") == 4);
    REQUIRE(longest_common_subsequence("same", "same") == 4);
    REQUIRE(longest_common_subsequence("abc", "xyz") == 0);
    REQUIRE(longest_common_subsequence("", "xyz") == 0);
}

TEST_CASE("StringMetrics: Puntuación de abreviación determinista", "[string_metrics]") {
    // usr_dir es una contracción determinista válida de usuario_direccion
    const Float64 score1 = abbreviation_score("usr_dir", "usuario_direccion");
    REQUIRE_THAT(score1, WithinAbs(1.0, 1e-12));

    // dir_env es contracción de direccion_envio
    const Float64 score2 = abbreviation_score("dir_env", "direccion_envio");
    REQUIRE_THAT(score2, WithinAbs(1.0, 1e-12));

    // Cadena completamente no relacionada
    const Float64 score3 = abbreviation_score("xyz", "usuario_direccion");
    REQUIRE_THAT(score3, WithinAbs(0.0, 1e-12));
}

TEST_CASE("StringMetrics: MinHash sobre k-shingles (invarianza de permutación)", "[string_metrics]") {
    MinHasher hasher(3);

    const std::string text_a = "usuario direccion envio domicilio";
    const std::string text_b = "domicilio envio direccion usuario"; // Permutación de palabras
    const std::string text_c = "factura impuesto tasa retencion";    // Completamente diferente

    const auto sig_a = hasher.compute_signature(text_a);
    const auto sig_b = hasher.compute_signature(text_b);
    const auto sig_c = hasher.compute_signature(text_c);

    const Float64 sim_permuted = MinHasher::estimate_jaccard(sig_a, sig_b);
    const Float64 sim_unrelated = MinHasher::estimate_jaccard(sig_a, sig_c);

    // Las permutaciones de palabras deben exhibir una alta similitud de Jaccard estimada
    REQUIRE(sim_permuted > 0.65);

    // Cadenas no relacionadas deben exhibir similitud nula o casi nula
    REQUIRE(sim_unrelated < 0.15);
}
