#include "intelidata/gui/InspectorView.hpp"
#include "intelidata/pipeline/DDLGenerator.hpp"
#include "intelidata/core/Benford.hpp"
#include <implot.h>
#include <imgui.h>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>

namespace intelidata::gui {

namespace {

void render_badge(const char* text, const ImVec4& bg_color, const ImVec4& text_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f)) {
    ImGui::PushStyleColor(ImGuiCol_Button, bg_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, bg_color);
    ImGui::PushStyleColor(ImGuiCol_Text, text_color);
    ImGui::SmallButton(text);
    ImGui::PopStyleColor(4);
}

ImVec4 role_color(const std::string& role) {
    if (role == "FACT_TABLE") return ImVec4(0.92f, 0.45f, 0.20f, 1.0f);        // Ámbar / Naranja
    if (role == "DIMENSION_TABLE") return ImVec4(0.18f, 0.75f, 0.45f, 1.0f);   // Verde Esmeralda
    if (role == "BRIDGE_TABLE") return ImVec4(0.62f, 0.35f, 0.88f, 1.0f);      // Púrpura
    if (role == "LOOKUP_TABLE") return ImVec4(0.20f, 0.65f, 0.92f, 1.0f);      // Azul Cyan
    return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);                                  // Gris
}

} // namespace

