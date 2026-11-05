#include "intelidata/core/Entropy.hpp"
#include <cmath>
#include <algorithm>

namespace intelidata::core {

bool EntropyResult::is_candidate_key(Float64 tolerance) const noexcept {
    if (total_count == 0) return false;
    return (distinct_count == total_count || distinct_ratio >= (1.0 - tolerance)) &&
           (normalized_entropy >= (1.0 - tolerance));
}

bool EntropyResult::is_low_cardinality(Float64 threshold) const noexcept {
    if (total_count == 0) return false;
    return (distinct_ratio <= threshold) || (distinct_count <= 10);
}

Float64 calculate_raw_shannon_entropy(std::span<const UInt64> frequencies, UInt64 total_count) noexcept {
    if (total_count == 0 || frequencies.empty()) {
        return 0.0;
    }

    const auto n = static_cast<Float64>(total_count);
    Float64 sum_f_log_f = 0.0;

    for (const UInt64 f : frequencies) {
        if (f > 0) {
            const auto f_d = static_cast<Float64>(f);
            sum_f_log_f += f_d * std::log2(f_d);
        }
    }

    const Float64 entropy = std::log2(n) - (sum_f_log_f / n);
    return std::max(0.0, entropy);
}

Float64 calculate_normalized_entropy(std::span<const UInt64> frequencies, UInt64 total_count, UInt64 distinct_count) noexcept {
    if (total_count == 0 || distinct_count <= 1) {
        return 0.0;
    }

    const Float64 raw_entropy = calculate_raw_shannon_entropy(frequencies, total_count);
    const Float64 max_entropy = std::log2(static_cast<Float64>(distinct_count));

    if (max_entropy <= 0.0) {
        return 0.0;
    }

    const Float64 normalized = raw_entropy / max_entropy;
    return std::clamp(normalized, 0.0, 1.0);
}

EntropyResult evaluate_entropy_from_frequencies(std::span<const UInt64> frequencies, UInt64 total_count) noexcept {
    EntropyResult res;
    res.total_count = total_count;
    res.distinct_count = frequencies.size();

    if (total_count == 0 || frequencies.empty()) {
        return res;
    }

    res.distinct_ratio = static_cast<Float64>(res.distinct_count) / static_cast<Float64>(total_count);
    res.raw_entropy_bits = calculate_raw_shannon_entropy(frequencies, total_count);
    res.normalized_entropy = calculate_normalized_entropy(frequencies, total_count, res.distinct_count);

    return res;
}

void CategoricalEntropyTracker::update(std::string_view value) {
    frequencies_[std::string(value)]++;
    total_count_++;
}

void CategoricalEntropyTracker::combine(const CategoricalEntropyTracker& other) {
    for (const auto& [val, count] : other.frequencies_) {
        frequencies_[val] += count;
    }
    total_count_ += other.total_count_;
}

EntropyResult CategoricalEntropyTracker::compute() const noexcept {
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

void CategoricalEntropyTracker::reset() noexcept {
    frequencies_.clear();
    total_count_ = 0;
}

} // namespace intelidata::core
