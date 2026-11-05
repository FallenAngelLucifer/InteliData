#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
#include <span>
#include <concepts>
#include <vector>

namespace intelidata::core {

struct EntropyResult {
    Float64 raw_entropy_bits{0.0};       // H(X) en bits
    Float64 normalized_entropy{0.0};    // \hat{H}(X) normalizado en [0, 1]
    UInt64 total_count{0};               // N (número total de observaciones no nulas)
    UInt64 distinct_count{0};            // |D| (número de valores distintos)
    Float64 distinct_ratio{0.0};         // |D| / N

    [[nodiscard]] bool is_candidate_key(Float64 tolerance = 1e-4) const noexcept;
    [[nodiscard]] bool is_low_cardinality(Float64 threshold = 0.05) const noexcept;
};

/**
 * @brief Funciones de cálculo puro de entropía a partir de conteos de frecuencias.
 */
[[nodiscard]] Float64 calculate_raw_shannon_entropy(std::span<const UInt64> frequencies, UInt64 total_count) noexcept;
[[nodiscard]] Float64 calculate_normalized_entropy(std::span<const UInt64> frequencies, UInt64 total_count, UInt64 distinct_count) noexcept;
[[nodiscard]] EntropyResult evaluate_entropy_from_frequencies(std::span<const UInt64> frequencies, UInt64 total_count) noexcept;

/**
 * @brief Acumulador de frecuencias y entropía para cadenas y datos categóricos.
 */
class CategoricalEntropyTracker {
public:
    CategoricalEntropyTracker() = default;

    void update(std::string_view value);

    template <typename Range>
    void update_batch(const Range& values) {
        for (const auto& v : values) {
            update(std::string_view(v));
        }
    }

    void combine(const CategoricalEntropyTracker& other);

    [[nodiscard]] EntropyResult compute() const noexcept;
    [[nodiscard]] UInt64 total_count() const noexcept { return total_count_; }
    [[nodiscard]] UInt64 distinct_count() const noexcept { return frequencies_.size(); }
    [[nodiscard]] const std::unordered_map<std::string, UInt64>& frequencies() const noexcept { return frequencies_; }

    void reset() noexcept;

private:
    std::unordered_map<std::string, UInt64> frequencies_;
    UInt64 total_count_{0};
};

/**
 * @brief Acumulador genérico de frecuencias para tipos con hash (enteros, flotantes binned, hashes).
 */
template <typename T>
class NumericEntropyTracker {
public:
    NumericEntropyTracker() = default;

    void update(T value) {
        frequencies_[value]++;
        total_count_++;
    }

    template <Numeric U>
    void update_batch(std::span<const U> values) {
        for (const auto& v : values) {
            update(static_cast<T>(v));
        }
    }

    void combine(const NumericEntropyTracker<T>& other) {
        for (const auto& [val, count] : other.frequencies_) {
            frequencies_[val] += count;
        }
        total_count_ += other.total_count_;
    }

    [[nodiscard]] EntropyResult compute() const noexcept {
        if (total_count_ == 0 || frequencies_.empty()) {
            return EntropyResult{};
        }
        std::vector<UInt64> freq_vec;
        freq_vec.reserve(frequencies_.size());
        for (const auto& [_, count] : frequencies_) {
            freq_vec.push_back(count);
        }
        return evaluate_entropy_from_frequencies(freq_vec, total_count_);
    }

    [[nodiscard]] UInt64 total_count() const noexcept { return total_count_; }
    [[nodiscard]] UInt64 distinct_count() const noexcept { return frequencies_.size(); }

    void reset() noexcept {
        frequencies_.clear();
        total_count_ = 0;
    }

private:
    std::unordered_map<T, UInt64> frequencies_;
    UInt64 total_count_{0};
};

} // namespace intelidata::core
