#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "intelidata/core/Benford.hpp"
#include <vector>
#include <cmath>

using namespace intelidata::core;
using Catch::Matchers::WithinRel;
using Catch::Matchers::WithinAbs;

TEST_CASE("Benford: Extracción de primer dígito significativo", "[benford]") {
    REQUIRE(extract_leading_digit(42.0) == 4);
    REQUIRE(extract_leading_digit(0.00789) == 7);
    REQUIRE(extract_leading_digit(-999.5) == 9);
    REQUIRE(extract_leading_digit(1.0) == 1);
    REQUIRE(extract_leading_digit(0.0) == std::nullopt);
    REQUIRE(extract_leading_digit(std::numeric_limits<double>::infinity()) == std::nullopt);

    REQUIRE(extract_leading_digit_str("  -0.00512 ") == 5);
    REQUIRE(extract_leading_digit_str("+1024") == 1);
    REQUIRE(extract_leading_digit_str("0000845.2") == 8);
    REQUIRE(extract_leading_digit_str("0.0000") == std::nullopt);
}

TEST_CASE("Benford: p-value analítico para chi-cuadrado con 8 grados de libertad", "[benford]") {
    // Para chi2 = 0, p-value debe ser 1.0
    REQUIRE_THAT(chi_square_8df_p_value(0.0), WithinAbs(1.0, 1e-12));

    // Valor crítico para alpha = 0.05 con df = 8 es aprox 15.5073
    REQUIRE_THAT(chi_square_8df_p_value(15.5073), WithinAbs(0.05, 1e-3));

    // Valor crítico para alpha = 0.01 con df = 8 es aprox 20.0902
    REQUIRE_THAT(chi_square_8df_p_value(20.0902), WithinAbs(0.01, 1e-3));
}

TEST_CASE("Benford: Secuencia que cumple la Ley de Benford (Potencias de 2)", "[benford]") {
    // Por el teorema de equidistribución de Weyl, las potencias de 2 siguen Benford exactamente
    BenfordAccumulator acc;
    double val = 1.0;
    for (int i = 0; i < 500; ++i) {
        acc.update(val);
        val *= 2.0;
        if (val > 1e15) {
            val /= 1e15; // Mantener dentro de rango numérico
        }
    }

    const auto result = acc.evaluate();
    REQUIRE(result.has_sufficient_sample);
    REQUIRE(result.p_value > 0.05);
    REQUIRE(result.mean_absolute_deviation < 0.015);
    REQUIRE(result.is_benford_distributed());
}

TEST_CASE("Benford: Secuencia uniforme o secuencial rechaza la Ley de Benford", "[benford]") {
    BenfordAccumulator acc;
    // Secuencia uniforme de 900 números donde cada dígito 1..9 aparece exactamente 100 veces
    for (int d = 1; d <= 9; ++d) {
        for (int i = 0; i < 100; ++i) {
            acc.update(static_cast<double>(d * 100 + i));
        }
    }

    const auto result = acc.evaluate();
    REQUIRE(result.has_sufficient_sample);
    // Debe rechazar con alta significancia (p << 0.001)
    REQUIRE(result.p_value < 1e-6);
    REQUIRE_FALSE(result.is_benford_distributed());
}
