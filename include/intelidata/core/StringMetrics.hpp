#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <array>
#include <cstdint>

namespace intelidata::core {

/**
 * @brief Extrae el esqueleto consonántico de una cadena.
 * Convierte a minúsculas, elimina caracteres especiales (_ - espaciado) y vocales (a, e, i, o, u, y).
 * Ejemplo: "usr_dir_envio" -> "srdrnv"
 */
[[nodiscard]] std::string extract_consonant_skeleton(std::string_view str);

/**
 * @brief Longest Common Subsequence (LCS) clásica en O(|A| * |B|).
 */
[[nodiscard]] Size longest_common_subsequence(std::string_view a, std::string_view b) noexcept;

/**
 * @brief Métrica de contracción de subsecuencia monótona para resolución de abreviaciones.
 * Score = LCS(phi(A), phi(B)) / len(phi(A))
 * Retorna 1.0 si phi(A) es una subsecuencia exacta no decreciente de phi(B).
 */
[[nodiscard]] Float64 abbreviation_score(std::string_view candidate_abbrev, std::string_view target_full) noexcept;

/**
 * @brief MinHash sobre k-shingles para estimar el índice de similitud de Jaccard
 * de forma invariante a permutaciones del orden de palabras.
 */
class MinHasher {
public:
    static constexpr Size NUM_HASHES = 64;

    using Signature = std::array<UInt64, NUM_HASHES>;

    explicit MinHasher(UInt32 k_shingle_size = 3, UInt64 seed = 0x9e3779b97f4a7c15ULL) noexcept;

    // Calcula la firma MinHash de una cadena
    [[nodiscard]] Signature compute_signature(std::string_view text) const;

    // Estima el índice de Jaccard entre dos firmas MinHash en [0.0, 1.0]
    [[nodiscard]] static Float64 estimate_jaccard(const Signature& sig_a, const Signature& sig_b) noexcept;

    // Calcula el índice de Jaccard exacto sobre conjuntos de k-shingles
    [[nodiscard]] Float64 exact_jaccard(std::string_view text_a, std::string_view text_b) const;

private:
    UInt32 k_{3};
    std::array<UInt64, NUM_HASHES> hash_seeds_{};
};

} // namespace intelidata::core
