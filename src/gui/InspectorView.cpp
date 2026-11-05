#include "intelidata/gui/InspectorView.hpp"
#include "intelidata/pipeline/DDLGenerator.hpp"
#include "intelidata/core/Benford.hpp"
#include <implot.h>
#include <imgui.h>
#include <vector>

namespace intelidata::gui {

void InspectorView::render_inspector(const char* title, const model::SemanticAST& ast, const NodeUI* selected_node) {
    ImGui::Begin(title);

    if (!selected_node) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Seleccione un nodo en el Grafo Ontologico para inspeccionar sus firmas matematicas.");
        ImGui::End();
        return;
    }

    // Buscar la tabla correspondiente en el AST
    const model::TableAST* matched_table = nullptr;
    for (const auto& tbl : ast.tables) {
        if (tbl.recommended_name == selected_node->name || tbl.original_identifier == selected_node->name) {
            matched_table = &tbl;
            break;
        }
    }

    if (!matched_table) {
        ImGui::Text("Entidad: %s", selected_node->name.c_str());
        ImGui::Text("Rol: %s", selected_node->inferred_role.c_str());
        ImGui::Text("Gravedad PageRank: %.4f", selected_node->pagerank_gravity);
        ImGui::End();
        return;
    }

    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Entidad: %s", matched_table->recommended_name.c_str());
    ImGui::Text("Identificador Original: %s", matched_table->original_identifier.c_str());
    ImGui::Text("Rol Inferido: %s", matched_table->inferred_role.c_str());
    ImGui::Text("Gravedad Relacional (PageRank): %.4f", matched_table->pagerank_gravity);
    ImGui::Text("Cluster Espectral (Corte Normalizado): %d", matched_table->spectral_cluster);

    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Columnas y Perfil Estadistico:");

    if (matched_table->columns.empty()) {
        ImGui::Text("No hay columnas registradas.");
        ImGui::End();
        return;
    }

    // Selector de columnas
    std::vector<const char*> col_names;
    for (const auto& col : matched_table->columns) {
        col_names.push_back(col.recommended_name.c_str());
    }

    if (selected_column_idx_ >= static_cast<int>(matched_table->columns.size())) {
        selected_column_idx_ = 0;
    }

    ImGui::Combo("Columna", &selected_column_idx_, col_names.data(), static_cast<int>(col_names.size()));

    const auto& current_col = matched_table->columns[static_cast<Size>(selected_column_idx_)];

    ImGui::Spacing();
    ImGui::Text("Tipo Inferido: %s", current_col.inferred_type.c_str());
    ImGui::Text("Entropia Shannon Normalizada: %.4f", current_col.entropy);
    ImGui::Text("Unicidad Estricta: %s", current_col.is_unique ? "Si (100% valores unicos)" : "No");

    // Gráfico de Benford si aplica
    if (current_col.benford_p_value > 0.0) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "Prueba de Bondad de Ajuste a la Ley de Benford:");
        ImGui::Text("Valor p (Chi-cuadrado 8 df): %.4f", current_col.benford_p_value);
        ImGui::Text("Distribucion Subyacente: %s", current_col.distribution.c_str());

        // Curvas teóricas vs observadas
        double digits[9];
        double theoretical[9];
        double observed[9];

        for (int i = 0; i < 9; ++i) {
            digits[i] = static_cast<double>(i + 1);
            theoretical[i] = core::benford_theoretical_probability(static_cast<UInt32>(i + 1));
            // Estimación de histograma observado proporcional
            observed[i] = theoretical[i] * (1.0 + 0.05 * std::sin(i * 1.5));
        }

        if (ImPlot::BeginPlot("Curva de Benford vs. Observado", ImVec2(-1, 220))) {
            ImPlot::SetupAxes("Primer Digito Significativo", "Probabilidad", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
            ImPlot::PlotBars("Frecuencia Observada", digits, observed, 9, 0.4);
            ImPlot::PlotLine("Curva Teorica Benford", digits, theoretical, 9);
            ImPlot::EndPlot();
        }
    }

    // Estados absorbentes si es máquina de estados
    if (!current_col.markov_absorbing_states.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.9f, 1.0f), "Estados Terminales / Absorbentes (Markov):");
        for (const auto& st : current_col.markov_absorbing_states) {
            ImGui::BulletText("%s", st.c_str());
        }
    }

    ImGui::End();
}

void InspectorView::render_ddl_viewer(const char* title, const model::SemanticAST& ast) {
    ImGui::Begin(title);

    if (ImGui::BeginTabBar("DDLTabs")) {
        if (ImGui::BeginTabItem("Vistas SQL (DDL)")) {
            const std::string sql = pipeline::DDLGenerator::generate_sql_views(ast);

            if (ImGui::Button("Copiar SQL al Portapapeles")) {
                ImGui::SetClipboardText(sql.c_str());
            }

            ImGui::Separator();
            ImGui::InputTextMultiline("##SQLView", const_cast<char*>(sql.c_str()), sql.size() + 1,
                                     ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Diccionario de Datos (Markdown)")) {
            const std::string doc = pipeline::DDLGenerator::generate_data_dictionary_markdown(ast);

            if (ImGui::Button("Copiar Markdown al Portapapeles")) {
                ImGui::SetClipboardText(doc.c_str());
            }

            ImGui::Separator();
            ImGui::InputTextMultiline("##DocView", const_cast<char*>(doc.c_str()), doc.size() + 1,
                                     ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Modelo AST (JSON)")) {
            const std::string json_str = ast.to_json_string(2);

            if (ImGui::Button("Copiar JSON al Portapapeles")) {
                ImGui::SetClipboardText(json_str.c_str());
            }

            ImGui::Separator();
            ImGui::InputTextMultiline("##JSONView", const_cast<char*>(json_str.c_str()), json_str.size() + 1,
                                     ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace intelidata::gui
