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

    // Genera documentación completa en formato Markdown con justificación matemática
    [[nodiscard]] static std::string generate_data_dictionary_markdown(const model::SemanticAST& ast);
};

} // namespace intelidata::pipeline