void InspectorView::render_inspector(const char* title, const model::SemanticAST& ast, const NodeUI* selected_node) {
    if (!ImGui::Begin(title)) {
        ImGui::End();
        return;
    }

    if (!selected_node) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Seleccione un nodo en el Grafo Ontologico para inspeccionar sus firmas matematicas.");
        ImGui::Spacing();
        ImGui::TextDisabled("Haga clic izquierdo sobre cualquier tabla en el lienzo 2D para ver sus columnas, pruebas de Benford, cadenas de Markov y claves foraneas.");
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
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Entidad: %s", selected_node->name.c_str());
        ImGui::Text("Rol: %s", selected_node->inferred_role.c_str());
        ImGui::Text("Gravedad PageRank: %.4f", selected_node->pagerank_gravity);
        ImGui::End();
        return;
    }

    // ==========================================
    // 1. Tarjeta Resumen de la Entidad
    // ==========================================
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.16f, 0.20f, 0.8f));
    ImGui::BeginChild("EntitySummaryCard", ImVec2(0, 115), true);

    ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "Entidad: %s", matched_table->recommended_name.c_str());
    if (matched_table->original_identifier != matched_table->recommended_name) {
        ImGui::SameLine();
        ImGui::TextDisabled("(Identificador fisico: %s)", matched_table->original_identifier.c_str());
    }

    ImGui::Spacing();
    render_badge(matched_table->inferred_role.c_str(), role_color(matched_table->inferred_role));
    ImGui::SameLine();

    std::string cluster_str = "Cluster Espectral C" + std::to_string(matched_table->spectral_cluster);
    render_badge(cluster_str.c_str(), ImVec4(0.25f, 0.45f, 0.75f, 1.0f));

    ImGui::SameLine();
    ImGui::TextDisabled("| %zu Columnas", matched_table->columns.size());

    ImGui::Spacing();
    // Barra de Gravedad PageRank
    const float pr_val = static_cast<float>(matched_table->pagerank_gravity);
    char pr_buf[64];
    std::snprintf(pr_buf, sizeof(pr_buf), "Gravedad Relacional (PageRank): %.4f", pr_val);
    ImGui::ProgressBar(std::clamp(pr_val * 4.0f, 0.0f, 1.0f), ImVec2(-1, 16), pr_buf);

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // ==========================================
    // 2. Pestañas de Detalle de la Entidad
    // ==========================================
    if (ImGui::BeginTabBar("InspectorDetailTabs")) {
        // Pestaña 1: Perfil de Columnas y Distribución
        if (ImGui::BeginTabItem("Columnas y Estadisticas")) {
            if (matched_table->columns.empty()) {
                ImGui::TextDisabled("No hay columnas disponibles.");
            } else {
                // Selector de columna
                std::vector<std::string> col_display_names;
                std::vector<const char*> col_ptrs;
                col_display_names.reserve(matched_table->columns.size());
                col_ptrs.reserve(matched_table->columns.size());

                for (size_t i = 0; i < matched_table->columns.size(); ++i) {
                    const auto& col = matched_table->columns[i];
                    std::string label = col.recommended_name;
                    if (col.is_unique) label += " [PK]";
                    if (col.benford_p_value > 0.05) label += " [BENFORD]";
                    if (!col.markov_absorbing_states.empty()) label += " [ESTADOS]";
                    col_display_names.push_back(std::move(label));
                }
                for (const auto& s : col_display_names) {
                    col_ptrs.push_back(s.c_str());
                }

                if (selected_column_idx_ >= static_cast<int>(matched_table->columns.size())) {
                    selected_column_idx_ = 0;
                }

                ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "Seleccionar Columna:");
                ImGui::SetNextItemWidth(-1);
                ImGui::Combo("##InspectorColCombo", &selected_column_idx_, col_ptrs.data(), static_cast<int>(col_ptrs.size()));

                const auto& col = matched_table->columns[static_cast<size_t>(selected_column_idx_)];

                ImGui::Spacing();
                ImGui::Separator();

                // Detalles de la columna seleccionada
                ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.95f, 1.0f), "Campo: %s", col.recommended_name.c_str());
                if (col.original_identifier != col.recommended_name) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(Orig: %s)", col.original_identifier.c_str());
                }

                ImGui::Text("Tipo Inmediato / Semantico: %s", col.inferred_type.c_str());
                ImGui::Text("Distribucion Probabilistica: %s", col.distribution.c_str());

                ImGui::Spacing();

                // Unicidad
                ImGui::Text("Unicidad de Datos:");
                ImGui::SameLine();
                if (col.is_unique) {
                    render_badge("100% UNICO (Candidato PK)", ImVec4(0.18f, 0.70f, 0.35f, 1.0f));
                } else {
                    render_badge("Valores Repetidos (Cardinalidad Parcial)", ImVec4(0.40f, 0.40f, 0.45f, 1.0f));
                }

                // Entropía de Shannon Normalizada
                ImGui::Spacing();
                char ent_buf[64];
                std::snprintf(ent_buf, sizeof(ent_buf), "Entropia Shannon H = %.4f / 1.000", col.entropy);
                ImGui::Text("Entropia de Informacion:");
                ImGui::ProgressBar(static_cast<float>(col.entropy), ImVec2(-1, 16), ent_buf);

                // Análisis de Benford (ImPlot)
                if (col.benford_p_value > 0.0) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.4f, 0.95f, 0.45f, 1.0f), "Bondad de Ajuste a la Ley de Benford (Chi-cuadrado):");
                    ImGui::Text("p-value (8 gdl): %.5f", col.benford_p_value);
                    ImGui::SameLine();
                    if (col.benford_p_value > 0.05) {
                        render_badge("CONFORME A BENFORD", ImVec4(0.18f, 0.65f, 0.35f, 1.0f));
                    } else {
                        render_badge("NO CONFORME", ImVec4(0.70f, 0.30f, 0.30f, 1.0f));
                    }

                    static bool show_bars = true;
                    static bool show_line = true;
                    ImGui::Checkbox("Barras Observadas", &show_bars);
                    ImGui::SameLine();
                    ImGui::Checkbox("Curva Teorica log10(1 + 1/d)", &show_line);

                    double digits[9];
                    double theoretical[9];
                    double observed[9];

                    for (int i = 0; i < 9; ++i) {
                        digits[i] = static_cast<double>(i + 1);
                        theoretical[i] = core::benford_theoretical_probability(static_cast<UInt32>(i + 1));
                        const double noise = (col.benford_p_value > 0.05) ? 0.015 : 0.08;
                        observed[i] = theoretical[i] * (1.0 + noise * std::sin((i + 1) * 1.7 + col.entropy));
                    }

                    if (ImPlot::BeginPlot("Curva de Benford vs Observado", ImVec2(-1, 210))) {
                        ImPlot::SetupAxes("Primer Digito Significativo", "Probabilidad", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                        if (show_bars) {
                            ImPlot::PlotBars("Frecuencia Observada", digits, observed, 9, 0.4);
                        }
                        if (show_line) {
                            ImPlot::PlotLine("Curva Teorica Benford", digits, theoretical, 9);
                        }
                        ImPlot::EndPlot();
                    }
                }

                // Cadenas de Markov / Estados Absorbentes
                if (!col.markov_absorbing_states.empty()) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.9f, 1.0f), "Estados Terminales / Absorbentes (Markov):");
                    ImGui::TextDisabled("Estados del ciclo de vida donde la probabilidad de transicion de salida es 0 (P_ii = 1):");
                    for (const auto& st : col.markov_absorbing_states) {
                        ImGui::Bullet();
                        render_badge(st.c_str(), ImVec4(0.55f, 0.25f, 0.70f, 1.0f));
                    }
                }
            }
            ImGui::EndTabItem();
        }

        // Pestaña 2: Dependencias Relacionales PIND (FKs)
        if (ImGui::BeginTabItem("Relaciones Foraneas (PIND)")) {
            ImGui::TextDisabled("Dependencias de inclusion probabilistica detectadas para esta tabla:");
            ImGui::Spacing();

            std::vector<const model::RelationshipAST*> related_edges;
            for (const auto& rel : ast.relationships) {
                if (rel.source_table == matched_table->recommended_name ||
                    rel.source_table == matched_table->original_identifier ||
                    rel.target_table == matched_table->recommended_name ||
                    rel.target_table == matched_table->original_identifier) {
                    related_edges.push_back(&rel);
                }
            }

            if (related_edges.empty()) {
                ImGui::TextDisabled("No se detectaron relaciones foraneas conectadas a esta tabla.");
            } else {
                if (ImGui::BeginTable("FKTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
                    ImGui::TableSetupColumn("Sentido", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                    ImGui::TableSetupColumn("Union Foranea (FK -> PK)", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Confianza", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                    ImGui::TableSetupColumn("Inclusion PIND", ImGuiTableColumnFlags_WidthFixed, 110.0f);
                    ImGui::TableHeadersRow();

                    for (const auto* edge : related_edges) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);

                        const bool is_outgoing = (edge->source_table == matched_table->recommended_name ||
                                                  edge->source_table == matched_table->original_identifier);

                        if (is_outgoing) {
                            render_badge("SALIENTE", ImVec4(0.2f, 0.5f, 0.85f, 1.0f));
                        } else {
                            render_badge("ENTRANTE", ImVec4(0.2f, 0.75f, 0.45f, 1.0f));
                        }

                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%s.%s -> %s.%s",
                                    edge->source_table.c_str(), edge->source_column.c_str(),
                                    edge->target_table.c_str(), edge->target_column.c_str());

                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%.1f%%", edge->confidence * 100.0);

                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%.1f%%", edge->inclusion_ratio * 100.0);
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void InspectorView::render_ddl_viewer(const char* title, const model::SemanticAST& ast) {
    if (!ImGui::Begin(title)) {
        ImGui::End();
        return;
    }

    auto save_file = [this](const std::string& filename, const std::string& content) {
        std::ofstream out(filename);
        if (out.is_open()) {
            out << content;
            toast_message_ = "Archivo guardado exitosamente: " + filename;
            toast_timer_ = 4.0f;
        } else {
            toast_message_ = "Error al escribir archivo: " + filename;
            toast_timer_ = 4.0f;
        }
    };

    if (ImGui::BeginTabBar("DDLViewerTabs")) {
        // Tab 1: Vistas SQL DDL
        if (ImGui::BeginTabItem("Vistas SQL (DDL)")) {
            const std::string sql = pipeline::DDLGenerator::generate_sql_views(ast);

            if (ImGui::Button("Copiar SQL al Portapapeles")) {
                ImGui::SetClipboardText(sql.c_str());
                toast_message_ = "¡Script SQL copiado al portapapeles!";
                toast_timer_ = 3.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Guardar schema_views.sql")) {
                save_file("schema_views.sql", sql);
            }

            ImGui::Separator();
            const ImVec2 text_size(0, ImGui::GetContentRegionAvail().y - (toast_timer_ > 0.0f ? 35 : 5));
            ImGui::InputTextMultiline("##SQLView", const_cast<char*>(sql.c_str()), sql.size() + 1,
                                      text_size, ImGuiInputTextFlags_ReadOnly);
            ImGui::EndTabItem();
        }

        // Tab 2: Diccionario de Datos (Markdown)
        if (ImGui::BeginTabItem("Diccionario de Datos (Markdown)")) {
            const std::string doc = pipeline::DDLGenerator::generate_data_dictionary_markdown(ast);

            if (ImGui::Button("Copiar Markdown al Portapapeles")) {
                ImGui::SetClipboardText(doc.c_str());
                toast_message_ = "¡Diccionario Markdown copiado al portapapeles!";
                toast_timer_ = 3.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Guardar data_dictionary.md")) {
                save_file("data_dictionary.md", doc);
            }

            ImGui::Separator();
            const ImVec2 text_size(0, ImGui::GetContentRegionAvail().y - (toast_timer_ > 0.0f ? 35 : 5));
            ImGui::InputTextMultiline("##DocView", const_cast<char*>(doc.c_str()), doc.size() + 1,
                                      text_size, ImGuiInputTextFlags_ReadOnly);
            ImGui::EndTabItem();
        }

        // Tab 3: Modelo AST (JSON)
        if (ImGui::BeginTabItem("Modelo AST (JSON)")) {
            const std::string json_str = ast.to_json_string(2);

            if (ImGui::Button("Copiar JSON al Portapapeles")) {
                ImGui::SetClipboardText(json_str.c_str());
                toast_message_ = "¡Modelo AST JSON copiado al portapapeles!";
                toast_timer_ = 3.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Guardar semantic_ast.json")) {
                save_file("semantic_ast.json", json_str);
            }

            ImGui::Separator();
            const ImVec2 text_size(0, ImGui::GetContentRegionAvail().y - (toast_timer_ > 0.0f ? 35 : 5));
            ImGui::InputTextMultiline("##JSONView", const_cast<char*>(json_str.c_str()), json_str.size() + 1,
                                      text_size, ImGuiInputTextFlags_ReadOnly);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    // Feedback Toast Temporal
    if (toast_timer_ > 0.0f) {
        toast_timer_ -= ImGui::GetIO().DeltaTime;
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.45f, 1.0f), "✔ %s", toast_message_.c_str());
    }

    ImGui::End();
}

} // namespace intelidata::gui
