#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include <vector>
#include <span>
#include <random>
#include <cstddef>
#include <utility>

namespace intelidata::ingestion {

/**
 * @brief Implementación de Reservoir Sampling (Algoritmo R de Alan G. Waterman / Jeffrey Vitter).
 * Permite seleccionar una muestra aleatoria uniforme no sesgada de tamaño K a partir de un flujo
 * de tamaño N desconocido en una sola pasada O(N) con memoria constante O(K).
 */
template <typename T>
class ReservoirSampler {
public:
    explicit ReservoirSampler(Size capacity, UInt64 seed = 13374242ULL)
        : capacity_(capacity), rng_(seed) {
        if (capacity_ > 0) {
            reservoir_.reserve(capacity_);
        }
    }

    void update(const T& item) {
        const UInt64 index = total_seen_++;
        if (reservoir_.size() < capacity_) {
            reservoir_.push_back(item);
        } else {
            std::uniform_int_distribution<UInt64> dist(0, index);
            const UInt64 j = dist(rng_);
            if (j < capacity_) {
                reservoir_[j] = item;
            }
        }
    }

    void update(T&& item) {
        const UInt64 index = total_seen_++;
        if (reservoir_.size() < capacity_) {
            reservoir_.push_back(std::move(item));
        } else {
            std::uniform_int_distribution<UInt64> dist(0, index);
            const UInt64 j = dist(rng_);
            if (j < capacity_) {
                reservoir_[j] = std::move(item);
            }
        }
    }

    template <typename Range>
    void update_batch(Range&& items) {
        for (auto&& item : items) {
            update(std::forward<decltype(item)>(item));
        }
    }

    [[nodiscard]] std::span<const T> sample() const noexcept {
        return std::span<const T>(reservoir_.data(), reservoir_.size());
    }

    [[nodiscard]] const std::vector<T>& sample_vector() const noexcept {
        return reservoir_;
    }

    [[nodiscard]] Size size() const noexcept {
        return reservoir_.size();
    }

    [[nodiscard]] Size capacity() const noexcept {
        return capacity_;
    }

    [[nodiscard]] UInt64 total_seen() const noexcept {
        return total_seen_;
    }

    [[nodiscard]] bool is_full() const noexcept {
        return reservoir_.size() >= capacity_;
    }

    void reset(UInt64 new_seed = 13374242ULL) noexcept {
        reservoir_.clear();
        total_seen_ = 0;
        rng_.seed(new_seed);
    }

private:
    Size capacity_{0};
    UInt64 total_seen_{0};
    std::vector<T> reservoir_;
    std::mt19937_64 rng_;
};

} // namespace intelidata::ingestion
