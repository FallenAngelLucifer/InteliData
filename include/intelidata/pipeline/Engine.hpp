#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include "intelidata/ingestion/DataAdapter.hpp"
#include "intelidata/model/SemanticAST.hpp"
#include "intelidata/model/DependencyGraph.hpp"
#include <memory>
#include <functional>

namespace intelidata::pipeline {

struct EngineOptions {
    Size max_sample_size{5000};
    Float64 fk_inclusion_threshold{0.95};
    Float64 benford_alpha{0.05};
    Float64 pagerank_damping{0.85};
};

using ProgressCallback = std::function<void(std::string_view phase, Float32 progress_0_to_1)>;

/**
 * @brief Orquestador determinista central de InteliData.
 * Ejecuta el pipeline de 5 capas: Muestreo, Perfilado Atómico, Dependencias PIND,
 * Dinámica Estocástica/PageRank y Álgebra Espectral para generar el SemanticAST.
 */
class Engine {
public:
    explicit Engine(EngineOptions options = EngineOptions{});

    void set_options(EngineOptions options) noexcept { options_ = options; }
    [[nodiscard]] const EngineOptions& options() const noexcept { return options_; }

    // Ejecuta el análisis integral sobre cualquier fuente de datos
    [[nodiscard]] model::SemanticAST analyze(
        ingestion::DataAdapter& adapter,
        ProgressCallback progress_cb = nullptr);

private:
    EngineOptions options_;
};

} // namespace intelidata::pipeline
