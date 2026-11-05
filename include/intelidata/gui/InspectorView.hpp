#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include "intelidata/model/SemanticAST.hpp"
#include "intelidata/gui/GraphCanvas.hpp"
#include <string>

namespace intelidata::gui {

/**
 * @brief Inspector estadístico avanzado con gráficos ImPlot
 * (Ley de Benford, Markov, entropía, relaciones foráneas)
 * y Centro de Exportación DDL / Markdown / JSON.
 */
class InspectorView {
public:
    InspectorView() = default;

    // Renderiza el panel inspector estadístico con gráficos ImPlot y detalles multivariados
    void render_inspector(const char* title, const model::SemanticAST& ast, const NodeUI* selected_node);

    // Renderiza el visor interactivo y exportador de SQL DDL, Diccionario Markdown y AST JSON
    void render_ddl_viewer(const char* title, const model::SemanticAST& ast);

    // Retorna el índice de columna seleccionada
    [[nodiscard]] int selected_column_idx() const noexcept { return selected_column_idx_; }

private:
    int selected_column_idx_{0};
    int inspector_tab_{0};
    float toast_timer_{0.0f};
    std::string toast_message_;
};

} // namespace intelidata::gui
