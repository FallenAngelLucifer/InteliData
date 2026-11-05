#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include <array>
#include <string_view>
#include <span>
#include <optional>

namespace intelidata::core {

struct BenfordResult {
    std::array<UInt64, 10> observed_counts{};     // Índices 1 a 9 (índice 0 no usado)
    std::array<Float64, 10> observed_proportions{}; // O_d / N
    std::array<Float64, 10> expected_proportions{}; // P(d) = log10(1 + 1/d)
    UInt64 total_valid_samples{0};
    Float64 chi_square_stat{0.0};
    Float64 p_value{0.0};
    Float64 mean_absolute_deviation{0.0}; // MAD de Nigrini
    bool has_sufficient_sample{false};    // N >= 50

    [[nodiscard]] bool is_benford_distributed(Float64 alpha = 0.05) const noexcept {
        return has_sufficient_sample && (p_value >= alpha);
    }
};

/**
 * @brief Extrae el primer dígito significativo d in {1..9} de un número.
 * Retorna std::nullopt si el número es cero, NaN o infinito.
 */
[[nodiscard]] std::optional<UInt32> extract_leading_digit(Float64 value) noexcept;

/**
 * @brief Extrae el primer dígito significativo a partir de una representación en cadena.
 */
[[nodiscard]] std::optional<UInt32> extract_leading_digit_str(std::string_view str) noexcept;

/**
 * @brief Retorna la probabilidad teórica de Benford para el dígito d (1..9).
 */
[[nodiscard]] Float64 benford_theoretical_probability(UInt32 digit) noexcept;

/**
 * @brief Calcula el p-value exacto para chi-cuadrado con 8 grados de libertad en forma analítica cerrada.
 */
[[nodiscard]] Float64 chi_square_8df_p_value(Float64 chi2) noexcept;

/**
 * @brief Acumulador de histograma del primer dígito para prueba de Benford.
 */
class BenfordAccumulator {
public:
    BenfordAccumulator() noexcept;

    // Actualización con número
    void update(Float64 value) noexcept;

    // Actualización con cadena numérica
    void update(std::string_view str) noexcept;

    // Actualización por lote para spans numéricos
    template <Numeric T>
    void update_batch(std::span<const T> values) noexcept {
        for (const auto& v : values) {
            update(static_cast<Float64>(v));
        }
    }

    // Fusión de acumuladores concurrentes
    void combine(const BenfordAccumulator& other) noexcept;
    BenfordAccumulator& operator+=(const BenfordAccumulator& other) noexcept;

    // Evaluación final del test de bondad de ajuste
    [[nodiscard]] BenfordResult evaluate() const noexcept;

    [[nodiscard]] UInt64 total_samples() const noexcept { return total_valid_samples_; }
    [[nodiscard]] const std::array<UInt64, 10>& counts() const noexcept { return counts_; }

    void reset() noexcept;

private:
    std::array<UInt64, 10> counts_{};
    UInt64 total_valid_samples_{0};
};

} // namespace intelidata::core
