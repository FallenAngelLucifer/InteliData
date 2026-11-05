#include "intelidata/model/SemanticAST.hpp"

namespace intelidata::model {

nlohmann::json ColumnAST::to_json() const {
    nlohmann::json j;
    j["original_identifier"] = original_identifier;
    j["inferred_type"] = inferred_type;
    j["recommended_name"] = recommended_name;
    j["entropy"] = entropy;
    j["is_unique"] = is_unique;
    if (benford_p_value > 0.0) {
        j["benford_p_value"] = benford_p_value;
    }
    if (!distribution.empty()) {
        j["distribution"] = distribution;
    }
    if (!markov_absorbing_states.empty()) {
        j["markov_absorbing_states"] = markov_absorbing_states;
    }
    if (!business_meaning.empty()) {
        j["business_meaning"] = business_meaning;
    }
    return j;
}

ColumnAST ColumnAST::from_json(const nlohmann::json& j) {
    ColumnAST col;
    col.original_identifier = j.value("original_identifier", "");
    col.inferred_type = j.value("inferred_type", "");
    col.recommended_name = j.value("recommended_name", "");
    col.entropy = j.value("entropy", 0.0);
    col.is_unique = j.value("is_unique", false);
    col.benford_p_value = j.value("benford_p_value", 0.0);
    col.distribution = j.value("distribution", "");
    if (j.contains("markov_absorbing_states")) {
        col.markov_absorbing_states = j["markov_absorbing_states"].get<std::vector<std::string>>();
    }
    col.business_meaning = j.value("business_meaning", "");
    return col;
}

nlohmann::json TableAST::to_json() const {
    nlohmann::json j;
    j["original_identifier"] = original_identifier;
    j["inferred_role"] = inferred_role;
    j["recommended_name"] = recommended_name;
    j["pagerank_gravity"] = pagerank_gravity;
    j["spectral_cluster"] = spectral_cluster;

    if (!business_purpose.empty()) j["business_purpose"] = business_purpose;
    if (!role_justification.empty()) j["role_justification"] = role_justification;
    if (!business_questions.empty()) j["business_questions"] = business_questions;
    if (!incoming_business_sources.empty()) j["incoming_business_sources"] = incoming_business_sources;
    if (!outgoing_business_targets.empty()) j["outgoing_business_targets"] = outgoing_business_targets;
    if (!quality_insights.empty()) j["quality_insights"] = quality_insights;

    nlohmann::json cols_arr = nlohmann::json::array();
    for (const auto& col : columns) {
        cols_arr.push_back(col.to_json());
    }
    j["columns"] = cols_arr;
    return j;
}

TableAST TableAST::from_json(const nlohmann::json& j) {
    TableAST tbl;
    tbl.original_identifier = j.value("original_identifier", "");
    tbl.inferred_role = j.value("inferred_role", "");
    tbl.recommended_name = j.value("recommended_name", "");
    tbl.pagerank_gravity = j.value("pagerank_gravity", 0.0);
    tbl.spectral_cluster = j.value("spectral_cluster", 0);

    tbl.business_purpose = j.value("business_purpose", "");
    tbl.role_justification = j.value("role_justification", "");
    if (j.contains("business_questions")) {
        tbl.business_questions = j["business_questions"].get<std::vector<std::string>>();
    }
    if (j.contains("incoming_business_sources")) {
        tbl.incoming_business_sources = j["incoming_business_sources"].get<std::vector<std::string>>();
    }
    if (j.contains("outgoing_business_targets")) {
        tbl.outgoing_business_targets = j["outgoing_business_targets"].get<std::vector<std::string>>();
    }
    if (j.contains("quality_insights")) {
        tbl.quality_insights = j["quality_insights"].get<std::vector<std::string>>();
    }

    if (j.contains("columns") && j["columns"].is_array()) {
        for (const auto& c_json : j["columns"]) {
            tbl.columns.push_back(ColumnAST::from_json(c_json));
        }
    }
    return tbl;
}

nlohmann::json RelationshipAST::to_json() const {
    nlohmann::json j;
    j["source_table"] = source_table;
    j["source_column"] = source_column;
    j["target_table"] = target_table;
    j["target_column"] = target_column;
    j["confidence"] = confidence;
    j["inclusion_ratio"] = inclusion_ratio;
    return j;
}

RelationshipAST RelationshipAST::from_json(const nlohmann::json& j) {
    RelationshipAST rel;
    rel.source_table = j.value("source_table", "");
    rel.source_column = j.value("source_column", "");
    rel.target_table = j.value("target_table", "");
    rel.target_column = j.value("target_column", "");
    rel.confidence = j.value("confidence", 0.0);
    rel.inclusion_ratio = j.value("inclusion_ratio", 0.0);
    return rel;
}

nlohmann::json FunctionalSubdomainAST::to_json() const {
    nlohmann::json j;
    j["cluster_id"] = cluster_id;
    j["name"] = name;
    j["description"] = description;
    j["table_names"] = table_names;
    return j;
}

FunctionalSubdomainAST FunctionalSubdomainAST::from_json(const nlohmann::json& j) {
    FunctionalSubdomainAST sub;
    sub.cluster_id = j.value("cluster_id", 0);
    sub.name = j.value("name", "");
    sub.description = j.value("description", "");
    if (j.contains("table_names")) {
        sub.table_names = j["table_names"].get<std::vector<std::string>>();
    }
    return sub;
}

nlohmann::json SystemSummaryAST::to_json() const {
    nlohmann::json j;
    j["detected_domain"] = detected_domain;
    j["confidence"] = confidence;
    j["fiedler_connectivity"] = fiedler_connectivity;
    j["total_tables"] = total_tables;
    j["total_relationships_inferred"] = total_relationships_inferred;

    if (!architecture_pattern.empty()) j["architecture_pattern"] = architecture_pattern;
    if (!business_narrative.empty()) j["business_narrative"] = business_narrative;
    if (!executive_insights.empty()) j["executive_insights"] = executive_insights;

    nlohmann::json sub_arr = nlohmann::json::array();
    for (const auto& s : subdomains) {
        sub_arr.push_back(s.to_json());
    }
    j["subdomains"] = sub_arr;

    return j;
}

SystemSummaryAST SystemSummaryAST::from_json(const nlohmann::json& j) {
    SystemSummaryAST sum;
    sum.detected_domain = j.value("detected_domain", "");
    sum.confidence = j.value("confidence", 0.0);
    sum.fiedler_connectivity = j.value("fiedler_connectivity", 0.0);
    sum.total_tables = j.value("total_tables", static_cast<Size>(0));
    sum.total_relationships_inferred = j.value("total_relationships_inferred", static_cast<Size>(0));

    sum.architecture_pattern = j.value("architecture_pattern", "");
    sum.business_narrative = j.value("business_narrative", "");
    if (j.contains("executive_insights")) {
        sum.executive_insights = j["executive_insights"].get<std::vector<std::string>>();
    }
    if (j.contains("subdomains") && j["subdomains"].is_array()) {
        for (const auto& s_json : j["subdomains"]) {
            sum.subdomains.push_back(FunctionalSubdomainAST::from_json(s_json));
        }
    }
    return sum;
}

nlohmann::json SemanticAST::to_json() const {
    nlohmann::json j;
    j["system_summary"] = system_summary.to_json();

    nlohmann::json tables_arr = nlohmann::json::array();
    for (const auto& t : tables) {
        tables_arr.push_back(t.to_json());
    }
    j["tables"] = tables_arr;

    nlohmann::json rels_arr = nlohmann::json::array();
    for (const auto& r : relationships) {
        rels_arr.push_back(r.to_json());
    }
    j["relationships"] = rels_arr;

    return j;
}

std::string SemanticAST::to_json_string(int indent) const {
    return to_json().dump(indent);
}

SemanticAST SemanticAST::from_json(const nlohmann::json& j) {
    SemanticAST ast;
    if (j.contains("system_summary")) {
        ast.system_summary = SystemSummaryAST::from_json(j["system_summary"]);
    }
    if (j.contains("tables") && j["tables"].is_array()) {
        for (const auto& t_json : j["tables"]) {
            ast.tables.push_back(TableAST::from_json(t_json));
        }
    }
    if (j.contains("relationships") && j["relationships"].is_array()) {
        for (const auto& r_json : j["relationships"]) {
            ast.relationships.push_back(RelationshipAST::from_json(r_json));
        }
    }
    return ast;
}

} // namespace intelidata::model
