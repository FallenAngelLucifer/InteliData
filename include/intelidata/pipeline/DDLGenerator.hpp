#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include "intelidata/model/SemanticAST.hpp"
#include <string>

namespace intelidata::pipeline {

/**
 * @brief Generador de Vistas SQL DDL y Diccionario de Datos en Markdown a partir del SemanticAST.
 */
class DDLGenerator {
public:
    // Genera script SQL de desofuscación con vistas CREATE VIEW
    [[nodiscard]] static std::string generate_sql_views(const model::SemanticAST& ast);

    // Genera documentación completa en formato Markdown con justificación matemática y diagramas visuales
    [[nodiscard]] static std::string generate_data_dictionary_markdown(const model::SemanticAST& ast);

    // Exporta el reporte completo y asegura la copia de las gráficas analíticas en images/
    static void export_report_with_assets(const model::SemanticAST& ast, const std::string& markdown_path);
};

} // namespace intelidata::pipeline
