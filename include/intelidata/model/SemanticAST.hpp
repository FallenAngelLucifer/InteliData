#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace intelidata::model {

struct ColumnAST {
    std::string original_identifier;
    std::string inferred_type;
    std::string recommended_name;
    Float64 entropy{0.0};
    bool is_unique{false};
    Float64 benford_p_value{0.0};
    Float64 benford_chi_square{0.0};
    Float64 benford_mad{0.0};
    UInt64 benford_samples{0};
    std::vector<Float64> benford_observed_digits; // 9 valores para dígitos d=1..9
    std::string benford_interpretation;
    std::string distribution;
    std::vector<std::string> markov_absorbing_states;
    std::string business_meaning;

    [[nodiscard]] nlohmann::json to_json() const;
    static ColumnAST from_json(const nlohmann::json& j);
};

struct TableAST {
    std::string original_identifier;
    std::string inferred_role;
    std::string recommended_name;
    Float64 pagerank_gravity{0.0};
    Int32 spectral_cluster{0};
    std::vector<ColumnAST> columns;

    // Interpretación semántica y narrativa de negocio
    std::string business_purpose;
    std::string role_justification;
    std::vector<std::string> business_questions;
    std::vector<std::string> incoming_business_sources;
    std::vector<std::string> outgoing_business_targets;
    std::vector<std::string> quality_insights;

    [[nodiscard]] nlohmann::json to_json() const;
    static TableAST from_json(const nlohmann::json& j);
};

struct RelationshipAST {
    std::string source_table;
    std::string source_column;
    std::string target_table;
    std::string target_column;
    Float64 confidence{0.0};
    Float64 inclusion_ratio{0.0};

    [[nodiscard]] nlohmann::json to_json() const;
    static RelationshipAST from_json(const nlohmann::json& j);
};

struct FunctionalSubdomainAST {
    Int32 cluster_id{0};
    std::string name;
    std::string description;
    std::vector<std::string> table_names;

    [[nodiscard]] nlohmann::json to_json() const;
    static FunctionalSubdomainAST from_json(const nlohmann::json& j);
};

struct SystemSummaryAST {
    std::string detected_domain{"Desconocido"};
    Float64 confidence{0.0};
    Float64 fiedler_connectivity{0.0};
    Size total_tables{0};
    Size total_relationships_inferred{0};

    // Diagnóstico ejecutivo y arquitectura del sistema
    std::string architecture_pattern;
    std::string business_narrative;
    std::string domain_icon;
    std::vector<std::string> industry_tags;
    std::vector<FunctionalSubdomainAST> subdomains;
    std::vector<std::string> executive_insights;

    [[nodiscard]] nlohmann::json to_json() const;
    static SystemSummaryAST from_json(const nlohmann::json& j);
};

struct SemanticAST {
    SystemSummaryAST system_summary;
    std::vector<TableAST> tables;
    std::vector<RelationshipAST> relationships;

    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] std::string to_json_string(int indent = 2) const;
    static SemanticAST from_json(const nlohmann::json& j);
};

} // namespace intelidata::model
