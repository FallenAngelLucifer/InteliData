#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include "intelidata/model/SemanticAST.hpp"
#include "intelidata/gui/GraphCanvas.hpp"
#include <string>

namespace intelidata::gui {

/**
 * @brief Inspector de Inteligencia de Negocio y Métricas Estadísticas Avanzadas:
 * Proporciona diagnóstico semántico ejecutivo, fichas interpretativas de entidad,
 * preguntas de negocio clave, análisis ImPlot de la Ley de Benford, cadenas de Markov
 * y Centro de Exportación DDL / Markdown / JSON.
 */
class InspectorView {
public:
    InspectorView() = default;

    // Renderiza el panel de inteligencia de negocio e inspección estadística
    void render_inspector(const char* title, const model::SemanticAST& ast, const NodeUI* selected_node);

    // Renderiza el visor interactivo y exportador de SQL DDL, Diccionario Markdown y AST JSON
    void render_ddl_viewer(const char* title, const model::SemanticAST& ast);

    // Retorna el índice de columna seleccionada
    [[nodiscard]] int selected_column_idx() const noexcept { return selected_column_idx_; }

    // Consulta si el usuario solicitó seleccionar una tabla desde las tarjetas de subdominios
    [[nodiscard]] bool has_table_selection_request() const noexcept { return !requested_table_.empty(); }
    [[nodiscard]] std::string consume_table_selection() {
        std::string res = std::move(requested_table_);
        requested_table_.clear();
        return res;
    }

private:
    void render_global_business_diagnostic(const model::SemanticAST& ast);
    void render_entity_business_sheet(const model::TableAST& tbl, const model::SemanticAST& ast);

    int selected_column_idx_{0};
    int inspector_tab_{0};
    float toast_timer_{0.0f};
    std::string toast_message_;
    std::string requested_table_;
};

} // namespace intelidata::gui
