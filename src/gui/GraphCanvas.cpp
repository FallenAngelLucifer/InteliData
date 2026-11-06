#include "intelidata/gui/GraphCanvas.hpp"
#include <cmath>
#include <algorithm>
#include <random>
#include <cctype>

namespace intelidata::gui {

GraphCanvas::GraphCanvas() = default;

void GraphCanvas::load_from_ast(const model::SemanticAST& ast) {
    nodes_.clear();
    edges_.clear();
    selected_node_ = nullptr;
    temperature_ = 100.0f;
    pan_offset_ = ImVec2(0.0f, 0.0f);
    zoom_ = 1.0f;

    std::unordered_map<std::string, Size> name_to_idx;

    std::mt19937 rng(42);
    std::uniform_real_distribution<Float32> dist(-120.0f, 120.0f);

    const Size total = ast.tables.size();
    for (Size i = 0; i < total; ++i) {
        const auto& tbl = ast.tables[i];
        NodeUI node;
        node.id = i;
        node.original_name = tbl.original_identifier;
        node.name = tbl.recommended_name.empty() ? tbl.original_identifier : tbl.recommended_name;
        node.inferred_role = tbl.inferred_role;
        node.pagerank_gravity = tbl.pagerank_gravity;
        node.spectral_cluster = tbl.spectral_cluster;
        node.column_count = tbl.columns.size();
        node.business_purpose = tbl.business_purpose;

        // Posicionamiento inicial radial con perturbación estocástica
        const Float32 angle = static_cast<Float32>(i) * (2.0f * 3.14159265f / static_cast<Float32>(std::max<Size>(1, total)));
        node.x = 450.0f + 220.0f * std::cos(angle) + dist(rng);
        node.y = 350.0f + 220.0f * std::sin(angle) + dist(rng);

        // Radio proporcional a la gravedad de PageRank
        node.radius = 24.0f + static_cast<Float32>(node.pagerank_gravity * 50.0f);

        name_to_idx[tbl.original_identifier] = i;
        nodes_.push_back(std::move(node));
    }

    update_node_colors();

    for (const auto& rel : ast.relationships) {
        const auto s_it = name_to_idx.find(rel.source_table);
        const auto t_it = name_to_idx.find(rel.target_table);

        if (s_it != name_to_idx.end() && t_it != name_to_idx.end()) {
            EdgeUI edge;
            edge.source_idx = s_it->second;
            edge.target_idx = t_it->second;
            edge.source_tbl = rel.source_table;
            edge.target_tbl = rel.target_table;
            edge.source_col = rel.source_column;
            edge.target_col = rel.target_column;
            edge.confidence = rel.confidence;
            edge.inclusion_ratio = rel.inclusion_ratio;
            edges_.push_back(std::move(edge));
        }
    }

    if (!nodes_.empty()) {
        selected_node_ = &nodes_[0];
    }
}

void GraphCanvas::select_node_by_name(std::string_view name) {
    for (const auto& n : nodes_) {
        if (n.original_name == name || n.name == name) {
            selected_node_ = &n;
            return;
        }
    }
}

void GraphCanvas::update_node_colors() {
    for (auto& node : nodes_) {
        switch (color_mode_) {
            case NodeColorMode::SpectralCluster: {
                if (node.spectral_cluster == 0) {
                    node.color = IM_COL32(40, 160, 230, 240); // Azul cian
                } else if (node.spectral_cluster == 1) {
                    node.color = IM_COL32(235, 95, 75, 240);  // Coral
                } else if (node.spectral_cluster == 2) {
                    node.color = IM_COL32(145, 100, 220, 240); // Púrpura
                } else {
                    node.color = IM_COL32(50, 190, 130, 240);  // Verde azulado
                }
                break;
            }
            case NodeColorMode::OntologicalRole: {
                if (node.inferred_role == "Dimension_Master_Entity") {
                    node.color = IM_COL32(46, 204, 113, 240); // Verde esmeralda
                } else if (node.inferred_role == "Fact_Hub_Transaction" || node.inferred_role == "Standard_Entity") {
                    node.color = IM_COL32(52, 152, 219, 240); // Azul relacional
                } else if (node.inferred_role == "Bridge_Lookup") {
                    node.color = IM_COL32(155, 89, 182, 240); // Amatista
                } else {
                    node.color = IM_COL32(241, 196, 15, 240); // Dorado
                }
                break;
            }
            case NodeColorMode::PageRankGravity: {
                // Gradiente térmico: de azul frío (baja gravedad) a rojo cálido (alta gravedad)
                const Float32 t = std::clamp(static_cast<Float32>(node.pagerank_gravity * 2.2f), 0.0f, 1.0f);
                const int r = static_cast<int>(30.0f + t * 205.0f);
                const int g = static_cast<int>(70.0f + (1.0f - std::abs(t - 0.5f) * 2.0f) * 150.0f);
                const int b = static_cast<int>(200.0f - t * 160.0f);
                node.color = IM_COL32(r, g, b, 240);
                break;
            }
        }
    }
}

void GraphCanvas::fit_to_screen(Float32 width, Float32 height) {
    if (nodes_.empty()) return;

    Float32 min_x = 1e9f, max_x = -1e9f;
    Float32 min_y = 1e9f, max_y = -1e9f;

    for (const auto& n : nodes_) {
        min_x = std::min(min_x, n.x - n.radius);
        max_x = std::max(max_x, n.x + n.radius);
        min_y = std::min(min_y, n.y - n.radius);
        max_y = std::max(max_y, n.y + n.radius);
    }

    const Float32 graph_w = std::max(100.0f, max_x - min_x);
    const Float32 graph_h = std::max(100.0f, max_y - min_y);

    const Float32 scale_x = (width - 120.0f) / graph_w;
    const Float32 scale_y = (height - 120.0f) / graph_h;
    zoom_ = std::clamp(std::min(scale_x, scale_y), 0.4f, 1.8f);

    const Float32 center_x = (min_x + max_x) * 0.5f;
    const Float32 center_y = (min_y + max_y) * 0.5f;

    pan_offset_.x = (width * 0.5f) - (center_x * zoom_);
    pan_offset_.y = (height * 0.5f) - (center_y * zoom_);
}

void GraphCanvas::update_physics(Float32 dt, Float32 width, Float32 height) {
    if (!physics_enabled_ || physics_paused_ || nodes_.empty() || temperature_ < 0.2f) {
        return;
    }

    const Size n = nodes_.size();
    const Float32 area = std::max(width * height, 120000.0f);
    const Float32 k = std::sqrt(area / static_cast<Float32>(n)) * 0.85f * repulsion_force_;
    const Float32 k2 = k * k;

    std::vector<ImVec2> disp(n, ImVec2(0.0f, 0.0f));

    // 1. Fuerzas de repulsión electrostática (Coulomb)
    for (Size i = 0; i < n; ++i) {
        for (Size j = i + 1; j < n; ++j) {
            Float32 dx = nodes_[i].x - nodes_[j].x;
            Float32 dy = nodes_[i].y - nodes_[j].y;
            Float32 d = std::sqrt(dx * dx + dy * dy);
            if (d < 1.0f) {
                dx = 1.0f;
                dy = 0.0f;
                d = 1.0f;
            }

            const Float32 force = k2 / d;
            const Float32 fx = (dx / d) * force;
            const Float32 fy = (dy / d) * force;

            disp[i].x += fx;
            disp[i].y += fy;
            disp[j].x -= fx;
            disp[j].y -= fy;
        }
    }

    // 2. Fuerzas de atracción por resortes elásticos (Hooke)
    for (const auto& edge : edges_) {
        const Size u = edge.source_idx;
        const Size v = edge.target_idx;
        if (u >= n || v >= n) continue;

        Float32 dx = nodes_[v].x - nodes_[u].x;
        Float32 dy = nodes_[v].y - nodes_[u].y;
        Float32 d = std::sqrt(dx * dx + dy * dy);
        if (d < 1.0f) d = 1.0f;

        const Float32 force = (d * d) / k;
        const Float32 fx = (dx / d) * force;
        const Float32 fy = (dy / d) * force;

        disp[u].x += fx;
        disp[u].y += fy;
        disp[v].x -= fx;
        disp[v].y -= fy;
    }

    // 3. Aplicar desplazamientos amortiguados por la temperatura
    for (Size i = 0; i < n; ++i) {
        if (nodes_[i].is_dragging) continue;

        const Float32 d_len = std::sqrt(disp[i].x * disp[i].x + disp[i].y * disp[i].y);
        if (d_len > 0.0f) {
            const Float32 step = std::min(d_len, temperature_);
            nodes_[i].x += (disp[i].x / d_len) * step * dt * 4.0f * physics_speed_;
            nodes_[i].y += (disp[i].y / d_len) * step * dt * 4.0f * physics_speed_;
        }
    }

    // Enfriamiento gradual
    temperature_ = std::max(0.1f, temperature_ * 0.985f);
}

void GraphCanvas::render(const char* title, Float32 width, Float32 height) {
    if (!ImGui::Begin(title)) {
        ImGui::End();
        return;
    }

    // ==========================================
    // BARRA DE HERRAMIENTAS SUPERIOR DEL CANVAS
    // ==========================================
    auto same_line_if_fits = [](float needed_w) {
        if (ImGui::GetContentRegionAvail().x >= needed_w + ImGui::GetStyle().ItemSpacing.x) {
            ImGui::SameLine();
        }
    };

    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputTextWithHint("##filter_graph", "Buscar tabla/rol...", search_filter_, sizeof(search_filter_));

    // Selector de modo de coloración
    same_line_if_fits(160.0f);
    ImGui::SetNextItemWidth(160.0f);
    const char* color_mode_items[] = { "Cluster Espectral", "Rol Ontologico", "Gravedad PageRank" };
    int current_mode_idx = static_cast<int>(color_mode_);
    if (ImGui::Combo("##color_mode", &current_mode_idx, color_mode_items, IM_ARRAYSIZE(color_mode_items))) {
        color_mode_ = static_cast<NodeColorMode>(current_mode_idx);
        update_node_colors();
    }

    // Controles de física y cámara
    const float phys_btn_w = ImGui::CalcTextSize("Reanudar Fisica").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    same_line_if_fits(phys_btn_w);
    if (ImGui::Button(physics_paused_ ? "Reanudar Fisica" : "Pausar Fisica")) {
        physics_paused_ = !physics_paused_;
    }

    const float fit_btn_w = ImGui::CalcTextSize("Centrar Vista").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    same_line_if_fits(fit_btn_w);
    if (ImGui::Button("Centrar Vista")) {
        const ImVec2 sz = ImGui::GetContentRegionAvail();
        fit_to_screen(sz.x, sz.y);
    }

    const float reorg_btn_w = ImGui::CalcTextSize("Reorganizar").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    same_line_if_fits(reorg_btn_w);
    if (ImGui::Button("Reorganizar")) {
        std::mt19937 rng(1337);
        std::uniform_real_distribution<Float32> dist(-60.0f, 60.0f);
        for (auto& n : nodes_) {
            n.x += dist(rng);
            n.y += dist(rng);
        }
        temperature_ = 80.0f;
    }

    const float chk_w = ImGui::CalcTextSize("Ver Nombres Aristas").x + ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x * 2.0f;
    same_line_if_fits(chk_w);
    ImGui::Checkbox("Ver Nombres Aristas", &show_edge_labels_);

    ImGui::Separator();

    // ==========================================
    // LIENZO DE DIBUJO CON PAN & ZOOM
    // ==========================================
    const ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    if (canvas_sz.x < 50.0f) canvas_sz.x = width;
    if (canvas_sz.y < 50.0f) canvas_sz.y = height;
    const ImVec2 canvas_p1(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(18, 20, 26, 255));
    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(45, 50, 62, 255));

