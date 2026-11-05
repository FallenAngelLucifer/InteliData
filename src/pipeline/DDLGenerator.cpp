#include "intelidata/pipeline/DDLGenerator.hpp"
#include <sstream>
#include <iomanip>

namespace intelidata::pipeline {

std::string DDLGenerator::generate_sql_views(const model::SemanticAST& ast) {
    std::ostringstream ss;
    ss << "-- ==============================================================================\n";
    ss << "-- Script de Desofuscacion Relacional Generado Automaticamente por InteliData\n";
    ss << "-- Dominio Detectado: " << ast.system_summary.detected_domain
       << " (Confianza: " << std::fixed << std::setprecision(1) << (ast.system_summary.confidence * 100.0) << "%)\n";
    ss << "-- ==============================================================================\n\n";

    for (const auto& tbl : ast.tables) {
        const std::string view_name = "v_" + tbl.recommended_name;
        ss << "-- Tabla Original: " << tbl.original_identifier << " -> Rol Inferido: " << tbl.inferred_role << "\n";
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
    ss << "> Documento generado automaticamente por el motor analitico **InteliData**.\n\n";

    ss << "## 1. Resumen Ejecutivo del Esquema\n\n";
    ss << "| Metrica | Valor |\n";
    ss << "| :--- | :--- |\n";
    ss << "| **Dominio Inferido** | " << ast.system_summary.detected_domain << " |\n";
    ss << "| **Nivel de Confianza** | " << std::fixed << std::setprecision(2) << (ast.system_summary.confidence * 100.0) << "% |\n";
    ss << "| **Conectividad de Fiedler (\\(\\lambda_1\\))** | " << std::fixed << std::setprecision(4) << ast.system_summary.fiedler_connectivity << " |\n";
    ss << "| **Total de Tablas Analizadas** | " << ast.system_summary.total_tables << " |\n";
    ss << "| **Relaciones PK/FK Descubiertas** | " << ast.system_summary.total_relationships_inferred << " |\n\n";

    if (!ast.relationships.empty()) {
        ss << "## 2. Dependencias de Inclusion (Relaciones Foraneas Descubiertas)\n\n";
        ss << "| Tabla Origen | Columna FK | Tabla Destino | Columna PK | Inclusividad | Confianza |\n";
        ss << "| :--- | :--- | :--- | :--- | :--- | :--- |\n";
        for (const auto& rel : ast.relationships) {
            ss << "| `" << rel.source_table << "` | `" << rel.source_column << "` | `"
               << rel.target_table << "` | `" << rel.target_column << "` | "
               << std::fixed << std::setprecision(1) << (rel.inclusion_ratio * 100.0) << "% | "
               << std::fixed << std::setprecision(1) << (rel.confidence * 100.0) << "% |\n";
        }
        ss << "\n";
    }

    ss << "## 3. Especificacion Detallada por Entidad\n\n";
    for (const auto& tbl : ast.tables) {
        ss << "### Tabla: `" << tbl.original_identifier << "` -> `" << tbl.recommended_name << "`\n";
        ss << "* **Rol Inferido:** `" << tbl.inferred_role << "`\n";
        ss << "* **Gravedad Relacional (PageRank):** " << std::fixed << std::setprecision(4) << tbl.pagerank_gravity << "\n";
        ss << "* **Subdominio Espectral (Cluster):** " << tbl.spectral_cluster << "\n\n";

        ss << "| Identificador Original | Nombre Recomendado | Tipo Inferido | Entropia \\(\\hat{H}\\) | Unica? | Benford \\(p\\)-value |\n";
        ss << "| :--- | :--- | :--- | :--- | :--- | :--- |\n";

        for (const auto& col : tbl.columns) {
            ss << "| `" << col.original_identifier << "` | **`" << col.recommended_name << "`** | `"
               << col.inferred_type << "` | "
               << std::fixed << std::setprecision(3) << col.entropy << " | "
               << (col.is_unique ? "Si" : "No") << " | "
               << (col.benford_p_value > 0.0 ? std::to_string(col.benford_p_value).substr(0, 5) : "N/A")
               << " |\n";
        }

        ss << "\n";
    }

    return ss.str();
}

} // namespace intelidata::pipeline
