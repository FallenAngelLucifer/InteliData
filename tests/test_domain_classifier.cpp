#include <catch2/catch_test_macros.hpp>
#include "intelidata/pipeline/DomainClassifier.hpp"
#include "intelidata/model/SemanticAST.hpp"

using namespace intelidata;
using namespace intelidata::pipeline;

TEST_CASE("DomainClassifier: Deteccion de Deportes de Combate, Torneos y Salas en Linea", "[domain]") {
    model::SemanticAST ast;

    // Tabla 1: Torneo
    model::TableAST tbl_torneo;
    tbl_torneo.original_identifier = "torneo";
    tbl_torneo.recommended_name = "dim_torneo";
    tbl_torneo.inferred_role = "Dimension_Master_Entity";
    tbl_torneo.pagerank_gravity = 0.25;
    tbl_torneo.spectral_cluster = 0;
    
    model::ColumnAST col_t_id;
    col_t_id.original_identifier = "id";
    col_t_id.is_unique = true;
    col_t_id.entropy = 1.0;
    tbl_torneo.columns.push_back(col_t_id);

    model::ColumnAST col_t_nom;
    col_t_nom.original_identifier = "nombre";
    tbl_torneo.columns.push_back(col_t_nom);

    model::ColumnAST col_t_tap;
    col_t_tap.original_identifier = "num_tapices";
    tbl_torneo.columns.push_back(col_t_tap);

    // Tabla 2: Peleador / Atleta
    model::TableAST tbl_peleador;
    tbl_peleador.original_identifier = "peleador";
    tbl_peleador.recommended_name = "dim_peleador";
    tbl_peleador.inferred_role = "Dimension_Master_Entity";
    tbl_peleador.pagerank_gravity = 0.35;
    tbl_peleador.spectral_cluster = 1;

    model::ColumnAST col_p_id;
    col_p_id.original_identifier = "id_peleador";
    col_p_id.is_unique = true;
    col_p_id.entropy = 1.0;
    tbl_peleador.columns.push_back(col_p_id);

    model::ColumnAST col_p_nom;
    col_p_nom.original_identifier = "nombres";
    tbl_peleador.columns.push_back(col_p_nom);

    // Tabla 3: Combates Activos (Conexión Dual FK a Peleador)
    model::TableAST tbl_combates;
    tbl_combates.original_identifier = "combates_activos";
    tbl_combates.recommended_name = "fact_combates_activos";
    tbl_combates.inferred_role = "Fact_Transaction";
    tbl_combates.pagerank_gravity = 0.15;
    tbl_combates.spectral_cluster = 0;

    model::ColumnAST col_c_torneo;
    col_c_torneo.original_identifier = "id_torneo";
    tbl_combates.columns.push_back(col_c_torneo);

    model::ColumnAST col_c_llave;
    col_c_llave.original_identifier = "llave_key";
    tbl_combates.columns.push_back(col_c_llave);

    model::ColumnAST col_c_match;
    col_c_match.original_identifier = "match_id";
    tbl_combates.columns.push_back(col_c_match);

    model::ColumnAST col_c_tapiz;
    col_c_tapiz.original_identifier = "tapiz";
    tbl_combates.columns.push_back(col_c_tapiz);

    model::ColumnAST col_c_rojo;
    col_c_rojo.original_identifier = "id_peleador_rojo";
    tbl_combates.columns.push_back(col_c_rojo);

    model::ColumnAST col_c_azul;
    col_c_azul.original_identifier = "id_peleador_azul";
    tbl_combates.columns.push_back(col_c_azul);

    model::ColumnAST col_c_act;
    col_c_act.original_identifier = "ultima_actividad";
    tbl_combates.columns.push_back(col_c_act);

    // Tabla 4: Conexiones de Torneo (Salas en Red y Latidos)
    model::TableAST tbl_conexiones;
    tbl_conexiones.original_identifier = "conexiones_torneo";
    tbl_conexiones.recommended_name = "rel_conexiones_torneo";
    tbl_conexiones.inferred_role = "Bridge_Lookup";
    tbl_conexiones.pagerank_gravity = 0.10;
    tbl_conexiones.spectral_cluster = 0;

    model::ColumnAST col_cx_disp;
    col_cx_disp.original_identifier = "nombre_dispositivo";
    tbl_conexiones.columns.push_back(col_cx_disp);

    model::ColumnAST col_cx_master;
    col_cx_master.original_identifier = "es_master";
    tbl_conexiones.columns.push_back(col_cx_master);

    model::ColumnAST col_cx_est;
    col_cx_est.original_identifier = "estado_conexion";
    col_cx_est.markov_absorbing_states = {"Aprobado", "Rechazado"};
    tbl_conexiones.columns.push_back(col_cx_est);

    model::ColumnAST col_cx_act;
    col_cx_act.original_identifier = "ultima_actividad";
    tbl_conexiones.columns.push_back(col_cx_act);

    ast.tables.push_back(tbl_torneo);
    ast.tables.push_back(tbl_peleador);
    ast.tables.push_back(tbl_combates);
    ast.tables.push_back(tbl_conexiones);

    // Relaciones (PIND)
    ast.relationships.push_back({"combates_activos", "id_peleador_rojo", "peleador", "id_peleador", 1.0, 1.0});
    ast.relationships.push_back({"combates_activos", "id_peleador_azul", "peleador", "id_peleador", 1.0, 1.0});
    ast.relationships.push_back({"combates_activos", "id_torneo", "torneo", "id", 1.0, 1.0});
    ast.relationships.push_back({"conexiones_torneo", "id_torneo", "torneo", "id", 1.0, 1.0});

    DomainClassifier::enrich_ast_with_business_intelligence(ast);

    // Verificaciones
    REQUIRE(ast.system_summary.detected_domain == "Deportes de Combate, Torneos y Salas en Línea");
    REQUIRE(ast.system_summary.confidence >= 0.85);
    REQUIRE(ast.system_summary.architecture_pattern == "Plataforma Concurrente de Torneos y Salas en Tiempo Real");
    REQUIRE(ast.system_summary.business_narrative.find("combates") != std::string::npos);
    REQUIRE(ast.system_summary.business_narrative.find("tapices") != std::string::npos);

    // Verificar ficha de entidad para peleador
    const auto it_pel = std::find_if(ast.tables.begin(), ast.tables.end(), [](const auto& t) {
        return t.original_identifier == "peleador";
    });
    REQUIRE(it_pel != ast.tables.end());
    REQUIRE(it_pel->business_purpose.find("atletas") != std::string::npos);
    REQUIRE(!it_pel->business_questions.empty());
    REQUIRE(it_pel->business_questions[0].find("victorias") != std::string::npos);

    // Verificar ficha de entidad para combates_activos
    const auto it_cb = std::find_if(ast.tables.begin(), ast.tables.end(), [](const auto& t) {
        return t.original_identifier == "combates_activos";
    });
    REQUIRE(it_cb != ast.tables.end());
    REQUIRE(it_cb->business_purpose.find("combate") != std::string::npos);
    REQUIRE(!it_cb->business_questions.empty());
}

