#include "intelidata/pipeline/Engine.hpp"
#include "intelidata/pipeline/DomainClassifier.hpp"
#include "intelidata/core/Welford.hpp"
#include "intelidata/core/Entropy.hpp"
#include "intelidata/core/Benford.hpp"
#include "intelidata/core/Markov.hpp"
#include "intelidata/core/PIND.hpp"
#include "intelidata/core/Spectral.hpp"
#include "intelidata/core/StringMetrics.hpp"
#include <unordered_set>
#include <charconv>
#include <algorithm>

namespace intelidata::pipeline {

Engine::Engine(EngineOptions options) : options_(options) {}

namespace {

struct ColumnProfile {
    std::string table_name;
    std::string column_name;
    std::vector<std::string> raw_samples;
    std::unordered_set<std::string> distinct_values;
    core::EntropyResult entropy_res;
    core::BenfordResult benford_res;
    core::WelfordAccumulator welford_acc;
    bool is_numeric{false};
    bool is_state_machine{false};
    std::vector<std::string> absorbing_states;
    bool is_natural_text{false};
    bool is_uuid_hash{false};
};

INTELIDATA_FORCE_INLINE std::string_view strip_column_prefix(std::string_view s) noexcept {
    for (std::string_view pfx : {"fld_", "col_", "c_", "tbl_", "t_"}) {
        if (s.starts_with(pfx)) {
            return s.substr(pfx.size());
        }
    }
    return s;
}

} // namespace

model::SemanticAST Engine::analyze(
    ingestion::DataAdapter& adapter,
    ProgressCallback progress_cb) {

    model::SemanticAST ast;
    const auto tables = adapter.list_tables();
    if (tables.empty()) {
        return ast;
    }

    if (progress_cb) progress_cb("Ingesta y Muestreo", 0.1f);

    // 1. Recolección de muestras y perfilado atómico
    std::vector<ColumnProfile> all_columns;
    std::unordered_map<std::string, std::vector<Size>> table_to_col_indices;

    for (Size t_idx = 0; t_idx < tables.size(); ++t_idx) {
        const auto& t_name = tables[t_idx];
        const auto schema = adapter.get_table_schema(t_name);

        for (const auto& col_schema : schema.columns) {
            ColumnProfile prof;
            prof.table_name = t_name;
            prof.column_name = col_schema.name;
            prof.raw_samples = adapter.sample_column(t_name, col_schema.name, options_.max_sample_size);

            core::CategoricalEntropyTracker entropy_tracker;
            core::BenfordAccumulator benford_acc;
            core::CharacterMarkovModel char_markov;
            core::MarkovChainModel state_markov;

            UInt64 numeric_parse_count = 0;

            for (const auto& val : prof.raw_samples) {
                entropy_tracker.update(val);
                prof.distinct_values.insert(val);

                // Intento de conversión numérica
                Float64 num_val = 0.0;
                auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), num_val);
                if (ec == std::errc()) {
                    numeric_parse_count++;
                    prof.welford_acc.update(num_val);
                    benford_acc.update(num_val);
                }

                char_markov.update(val);
            }

            prof.entropy_res = entropy_tracker.compute();

            const Float64 numeric_ratio = prof.raw_samples.empty() ? 0.0 :
                (static_cast<Float64>(numeric_parse_count) / static_cast<Float64>(prof.raw_samples.size()));
            prof.is_numeric = (numeric_ratio >= 0.85);

            if (prof.is_numeric && prof.welford_acc.count() >= 30) {
                prof.benford_res = benford_acc.evaluate();
            }

            // Análisis léxico de Markov
            if (!prof.is_numeric && prof.raw_samples.size() >= 10) {
                prof.is_uuid_hash = char_markov.is_high_entropy_hash_or_uuid();
                prof.is_natural_text = char_markov.is_natural_language();
            }

            // Si es de baja cardinalidad, evaluar si modela máquina de estados finitos
            if (prof.entropy_res.is_low_cardinality() && prof.distinct_values.size() >= 2 && prof.distinct_values.size() <= 20) {
                state_markov.add_sequence(prof.raw_samples);
                const auto markov_res = state_markov.analyze();
                if (markov_res.is_discrete_state_machine) {
                    prof.is_state_machine = true;
                    prof.absorbing_states = markov_res.absorbing_states;
                }
            }

            const Size col_global_idx = all_columns.size();
            all_columns.push_back(std::move(prof));
            table_to_col_indices[t_name].push_back(col_global_idx);
        }

