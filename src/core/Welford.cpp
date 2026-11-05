#include "intelidata/core/Welford.hpp"
#include <cmath>
#include <algorithm>

namespace intelidata::core {

WelfordAccumulator::WelfordAccumulator() noexcept = default;

void WelfordAccumulator::reset() noexcept {
    count_ = 0;
    mean_ = 0.0;
    m2_ = 0.0;
    m3_ = 0.0;
    m4_ = 0.0;
    min_ = std::numeric_limits<Float64>::infinity();
    max_ = -std::numeric_limits<Float64>::infinity();
}

void WelfordAccumulator::update(Float64 x) noexcept {
    const UInt64 n1 = count_;
    count_ += 1;
    const auto n = static_cast<Float64>(count_);

    min_ = std::min(min_, x);
    max_ = std::max(max_, x);

    const Float64 delta = x - mean_;
    const Float64 delta_n = delta / n;
    const Float64 delta_n2 = delta_n * delta_n;
    const Float64 term1 = delta * delta_n * static_cast<Float64>(n1);

    mean_ += delta_n;

    m4_ += term1 * delta_n2 * (n * n - 3.0 * n + 3.0) +
           6.0 * delta_n2 * m2_ -
           4.0 * delta_n * m3_;

    m3_ += term1 * delta_n * (n - 2.0) -
           3.0 * delta_n * m2_;

    m2_ += term1;
}

void WelfordAccumulator::combine(const WelfordAccumulator& other) noexcept {
    if (other.count_ == 0) {
        return;
    }
    if (count_ == 0) {
        *this = other;
        return;
    }

    const auto na = static_cast<Float64>(count_);
    const auto nb = static_cast<Float64>(other.count_);
    const Float64 n = na + nb;
    const Float64 delta = other.mean_ - mean_;
    const Float64 delta2 = delta * delta;
    const Float64 delta3 = delta2 * delta;
    const Float64 delta4 = delta2 * delta2;

    const Float64 new_mean = mean_ + delta * (nb / n);

    const Float64 new_m2 = m2_ + other.m2_ + delta2 * (na * nb / n);

    const Float64 new_m3 = m3_ + other.m3_ +
                           delta3 * (na * nb * (na - nb) / (n * n)) +
                           3.0 * delta * (na * other.m2_ - nb * m2_) / n;

    const Float64 new_m4 = m4_ + other.m4_ +
                           delta4 * (na * nb * (na * na - na * nb + nb * nb) / (n * n * n)) +
                           6.0 * delta2 * (na * na * other.m2_ + nb * nb * m2_) / (n * n) +
                           4.0 * delta * (na * other.m3_ - nb * m3_) / n;

    count_ = count_ + other.count_;
    mean_ = new_mean;
    m2_ = new_m2;
    m3_ = new_m3;
    m4_ = new_m4;
    min_ = std::min(min_, other.min_);
    max_ = std::max(max_, other.max_);
}

WelfordAccumulator& WelfordAccumulator::operator+=(const WelfordAccumulator& other) noexcept {
    combine(other);
    return *this;
}

WelfordAccumulator operator+(const WelfordAccumulator& a, const WelfordAccumulator& b) noexcept {
    WelfordAccumulator result = a;
    result.combine(b);
    return result;
}

Float64 WelfordAccumulator::variance(bool sample) const noexcept {
    if (count_ == 0) return 0.0;
    if (sample) {
        return count_ > 1 ? (m2_ / static_cast<Float64>(count_ - 1)) : 0.0;
    }
    return m2_ / static_cast<Float64>(count_);
}

Float64 WelfordAccumulator::standard_deviation(bool sample) const noexcept {
    const Float64 v = variance(sample);
    return v > 0.0 ? std::sqrt(v) : 0.0;
}

Float64 WelfordAccumulator::skewness() const noexcept {
    if (count_ < 3 || m2_ <= 0.0) return 0.0;
    const auto n = static_cast<Float64>(count_);
    const Float64 variance_pop = m2_ / n;
    return (m3_ / n) / std::pow(variance_pop, 1.5);
}

Float64 WelfordAccumulator::kurtosis() const noexcept {
    if (count_ < 4 || m2_ <= 0.0) return 0.0;
    const auto n = static_cast<Float64>(count_);
    const Float64 variance_pop = m2_ / n;
    return (m4_ / n) / (variance_pop * variance_pop);
}

Float64 WelfordAccumulator::excess_kurtosis() const noexcept {
    const Float64 k = kurtosis();
    return (count_ >= 4 && m2_ > 0.0) ? (k - 3.0) : 0.0;
}

} // namespace intelidata::core
