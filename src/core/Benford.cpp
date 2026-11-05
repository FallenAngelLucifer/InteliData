#include "intelidata/core/Benford.hpp"
#include <cmath>
#include <cctype>
#include <algorithm>

namespace intelidata::core {

std::optional<UInt32> extract_leading_digit(Float64 value) noexcept {
    if (!std::isfinite(value) || value == 0.0) {
        return std::nullopt;
    }

    Float64 a = std::abs(value);
    const Float64 exp_val = std::floor(std::log10(a));
    a /= std::pow(10.0, exp_val);

    while (a >= 10.0) {
        a /= 10.0;
    }
    while (a < 1.0) {
        a *= 10.0;
    }

    auto digit = static_cast<int>(a);
    digit = std::clamp(digit, 1, 9);
    return static_cast<UInt32>(digit);
}

std::optional<UInt32> extract_leading_digit_str(std::string_view str) noexcept {
    Size i = 0;
    const Size n = str.size();

    // Omitir espacios en blanco iniciales
    while (i < n && std::isspace(static_cast<unsigned char>(str[i]))) {
        ++i;
    }

    // Omitir signo opcional (+ o -)
    if (i < n && (str[i] == '+' || str[i] == '-')) {
        ++i;
    }

    // Omitir ceros y punto decimal previo al primer dígito significativo
    bool seen_dot = false;
    while (i < n) {
        const char c = str[i];
        if (c >= '1' && c <= '9') {
            return static_cast<UInt32>(c - '0');
        }
        if (c == '0') {
            ++i;
            continue;
        }
        if (c == '.' && !seen_dot) {
            seen_dot = true;
            ++i;
            continue;
        }
        break; // Carácter no numérico
    }

    return std::nullopt;
}

Float64 benford_theoretical_probability(UInt32 digit) noexcept {
    if (digit < 1 || digit > 9) {
        return 0.0;
    }
    return std::log10(1.0 + 1.0 / static_cast<Float64>(digit));
}

Float64 chi_square_8df_p_value(Float64 chi2) noexcept {
    if (chi2 <= 0.0) {
        return 1.0;
    }
    if (std::isnan(chi2)) {
        return 0.0;
    }

    const Float64 s = chi2 * 0.5;
    if (s > 100.0) {
        return 0.0; // Prácticamente cero, evitar underflow
    }

    // Para df = 8, k/2 = 4:
    // Q(4, s) = e^(-s) * (1 + s + s^2/2 + s^3/6)
    const Float64 s2 = s * s;
    const Float64 s3 = s2 * s;
    const Float64 poly = 1.0 + s + (s2 * 0.5) + (s3 * (1.0 / 6.0));
    const Float64 p = std::exp(-s) * poly;

    return std::clamp(p, 0.0, 1.0);
}

BenfordAccumulator::BenfordAccumulator() noexcept {
    reset();
}

void BenfordAccumulator::reset() noexcept {
    counts_.fill(0);
    total_valid_samples_ = 0;
}

void BenfordAccumulator::update(Float64 value) noexcept {
    const auto digit_opt = extract_leading_digit(value);
    if (digit_opt) {
        counts_[*digit_opt]++;
        total_valid_samples_++;
    }
}

void BenfordAccumulator::update(std::string_view str) noexcept {
    const auto digit_opt = extract_leading_digit_str(str);
    if (digit_opt) {
        counts_[*digit_opt]++;
        total_valid_samples_++;
    }
}

void BenfordAccumulator::combine(const BenfordAccumulator& other) noexcept {
    for (Size d = 1; d <= 9; ++d) {
        counts_[d] += other.counts_[d];
    }
    total_valid_samples_ += other.total_valid_samples_;
}

BenfordAccumulator& BenfordAccumulator::operator+=(const BenfordAccumulator& other) noexcept {
    combine(other);
    return *this;
}

BenfordResult BenfordAccumulator::evaluate() const noexcept {
    BenfordResult res;
    res.observed_counts = counts_;
    res.total_valid_samples = total_valid_samples_;
    res.has_sufficient_sample = (total_valid_samples_ >= 50);

    // Calcular probabilidades teóricas
    for (UInt32 d = 1; d <= 9; ++d) {
        res.expected_proportions[d] = benford_theoretical_probability(d);
    }

    if (total_valid_samples_ == 0) {
        return res;
    }

    const auto n = static_cast<Float64>(total_valid_samples_);
    Float64 chi2 = 0.0;
    Float64 mad_sum = 0.0;

    for (UInt32 d = 1; d <= 9; ++d) {
        const Float64 o_d = static_cast<Float64>(counts_[d]);
        const Float64 prop = o_d / n;
        res.observed_proportions[d] = prop;

        const Float64 expected_d = n * res.expected_proportions[d];
        const Float64 diff = o_d - expected_d;

        if (expected_d > 0.0) {
            chi2 += (diff * diff) / expected_d;
        }

        mad_sum += std::abs(prop - res.expected_proportions[d]);
    }

    res.chi_square_stat = chi2;
    res.mean_absolute_deviation = mad_sum / 9.0;
    res.p_value = chi_square_8df_p_value(chi2);

    return res;
}

} // namespace intelidata::core
