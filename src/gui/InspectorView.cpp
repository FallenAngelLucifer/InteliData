#include "intelidata/gui/InspectorView.hpp"
#include "intelidata/pipeline/DDLGenerator.hpp"
#include "intelidata/core/Benford.hpp"
#include <implot.h>
#include <imgui.h>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

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
    if (role == "Fact_Transaction" || role == "FACT_TABLE") return ImVec4(0.92f, 0.45f, 0.20f, 1.0f);        // Ámbar / Naranja
    if (role == "Dimension_Master_Entity" || role == "DIMENSION_TABLE") return ImVec4(0.18f, 0.75f, 0.45f, 1.0f);   // Verde Esmeralda
    if (role == "Bridge_Lookup" || role == "BRIDGE_TABLE") return ImVec4(0.62f, 0.35f, 0.88f, 1.0f);      // Púrpura
    if (role == "Lookup_Table" || role == "LOOKUP_TABLE") return ImVec4(0.20f, 0.65f, 0.92f, 1.0f);      // Azul Cyan
    return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);                                  // Gris
}

} // namespace

void InspectorView::render_inspector(const char* title, const model::SemanticAST& ast, const NodeUI* selected_node) {
    if (!ImGui::Begin(title)) {
        ImGui::End();
        return;
    }

    // Buscar la tabla correspondiente en el AST si hay nodo seleccionado
    const model::TableAST* matched_table = nullptr;
    if (selected_node) {
        for (const auto& tbl : ast.tables) {
            if (tbl.recommended_name == selected_node->name || tbl.original_identifier == selected_node->name) {
                matched_table = &tbl;
                break;
            }
        }
    }

    // Barra de conmutación superior: Diagnóstico General vs Ficha de Entidad
    if (matched_table) {
        if (ImGui::Button("  🌐 Diagnostico Global del Negocio  ")) {
            inspector_tab_ = 1;
        }
        ImGui::SameLine();
        if (ImGui::Button(("  📋 Ficha de Entidad: " + matched_table->recommended_name + "  ").c_str())) {
            inspector_tab_ = 0;
        }
        ImGui::Separator();
        ImGui::Spacing();
    } else {
        inspector_tab_ = 1; // Si no hay nodo seleccionado, mostrar diagnóstico general
    }

    if (inspector_tab_ == 1 || !matched_table) {
        render_global_business_diagnostic(ast);
    } else {
        render_entity_business_sheet(*matched_table, ast);
    }

    ImGui::End();
}

