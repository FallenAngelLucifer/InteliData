#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include "intelidata/model/SemanticAST.hpp"
#include "intelidata/gui/GraphCanvas.hpp"
#include <string>

namespace intelidata::gui {

/**
 * @brief Inspector estadístico con curvas de ImPlot (Ley de Benford, Markov, entropía)
 * y Visor de Código DDL en vivo.
 */
class InspectorView {
public:
    InspectorView() = default;

    // Renderiza el panel inspector estadístico con gráficos ImPlot
    void render_inspector(const char* title, const model::SemanticAST& ast, const NodeUI* selected_node);

    // Renderiza el visor interactivo de SQL DDL y Diccionario Markdown
    void render_ddl_viewer(const char* title, const model::SemanticAST& ast);

private:
    int selected_column_idx_{0};
};

} // namespace intelidata::gui
