#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include <string>
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <span>
#include <vector>

namespace intelidata::core {

struct InclusionResult {
    Float64 inclusion_ratio{0.0};  // |V(c_a) \cap V(c_b)| / |V(c_a)|
    UInt64 matched_count{0};
    UInt64 source_distinct_count{0};
    UInt64 target_distinct_count{0};
    bool is_likely_foreign_key{false};
    Float64 confidence{0.0};
};

/**
 * @brief Calcula el grado de inclusión probabilística S(c_a \subseteq c_b).
 * @param source_distinct Conjunto de valores únicos en la columna origen c_a.
 * @param target_distinct Conjunto de valores únicos en la columna destino c_b.
 * @param target_is_unique Si la columna destino exhibe unicidad estricta (candidato a PK).
 * @param epsilon Tolerancia a ruido de muestreo (default: 0.02, es decir 98% de inclusión).
 */
[[nodiscard]] InclusionResult evaluate_inclusion(
    const std::unordered_set<std::string>& source_distinct,
    const std::unordered_set<std::string>& target_distinct,
    bool target_is_unique = true,
    Float64 epsilon = 0.02) noexcept;

/**
 * @brief Calcula la Información Mutua Normalizada (NMI) entre dos columnas alineadas X e Y.
 * NMI(X; Y) = 2 * I(X; Y) / (H(X) + H(Y))
 * Detecta dependencias funcionales ocultas, correlaciones no lineales y jerarquías (e.g. Código Postal -> Ciudad).
 */
[[nodiscard]] Float64 compute_nmi(
    std::span<const std::string> x_values,
    std::span<const std::string> y_values) noexcept;

[[nodiscard]] Float64 compute_nmi_views(
    std::span<const std::string_view> x_values,
    std::span<const std::string_view> y_values) noexcept;

} // namespace intelidata::core
