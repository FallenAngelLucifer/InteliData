#include "intelidata/core/StringMetrics.hpp"
#include <cctype>
#include <algorithm>
#include <unordered_set>

namespace intelidata::core {

namespace {

// Algoritmo MurmurHash3 / SplitMix64 para hash de shingles de alta dispersión
INTELIDATA_FORCE_INLINE UInt64 hash_shingle(std::string_view shingle, UInt64 seed) noexcept {
    UInt64 h = seed ^ (static_cast<UInt64>(shingle.size()) * 0x517cc1b727220a95ULL);
    for (const char c : shingle) {
        h ^= static_cast<UInt8>(c);
        h *= 0x5bd1e9955bd1e995ULL;
        h ^= h >> 47;
    }
    // Finalizador de 64 bits de MurmurHash3
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

} // namespace

std::string extract_consonant_skeleton(std::string_view str) {
    std::string result;
    result.reserve(str.size());

    for (const char c : str) {
        const auto uc = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(c)));
        if (uc >= 'a' && uc <= 'z') {
            if (uc != 'a' && uc != 'e' && uc != 'i' && uc != 'o' && uc != 'u' && uc != 'y') {
                result.push_back(static_cast<char>(uc));
            }
        }
    }

    // Si el esqueleto quedó vacío (por ejemplo "id", "io", "ao"), conservar letras alfanuméricas
    if (result.empty()) {
        for (const char c : str) {
            const auto uc = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(c)));
            if ((uc >= 'a' && uc <= 'z') || (uc >= '0' && uc <= '9')) {
                result.push_back(static_cast<char>(uc));
            }
        }
    }

    return result;
}

Size longest_common_subsequence(std::string_view a, std::string_view b) noexcept {
    const Size m = a.size();
    const Size n = b.size();

    if (m == 0 || n == 0) {
        return 0;
    }

    // Uso de buffer 1D rodante O(n) para máxima eficiencia de caché L1
    std::vector<Size> dp_prev(n + 1, 0);
    std::vector<Size> dp_curr(n + 1, 0);

    for (Size i = 1; i <= m; ++i) {
        for (Size j = 1; j <= n; ++j) {
            if (a[i - 1] == b[j - 1]) {
                dp_curr[j] = dp_prev[j - 1] + 1;
            } else {
                dp_curr[j] = std::max(dp_prev[j], dp_curr[j - 1]);
            }
        }
        dp_prev = dp_curr;
    }

    return dp_curr[n];
}

Float64 abbreviation_score(std::string_view candidate_abbrev, std::string_view target_full) noexcept {
    const std::string skel_a = extract_consonant_skeleton(candidate_abbrev);
    const std::string skel_b = extract_consonant_skeleton(target_full);

    if (skel_a.empty()) {
        return 0.0;
    }

    const Size lcs = longest_common_subsequence(skel_a, skel_b);
    const Float64 score = static_cast<Float64>(lcs) / static_cast<Float64>(skel_a.size());
    return std::clamp(score, 0.0, 1.0);
}

MinHasher::MinHasher(UInt32 k_shingle_size, UInt64 seed) noexcept
    : k_(k_shingle_size > 0 ? k_shingle_size : 3) {
    // Generación determinista de semillas pseudoaleatorias para las NUM_HASHES funciones
    UInt64 s = seed;
    for (Size i = 0; i < NUM_HASHES; ++i) {
        s ^= s >> 12;
        s ^= s << 25;
        s ^= s >> 27;
        hash_seeds_[i] = s * 0x2545F4914F6CDD1DULL;
    }
}

MinHasher::Signature MinHasher::compute_signature(std::string_view text) const {
    Signature sig;
    sig.fill(std::numeric_limits<UInt64>::max());

    if (text.empty()) {
        return sig;
    }

    // Normalizar a minúsculas
    std::string normalized;
    normalized.reserve(text.size());
    for (const char c : text) {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    // Si el texto es más corto que k, tratar el texto completo como un único shingle
    if (normalized.size() <= k_) {
        for (Size h = 0; h < NUM_HASHES; ++h) {
            sig[h] = hash_shingle(normalized, hash_seeds_[h]);
        }
        return sig;
    }

    // Extraer k-shingles deslizantes y actualizar mínimos
    const Size num_shingles = normalized.size() - k_ + 1;
    for (Size i = 0; i < num_shingles; ++i) {
        std::string_view shingle(&normalized[i], k_);
        for (Size h = 0; h < NUM_HASHES; ++h) {
            const UInt64 hash_val = hash_shingle(shingle, hash_seeds_[h]);
            sig[h] = std::min(sig[h], hash_val);
        }
    }

    return sig;
}

Float64 MinHasher::estimate_jaccard(const Signature& sig_a, const Signature& sig_b) noexcept {
    Size matches = 0;
    for (Size i = 0; i < NUM_HASHES; ++i) {
        if (sig_a[i] == sig_b[i]) {
            matches++;
        }
    }
    return static_cast<Float64>(matches) / static_cast<Float64>(NUM_HASHES);
}

Float64 MinHasher::exact_jaccard(std::string_view text_a, std::string_view text_b) const {
    if (text_a.empty() && text_b.empty()) return 1.0;
    if (text_a.empty() || text_b.empty()) return 0.0;

    auto extract_shingle_set = [this](std::string_view text) {
        std::unordered_set<std::string> set;
        std::string norm;
        norm.reserve(text.size());
        for (const char c : text) {
            norm.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        if (norm.size() <= k_) {
            set.insert(norm);
            return set;
        }
        const Size count = norm.size() - k_ + 1;
        for (Size i = 0; i < count; ++i) {
            set.insert(norm.substr(i, k_));
        }
        return set;
    };

    const auto set_a = extract_shingle_set(text_a);
    const auto set_b = extract_shingle_set(text_b);

    if (set_a.empty() && set_b.empty()) return 1.0;
    if (set_a.empty() || set_b.empty()) return 0.0;

    Size intersection_size = 0;
    for (const auto& sh : set_a) {
        if (set_b.contains(sh)) {
            intersection_size++;
        }
    }

    const Size union_size = set_a.size() + set_b.size() - intersection_size;
    if (union_size == 0) return 0.0;
    return static_cast<Float64>(intersection_size) / static_cast<Float64>(union_size);
}

} // namespace intelidata::core
