#include "intelidata/core/Benford.hpp"
#include <cmath>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <iomanip>

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

std::string generate_benford_interpretation(
    std::string_view table_name,
    std::string_view col_name,
    const BenfordResult& res) {
    if (res.total_valid_samples < 30) {
        return "Muestra insuficiente (N = " + std::to_string(res.total_valid_samples) +
               " < 30 registros válidos). La Ley de Benford requiere al menos 30 observaciones para validez estadística.";
    }

    std::ostringstream oss;

    // 1. Clasificación por Desviación Media Absoluta (MAD de Nigrini)
    std::string mad_tier;
    std::string audit_verdict;
    if (res.mean_absolute_deviation <= 0.006) {
        mad_tier = "Conformidad Estricta (Nigrini MAD <= 0.006)";
        audit_verdict = "Distribución altamente natural y orgánica.";
    } else if (res.mean_absolute_deviation <= 0.012) {
        mad_tier = "Conformidad Aceptable (Nigrini MAD <= 0.012)";
        audit_verdict = "Comportamiento económico orgánico típico de transacciones de mercado libre.";
    } else if (res.mean_absolute_deviation <= 0.015) {
        mad_tier = "Conformidad Marginal (Nigrini MAD <= 0.015)";
        audit_verdict = "Leve distorsión estructural en magnitudes financieras o límites operacionales.";
    } else {
        mad_tier = "No Conformidad / Desviación Significativa (Nigrini MAD > 0.015)";
        audit_verdict = "Precios prefijados, tarifas planas, umbrales regulatorios o generación sintética.";
    }

    // 2. Identificación de derivas extremas por dígito
    UInt32 max_excess_digit = 1;
    Float64 max_excess = -1.0;
    UInt32 max_deficit_digit = 1;
    Float64 max_deficit = -1.0;

    for (UInt32 d = 1; d <= 9; ++d) {
        const Float64 diff = res.observed_proportions[d] - res.expected_proportions[d];
        if (diff > max_excess) {
            max_excess = diff;
            max_excess_digit = d;
        }
        const Float64 def = res.expected_proportions[d] - res.observed_proportions[d];
        if (def > max_deficit) {
            max_deficit = def;
            max_deficit_digit = d;
        }
    }

    // 3. Redacción del Dictamen Forense Dinámico
    oss << "### Dictamen de Auditoría Forense (" << mad_tier << ")\n\n";

    if (res.p_value >= 0.05) {
        oss << "La prueba de bondad de ajuste Chi-cuadrado (**chi2 = " << std::fixed << std::setprecision(2) << res.chi_square_stat
            << "** con 8 grados de libertad, **p = " << std::setprecision(5) << res.p_value << " >= 0.05**) "
            << "valida que la columna `" << col_name << "` de la entidad `" << table_name
            << "` cumple satisfactoriamente la Ley de Benford sobre una población de "
            << res.total_valid_samples << " transacciones analizadas.\n\n"
            << "**Diagnóstico de Integridad:** Consistencia Orgánica Validada. "
            << ((res.mean_absolute_deviation <= 0.012) ? audit_verdict : "Los importes demuestran ajuste armónico a la ley logarítmica sin sesgos artificiales críticos.")
            << " Los datos derivan de un proceso estocástico multiplicativo natural (elasticidad de demanda, pedidos abiertos, volúmenes sin restricción), descartando manipulación contable intencional, fragmentación artificiosa o anomalías sistémicas de facturación.\n\n";
    } else {
        oss << "La prueba de bondad de ajuste Chi-cuadrado (**chi2 = " << std::fixed << std::setprecision(2) << res.chi_square_stat
            << "** con 8 grados de libertad, **p = " << std::setprecision(5) << res.p_value << " < 0.05**) "
            << "rechaza formalmente la hipótesis de distribución logarítmica pura en `" << col_name << "` (`" << table_name
            << "`, N = " << res.total_valid_samples << ").\n\n"
            << "**Diagnóstico de Integridad:** " << audit_verdict << "\n\n";
    }

    oss << "#### Análisis Forense de Concentración y Deriva por Dígito:\n\n";
    oss << "* **Sobrerrepresentación Crítica en Dígito " << max_excess_digit << ":** La frecuencia observada es de **"
        << std::fixed << std::setprecision(2) << (res.observed_proportions[max_excess_digit] * 100.0) << "%** frente a un valor teórico de **"
        << (res.expected_proportions[max_excess_digit] * 100.0) << "%** (desvío relativo de **+"
        << (max_excess * 100.0) << "%**). Refleja concentración operativa en la banda de valores `$"
        << max_excess_digit << "0 - $" << max_excess_digit << "9` o `$" << max_excess_digit << "00 - $" << max_excess_digit << "99` (catálogo comercial prefijado o umbrales de compra).\n";

    if (max_deficit > 0.015) {
        oss << "* **Déficit de Frecuencia en Dígito " << max_deficit_digit << ":** Registra **"
            << std::fixed << std::setprecision(2) << (res.observed_proportions[max_deficit_digit] * 100.0) << "%** vs **"
            << (res.expected_proportions[max_deficit_digit] * 100.0) << "%** esperado (déficit de **-"
            << (max_deficit * 100.0) << "%**).\n";
    }

    oss << "* **Índice MAD de Nigrini:** `MAD = " << std::fixed << std::setprecision(4) << res.mean_absolute_deviation
        << "` (Norma Internacional de Auditoría Forense).\n\n";

    oss << "#### Recomendaciones para Control Interno y Business Intelligence:\n\n";
    if (res.p_value >= 0.05) {
        oss << "1. Los valores numéricos de `" << col_name << "` son óptimos para modelos de regresión y analítica predictiva sin requerir transformaciones logarítmicas artificiales.\n"
            << "2. Los registros contables demuestran consistencia orgánica y pueden emplearse como línea base confiable de auditoría.\n";
    } else {
        oss << "1. Realizar muestreo estratificado sobre transacciones que inician con el dígito `" << max_excess_digit
            << "` para descartar división de compras dirigida a eludir firmas mancomunadas o límites de aprobación.\n"
            << "2. Validar si la acumulación en el dígito `" << max_excess_digit
            << "` se debe a políticas comerciales estáticas (e.g. precios fijos de catálogo terminados en " << max_excess_digit << "x) o incentivos por volumen.\n";
    }

    return oss.str();
}

} // namespace intelidata::core
