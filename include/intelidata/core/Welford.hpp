#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include <concepts>
#include <span>
#include <limits>

namespace intelidata::core {

/**
 * @brief Acumulador estadístico de una sola pasada (algoritmo de Welford / Pébay).
 * Calcula media, varianza, asimetría (skewness) y curtosis de forma numéricamente estable,
 * soportando paralelismo y fusión asociativa (combine).
 */
class WelfordAccumulator {
public:
    WelfordAccumulator() noexcept;

    // Actualización elemento a elemento
    void update(Float64 x) noexcept;

    // Actualización en lote para spans de cualquier tipo numérico
    template <Numeric T>
    void update_batch(std::span<const T> values) noexcept {
        for (const auto& v : values) {
            update(static_cast<Float64>(v));
        }
    }

    // Fusión paralela de dos acumuladores (Pébay 2008)
    void combine(const WelfordAccumulator& other) noexcept;
    WelfordAccumulator& operator+=(const WelfordAccumulator& other) noexcept;

    // Métricas y momentos
    [[nodiscard]] UInt64 count() const noexcept { return count_; }
    [[nodiscard]] Float64 mean() const noexcept { return mean_; }
    [[nodiscard]] Float64 variance(bool sample = true) const noexcept;
    [[nodiscard]] Float64 standard_deviation(bool sample = true) const noexcept;
    [[nodiscard]] Float64 skewness() const noexcept;
    [[nodiscard]] Float64 kurtosis() const noexcept;
    [[nodiscard]] Float64 excess_kurtosis() const noexcept;
    [[nodiscard]] Float64 min() const noexcept { return min_; }
    [[nodiscard]] Float64 max() const noexcept { return max_; }

    // Reinicializar
    void reset() noexcept;

private:
    UInt64 count_{0};
    Float64 mean_{0.0};
    Float64 m2_{0.0};
    Float64 m3_{0.0};
    Float64 m4_{0.0};
    Float64 min_{std::numeric_limits<Float64>::infinity()};
    Float64 max_{-std::numeric_limits<Float64>::infinity()};
};

[[nodiscard]] WelfordAccumulator operator+(const WelfordAccumulator& a, const WelfordAccumulator& b) noexcept;

} // namespace intelidata::core
