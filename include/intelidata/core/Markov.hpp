#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <span>
#include <array>
#include <Eigen/Dense>

namespace intelidata::core {

struct MarkovStateResult {
    std::vector<std::string> states;
    Eigen::MatrixXd transition_matrix; // P(S_{t+1} = j | S_t = i)
    std::vector<std::string> absorbing_states; // Estados donde P(k -> k) >= 0.99
    Float64 average_entropy{0.0};
    bool is_discrete_state_machine{false};
};

/**
 * @brief Modelo de Cadena de Markov para estados discretos de negocio (ciclo de vida de pedidos, tickets, etc.).
 */
class MarkovChainModel {
public:
    MarkovChainModel() = default;

    // Registra una transición observada S_t -> S_{t+1}
    void add_transition(std::string_view from_state, std::string_view to_state);

    // Registra una secuencia cronológica completa de estados para una misma entidad
    void add_sequence(std::span<const std::string> sequence);

    // Fusión asociativa de modelos
    void combine(const MarkovChainModel& other);

    // Calcula la matriz estocástica y detecta estados absorbentes
    [[nodiscard]] MarkovStateResult analyze(Float64 absorbing_threshold = 0.98) const;

    [[nodiscard]] UInt64 total_transitions() const noexcept { return total_transitions_; }
    void reset() noexcept;

private:
    std::unordered_map<std::string, Size> state_to_idx_;
    std::vector<std::string> idx_to_state_;
    std::vector<std::unordered_map<Size, UInt64>> transitions_; // [from_idx][to_idx] -> count
    UInt64 total_transitions_{0};

    Size get_or_create_state_idx(std::string_view state);
};

/**
 * @brief Modelo léxico de Markov sobre caracteres para clasificar cadenas sin LLMs
 * (detecta hashes, UUIDs, correos y lenguaje natural humano por patrones fonotácticos).
 */
class CharacterMarkovModel {
public:
    static constexpr Size ALPHABET_SIZE = 128; // Caracteres ASCII estándar

    CharacterMarkovModel() noexcept;

    // Procesa un texto o identificador
    void update(std::string_view text) noexcept;

    // Entropía media de transición de caracteres en bits
    [[nodiscard]] Float64 transition_entropy() const noexcept;

    // Clasificación heurística formal
    [[nodiscard]] bool is_high_entropy_hash_or_uuid() const noexcept;
    [[nodiscard]] bool is_natural_language() const noexcept;

    void reset() noexcept;

private:
    std::array<std::array<UInt64, ALPHABET_SIZE>, ALPHABET_SIZE> matrix_{};
    std::array<UInt64, ALPHABET_SIZE> row_totals_{};
    UInt64 total_transitions_{0};
};

} // namespace intelidata::core
