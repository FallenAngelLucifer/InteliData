#include <catch2/catch_test_macros.hpp>
#include "intelidata/pipeline/DomainClassifier.hpp"
#include "intelidata/model/SemanticAST.hpp"

using namespace intelidata;
using namespace intelidata::pipeline;

TEST_CASE("DomainClassifier: Deteccion de Videojuegos, Gaming y Combates en Linea", "[domain]") {
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

    model::ColumnAST col_c_rojo;
    col_c_rojo.original_identifier = "id_peleador_rojo";
    tbl_combates.columns.push_back(col_c_rojo);

    model::ColumnAST col_c_azul;
    col_c_azul.original_identifier = "id_peleador_azul";
    tbl_combates.columns.push_back(col_c_azul);

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

    model::ColumnAST col_cx_act;
    col_cx_act.original_identifier = "ultima_actividad";
    tbl_conexiones.columns.push_back(col_cx_act);

    ast.tables.push_back(tbl_torneo);
    ast.tables.push_back(tbl_peleador);
    ast.tables.push_back(tbl_combates);
    ast.tables.push_back(tbl_conexiones);

    ast.relationships.push_back({"combates_activos", "id_peleador_rojo", "peleador", "id_peleador", 1.0, 1.0});
    ast.relationships.push_back({"combates_activos", "id_peleador_azul", "peleador", "id_peleador", 1.0, 1.0});
    ast.relationships.push_back({"combates_activos", "id_torneo", "torneo", "id", 1.0, 1.0});
    ast.relationships.push_back({"conexiones_torneo", "id_torneo", "torneo", "id", 1.0, 1.0});

    DomainClassifier::enrich_ast_with_business_intelligence(ast);

    REQUIRE(ast.system_summary.detected_domain == "Videojuegos, Gaming y Plataformas de Combate/Salas en Línea");
    REQUIRE(ast.system_summary.confidence >= 0.85);
    REQUIRE(ast.system_summary.business_narrative.find("combates") != std::string::npos);

    const auto it_pel = std::find_if(ast.tables.begin(), ast.tables.end(), [](const auto& t) {
        return t.original_identifier == "peleador";
    });
    REQUIRE(it_pel != ast.tables.end());
    REQUIRE(it_pel->business_purpose.find("atletas") != std::string::npos);
}

TEST_CASE("DomainClassifier: Deteccion de Salud y Gestion Clinica Hospitalaria", "[domain]") {
    model::SemanticAST ast;

    model::TableAST tbl_pacientes;
    tbl_pacientes.original_identifier = "pacientes";
    tbl_pacientes.recommended_name = "dim_pacientes";
    tbl_pacientes.inferred_role = "Dimension_Master_Entity";
    tbl_pacientes.pagerank_gravity = 0.40;

    model::ColumnAST col_pac_id;
    col_pac_id.original_identifier = "id_paciente";
    col_pac_id.is_unique = true;
    col_pac_id.entropy = 1.0;
    tbl_pacientes.columns.push_back(col_pac_id);

    model::ColumnAST col_pac_nom;
    col_pac_nom.original_identifier = "nombre_paciente";
    tbl_pacientes.columns.push_back(col_pac_nom);

    model::TableAST tbl_medicos;
    tbl_medicos.original_identifier = "medicos";
    tbl_medicos.recommended_name = "dim_medicos";
    tbl_medicos.inferred_role = "Dimension_Master_Entity";
    tbl_medicos.pagerank_gravity = 0.30;

    model::ColumnAST col_med_id;
    col_med_id.original_identifier = "id_medico";
    col_med_id.is_unique = true;
    tbl_medicos.columns.push_back(col_med_id);

    model::ColumnAST col_esp;
    col_esp.original_identifier = "especialidad_clinica";
    tbl_medicos.columns.push_back(col_esp);

    model::TableAST tbl_citas;
    tbl_citas.original_identifier = "consultas_medicas";
    tbl_citas.recommended_name = "fact_consultas_medicas";
    tbl_citas.inferred_role = "Fact_Transaction";
    tbl_citas.pagerank_gravity = 0.20;

    model::ColumnAST col_c_pac;
    col_c_pac.original_identifier = "id_paciente";
    tbl_citas.columns.push_back(col_c_pac);

    model::ColumnAST col_c_med;
    col_c_med.original_identifier = "id_medico";
    tbl_citas.columns.push_back(col_c_med);

    model::ColumnAST col_diag;
    col_diag.original_identifier = "diagnostico_cie10";
    tbl_citas.columns.push_back(col_diag);

    model::ColumnAST col_est;
    col_est.original_identifier = "estado_atencion";
    col_est.markov_absorbing_states = {"Atendida", "Cancelada"};
    tbl_citas.columns.push_back(col_est);

    ast.tables.push_back(tbl_pacientes);
    ast.tables.push_back(tbl_medicos);
    ast.tables.push_back(tbl_citas);

    ast.relationships.push_back({"consultas_medicas", "id_paciente", "pacientes", "id_paciente", 1.0, 1.0});
    ast.relationships.push_back({"consultas_medicas", "id_medico", "medicos", "id_medico", 1.0, 1.0});

    DomainClassifier::enrich_ast_with_business_intelligence(ast);

    REQUIRE(ast.system_summary.detected_domain == "Salud, Hospitales y Gestión Clínica");
    REQUIRE(ast.system_summary.confidence >= 0.85);
    REQUIRE(ast.system_summary.business_narrative.find("clinica") != std::string::npos);

    const auto it_pac = std::find_if(ast.tables.begin(), ast.tables.end(), [](const auto& t) {
        return t.original_identifier == "pacientes";
    });
    REQUIRE(it_pac != ast.tables.end());
    REQUIRE(it_pac->business_purpose.find("pacientes") != std::string::npos);
}

