#include "intelidata/core/Markov.hpp"
#include <cmath>
#include <algorithm>

namespace intelidata::core {

Size MarkovChainModel::get_or_create_state_idx(std::string_view state) {
    const std::string s(state);
    const auto it = state_to_idx_.find(s);
    if (it != state_to_idx_.end()) {
        return it->second;
    }

    const Size new_idx = idx_to_state_.size();
    state_to_idx_[s] = new_idx;
    idx_to_state_.push_back(s);
    transitions_.emplace_back();
    return new_idx;
}

void MarkovChainModel::add_transition(std::string_view from_state, std::string_view to_state) {
    const Size from_idx = get_or_create_state_idx(from_state);
    const Size to_idx = get_or_create_state_idx(to_state);

    transitions_[from_idx][to_idx]++;
    total_transitions_++;
}

void MarkovChainModel::add_sequence(std::span<const std::string> sequence) {
    if (sequence.size() < 2) return;
    for (Size i = 0; i < sequence.size() - 1; ++i) {
        add_transition(sequence[i], sequence[i + 1]);
    }
}

void MarkovChainModel::combine(const MarkovChainModel& other) {
    for (Size i = 0; i < other.idx_to_state_.size(); ++i) {
        const std::string& from_state = other.idx_to_state_[i];
        for (const auto& [to_idx, count] : other.transitions_[i]) {
            const std::string& to_state = other.idx_to_state_[to_idx];
            const Size my_from = get_or_create_state_idx(from_state);
            const Size my_to = get_or_create_state_idx(to_state);
            transitions_[my_from][my_to] += count;
            total_transitions_ += count;
        }
    }
}

MarkovStateResult MarkovChainModel::analyze(Float64 absorbing_threshold) const {
    MarkovStateResult res;
    res.states = idx_to_state_;
    const Size n = idx_to_state_.size();

    if (n == 0) {
        return res;
    }

    res.transition_matrix = Eigen::MatrixXd::Zero(static_cast<Eigen::Index>(n), static_cast<Eigen::Index>(n));
    Float64 weighted_entropy_sum = 0.0;

    for (Size i = 0; i < n; ++i) {
        UInt64 row_sum = 0;
        for (const auto& [_, count] : transitions_[i]) {
            row_sum += count;
        }

        const auto row_idx = static_cast<Eigen::Index>(i);

        if (row_sum > 0) {
            Float64 row_entropy = 0.0;
            const auto row_f = static_cast<Float64>(row_sum);

            for (const auto& [to_idx, count] : transitions_[i]) {
                const Float64 p_ij = static_cast<Float64>(count) / row_f;
                res.transition_matrix(row_idx, static_cast<Eigen::Index>(to_idx)) = p_ij;
                if (p_ij > 0.0) {
                    row_entropy -= p_ij * std::log2(p_ij);
                }
            }

            // Verificar si el estado i es absorbente (P(i -> i) >= threshold)
            const Float64 self_prob = res.transition_matrix(row_idx, row_idx);
            if (self_prob >= absorbing_threshold) {
                res.absorbing_states.push_back(idx_to_state_[i]);
            }

            if (total_transitions_ > 0) {
                weighted_entropy_sum += (row_f / static_cast<Float64>(total_transitions_)) * row_entropy;
            }
        } else {
            // Estado terminal sin transiciones salientes: actúa como absorbente
            res.transition_matrix(row_idx, row_idx) = 1.0;
            res.absorbing_states.push_back(idx_to_state_[i]);
        }
    }

    res.average_entropy = weighted_entropy_sum;
    res.is_discrete_state_machine = (n >= 2 && n <= 50) && (!res.absorbing_states.empty() || res.average_entropy < 2.0);

    return res;
}

void MarkovChainModel::reset() noexcept {
    state_to_idx_.clear();
    idx_to_state_.clear();
    transitions_.clear();
    total_transitions_ = 0;
}

CharacterMarkovModel::CharacterMarkovModel() noexcept {
    reset();
}

void CharacterMarkovModel::reset() noexcept {
    for (auto& row : matrix_) {
        row.fill(0);
    }
    row_totals_.fill(0);
    total_transitions_ = 0;
}

void CharacterMarkovModel::update(std::string_view text) noexcept {
    if (text.size() < 2) return;

    for (Size i = 0; i < text.size() - 1; ++i) {
        const auto c1 = static_cast<UInt8>(text[i]);
        const auto c2 = static_cast<UInt8>(text[i + 1]);

        if (c1 < ALPHABET_SIZE && c2 < ALPHABET_SIZE) {
            matrix_[c1][c2]++;
            row_totals_[c1]++;
            total_transitions_++;
        }
    }
}

Float64 CharacterMarkovModel::transition_entropy() const noexcept {
    if (total_transitions_ == 0) return 0.0;

    Float64 avg_entropy = 0.0;
    const auto total_f = static_cast<Float64>(total_transitions_);

    for (Size i = 0; i < ALPHABET_SIZE; ++i) {
        const UInt64 row_count = row_totals_[i];
        if (row_count == 0) continue;

        const Float64 row_f = static_cast<Float64>(row_count);
        Float64 row_entropy = 0.0;

        for (Size j = 0; j < ALPHABET_SIZE; ++j) {
            const UInt64 count = matrix_[i][j];
            if (count > 0) {
                const Float64 p = static_cast<Float64>(count) / row_f;
                row_entropy -= p * std::log2(p);
            }
        }

        avg_entropy += (row_f / total_f) * row_entropy;
    }

    return avg_entropy;
}

bool CharacterMarkovModel::is_high_entropy_hash_or_uuid() const noexcept {
    if (total_transitions_ < 200) return false;
    // Hashes (MD5, SHA, hex UUIDs) presentan entropía de transición alta (>= 3.2 bits)
    return transition_entropy() >= 3.2;
}

bool CharacterMarkovModel::is_natural_language() const noexcept {
    if (total_transitions_ < 200) return false;
    // El lenguaje natural humano presenta fuertes restricciones fonotácticas (entropía < 2.8 bits)
    return transition_entropy() < 2.8;
}

} // namespace intelidata::core
