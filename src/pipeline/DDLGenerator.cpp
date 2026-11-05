#include "intelidata/pipeline/DDLGenerator.hpp"
#include <sstream>
#include <iomanip>

namespace intelidata::pipeline {

std::string DDLGenerator::generate_sql_views(const model::SemanticAST& ast) {
    std::ostringstream ss;
    ss << "-- ==============================================================================\n";
    ss << "-- Script de Desofuscacion y Vistas Semanticas Generado por InteliData\n";
    ss << "-- Dominio Detectado: " << ast.system_summary.detected_domain
       << " (Confianza: " << std::fixed << std::setprecision(1) << (ast.system_summary.confidence * 100.0) << "%)\n";
    if (!ast.system_summary.architecture_pattern.empty()) {
        ss << "-- Patron Arquitectonico: " << ast.system_summary.architecture_pattern << "\n";
    }
    ss << "-- ==============================================================================\n\n";

    for (const auto& tbl : ast.tables) {
        const std::string view_name = "v_" + tbl.recommended_name;
        ss << "-- ------------------------------------------------------------------------------\n";
        ss << "-- Entidad: " << tbl.recommended_name << " (Original: " << tbl.original_identifier << ")\n";
        ss << "-- Rol Inferido: " << tbl.inferred_role << " | PageRank: " << std::fixed << std::setprecision(4) << tbl.pagerank_gravity << "\n";
        if (!tbl.business_purpose.empty()) {
            ss << "-- Proposito: " << tbl.business_purpose << "\n";
        }
        ss << "-- ------------------------------------------------------------------------------\n";
        ss << "CREATE OR REPLACE VIEW " << view_name << " AS\n";
        ss << "SELECT\n";

        for (Size i = 0; i < tbl.columns.size(); ++i) {
            const auto& col = tbl.columns[i];
            ss << "    \"" << col.original_identifier << "\" AS \"" << col.recommended_name << "\"";
            if (i + 1 < tbl.columns.size()) {
                ss << ",\n";
            } else {
                ss << "\n";
            }
        }

        ss << "FROM \"" << tbl.original_identifier << "\";\n\n";
    }

    return ss.str();
}

std::string DDLGenerator::generate_data_dictionary_markdown(const model::SemanticAST& ast) {
    std::ostringstream ss;
    ss << "# Diccionario de Datos y Reconstruccion Ontologica\n\n";
    ss << "> Documento de inteligencia y arquitectura ontologica generado por **InteliData**.\n\n";

    ss << "## 1. Diagnostico Ejecutivo y Narrativa del Negocio\n\n";
    if (!ast.system_summary.business_narrative.empty()) {
        ss << "**Historia y Operacion del Sistema:**\n\n"
           << "> " << ast.system_summary.business_narrative << "\n\n";
    }

    ss << "| Metrica de Arquitectura | Diagnostico Inferido |\n";
    ss << "| :--- | :--- |\n";
    ss << "| **Dominio Inferido** | " << ast.system_summary.detected_domain << " |\n";
    ss << "| **Nivel de Confianza** | " << std::fixed << std::setprecision(1) << (ast.system_summary.confidence * 100.0) << "% |\n";
    if (!ast.system_summary.architecture_pattern.empty()) {
        ss << "| **Patron de Arquitectura** | " << ast.system_summary.architecture_pattern << " |\n";
    }
    ss << "| **Conectividad de Fiedler (\\(\\lambda_1\\))** | " << std::fixed << std::setprecision(4) << ast.system_summary.fiedler_connectivity << " |\n";
    ss << "| **Total de Entidades Reconstruidas** | " << ast.system_summary.total_tables << " |\n";
    ss << "| **Dependencias Foraneas PIND** | " << ast.system_summary.total_relationships_inferred << " |\n\n";

    if (!ast.system_summary.executive_insights.empty()) {
        ss << "### Hallazgos Clave y Conclusiones de Integridad:\n\n";
        for (const auto& ins : ast.system_summary.executive_insights) {
            ss << "* " << ins << "\n";
        }
        ss << "\n";
    }

    if (!ast.system_summary.subdomains.empty()) {
        ss << "## 2. Modulos y Subdominios Funcionales de la Empresa\n\n";
        for (const auto& sub : ast.system_summary.subdomains) {
            ss << "### Subdominio: " << sub.name << "\n";
            ss << "* **Descripcion:** " << sub.description << "\n";
            ss << "* **Tablas Componentes:** ";
            for (size_t i = 0; i < sub.table_names.size(); ++i) {
                ss << "`" << sub.table_names[i] << "`" << (i + 1 < sub.table_names.size() ? ", " : "\n\n");
            }
        }
    }

    if (!ast.relationships.empty()) {
        ss << "## 3. Matriz de Dependencias Relacionales (Claves Foraneas PIND)\n\n";
        ss << "| Origen (FK) | Destino (PK) | Confianza | Inclusividad PIND |\n";
        ss << "| :--- | :--- | :--- | :--- |\n";
        for (const auto& rel : ast.relationships) {
            ss << "| `" << rel.source_table << "." << rel.source_column << "` | `"
               << rel.target_table << "." << rel.target_column << "` | "
               << std::fixed << std::setprecision(1) << (rel.confidence * 100.0) << "% | "
               << std::fixed << std::setprecision(1) << (rel.inclusion_ratio * 100.0) << "% |\n";
        }
        ss << "\n";
    }

    ss << "## 4. Fichas de Inteligencia de Negocio por Entidad\n\n";
    for (const auto& tbl : ast.tables) {
        ss << "### Entidad: `" << tbl.recommended_name << "`"
           << (tbl.original_identifier != tbl.recommended_name ? " (Fisica: `" + tbl.original_identifier + "`)" : "")
           << "\n\n";

        ss << "* **Rol Ontologico:** `" << tbl.inferred_role << "`\n";
        ss << "* **Gravedad Relacional (PageRank):** " << std::fixed << std::setprecision(4) << tbl.pagerank_gravity << "\n";
        ss << "* **Subdominio Espectral (Cluster):** C" << tbl.spectral_cluster << "\n";

        if (!tbl.business_purpose.empty()) {
            ss << "* **Proposito en el Negocio:** " << tbl.business_purpose << "\n";
        }
        if (!tbl.role_justification.empty()) {
            ss << "* **Justificacion del Rol:** " << tbl.role_justification << "\n";
        }
        if (!tbl.incoming_business_sources.empty()) {
            ss << "* **Flujo Entrante (Recibe datos de):** ";
            for (size_t i = 0; i < tbl.incoming_business_sources.size(); ++i) {
                ss << tbl.incoming_business_sources[i] << (i + 1 < tbl.incoming_business_sources.size() ? ", " : "\n");
            }
        }
        if (!tbl.outgoing_business_targets.empty()) {
            ss << "* **Flujo Saliente (Alimenta a):** ";
            for (size_t i = 0; i < tbl.outgoing_business_targets.size(); ++i) {
                ss << tbl.outgoing_business_targets[i] << (i + 1 < tbl.outgoing_business_targets.size() ? ", " : "\n");
            }
        }

        if (!tbl.business_questions.empty()) {
            ss << "\n**Preguntas Clave de Negocio que Responde esta Tabla:**\n\n";
            for (const auto& q : tbl.business_questions) {
                ss << "  1. " << q << "\n";
            }
        }

        ss << "\n| Campo Original | Nombre Canonico | Rol Semantico | Entropia | Unico? | Benford p-val |\n";
        ss << "| :--- | :--- | :--- | :--- | :--- | :--- |\n";

        for (const auto& col : tbl.columns) {
            ss << "| `" << col.original_identifier << "` | **`" << col.recommended_name << "`** | `"
               << col.inferred_type << "` | "
               << std::fixed << std::setprecision(3) << col.entropy << " | "
               << (col.is_unique ? "Si [PK]" : "No") << " | "
               << (col.benford_p_value > 0.0 ? std::to_string(col.benford_p_value).substr(0, 5) : "-")
               << " |\n";
        }

        ss << "\n---\n\n";
    }

    return ss.str();
}

} // namespace intelidata::pipeline