TEST_CASE("DomainClassifier: Deteccion de Educacion y Universidades", "[domain]") {
    model::SemanticAST ast;

    model::TableAST tbl_est;
    tbl_est.original_identifier = "estudiantes";
    tbl_est.recommended_name = "dim_estudiantes";
    tbl_est.inferred_role = "Dimension_Master_Entity";
    tbl_est.pagerank_gravity = 0.40;

    model::ColumnAST col_e_id;
    col_e_id.original_identifier = "id_estudiante";
    col_e_id.is_unique = true;
    tbl_est.columns.push_back(col_e_id);

    model::TableAST tbl_cur;
    tbl_cur.original_identifier = "cursos_asignaturas";
    tbl_cur.recommended_name = "dim_cursos_asignaturas";
    tbl_cur.inferred_role = "Dimension_Master_Entity";
    tbl_cur.pagerank_gravity = 0.30;

    model::ColumnAST col_c_id;
    col_c_id.original_identifier = "id_curso";
    col_c_id.is_unique = true;
    tbl_cur.columns.push_back(col_c_id);

    model::TableAST tbl_mat;
    tbl_mat.original_identifier = "matriculas_calificaciones";
    tbl_mat.recommended_name = "fact_matriculas_calificaciones";
    tbl_mat.inferred_role = "Fact_Transaction";
    tbl_mat.pagerank_gravity = 0.20;

    model::ColumnAST col_m_est;
    col_m_est.original_identifier = "id_estudiante";
    tbl_mat.columns.push_back(col_m_est);

    model::ColumnAST col_m_cur;
    col_m_cur.original_identifier = "id_curso";
    tbl_mat.columns.push_back(col_m_cur);

    model::ColumnAST col_nota;
    col_nota.original_identifier = "nota_final";
    tbl_mat.columns.push_back(col_nota);

    ast.tables.push_back(tbl_est);
    ast.tables.push_back(tbl_cur);
    ast.tables.push_back(tbl_mat);

    ast.relationships.push_back({"matriculas_calificaciones", "id_estudiante", "estudiantes", "id_estudiante", 1.0, 1.0});
    ast.relationships.push_back({"matriculas_calificaciones", "id_curso", "cursos_asignaturas", "id_curso", 1.0, 1.0});

    DomainClassifier::enrich_ast_with_business_intelligence(ast);

    REQUIRE(ast.system_summary.detected_domain == "Educación, Universidades y Gestión Académica");
    REQUIRE(ast.system_summary.confidence >= 0.85);
    REQUIRE(ast.system_summary.business_narrative.find("estudiantil") != std::string::npos);
}