    // Dibujar cuadrícula de fondo sutil con coordenadas de cámara
    const Float32 grid_step = 64.0f * zoom_;
    const Float32 grid_offset_x = std::fmod(pan_offset_.x, grid_step);
    const Float32 grid_offset_y = std::fmod(pan_offset_.y, grid_step);

    for (Float32 x = grid_offset_x; x < canvas_sz.x; x += grid_step) {
        draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y), ImVec2(canvas_p0.x + x, canvas_p1.y), IM_COL32(28, 32, 42, 120));
    }
    for (Float32 y = grid_offset_y; y < canvas_sz.y; y += grid_step) {
        draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y), ImVec2(canvas_p1.x, canvas_p0.y + y), IM_COL32(28, 32, 42, 120));
    }

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mouse_pos = io.MousePos;
    const bool is_hovered = (mouse_pos.x >= canvas_p0.x && mouse_pos.x <= canvas_p1.x &&
                             mouse_pos.y >= canvas_p0.y && mouse_pos.y <= canvas_p1.y);

    // Funciones de transformación 2D
    auto world_to_screen = [&](Float32 wx, Float32 wy) -> ImVec2 {
        return ImVec2(canvas_p0.x + pan_offset_.x + wx * zoom_,
                      canvas_p0.y + pan_offset_.y + wy * zoom_);
    };

    auto screen_to_world = [&](ImVec2 sp) -> ImVec2 {
        return ImVec2((sp.x - canvas_p0.x - pan_offset_.x) / zoom_,
                      (sp.y - canvas_p0.y - pan_offset_.y) / zoom_);
    };

    // Gestión de Pan (arrastrar con botón derecho o rueda central)
    if (is_hovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle))) {
        pan_offset_.x += io.MouseDelta.x;
        pan_offset_.y += io.MouseDelta.y;
    }

    // Gestión de Zoom centrado en cursor
    if (is_hovered && io.MouseWheel != 0.0f) {
        const ImVec2 mouse_world = screen_to_world(mouse_pos);
        zoom_ = std::clamp(zoom_ + io.MouseWheel * 0.12f * zoom_, 0.25f, 3.5f);
        pan_offset_.x = (mouse_pos.x - canvas_p0.x) - mouse_world.x * zoom_;
        pan_offset_.y = (mouse_pos.y - canvas_p0.y) - mouse_world.y * zoom_;
    }

    // Actualizar simulación física
    update_physics(io.DeltaTime, canvas_sz.x / zoom_, canvas_sz.y / zoom_);

    // Preparar filtro de búsqueda
    std::string filter_str = search_filter_;
    std::transform(filter_str.begin(), filter_str.end(), filter_str.begin(), ::tolower);

    // ==========================================
    // 1. DIBUJAR ARISTAS DIRIGIDAS (CURVAS BÉZIER)
    // ==========================================
    for (const auto& edge : edges_) {
        if (edge.source_idx >= nodes_.size() || edge.target_idx >= nodes_.size()) continue;
        const auto& n1 = nodes_[edge.source_idx];
        const auto& n2 = nodes_[edge.target_idx];

        const ImVec2 p1 = world_to_screen(n1.x, n1.y);
        const ImVec2 p2 = world_to_screen(n2.x, n2.y);

        const ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
        const ImVec2 cp1(mid.x - (p2.y - p1.y) * 0.15f, mid.y + (p2.x - p1.x) * 0.15f);

        const Float32 edge_thickness = std::clamp(2.0f * zoom_, 1.5f, 4.0f);
        draw_list->AddBezierQuadratic(p1, cp1, p2, IM_COL32(140, 165, 205, 180), edge_thickness);

        // Flecha direccional
        const Float32 dx = p2.x - cp1.x;
        const Float32 dy = p2.y - cp1.y;
        const Float32 len = std::sqrt(dx * dx + dy * dy);
        const Float32 target_radius = n2.radius * zoom_;

        if (len > 1.0f) {
            const ImVec2 dir(dx / len, dy / len);
            const ImVec2 tip(p2.x - dir.x * target_radius, p2.y - dir.y * target_radius);
            const Float32 arrow_sz = 10.0f * zoom_;
            const ImVec2 left(tip.x - dir.x * arrow_sz - dir.y * (arrow_sz * 0.5f), tip.y - dir.y * arrow_sz + dir.x * (arrow_sz * 0.5f));
            const ImVec2 right(tip.x - dir.x * arrow_sz + dir.y * (arrow_sz * 0.5f), tip.y - dir.y * arrow_sz - dir.x * (arrow_sz * 0.5f));
            draw_list->AddTriangleFilled(tip, left, right, IM_COL32(210, 225, 245, 230));
        }

        // Etiquetas opcionales sobre las aristas
        if (show_edge_labels_) {
            const std::string label = edge.source_col + " -> " + edge.target_col;
            draw_list->AddText(ImVec2(cp1.x - 20, cp1.y - 10), IM_COL32(200, 215, 235, 200), label.c_str());
        }

        // Tooltip al pasar cursor cerca del punto de control de la arista
        const Float32 dist_edge = std::sqrt((mouse_pos.x - cp1.x) * (mouse_pos.x - cp1.x) +
                                            (mouse_pos.y - cp1.y) * (mouse_pos.y - cp1.y));
        if (is_hovered && dist_edge < 16.0f) {
            ImGui::BeginTooltip();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Relacion Foranea (FK):");
            ImGui::Text("%s.%s -> %s.%s", edge.source_tbl.c_str(), edge.source_col.c_str(), edge.target_tbl.c_str(), edge.target_col.c_str());
            ImGui::Separator();
            ImGui::Text("Inclusividad: %.1f%% | Confianza: %.1f%%", edge.inclusion_ratio * 100.0, edge.confidence * 100.0);
            ImGui::EndTooltip();
        }
    }

    // ==========================================
    // 2. DIBUJAR NODOS E INTERACCIÓN
    // ==========================================
    for (auto& node : nodes_) {
        const ImVec2 center = world_to_screen(node.x, node.y);
        const Float32 radius = node.radius * zoom_;

        const Float32 dist_sq = (mouse_pos.x - center.x) * (mouse_pos.x - center.x) +
                                (mouse_pos.y - center.y) * (mouse_pos.y - center.y);
        const bool hovered = is_hovered && (dist_sq <= radius * radius);

        // Comprobar coincidencia con filtro de búsqueda
        bool matches_search = true;
        if (!filter_str.empty()) {
            std::string lower_name = node.name;
            std::string lower_role = node.inferred_role;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            std::transform(lower_role.begin(), lower_role.end(), lower_role.begin(), ::tolower);
            matches_search = (lower_name.find(filter_str) != std::string::npos ||
                              lower_role.find(filter_str) != std::string::npos);
        }

        // Interacción con clic para arrastrar o seleccionar
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            node.is_dragging = true;
            selected_node_ = &node;
            temperature_ = 25.0f;
        }

        if (node.is_dragging) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                const ImVec2 new_wpos = screen_to_world(mouse_pos);
                node.x = new_wpos.x;
                node.y = new_wpos.y;
            } else {
                node.is_dragging = false;
            }
        }

        // Opacidad según filtro de búsqueda
        ImU32 fill_color = node.color;
        if (!matches_search) {
            // Atenuar opacidad
            fill_color = (fill_color & 0x00FFFFFF) | (0x35 << 24);
        }

        // Dibujar círculo del nodo
        draw_list->AddCircleFilled(center, radius, fill_color, 36);

        // Borde resaltado
        if (&node == selected_node_) {
            draw_list->AddCircle(center, radius + 4.0f * zoom_, IM_COL32(255, 230, 80, 255), 36, 3.5f * zoom_);
        } else if (hovered) {
            draw_list->AddCircle(center, radius + 2.5f * zoom_, IM_COL32(255, 255, 255, 230), 36, 2.5f * zoom_);
        } else {
            draw_list->AddCircle(center, radius, IM_COL32(25, 30, 42, 220), 36, 1.5f * zoom_);
        }

        // Etiqueta del nombre del nodo centrada
        if (zoom_ > 0.45f) {
            const ImVec2 text_sz = ImGui::CalcTextSize(node.name.c_str());
            const ImVec2 text_pos(center.x - text_sz.x * 0.5f, center.y - text_sz.y * 0.5f);
            draw_list->AddText(text_pos, matches_search ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 180, 180, 120), node.name.c_str());
        }

        // Tooltip enriquecido al posar cursor
        if (hovered && !node.is_dragging) {
            ImGui::BeginTooltip();
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.5f, 1.0f), "%s", node.name.c_str());
            if (node.name != node.original_name) {
                ImGui::TextDisabled("Original: %s", node.original_name.c_str());
            }
            ImGui::Separator();
            ImGui::Text("Rol Inferido: %s", node.inferred_role.c_str());
            ImGui::Text("Columnas: %zu", node.column_count);
            ImGui::Text("Gravedad PageRank: %.4f", node.pagerank_gravity);
            ImGui::Text("Cluster Espectral: C%d", node.spectral_cluster);
            if (!node.business_purpose.empty()) {
                ImGui::Separator();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
                ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.90f, 1.0f), "%s", node.business_purpose.c_str());
                ImGui::PopTextWrapPos();
            }
            ImGui::Separator();
            ImGui::TextDisabled("Clic izquierdo para abrir Ficha de Negocio y Metricas.");
            ImGui::EndTooltip();
        }
    }

    ImGui::End();
}

} // namespace intelidata::gui