TEST_CASE("DomainClassifier: Deteccion de E-Commerce y Ventas", "[domain]") {
    model::SemanticAST ast;

    model::TableAST tbl_orders;
    tbl_orders.original_identifier = "orders";
    tbl_orders.recommended_name = "fact_orders";
    tbl_orders.inferred_role = "Fact_Transaction";
    tbl_orders.pagerank_gravity = 0.3;

    model::ColumnAST col_monto;
    col_monto.original_identifier = "total_amount";
    col_monto.inferred_type = "Currency_Monetary_Amount";
    col_monto.benford_p_value = 0.45;
    tbl_orders.columns.push_back(col_monto);

    model::TableAST tbl_cust;
    tbl_cust.original_identifier = "customers";
    tbl_cust.recommended_name = "dim_customers";
    tbl_cust.inferred_role = "Dimension_Master_Entity";
    tbl_cust.pagerank_gravity = 0.4;

    ast.tables.push_back(tbl_orders);
    ast.tables.push_back(tbl_cust);

    ast.relationships.push_back({"orders", "customer_id", "customers", "id", 1.0, 1.0});

    DomainClassifier::enrich_ast_with_business_intelligence(ast);

    REQUIRE(ast.system_summary.detected_domain == "E-Commerce / Ventas y Facturación Comercial");
    REQUIRE(ast.system_summary.business_narrative.find("venta") != std::string::npos);
}

TEST_CASE("DomainClassifier: Inferencia Adaptativa General para Esquema Desconocido", "[domain]") {
    model::SemanticAST ast;

    model::TableAST tbl_a;
    tbl_a.original_identifier = "maestro_recursos";
    tbl_a.recommended_name = "dim_maestro_recursos";
    tbl_a.inferred_role = "Dimension_Master_Entity";
    tbl_a.pagerank_gravity = 0.6;

    model::ColumnAST col_pk;
    col_pk.original_identifier = "id_rec";
    col_pk.is_unique = true;
    col_pk.entropy = 1.0;
    tbl_a.columns.push_back(col_pk);

    model::TableAST tbl_b;
    tbl_b.original_identifier = "registro_eventos";
    tbl_b.recommended_name = "rel_registro_eventos";
    tbl_b.inferred_role = "Standard_Entity";
    tbl_b.pagerank_gravity = 0.4;

    ast.tables.push_back(tbl_a);
    ast.tables.push_back(tbl_b);

    ast.relationships.push_back({"registro_eventos", "id_rec", "maestro_recursos", "id_rec", 1.0, 1.0});

    DomainClassifier::enrich_ast_with_business_intelligence(ast);

    // Debe inferir un dominio general adaptativo
    REQUIRE(ast.system_summary.detected_domain.find("Maestro_recursos") != std::string::npos);
    // No debe asumir que es ventas
    REQUIRE(ast.system_summary.business_narrative.find("ventas") == std::string::npos);
    REQUIRE(ast.system_summary.business_narrative.find("pedido") == std::string::npos);
}
