#include "intelidata/gui/InspectorView.hpp"
#include "intelidata/gui/MarkdownRenderer.hpp"
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
    const float pad_x = 6.0f;
    const float pad_y = 2.0f;
    const ImVec2 txt_sz = ImGui::CalcTextSize(text);
    const ImVec2 badge_sz(txt_sz.x + pad_x * 2.0f, txt_sz.y + pad_y * 2.0f);

    const ImVec2 cur = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    const ImU32 col_bg = ImGui::ColorConvertFloat4ToU32(bg_color);
    const ImU32 col_txt = ImGui::ColorConvertFloat4ToU32(text_color);

    draw_list->AddRectFilled(cur, ImVec2(cur.x + badge_sz.x, cur.y + badge_sz.y), col_bg, 4.0f);
    draw_list->AddText(ImVec2(cur.x + pad_x, cur.y + pad_y), col_txt, text);

    ImGui::Dummy(badge_sz);
}

void render_responsive_badge(const char* text, const ImVec4& bg_color, const ImVec4& text_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f), bool try_same_line = true) {
    const float pad_x = 6.0f;
    const float badge_w = ImGui::CalcTextSize(text).x + pad_x * 2.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float max_x = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x - ImGui::GetStyle().ScrollbarSize - 6.0f;

    if (try_same_line && (ImGui::GetItemRectMax().x + spacing + badge_w <= max_x)) {
        ImGui::SameLine(0.0f, spacing);
    }
    render_badge(text, bg_color, text_color);
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

    // Barra de conmutación superior: Diagnóstico General vs Ficha de Entidad (adaptativo)
    if (matched_table) {
        const float btn1_w = ImGui::CalcTextSize("  [Diagnostico Global del Negocio]  ").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        const std::string btn2_text = "  [Ficha de Entidad: " + matched_table->recommended_name + "]  ";
        const float btn2_w = ImGui::CalcTextSize(btn2_text.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        const float avail_w = ImGui::GetContentRegionAvail().x;

        if (ImGui::Button("  [Diagnostico Global del Negocio]  ")) {
            inspector_tab_ = 1;
        }
        if (avail_w >= btn1_w + btn2_w + ImGui::GetStyle().ItemSpacing.x) {
            ImGui::SameLine();
        }
        if (ImGui::Button(btn2_text.c_str())) {
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

    std::string domain_badge_text = (ast.system_summary.domain_icon.empty() ? "" : ast.system_summary.domain_icon + " ") + ast.system_summary.detected_domain;
    render_responsive_badge(domain_badge_text.c_str(), ImVec4(0.18f, 0.65f, 0.40f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 1.0f), false);

    char conf_buf[32];
    std::snprintf(conf_buf, sizeof(conf_buf), "Confianza: %.1f%%", ast.system_summary.confidence * 100.0);
    render_responsive_badge(conf_buf, ImVec4(0.20f, 0.50f, 0.80f, 1.0f));

    if (!ast.system_summary.architecture_pattern.empty()) {
        render_responsive_badge(ast.system_summary.architecture_pattern.c_str(), ImVec4(0.60f, 0.35f, 0.85f, 1.0f));
    }

    if (!ast.system_summary.industry_tags.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("Sectores Afines:");
        for (const auto& tag : ast.system_summary.industry_tags) {
            render_responsive_badge(("# " + tag).c_str(), ImVec4(0.20f, 0.32f, 0.45f, 1.0f));
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ==========================================
    // 2. Historia y Operación del Negocio
    // ==========================================
    ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.30f, 1.0f), "PROPOSITO Y FUNCIONAMIENTO DEL NEGOCIO:");
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.16f, 0.22f, 0.9f));
    ImGui::BeginChild("BusinessNarrativeCard", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PushTextWrapPos(0.0f);
    if (!ast.system_summary.business_narrative.empty()) {
        MarkdownRenderer::render_formatted_text(ast.system_summary.business_narrative, true);
    } else {
        ImGui::TextWrapped("El sistema ha reconstruido un modelo relacional de %zu entidades y %zu dependencias foraneas.",
                           ast.tables.size(), ast.relationships.size());
    }
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // ==========================================
    // 3. Módulos y Subdominios Funcionales
    // ==========================================
    if (!ast.system_summary.subdomains.empty()) {
        ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.60f, 1.0f), "SUBDOMINIOS Y MODULOS DE LA EMPRESA (Corte Espectral):");
        ImGui::TextDisabled("Agrupacion funcional derivada del Laplaciano normalizado y el vector de Fiedler:");
        for (const auto& sub : ast.system_summary.subdomains) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.15f, 0.19f, 0.8f));
            ImGui::BeginChild(("Subdomain_" + std::to_string(sub.cluster_id)).c_str(), ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

            ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "* %s", sub.name.c_str());
            ImGui::SameLine();
            char c_buf[16];
            std::snprintf(c_buf, sizeof(c_buf), "Cluster C%d", sub.cluster_id);
            render_badge(c_buf, ImVec4(0.25f, 0.40f, 0.65f, 1.0f));

            ImGui::TextWrapped("%s", sub.description.c_str());

            ImGui::Spacing();
            ImGui::TextDisabled("Tablas del modulo:");
            const float max_sub_x = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x - ImGui::GetStyle().ScrollbarSize - 4.0f;
            for (size_t b_idx = 0; b_idx < sub.table_names.size(); ++b_idx) {
                const auto& tbl_name = sub.table_names[b_idx];
                const float btn_w = ImGui::CalcTextSize(tbl_name.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
                const float spacing = ImGui::GetStyle().ItemSpacing.x;
                if (b_idx > 0 && (ImGui::GetItemRectMax().x + spacing + btn_w <= max_sub_x)) {
                    ImGui::SameLine(0.0f, spacing);
                }
                ImGui::PushID(static_cast<int>(b_idx));
                if (ImGui::SmallButton(tbl_name.c_str())) {
                    requested_table_ = tbl_name;
                }
                ImGui::PopID();
            }

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
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f), "HALLAZGOS Y AUDITORIA FORENSE:");
        for (const auto& ins : ast.system_summary.executive_insights) {
            ImGui::Bullet();
            ImGui::SameLine();
            MarkdownRenderer::render_formatted_text(ins, true);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("[INFO] Seleccione cualquier tabla en el Grafo Ontologico 2D para abrir su Ficha de Negocio detallada.");
}