TEST_CASE("DomainClassifier: Deteccion de CRM, Soporte Tecnico y Ticketing", "[domain]") {
    model::SemanticAST ast;

    model::TableAST tbl_agentes;
    tbl_agentes.original_identifier = "agentes_soporte";
    tbl_agentes.recommended_name = "dim_agentes_soporte";
    tbl_agentes.inferred_role = "Dimension_Master_Entity";
    tbl_agentes.pagerank_gravity = 0.35;

    model::ColumnAST col_ag_id;
    col_ag_id.original_identifier = "id_agente";
    col_ag_id.is_unique = true;
    tbl_agentes.columns.push_back(col_ag_id);

    model::TableAST tbl_tickets;
    tbl_tickets.original_identifier = "tickets_incidencias";
    tbl_tickets.recommended_name = "fact_tickets_incidencias";
    tbl_tickets.inferred_role = "Fact_Transaction";
    tbl_tickets.pagerank_gravity = 0.40;

    model::ColumnAST col_tk_id;
    col_tk_id.original_identifier = "ticket_id";
    col_tk_id.is_unique = true;
    tbl_tickets.columns.push_back(col_tk_id);

    model::ColumnAST col_tk_ag;
    col_tk_ag.original_identifier = "id_agente";
    tbl_tickets.columns.push_back(col_tk_ag);

    model::ColumnAST col_tk_sla;
    col_tk_sla.original_identifier = "sla_horas";
    tbl_tickets.columns.push_back(col_tk_sla);

    model::ColumnAST col_tk_est;
    col_tk_est.original_identifier = "estado_incidencia";
    col_tk_est.markov_absorbing_states = {"Cerrado", "Resuelto"};
    tbl_tickets.columns.push_back(col_tk_est);

    ast.tables.push_back(tbl_agentes);
    ast.tables.push_back(tbl_tickets);

    ast.relationships.push_back({"tickets_incidencias", "id_agente", "agentes_soporte", "id_agente", 1.0, 1.0});

    DomainClassifier::enrich_ast_with_business_intelligence(ast);

    REQUIRE(ast.system_summary.detected_domain == "CRM, Atención al Cliente, Helpdesk y Ticketing");
    REQUIRE(ast.system_summary.confidence >= 0.85);
    REQUIRE(ast.system_summary.business_narrative.find("tickets") != std::string::npos);
}

TEST_CASE("DomainClassifier: Deteccion de Hoteleria y Reservas", "[domain]") {
    model::SemanticAST ast;

    model::TableAST tbl_hab;
    tbl_hab.original_identifier = "habitaciones_hotel";
    tbl_hab.recommended_name = "dim_habitaciones_hotel";
    tbl_hab.inferred_role = "Dimension_Master_Entity";
    tbl_hab.pagerank_gravity = 0.35;

    model::ColumnAST col_h_id;
    col_h_id.original_identifier = "id_habitacion";
    col_h_id.is_unique = true;
    tbl_hab.columns.push_back(col_h_id);

    model::TableAST tbl_res;
    tbl_res.original_identifier = "reservas_booking";
    tbl_res.recommended_name = "fact_reservas_booking";
    tbl_res.inferred_role = "Fact_Transaction";
    tbl_res.pagerank_gravity = 0.40;

    model::ColumnAST col_r_hab;
    col_r_hab.original_identifier = "id_habitacion";
    tbl_res.columns.push_back(col_r_hab);

    model::ColumnAST col_r_in;
    col_r_in.original_identifier = "fecha_checkin";
    tbl_res.columns.push_back(col_r_in);

    model::ColumnAST col_r_out;
    col_r_out.original_identifier = "fecha_checkout";
    tbl_res.columns.push_back(col_r_out);

    ast.tables.push_back(tbl_hab);
    ast.tables.push_back(tbl_res);

    ast.relationships.push_back({"reservas_booking", "id_habitacion", "habitaciones_hotel", "id_habitacion", 1.0, 1.0});

    DomainClassifier::enrich_ast_with_business_intelligence(ast);

    REQUIRE(ast.system_summary.detected_domain == "Hotelería, Turismo y Gestión de Reservas");
    REQUIRE(ast.system_summary.confidence >= 0.85);
    REQUIRE(ast.system_summary.business_narrative.find("hotelera") != std::string::npos);
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

    REQUIRE(ast.system_summary.detected_domain.find("Maestro_recursos") != std::string::npos);
    REQUIRE(ast.system_summary.business_narrative.find("ventas") == std::string::npos);
    REQUIRE(ast.system_summary.business_narrative.find("combates") == std::string::npos);
}
