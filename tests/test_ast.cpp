#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "intelidata/model/SemanticAST.hpp"

using namespace intelidata;
using namespace intelidata::model;
using Catch::Matchers::WithinRel;
using Catch::Matchers::WithinAbs;

TEST_CASE("SemanticAST: Serializacion y deserializacion JSON (Round-Trip)", "[ast]") {
    SemanticAST ast;
    ast.system_summary.detected_domain = "E-Commerce / Logistica de Pedidos";
    ast.system_summary.confidence = 0.948;
    ast.system_summary.fiedler_connectivity = 0.28;
    ast.system_summary.total_tables = 1;
    ast.system_summary.total_relationships_inferred = 1;

    TableAST tbl;
    tbl.original_identifier = "t_04";
    tbl.inferred_role = "Fact_Transaction";
    tbl.recommended_name = "ordenes_compra";
    tbl.pagerank_gravity = 0.385;
    tbl.spectral_cluster = 1;

    ColumnAST c1;
    c1.original_identifier = "c_1";
    c1.inferred_type = "Surrogate_Primary_Key";
    c1.recommended_name = "id_orden";
    c1.entropy = 1.0;
    c1.is_unique = true;

    ColumnAST c2;
    c2.original_identifier = "c_9";
    c2.inferred_type = "Currency_Monetary_Amount";
    c2.recommended_name = "total_orden";
    c2.benford_p_value = 0.988;
    c2.distribution = "log-normal";

    tbl.columns.push_back(c1);
    tbl.columns.push_back(c2);
    ast.tables.push_back(tbl);

    RelationshipAST rel;
    rel.source_table = "t_04";
    rel.source_column = "c_1";
    rel.target_table = "t_01";
    rel.target_column = "id";
    rel.confidence = 0.98;
    rel.inclusion_ratio = 1.0;
    ast.relationships.push_back(rel);

    // Serializar a JSON
    const nlohmann::json json_out = ast.to_json();
    REQUIRE(json_out["system_summary"]["detected_domain"] == "E-Commerce / Logistica de Pedidos");
    REQUIRE(json_out["tables"].size() == 1);
    REQUIRE(json_out["tables"][0]["columns"].size() == 2);
    REQUIRE(json_out["relationships"].size() == 1);

    // Deserializar desde JSON
    const SemanticAST ast_recovered = SemanticAST::from_json(json_out);
    REQUIRE(ast_recovered.system_summary.detected_domain == ast.system_summary.detected_domain);
    REQUIRE_THAT(ast_recovered.system_summary.confidence, WithinAbs(ast.system_summary.confidence, 1e-6));
    REQUIRE(ast_recovered.tables.size() == 1);
    REQUIRE(ast_recovered.tables[0].columns.size() == 2);
    REQUIRE(ast_recovered.tables[0].columns[0].original_identifier == "c_1");
    REQUIRE(ast_recovered.tables[0].columns[0].recommended_name == "id_orden");
    REQUIRE(ast_recovered.tables[0].columns[1].inferred_type == "Currency_Monetary_Amount");
}