void InspectorView::render_entity_business_sheet(const model::TableAST& tbl, const model::SemanticAST& ast) {
    // ==========================================
    // 1. Tarjeta Cabecera de la Entidad
    // ==========================================
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.16f, 0.22f, 0.9f));
    ImGui::BeginChild("EntityHeaderCard", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

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
        const ImGuiTabItemFlags tab0_flags = (entity_sheet_tab_ == 0) ? ImGuiTabItemFlags_SetSelected : 0;
        // Pestaña 1: Interpretación de Negocio y Preguntas Clave
        if (ImGui::BeginTabItem("[Negocio] Proposito y Logica", nullptr, tab0_flags)) {
            ImGui::Spacing();

            // Propósito en el Negocio
            ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.60f, 1.0f), "ROL Y SIGNIFICADO EN EL NEGOCIO:");
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.15f, 0.20f, 0.8f));
            ImGui::BeginChild("PurposeCard", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
            ImGui::PushTextWrapPos(0.0f);
            if (!tbl.business_purpose.empty()) {
                MarkdownRenderer::render_formatted_text(tbl.business_purpose, true);
            } else {
                ImGui::TextWrapped("Entidad operativa del esquema relacional analizado.");
            }
            ImGui::PopTextWrapPos();
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Justificación del Rol
            if (!tbl.role_justification.empty()) {
                ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f), "JUSTIFICACION DEL ROL ONTOLOGICO:");
                MarkdownRenderer::render_formatted_text(tbl.role_justification, true);
                ImGui::Spacing();
            }

            // Flujo Operativo de Datos (Entradas y Salidas)
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.40f, 0.85f, 1.0f, 1.0f), "FLUJO OPERATIVO DE DATOS:");

            ImGui::TextDisabled("<- Recibe datos de (Dependencias Clave):");
            if (tbl.incoming_business_sources.empty()) {
                ImGui::BulletText("(Entidad raiz independiente / sin dependencias entrantes)");
            } else {
                for (const auto& src : tbl.incoming_business_sources) {
                    ImGui::Bullet();
                    ImGui::SameLine();
                    MarkdownRenderer::render_formatted_text(src, true);
                }
            }

            ImGui::Spacing();
            ImGui::TextDisabled("-> Alimenta a (Entidades dependientes):");
            if (tbl.outgoing_business_targets.empty()) {
                ImGui::BulletText("(Entidad terminal / sin dependencias salientes directas)");
            } else {
                for (const auto& tgt : tbl.outgoing_business_targets) {
                    ImGui::Bullet();
                    ImGui::SameLine();
                    MarkdownRenderer::render_formatted_text(tgt, true);
                }
            }

            ImGui::Spacing();
            ImGui::Separator();

            // Preguntas Clave de Negocio que Responde
            if (!tbl.business_questions.empty()) {
                ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), "PREGUNTAS CLAVE DE NEGOCIO QUE RESPONDE ESTA TABLA:");
                ImGui::Spacing();
                int q_idx = 1;
                for (const auto& q : tbl.business_questions) {
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.6f, 1.0f), "  %d.", q_idx++);
                    ImGui::SameLine();
                    MarkdownRenderer::render_formatted_text(q, true);
                }
                ImGui::Spacing();
            }

            // Diagnóstico de Calidad e Integridad
            if (!tbl.quality_insights.empty()) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.40f, 1.0f), "DIAGNOSTICO DE CALIDAD E INTEGRIDAD:");
                for (const auto& qi : tbl.quality_insights) {
                    ImGui::Bullet();
                    ImGui::SameLine();
                    MarkdownRenderer::render_formatted_text(qi, true);
                }
            }

            ImGui::EndTabItem();
        }

        // Pestaña 2: Columnas y Tipos Semánticos
        const ImGuiTabItemFlags tab1_flags = (entity_sheet_tab_ == 1) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem("[Metricas] Columnas y Estadisticas", nullptr, tab1_flags)) {
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
        const ImGuiTabItemFlags tab2_flags = (entity_sheet_tab_ == 2) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem("[Benford] Auditoria Forense", nullptr, tab2_flags)) {
            ImGui::Spacing();
            ImGui::TextWrapped("La Ley de Benford modela la distribucion del primer digito significativo en magnitudes economicas y financieras naturales. Permite auditar si los montos provienen de transacciones reales o datos sinteticos.");
            ImGui::Spacing();
            ImGui::Separator();

            std::vector<const model::ColumnAST*> benford_cols;
            for (const auto& col : tbl.columns) {
                if (col.benford_samples >= 30 || !col.benford_observed_digits.empty() || col.benford_p_value > 0.0) {
                    benford_cols.push_back(&col);
                }
            }

            auto score_benford_c = [](const model::ColumnAST* c) -> int64_t {
                if (!c) return 0;
                std::string lower = c->recommended_name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                int64_t score = static_cast<int64_t>(c->benford_samples);
                if (lower.find("price") != std::string::npos || lower.find("precio") != std::string::npos) score += 50000;
                if (lower.find("total") != std::string::npos || lower.find("amount") != std::string::npos || lower.find("monto") != std::string::npos) score += 40000;
                if (lower.find("freight") != std::string::npos || lower.find("flete") != std::string::npos || lower.find("cost") != std::string::npos) score += 35000;
                if (lower.find("discount") != std::string::npos || lower.find("descuento") != std::string::npos) score += 20000;
                if (lower.find("sales") != std::string::npos || lower.find("venta") != std::string::npos) score += 30000;
                if (c->is_unique || lower.find("id") != std::string::npos || lower.find("codigo") != std::string::npos) score -= 10000;
                return score;
            };

            std::sort(benford_cols.begin(), benford_cols.end(),
                      [&score_benford_c](const auto* a, const auto* b) {
                          return score_benford_c(a) > score_benford_c(b);
                      });

            if (benford_cols.empty()) {
                ImGui::Spacing();
                ImGui::TextDisabled("Esta entidad no contiene columnas numericas con volumen transaccional suficiente (N >= 30) para auditoria forense de Benford.");
            } else {
                if (selected_benford_col_idx_ < 0 || selected_benford_col_idx_ >= static_cast<int>(benford_cols.size())) {
                    selected_benford_col_idx_ = 0;
                }

                if (benford_cols.size() > 1) {
                    ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "Seleccionar Metrica Contable / Transaccional:");
                    std::string preview = benford_cols[selected_benford_col_idx_]->recommended_name;
                    if (ImGui::BeginCombo("##BenfordColCombo", preview.c_str())) {
                        for (int i = 0; i < static_cast<int>(benford_cols.size()); ++i) {
                            const bool is_sel = (selected_benford_col_idx_ == i);
                            if (ImGui::Selectable(benford_cols[i]->recommended_name.c_str(), is_sel)) {
                                selected_benford_col_idx_ = i;
                            }
                            if (is_sel) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::Spacing();
                }

                const auto& col = *benford_cols[selected_benford_col_idx_];

                // Tarjetas de Métricas Estadísticas Benford
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 0.85f));
                if (ImGui::BeginChild("BenfordMetricSummary", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY)) {
                    const float b_avail_w = ImGui::GetContentRegionAvail().x;
                    const int b_cols = (b_avail_w < 400.0f) ? 2 : 4;
                    ImGui::Columns(b_cols, "BenfordCols", false);

                    // Col 1: Columna y N
                    ImGui::TextDisabled("Variable y Muestra");
                    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "%s", col.recommended_name.c_str());
                    ImGui::TextDisabled("N = %llu regs", static_cast<unsigned long long>(col.benford_samples));
                    ImGui::NextColumn();

                    // Col 2: Chi-Cuadrado
                    ImGui::TextDisabled("Chi-Cuadrado (8 gdl)");
                    ImGui::Text("%.2f", col.benford_chi_square);
                    if (col.benford_chi_square <= 15.51) {
                        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "Bajo umbral critico");
                    } else {
                        ImGui::TextColored(ImVec4(0.95f, 0.5f, 0.4f, 1.0f), "> 15.51 (Critico)");
                    }
                    ImGui::NextColumn();

                    // Col 3: P-Valor
                    ImGui::TextDisabled("P-Valor Analitico");
                    ImGui::Text("%.5f", col.benford_p_value);
                    if (col.benford_p_value >= 0.05) {
                        render_badge("H0: ORGANICA", ImVec4(0.18f, 0.65f, 0.35f, 1.0f));
                    } else {
                        render_badge("H0: RECHAZADA", ImVec4(0.70f, 0.30f, 0.30f, 1.0f));
                    }
                    ImGui::NextColumn();

                    // Col 4: Nigrini MAD
                    ImGui::TextDisabled("Conformidad Nigrini MAD");
                    ImGui::Text("%.4f", col.benford_mad);
                    if (col.benford_mad <= 0.006) {
                        render_badge("ESTRICTA", ImVec4(0.18f, 0.65f, 0.35f, 1.0f));
                    } else if (col.benford_mad <= 0.012) {
                        render_badge("ACEPTABLE", ImVec4(0.25f, 0.60f, 0.70f, 1.0f));
                    } else if (col.benford_mad <= 0.015) {
                        render_badge("MARGINAL", ImVec4(0.85f, 0.60f, 0.20f, 1.0f));
                    } else {
                        render_badge("NO CONFORME", ImVec4(0.75f, 0.25f, 0.25f, 1.0f));
                    }

                    ImGui::Columns(1);
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();

                ImGui::Spacing();

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
                    if (i < static_cast<int>(col.benford_observed_digits.size())) {
                        observed[i] = col.benford_observed_digits[i];
                    } else {
                        observed[i] = theoretical[i];
                    }
                }

                if (ImPlot::BeginPlot("Curva de Benford vs Frecuencia Observada", ImVec2(-1, 230))) {
                    ImPlot::SetupAxes("Primer Digito Significativo", "Probabilidad", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                    if (show_bars) {
                        ImPlot::PlotBars("Frecuencia Observada", digits, observed, 9, 0.4);
                    }
                    if (show_line) {
                        ImPlot::PlotLine("Curva Teorica Benford", digits, theoretical, 9);
                    }
                    ImPlot::EndPlot();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Dictamen Forense e Interpretación Dinámica
                ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "DICTAMEN E INTERPRETACION FORENSE:");
                ImGui::Spacing();

                std::string interp = col.benford_interpretation;
                if (interp.empty()) {
                    core::BenfordResult res{};
                    res.total_valid_samples = col.benford_samples > 0 ? col.benford_samples : 100;
                    res.chi_square_stat = col.benford_chi_square;
                    res.p_value = col.benford_p_value;
                    res.mean_absolute_deviation = col.benford_mad;
                    for (size_t d = 1; d <= 9; ++d) {
                        res.expected_proportions[d] = core::benford_theoretical_probability(static_cast<UInt32>(d));
                        if (d - 1 < col.benford_observed_digits.size()) {
                            res.observed_proportions[d] = col.benford_observed_digits[d - 1];
                        } else {
                            res.observed_proportions[d] = res.expected_proportions[d];
                        }
                    }
                    interp = core::generate_benford_interpretation(tbl.recommended_name, col.recommended_name, res);
                }

                MarkdownRenderer::render_document(interp);
            }

            ImGui::EndTabItem();
        }

        // Pestaña 4: Dependencias Foráneas PIND
        const ImGuiTabItemFlags tab3_flags = (entity_sheet_tab_ == 3) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem("[PIND] Dependencias Foraneas", nullptr, tab3_flags)) {
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
                    ImGui::TableSetupColumn("Sentido", ImGuiTableColumnFlags_WidthFixed, 62.0f);
                    ImGui::TableSetupColumn("Dependencia Foranea (FK -> PK)", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Conf.", ImGuiTableColumnFlags_WidthFixed, 55.0f);
                    ImGui::TableSetupColumn("Inclusion", ImGuiTableColumnFlags_WidthFixed, 65.0f);
                    ImGui::TableHeadersRow();

                    for (size_t edge_idx = 0; edge_idx < related_edges.size(); ++edge_idx) {
                        const auto* edge = related_edges[edge_idx];
                        ImGui::TableNextRow();
                        ImGui::PushID(static_cast<int>(edge_idx));
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
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
        entity_sheet_tab_ = -1;
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
        const ImGuiTabItemFlags ddl0_flags = (ddl_viewer_tab_ == 0) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem("Vistas SQL (DDL)", nullptr, ddl0_flags)) {
            const std::string sql = pipeline::DDLGenerator::generate_sql_views(ast);

            if (ImGui::Button("Copiar SQL al Portapapeles")) {
                ImGui::SetClipboardText(sql.c_str());
                toast_message_ = "¡Script SQL copiado al portapapeles!";
                toast_timer_ = 3.0f;
            }
            const float save_sql_w = ImGui::CalcTextSize("Guardar schema_views.sql").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            if (ImGui::GetContentRegionAvail().x >= save_sql_w + ImGui::GetStyle().ItemSpacing.x) {
                ImGui::SameLine();
            }
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
        const ImGuiTabItemFlags ddl1_flags = (ddl_viewer_tab_ == 1) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem("Diccionario de Datos (Markdown)", nullptr, ddl1_flags)) {
            const std::string doc = pipeline::DDLGenerator::generate_data_dictionary_markdown(ast);

            static int markdown_view_mode = 0; // 0: Interpretado, 1: Codigo Crudo
            ImGui::RadioButton("Documento Interpretado", &markdown_view_mode, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Codigo Markdown Crudo", &markdown_view_mode, 1);

            if (ImGui::Button("Copiar Markdown al Portapapeles")) {
                ImGui::SetClipboardText(doc.c_str());
                toast_message_ = "¡Diccionario Markdown copiado al portapapeles!";
                toast_timer_ = 3.0f;
            }
            const float save_md_w = ImGui::CalcTextSize("Guardar data_dictionary.md").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            if (ImGui::GetContentRegionAvail().x >= save_md_w + ImGui::GetStyle().ItemSpacing.x) {
                ImGui::SameLine();
            }
            if (ImGui::Button("Guardar data_dictionary.md")) {
                intelidata::pipeline::DDLGenerator::export_report_with_assets(ast, "data_dictionary.md");
                toast_message_ = "¡data_dictionary.md y graficas guardadas en images/!";
                toast_timer_ = 3.0f;
            }

            ImGui::Separator();
            const float avail_y = ImGui::GetContentRegionAvail().y - (toast_timer_ > 0.0f ? 35.0f : 5.0f);

            if (markdown_view_mode == 0) {
                // Modo Interpretado: Documento interactivo con tablas nativas, encabezados y formato
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.12f, 0.16f, 1.0f));
                if (ImGui::BeginChild("##InterpretedMarkdownDoc", ImVec2(0, avail_y), true)) {
                    MarkdownRenderer::render_document(doc);
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();
            } else {
                // Modo Crudo: Editor de solo lectura
                ImGui::InputTextMultiline("##DocView", const_cast<char*>(doc.c_str()), doc.size() + 1,
                                          ImVec2(0, avail_y), ImGuiInputTextFlags_ReadOnly);
            }
            ImGui::EndTabItem();
        }

        // Tab 3: Modelo AST (JSON)
        const ImGuiTabItemFlags ddl2_flags = (ddl_viewer_tab_ == 2) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem("Modelo AST (JSON)", nullptr, ddl2_flags)) {
            const std::string json_str = ast.to_json_string(2);

            if (ImGui::Button("Copiar JSON al Portapapeles")) {
                ImGui::SetClipboardText(json_str.c_str());
                toast_message_ = "¡Modelo AST JSON copiado al portapapeles!";
                toast_timer_ = 3.0f;
            }
            const float save_json_w = ImGui::CalcTextSize("Guardar semantic_ast.json").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            if (ImGui::GetContentRegionAvail().x >= save_json_w + ImGui::GetStyle().ItemSpacing.x) {
                ImGui::SameLine();
            }
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
        ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.45f, 1.0f), "[OK] %s", toast_message_.c_str());
    }

    ImGui::End();
}

} // namespace intelidata::gui