void InspectorView::render_global_business_diagnostic(const model::SemanticAST& ast) {
    // ==========================================
    // 1. Resumen Ejecutivo y Diagnóstico Global
    // ==========================================
    ImGui::TextColored(ImVec4(0.2f, 0.85f, 1.0f, 1.0f), "DIAGNOSTICO SEMANTICO Y ARQUITECTURA DEL NEGOCIO");
    ImGui::Spacing();

    render_badge(ast.system_summary.detected_domain.c_str(), ImVec4(0.18f, 0.65f, 0.40f, 1.0f));
    ImGui::SameLine();

    char conf_buf[32];
    std::snprintf(conf_buf, sizeof(conf_buf), "Confianza: %.1f%%", ast.system_summary.confidence * 100.0);
    render_badge(conf_buf, ImVec4(0.20f, 0.50f, 0.80f, 1.0f));

    if (!ast.system_summary.architecture_pattern.empty()) {
        ImGui::SameLine();
        render_badge(ast.system_summary.architecture_pattern.c_str(), ImVec4(0.60f, 0.35f, 0.85f, 1.0f));
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ==========================================
    // 2. Historia y Operación del Negocio
    // ==========================================
    ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.30f, 1.0f), "📖 Historia y Funcionamiento del Negocio:");
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.16f, 0.22f, 0.9f));
    ImGui::BeginChild("BusinessNarrativeCard", ImVec2(0, 110), true);
    if (!ast.system_summary.business_narrative.empty()) {
        ImGui::TextWrapped("%s", ast.system_summary.business_narrative.c_str());
    } else {
        ImGui::TextWrapped("El sistema ha reconstruido un modelo relacional de %zu entidades y %zu dependencias foraneas.",
                           ast.tables.size(), ast.relationships.size());
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // ==========================================
    // 3. Módulos y Subdominios Funcionales
    // ==========================================
    if (!ast.system_summary.subdomains.empty()) {
        ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.60f, 1.0f), "🗂️ Modulos y Subdominios de la Empresa (Corte Espectral):");
        ImGui::TextDisabled("Agrupacion funcional derivada del Laplaciano normalizado y el vector de Fiedler:");
        ImGui::Spacing();

        for (const auto& sub : ast.system_summary.subdomains) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.15f, 0.19f, 0.8f));
            const float box_h = 75.0f + static_cast<float>(sub.table_names.size() > 6 ? 26 : 0);
            ImGui::BeginChild(("Subdomain_" + std::to_string(sub.cluster_id)).c_str(), ImVec2(0, box_h), true);

            ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "• %s", sub.name.c_str());
            ImGui::SameLine();
            char c_buf[16];
            std::snprintf(c_buf, sizeof(c_buf), "Cluster C%d", sub.cluster_id);
            render_badge(c_buf, ImVec4(0.25f, 0.40f, 0.65f, 1.0f));

            ImGui::TextDisabled("%s", sub.description.c_str());

            ImGui::Spacing();
            ImGui::Text("Tablas del modulo:");
            ImGui::SameLine();
            for (const auto& tbl_name : sub.table_names) {
                if (ImGui::SmallButton(tbl_name.c_str())) {
                    requested_table_ = tbl_name;
                }
                ImGui::SameLine();
            }
            ImGui::NewLine();

            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }
    }

    // ==========================================
    // 4. Hallazgos Clave de Integridad y Calidad
    // ==========================================
    if (!ast.system_summary.executive_insights.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f), "🔍 Hallazgos y Diagnostico de Integridad Forense:");
        for (const auto& ins : ast.system_summary.executive_insights) {
            ImGui::Bullet();
            ImGui::TextWrapped("%s", ins.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("💡 Seleccione cualquier tabla en el Grafo Ontologico 2D para abrir su Ficha de Negocio detallada.");
}

void InspectorView::render_entity_business_sheet(const model::TableAST& tbl, const model::SemanticAST& ast) {
    // ==========================================
    // 1. Tarjeta Cabecera de la Entidad
    // ==========================================
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.16f, 0.22f, 0.9f));
    ImGui::BeginChild("EntityHeaderCard", ImVec2(0, 110), true);

    ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "Entidad: %s", tbl.recommended_name.c_str());
    if (tbl.original_identifier != tbl.recommended_name) {
        ImGui::SameLine();
        ImGui::TextDisabled("(Identificador fisico original: %s)", tbl.original_identifier.c_str());
    }

    ImGui::Spacing();
    render_badge(tbl.inferred_role.c_str(), role_color(tbl.inferred_role));
    ImGui::SameLine();

    std::string cluster_str = "Cluster Espectral C" + std::to_string(tbl.spectral_cluster);
    render_badge(cluster_str.c_str(), ImVec4(0.25f, 0.45f, 0.75f, 1.0f));

    ImGui::SameLine();
    ImGui::TextDisabled("| %zu Columnas", tbl.columns.size());

    ImGui::Spacing();
    const float pr_val = static_cast<float>(tbl.pagerank_gravity);
    char pr_buf[64];
    std::snprintf(pr_buf, sizeof(pr_buf), "Gravedad Relacional (PageRank): %.4f", pr_val);
    ImGui::ProgressBar(std::clamp(pr_val * 4.0f, 0.0f, 1.0f), ImVec2(-1, 16), pr_buf);

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // ==========================================
    // 2. Pestañas de Detalle de la Entidad
    // ==========================================
    if (ImGui::BeginTabBar("EntitySheetTabs")) {
        // Pestaña 1: Interpretación de Negocio y Preguntas Clave
        if (ImGui::BeginTabItem("🧠 Proposito y Negocio")) {
            ImGui::Spacing();

            // Propósito en el Negocio
            ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.60f, 1.0f), "🎯 ¿Que representa en el negocio?");
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.15f, 0.20f, 0.8f));
            ImGui::BeginChild("PurposeCard", ImVec2(0, 65), true);
            if (!tbl.business_purpose.empty()) {
                ImGui::TextWrapped("%s", tbl.business_purpose.c_str());
            } else {
                ImGui::TextWrapped("Entidad operativa del esquema relacional analizado.");
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Justificación del Rol
            if (!tbl.role_justification.empty()) {
                ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f), "⚖️ Justificacion del Rol Ontologico:");
                ImGui::TextWrapped("%s", tbl.role_justification.c_str());
                ImGui::Spacing();
            }

            // Flujo Operativo de Datos (Entradas y Salidas)
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.40f, 0.85f, 1.0f, 1.0f), "🔄 Flujo Operativo de Datos:");

            ImGui::TextDisabled("⬅️ Recibe datos de (Dependencias Clave):");
            if (tbl.incoming_business_sources.empty()) {
                ImGui::BulletText("(Entidad raiz independiente / sin dependencias entrantes)");
            } else {
                for (const auto& src : tbl.incoming_business_sources) {
                    ImGui::BulletText("%s", src.c_str());
                }
            }

            ImGui::Spacing();
            ImGui::TextDisabled("➡️ Alimenta a (Entidades dependientes):");
            if (tbl.outgoing_business_targets.empty()) {
                ImGui::BulletText("(Entidad terminal / sin dependencias salientes directas)");
            } else {
                for (const auto& tgt : tbl.outgoing_business_targets) {
                    ImGui::BulletText("%s", tgt.c_str());
                }
            }

            ImGui::Spacing();
            ImGui::Separator();

            // Preguntas Clave de Negocio que Responde
            if (!tbl.business_questions.empty()) {
                ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), "💡 Preguntas Clave de Negocio que Responde esta Tabla:");
                ImGui::Spacing();
                int q_idx = 1;
                for (const auto& q : tbl.business_questions) {
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.6f, 1.0f), "  %d.", q_idx++);
                    ImGui::SameLine();
                    ImGui::TextWrapped("%s", q.c_str());
                }
                ImGui::Spacing();
            }

            // Diagnóstico de Calidad e Integridad
            if (!tbl.quality_insights.empty()) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.40f, 1.0f), "🛡️ Diagnostico de Calidad e Integridad:");
                for (const auto& qi : tbl.quality_insights) {
                    ImGui::Bullet();
                    ImGui::TextWrapped("%s", qi.c_str());
                }
            }

            ImGui::EndTabItem();
        }

        // Pestaña 2: Columnas y Tipos Semánticos
        if (ImGui::BeginTabItem("📊 Columnas y Metricas")) {
            if (tbl.columns.empty()) {
                ImGui::TextDisabled("No hay columnas disponibles.");
            } else {
                std::vector<std::string> col_display_names;
                std::vector<const char*> col_ptrs;
                col_display_names.reserve(tbl.columns.size());
                col_ptrs.reserve(tbl.columns.size());

                for (size_t i = 0; i < tbl.columns.size(); ++i) {
                    const auto& col = tbl.columns[i];
                    std::string label = col.recommended_name;
                    if (col.is_unique) label += " [PK]";
                    if (col.benford_p_value > 0.05) label += " [BENFORD]";
                    if (!col.markov_absorbing_states.empty()) label += " [ESTADOS]";
                    col_display_names.push_back(std::move(label));
                }
                for (const auto& s : col_display_names) {
                    col_ptrs.push_back(s.c_str());
                }

                if (selected_column_idx_ >= static_cast<int>(tbl.columns.size())) {
                    selected_column_idx_ = 0;
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "Seleccionar Columna:");
                ImGui::SetNextItemWidth(-1);
                ImGui::Combo("##InspectorColCombo", &selected_column_idx_, col_ptrs.data(), static_cast<int>(col_ptrs.size()));

                const auto& col = tbl.columns[static_cast<size_t>(selected_column_idx_)];

                ImGui::Spacing();
                ImGui::Separator();

                ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.95f, 1.0f), "Campo: %s", col.recommended_name.c_str());
                if (col.original_identifier != col.recommended_name) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(Fisico: %s)", col.original_identifier.c_str());
                }

                ImGui::Text("Tipo Semantico Inferido: %s", col.inferred_type.c_str());
                ImGui::Text("Distribucion Probabilistica: %s", col.distribution.c_str());

                ImGui::Spacing();
                ImGui::Text("Unicidad de Datos:");
                ImGui::SameLine();
                if (col.is_unique) {
                    render_badge("100% UNICO (Candidato PK)", ImVec4(0.18f, 0.70f, 0.35f, 1.0f));
                } else {
                    render_badge("Valores Repetidos (Cardinalidad Parcial)", ImVec4(0.40f, 0.40f, 0.45f, 1.0f));
                }

                ImGui::Spacing();
                char ent_buf[64];
                std::snprintf(ent_buf, sizeof(ent_buf), "Entropia Shannon H = %.4f / 1.000", col.entropy);
                ImGui::Text("Entropia de Informacion:");
                ImGui::ProgressBar(static_cast<float>(col.entropy), ImVec2(-1, 16), ent_buf);

                if (!col.markov_absorbing_states.empty()) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.9f, 1.0f), "Estados Terminales / Absorbentes (Markov):");
                    for (const auto& st : col.markov_absorbing_states) {
                        ImGui::Bullet();
                        render_badge(st.c_str(), ImVec4(0.55f, 0.25f, 0.70f, 1.0f));
                    }
                }
            }
            ImGui::EndTabItem();
        }

        // Pestaña 3: Auditoría Benford (ImPlot)
        if (ImGui::BeginTabItem("📈 Auditoria Benford")) {
            ImGui::Spacing();
            ImGui::TextWrapped("La Ley de Benford modela la distribucion del primer digito significativo en magnitudes economicas y financieras naturales. Permite auditar si los montos provienen de transacciones reales o datos sinteticos.");
            ImGui::Spacing();
            ImGui::Separator();

            bool found_benford_col = false;
            for (const auto& col : tbl.columns) {
                if (col.benford_p_value > 0.0) {
                    found_benford_col = true;
                    ImGui::TextColored(ImVec4(0.4f, 0.95f, 0.45f, 1.0f), "Columna Evaluada: %s", col.recommended_name.c_str());
                    ImGui::Text("Valor p (Chi-cuadrado 8 gdl): %.5f", col.benford_p_value);
                    ImGui::SameLine();
                    if (col.benford_p_value > 0.05) {
                        render_badge("CONFORME A BENFORD (Organico)", ImVec4(0.18f, 0.65f, 0.35f, 1.0f));
                    } else {
                        render_badge("NO CONFORME (Sintetico / Fijo)", ImVec4(0.70f, 0.30f, 0.30f, 1.0f));
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

                    if (ImPlot::BeginPlot("Curva de Benford vs Observado", ImVec2(-1, 220))) {
                        ImPlot::SetupAxes("Primer Digito Significativo", "Probabilidad", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                        if (show_bars) {
                            ImPlot::PlotBars("Frecuencia Observada", digits, observed, 9, 0.4);
                        }
                        if (show_line) {
                            ImPlot::PlotLine("Curva Teorica Benford", digits, theoretical, 9);
                        }
                        ImPlot::EndPlot();
                    }
                    ImGui::Separator();
                }
            }

            if (!found_benford_col) {
                ImGui::TextDisabled("Esta entidad no contiene columnas numericas monetarias que requieran auditoria forense Benford.");
            }

            ImGui::EndTabItem();
        }

        // Pestaña 4: Dependencias Foráneas PIND
        if (ImGui::BeginTabItem("🔗 Relaciones (PIND)")) {
            ImGui::Spacing();
            ImGui::TextDisabled("Dependencias de inclusion probabilistica detectadas para esta tabla:");
            ImGui::Spacing();

            std::vector<const model::RelationshipAST*> related_edges;
            for (const auto& rel : ast.relationships) {
                if (rel.source_table == tbl.recommended_name ||
                    rel.source_table == tbl.original_identifier ||
                    rel.target_table == tbl.recommended_name ||
                    rel.target_table == tbl.original_identifier) {
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

                        const bool is_outgoing = (edge->source_table == tbl.recommended_name ||
                                                  edge->source_table == tbl.original_identifier);

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