        if (progress_cb) {
            const auto p = 0.1f + 0.3f * (static_cast<Float32>(t_idx + 1) / static_cast<Float32>(tables.size()));
            progress_cb("Perfilado Atómico", p);
        }
    }

    if (progress_cb) progress_cb("Descubrimiento de Dependencias PIND", 0.45f);

    // 2. Dependencias de Inclusión (PIND) y Construcción de Grafo
    model::DependencyGraph graph;
    for (const auto& t_name : tables) {
        graph.add_table(t_name);
    }

    std::vector<model::RelationshipAST> relationships;

    // Identificar la clave primaria representativa de cada tabla (primer candidato a clave)
    std::unordered_map<std::string, Size> table_pk_col_idx;
    for (const auto& t_name : tables) {
        for (const auto col_idx : table_to_col_indices[t_name]) {
            if (all_columns[col_idx].entropy_res.is_candidate_key()) {
                table_pk_col_idx[t_name] = col_idx;
                break;
            }
        }
    }

    for (Size i = 0; i < all_columns.size(); ++i) {
        const auto& col_a = all_columns[i];
        if (col_a.distinct_values.size() < 2) continue;

        struct CandidateTarget {
            Size target_col_idx;
            Float64 inclusion_ratio;
            Float64 score;
            Float64 confidence;
        };
        std::vector<CandidateTarget> candidates;

        for (Size j = 0; j < all_columns.size(); ++j) {
            if (i == j) continue;
            const auto& col_b = all_columns[j];
            if (col_a.table_name == col_b.table_name) continue;

            // Para que B sea candidata a clave foránea de A, B debe ser clave candidata
            if (!col_b.entropy_res.is_candidate_key()) continue;

            const auto clean_a = strip_column_prefix(col_a.column_name);
            const auto clean_b = strip_column_prefix(col_b.column_name);
            const auto clean_tbl_b = strip_column_prefix(col_b.table_name);

            // Si tanto A como B son las claves primarias de sus respectivas tablas,
            // evitar vincular autonuméricos independientes a menos que compartan nombre/afinidad
            const bool is_table_pk_a = (table_pk_col_idx.contains(col_a.table_name) && table_pk_col_idx[col_a.table_name] == i);
            const bool is_table_pk_b = (table_pk_col_idx.contains(col_b.table_name) && table_pk_col_idx[col_b.table_name] == j);

            if (is_table_pk_a && is_table_pk_b) {
                const auto skel_a = core::extract_consonant_skeleton(clean_a);
                const auto skel_b = core::extract_consonant_skeleton(clean_b);
                const auto lcs_len = core::longest_common_subsequence(skel_a, skel_b);
                const Float64 sim = (skel_a.empty() || skel_b.empty()) ? 0.0 :
                    (static_cast<Float64>(lcs_len) / static_cast<Float64>(std::max(skel_a.size(), skel_b.size())));
                if (sim < 0.7) continue;
            }

            const auto inc_res = core::evaluate_inclusion(
                col_a.distinct_values,
                col_b.distinct_values,
                true,
                1.0 - options_.fk_inclusion_threshold
            );

            if (inc_res.is_likely_foreign_key) {
                // Afinidad léxica con columna y tabla destino
                const Float64 sim_col = core::abbreviation_score(clean_a, clean_b);
                const Float64 sim_tbl = core::abbreviation_score(clean_a, clean_tbl_b);
                const Float64 sim_lex = std::max(sim_col, sim_tbl);

                // Ratio de cobertura de cardinalidad |D(A)| / |D(B)|
                const Float64 card_ratio = static_cast<Float64>(col_a.distinct_values.size()) /
                                           static_cast<Float64>(std::max<Size>(1, col_b.distinct_values.size()));

                // Descartar falsos positivos accidentales de enteros pequeños sin afinidad léxica
                if (sim_lex < 0.2 && card_ratio < 0.05) continue;
                if (col_a.distinct_values.size() < 10 && sim_lex < 0.4) continue;

                const Float64 card_factor = std::pow(std::clamp(card_ratio, 0.01, 1.0), 0.2);
                const Float64 score = inc_res.inclusion_ratio * card_factor * (1.0 + 0.5 * sim_lex);
                const Float64 conf = std::clamp(inc_res.confidence * (0.8 + 0.2 * sim_lex), 0.0, 1.0);

                candidates.push_back({j, inc_res.inclusion_ratio, score, conf});
            }
        }

        if (!candidates.empty()) {
            const auto best_it = std::max_element(
                candidates.begin(), candidates.end(),
                [](const CandidateTarget& a, const CandidateTarget& b) {
                    return a.score < b.score;
                }
            );

            const auto& col_b = all_columns[best_it->target_col_idx];

            model::RelationshipEdge edge;
            edge.source_table = col_a.table_name;
            edge.source_column = col_a.column_name;
            edge.target_table = col_b.table_name;
            edge.target_column = col_b.column_name;
            edge.inclusion_ratio = best_it->inclusion_ratio;
            edge.confidence = best_it->confidence;
            edge.is_foreign_key = true;

            graph.add_edge(edge);

            model::RelationshipAST rel_ast;
            rel_ast.source_table = edge.source_table;
            rel_ast.source_column = edge.source_column;
            rel_ast.target_table = edge.target_table;
            rel_ast.target_column = edge.target_column;
            rel_ast.inclusion_ratio = edge.inclusion_ratio;
            rel_ast.confidence = edge.confidence;
            relationships.push_back(std::move(rel_ast));
        }
    }

    if (progress_cb) progress_cb("Álgebra Espectral y PageRank", 0.70f);

    // 3. PageRank y Análisis Espectral
    graph.compute_pagerank(options_.pagerank_damping);
    const Eigen::MatrixXd adj_matrix = graph.build_adjacency_matrix();
    const core::SpectralDecompositionResult spectral_res = core::compute_spectral_analysis(adj_matrix);

    // 4. Inferencia Semántica y Construcción del AST
    if (progress_cb) progress_cb("Síntesis Semántica", 0.90f);

    ast.relationships = std::move(relationships);
    ast.system_summary.total_tables = tables.size();
    ast.system_summary.total_relationships_inferred = ast.relationships.size();
    ast.system_summary.fiedler_connectivity = spectral_res.fiedler_value;

    for (Size t_idx = 0; t_idx < tables.size(); ++t_idx) {
        const auto& t_name = tables[t_idx];
        const auto* table_node = graph.find_table(t_name);

        model::TableAST tbl_ast;
        tbl_ast.original_identifier = t_name;
        tbl_ast.pagerank_gravity = table_node ? table_node->pagerank_gravity : (1.0 / static_cast<Float64>(tables.size()));
        tbl_ast.spectral_cluster = (t_idx < spectral_res.spectral_clusters.size()) ? spectral_res.spectral_clusters[t_idx] : 0;

        UInt32 fk_count = 0;
        UInt32 pk_count = 0;
        UInt32 financial_col_count = 0;

        for (const auto col_idx : table_to_col_indices[t_name]) {
            const auto& prof = all_columns[col_idx];
            model::ColumnAST col_ast;
            col_ast.original_identifier = prof.column_name;
            col_ast.entropy = prof.entropy_res.normalized_entropy;
            col_ast.is_unique = (prof.entropy_res.distinct_count == prof.entropy_res.total_count && prof.entropy_res.total_count > 0);

            // Verificar si esta columna es una clave foránea saliente
            bool is_fk = false;
            std::string fk_target_table;
            for (const auto& rel : ast.relationships) {
                if (rel.source_table == t_name && rel.source_column == prof.column_name) {
                    is_fk = true;
                    fk_target_table = rel.target_table;
                    break;
                }
            }

            // Inferencia de Rol de Columna
            if (is_fk) {
                col_ast.inferred_type = "Foreign_Key_Reference";
                col_ast.recommended_name = "id_" + fk_target_table;
                fk_count++;
            } else if (prof.is_numeric && prof.benford_res.is_benford_distributed(options_.benford_alpha)) {
                col_ast.inferred_type = "Currency_Monetary_Amount";
                col_ast.recommended_name = "monto_" + prof.column_name;
                col_ast.benford_p_value = prof.benford_res.p_value;
                col_ast.distribution = "log-normal";
                financial_col_count++;
            } else if (prof.is_uuid_hash) {
                col_ast.inferred_type = "Identifier_UUID_Token";
                col_ast.recommended_name = "token_" + prof.column_name;
            } else if (prof.entropy_res.is_candidate_key()) {
                if (pk_count == 0) {
                    col_ast.inferred_type = "Surrogate_Primary_Key";
                    col_ast.recommended_name = "id_" + t_name;
                } else {
                    col_ast.inferred_type = "Natural_Unique_Key";
                    col_ast.recommended_name = "codigo_" + prof.column_name;
                }
                pk_count++;
            } else if (prof.is_state_machine) {
                col_ast.inferred_type = "Discrete_State_Machine";
                col_ast.recommended_name = "estado_" + t_name;
                col_ast.markov_absorbing_states = prof.absorbing_states;
            } else if (prof.is_natural_text) {
                col_ast.inferred_type = "Natural_Text_Description";
                col_ast.recommended_name = "descripcion_" + prof.column_name;
            } else if (prof.entropy_res.is_low_cardinality()) {
                col_ast.inferred_type = "Categorical_Code";
                col_ast.recommended_name = "tipo_" + prof.column_name;
            } else {
                col_ast.inferred_type = prof.is_numeric ? "Numerical_Metric" : "Descriptive_Attribute";
                col_ast.recommended_name = prof.column_name;
            }

            tbl_ast.columns.push_back(std::move(col_ast));
        }

        // Inferencia de Rol de Tabla
        (void)pk_count;
        if (fk_count >= 2 && financial_col_count >= 1) {
            tbl_ast.inferred_role = "Fact_Transaction";
            tbl_ast.recommended_name = "fact_" + t_name;
        } else if (fk_count >= 2 && financial_col_count == 0) {
            tbl_ast.inferred_role = "Bridge_Lookup";
            tbl_ast.recommended_name = "rel_" + t_name;
        } else if (tbl_ast.pagerank_gravity >= (1.5 / static_cast<Float64>(tables.size()))) {
            tbl_ast.inferred_role = "Dimension_Master_Entity";
            tbl_ast.recommended_name = "dim_" + t_name;
        } else {
            tbl_ast.inferred_role = "Standard_Entity";
            tbl_ast.recommended_name = t_name;
        }

        ast.tables.push_back(std::move(tbl_ast));
    }

    // 5. Síntesis e Interpretación Semántica Multidominio y Diagnóstico Adaptativo
    DomainClassifier::enrich_ast_with_business_intelligence(ast);

    if (progress_cb) progress_cb("Completado", 1.0f);

    return ast;
}

} // namespace intelidata::pipeline
