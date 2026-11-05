#include "intelidata/core/PIND.hpp"
#include "intelidata/core/Entropy.hpp"
#include <cmath>
#include <algorithm>

namespace intelidata::core {

InclusionResult evaluate_inclusion(
    const std::unordered_set<std::string>& source_distinct,
    const std::unordered_set<std::string>& target_distinct,
    bool target_is_unique,
    Float64 epsilon) noexcept {
    InclusionResult res;
    res.source_distinct_count = source_distinct.size();
    res.target_distinct_count = target_distinct.size();

    if (source_distinct.empty()) {
        return res;
    }

    UInt64 matched = 0;
    for (const auto& val : source_distinct) {
        if (target_distinct.contains(val)) {
            matched++;
        }
    }

    res.matched_count = matched;
    res.inclusion_ratio = static_cast<Float64>(matched) / static_cast<Float64>(res.source_distinct_count);
    res.is_likely_foreign_key = (res.inclusion_ratio >= (1.0 - epsilon)) && target_is_unique;
    res.confidence = res.inclusion_ratio * (target_is_unique ? 1.0 : 0.6);

    return res;
}

namespace {

struct PairHash {
    template <typename T1, typename T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const noexcept {
        const std::size_t h1 = std::hash<T1>{}(p.first);
        const std::size_t h2 = std::hash<T2>{}(p.second);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

} // namespace

Float64 compute_nmi_views(
    std::span<const std::string_view> x_values,
    std::span<const std::string_view> y_values) noexcept {
    const Size n_total = std::min(x_values.size(), y_values.size());
    if (n_total == 0) {
        return 0.0;
    }

    std::unordered_map<std::string_view, UInt64> freq_x;
    std::unordered_map<std::string_view, UInt64> freq_y;
    std::unordered_map<std::pair<std::string_view, std::string_view>, UInt64, PairHash> freq_xy;

    for (Size i = 0; i < n_total; ++i) {
        freq_x[x_values[i]]++;
        freq_y[y_values[i]]++;
        freq_xy[{x_values[i], y_values[i]}]++;
    }

    const auto n = static_cast<Float64>(n_total);

    auto compute_entropy_from_map = [n](const auto& map) {
        Float64 sum_f_log_f = 0.0;
        for (const auto& [_, count] : map) {
            const auto c = static_cast<Float64>(count);
            sum_f_log_f += c * std::log2(c);
        }
        return std::max(0.0, std::log2(n) - (sum_f_log_f / n));
    };

    const Float64 h_x = compute_entropy_from_map(freq_x);
    const Float64 h_y = compute_entropy_from_map(freq_y);
    const Float64 h_xy = compute_entropy_from_map(freq_xy);

    const Float64 mi = std::max(0.0, h_x + h_y - h_xy);
    const Float64 denom = h_x + h_y;

    if (denom <= 1e-12) {
        return 0.0;
    }

    const Float64 nmi = (2.0 * mi) / denom;
    return std::clamp(nmi, 0.0, 1.0);
}

Float64 compute_nmi(
    std::span<const std::string> x_values,
    std::span<const std::string> y_values) noexcept {
    const Size n = std::min(x_values.size(), y_values.size());
    if (n == 0) return 0.0;

    std::vector<std::string_view> vx(n);
    std::vector<std::string_view> vy(n);
    for (Size i = 0; i < n; ++i) {
        vx[i] = x_values[i];
        vy[i] = y_values[i];
    }
    return compute_nmi_views(vx, vy);
}

} // namespace intelidata::core
