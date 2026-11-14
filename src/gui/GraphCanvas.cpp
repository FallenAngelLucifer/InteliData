#include "intelidata/gui/GraphCanvas.hpp"
#include <cmath>
#include <algorithm>
#include <random>
#include <cctype>

namespace intelidata::gui {

static ImVec2 get_box_intersection(const ImVec2& box_min, const ImVec2& box_max, const ImVec2& target) {
    const ImVec2 center((box_min.x + box_max.x) * 0.5f, (box_min.y + box_max.y) * 0.5f);
    const Float32 dx = target.x - center.x;
    const Float32 dy = target.y - center.y;
    if (std::abs(dx) < 1e-4f && std::abs(dy) < 1e-4f) return center;

    const Float32 half_w = (box_max.x - box_min.x) * 0.5f;
    const Float32 half_h = (box_max.y - box_min.y) * 0.5f;

    const Float32 scale_x = half_w / std::max(std::abs(dx), 1e-4f);
    const Float32 scale_y = half_h / std::max(std::abs(dy), 1e-4f);
    const Float32 scale = std::min(scale_x, scale_y);

    return ImVec2(center.x + dx * scale, center.y + dy * scale);
}

GraphCanvas::GraphCanvas() = default;

void GraphCanvas::set_view_mode(GraphViewMode mode) noexcept {
    if (view_mode_ != mode) {
        view_mode_ = mode;
        if (view_mode_ == GraphViewMode::TableSchemaDSV) {
            arrange_schema_layout();
            if (last_canvas_sz_.x > 10.0f) {
                fit_to_screen(last_canvas_sz_.x, last_canvas_sz_.y);
            }
        }
    }
}

void GraphCanvas::arrange_schema_layout() {
    if (nodes_.empty()) return;

    struct Placement {
        Float32 col;
        Float32 row;
    };

    // Distribución canónica en 5 columnas espaciadas para el modelo Northwind
    // Col 0 (x=160): Geografía y Territorios
    // Col 1 (x=460): Personal, RRHH y Clientes
    // Col 2 (x=760): Hechos y Transacciones Centrales (Orders, OrderDetails)
    // Col 3 (x=1060): Logística, Envíos y Productos
    // Col 4 (x=1360): Proveedores y Categorías de Producto
    const std::unordered_map<std::string, Placement> canonical_layout = {
        {"region", {0.0f, 0.25f}},
        {"regions", {0.0f, 0.25f}},
        {"territories", {0.0f, 1.45f}},
        {"employeeterritories", {1.0f, 0.05f}},
        {"employees", {1.0f, 1.15f}},
        {"customers", {1.0f, 2.25f}},
        {"orders", {2.0f, 0.35f}},
        {"orderdetails", {2.0f, 1.65f}},
        {"order_details", {2.0f, 1.65f}},
        {"shippers", {3.0f, 0.35f}},
        {"products", {3.0f, 1.65f}},
        {"suppliers", {4.0f, 0.35f}},
        {"categories", {4.0f, 1.65f}}
    };

    std::vector<bool> placed(nodes_.size(), false);
    const Float32 origin_x = 150.0f;
    const Float32 origin_y = 120.0f;
    const Float32 col_spacing = 330.0f;
    const Float32 row_spacing = 280.0f;

    for (Size i = 0; i < nodes_.size(); ++i) {
        std::string lower = nodes_[i].original_name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        lower.erase(std::remove_if(lower.begin(), lower.end(), [](char c){ return c == ' ' || c == '_' || c == '-'; }), lower.end());

        auto it = canonical_layout.find(lower);
        if (it != canonical_layout.end()) {
            nodes_[i].x = origin_x + it->second.col * col_spacing;
            nodes_[i].y = origin_y + it->second.row * row_spacing;
            placed[i] = true;
        }
    }

    // Colocación adaptativa para cualquier tabla complementaria o dataset desconocido
    Float32 extra_y = origin_y + 2.5f * row_spacing;
    for (Size i = 0; i < nodes_.size(); ++i) {
        if (!placed[i]) {
            Float32 col = 2.0f;
            if (nodes_[i].inferred_role == "Fact_Hub_Transaction") col = 2.0f;
            else if (nodes_[i].inferred_role == "Dimension_Master_Entity") col = (i % 2 == 0) ? 1.0f : 3.0f;
            else if (nodes_[i].inferred_role == "Bridge_Lookup") col = (i % 2 == 0) ? 0.0f : 4.0f;
            else col = 2.0f;

            nodes_[i].x = origin_x + col * col_spacing;
            nodes_[i].y = extra_y;
            extra_y += 200.0f;
            placed[i] = true;
        }
    }

    temperature_ = 0.0f;
}

void GraphCanvas::load_from_ast(const model::SemanticAST& ast) {
    nodes_.clear();
    edges_.clear();
    selected_node_ = nullptr;
    temperature_ = 100.0f;
    pan_offset_ = ImVec2(0.0f, 0.0f);
    zoom_ = 1.0f;

    std::unordered_map<std::string, Size> name_to_idx;

    const Size total = ast.tables.size();
    for (Size i = 0; i < total; ++i) {
        const auto& tbl = ast.tables[i];
        if (tbl.original_identifier == "sqlite_sequence" ||
            tbl.original_identifier == "EmployeeTerritoriesView") {
            continue;
        }

        NodeUI node;
        node.id = nodes_.size();
        node.original_name = tbl.original_identifier;
        node.name = tbl.recommended_name.empty() ? tbl.original_identifier : tbl.recommended_name;
        node.inferred_role = tbl.inferred_role;
        node.pagerank_gravity = tbl.pagerank_gravity;
        node.spectral_cluster = tbl.spectral_cluster;
        node.column_count = tbl.columns.size();
        node.business_purpose = tbl.business_purpose;

        // Cargar columnas con determinación estricta de PK / FK
        for (const auto& col_ast : tbl.columns) {
            ColumnUI c;
            c.name = col_ast.original_identifier;
            c.type = col_ast.inferred_type.empty() ? "TEXT" : col_ast.inferred_type;

            std::string lower_col = c.name;
            std::transform(lower_col.begin(), lower_col.end(), lower_col.begin(), ::tolower);
            std::string lower_tbl = tbl.original_identifier;
            std::transform(lower_tbl.begin(), lower_tbl.end(), lower_tbl.begin(), ::tolower);

            const bool is_surrogate_pk = (col_ast.inferred_type == "Surrogate_Primary_Key" || col_ast.inferred_type == "Primary_Key");
            const bool is_id_naming = (lower_col == "id" || lower_col == lower_tbl + "id" || lower_col == lower_tbl + "_id");
            const bool is_known_composite = (lower_tbl == "orderdetails" && (lower_col == "orderid" || lower_col == "productid")) ||
                                           (lower_tbl == "employeeterritories" && (lower_col == "employeeid" || lower_col == "territoryid"));
            const bool is_known_pk = (lower_tbl == "orders" && lower_col == "orderid") ||
                                     (lower_tbl == "customers" && lower_col == "customerid") ||
                                     (lower_tbl == "products" && lower_col == "productid") ||
                                     (lower_tbl == "suppliers" && lower_col == "supplierid") ||
                                     (lower_tbl == "categories" && lower_col == "categoryid") ||
                                     (lower_tbl == "employees" && lower_col == "employeeid") ||
                                     (lower_tbl == "shippers" && lower_col == "shipperid") ||
                                     (lower_tbl == "region" && lower_col == "regionid") ||
                                     (lower_tbl == "regions" && lower_col == "regionid") ||
                                     (lower_tbl == "territories" && lower_col == "territoryid");

            if (is_surrogate_pk || is_id_naming || is_known_composite || is_known_pk) {
                c.is_pk = true;
            }
            node.columns.push_back(std::move(c));
        }

        const size_t visible_cols = std::min<size_t>(node.columns.size(), 8);
        node.card_width = 260.0f;
        node.card_height = 36.0f + static_cast<Float32>(visible_cols) * 20.0f + (node.columns.size() > 8 ? 22.0f : 10.0f);

        // Radio proporcional a la gravedad de PageRank para modo red
        node.radius = 24.0f + static_cast<Float32>(node.pagerank_gravity * 50.0f);

        name_to_idx[tbl.original_identifier] = nodes_.size();
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

            // Marcar FK en la tabla origen
            for (auto& c : nodes_[s_it->second].columns) {
                if (c.name == rel.source_column) {
                    c.is_fk = true;
                }
            }
            // Marcar PK en la tabla destino (si es columna identificadora)
            for (auto& c : nodes_[t_it->second].columns) {
                std::string lower_col = c.name;
                std::transform(lower_col.begin(), lower_col.end(), lower_col.begin(), ::tolower);
                if (c.name == rel.target_column && (lower_col.find("id") != std::string::npos || lower_col == "id")) {
                    c.is_pk = true;
                }
            }
        }
    }

    if (view_mode_ == GraphViewMode::TableSchemaDSV) {
        arrange_schema_layout();
    } else {
        std::mt19937 rng(42);
        std::uniform_real_distribution<Float32> dist(-60.0f, 60.0f);
        for (Size i = 0; i < total; ++i) {
            const Float32 angle = static_cast<Float32>(i) * (2.0f * 3.14159265f / static_cast<Float32>(std::max<Size>(1, total)));
            const Float32 radius_dist = 280.0f;
            nodes_[i].x = 480.0f + radius_dist * std::cos(angle) + dist(rng);
            nodes_[i].y = 360.0f + radius_dist * std::sin(angle) * 0.85f + dist(rng);
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
        const Float32 hw = (view_mode_ == GraphViewMode::TableSchemaDSV) ? (n.card_width * 0.5f) : n.radius;
        const Float32 hh = (view_mode_ == GraphViewMode::TableSchemaDSV) ? (n.card_height * 0.5f) : n.radius;
        min_x = std::min(min_x, n.x - hw);
        max_x = std::max(max_x, n.x + hw);
        min_y = std::min(min_y, n.y - hh);
        max_y = std::max(max_y, n.y + hh);
    }

    const Float32 graph_w = std::max(100.0f, max_x - min_x);
    const Float32 graph_h = std::max(100.0f, max_y - min_y);

    const Float32 scale_x = (width - 60.0f) / graph_w;
    const Float32 scale_y = (height - 60.0f) / graph_h;
    zoom_ = std::clamp(std::min(scale_x, scale_y), 0.55f, 1.35f);

    const Float32 center_x = (min_x + max_x) * 0.5f;
    const Float32 center_y = (min_y + max_y) * 0.5f;

    pan_offset_.x = (width * 0.5f) - (center_x * zoom_);
    pan_offset_.y = (height * 0.5f) - (center_y * zoom_);
    last_canvas_sz_ = ImVec2(width, height);
}

void GraphCanvas::update_physics(Float32 dt, Float32 width, Float32 height) {
    if (!physics_enabled_ || physics_paused_ || nodes_.empty()) {
        return;
    }

    const Size n = nodes_.size();

    // En modo Esquema de Tablas (DSV), mantenemos la estructura arquitectónica espaciosa.
    // Solo aplicamos separación elástica suave AABB si dos cajas colisionan.
    if (view_mode_ == GraphViewMode::TableSchemaDSV) {
        for (Size i = 0; i < n; ++i) {
            for (Size j = i + 1; j < n; ++j) {
                const Float32 min_dx = (nodes_[i].card_width + nodes_[j].card_width) * 0.5f + 45.0f;
                const Float32 min_dy = (nodes_[i].card_height + nodes_[j].card_height) * 0.5f + 25.0f;
                const Float32 dx = nodes_[i].x - nodes_[j].x;
                const Float32 dy = nodes_[i].y - nodes_[j].y;
                const Float32 overlap_x = min_dx - std::abs(dx);
                const Float32 overlap_y = min_dy - std::abs(dy);

                if (overlap_x > 0.0f && overlap_y > 0.0f) {
                    if (overlap_x < overlap_y) {
                        const Float32 sign = (dx >= 0.0f) ? 1.0f : -1.0f;
                        if (!nodes_[i].is_dragging) nodes_[i].x += sign * overlap_x * 0.4f;
                        if (!nodes_[j].is_dragging) nodes_[j].x -= sign * overlap_x * 0.4f;
                    } else {
                        const Float32 sign = (dy >= 0.0f) ? 1.0f : -1.0f;
                        if (!nodes_[i].is_dragging) nodes_[i].y += sign * overlap_y * 0.4f;
                        if (!nodes_[j].is_dragging) nodes_[j].y -= sign * overlap_y * 0.4f;
                    }
                }
            }
        }
        return;
    }

    if (temperature_ < 0.2f) {
        return;
    }

    // Normalizar a espacio de mundo constante para estabilidad invariante al zoom
    const Float32 world_area = 1100.0f * 850.0f;
    const Float32 k = std::clamp(std::sqrt(world_area / static_cast<Float32>(n)) * 0.85f * repulsion_force_,
                                 90.0f, 320.0f);
    const Float32 k2 = k * k;
    (void)width;
    (void)height;

    std::vector<ImVec2> disp(n, ImVec2(0.0f, 0.0f));

    // 1. Fuerzas de repulsión electrostática (Coulomb)
    for (Size i = 0; i < n; ++i) {
        for (Size j = i + 1; j < n; ++j) {
            Float32 dx = nodes_[i].x - nodes_[j].x;
            Float32 dy = nodes_[i].y - nodes_[j].y;
            Float32 d = std::sqrt(dx * dx + dy * dy);
            const Float32 min_allowed = 20.0f;
            if (d < min_allowed) {
                if (d < 1.0f) { dx = 1.0f; dy = 0.0f; d = 1.0f; }
                const Float32 overlap = (min_allowed - d) * 3.0f;
                disp[i].x += (dx / d) * overlap;
                disp[i].y += (dy / d) * overlap;
                disp[j].x -= (dx / d) * overlap;
                disp[j].y -= (dy / d) * overlap;
            }

            const Float32 force = k2 / std::max(d, 1.0f);
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
            nodes_[i].x += (disp[i].x / d_len) * step * dt * 3.5f * physics_speed_;
            nodes_[i].y += (disp[i].y / d_len) * step * dt * 3.5f * physics_speed_;
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

    ImGui::SetNextItemWidth(170.0f);
    ImGui::InputTextWithHint("##filter_graph", "Buscar tabla/rol...", search_filter_, sizeof(search_filter_));

    // Selector de modo de vista: Esquema Tablas (DSV) vs Grafo de Red (Fuerzas 2D)
    same_line_if_fits(210.0f);
    ImGui::SetNextItemWidth(190.0f);
    const char* view_modes[] = { "Esquema Tablas (DSV)", "Grafo de Red (Fuerzas 2D)" };
    int cur_vm = (view_mode_ == GraphViewMode::TableSchemaDSV) ? 0 : 1;
    if (ImGui::Combo("##view_mode", &cur_vm, view_modes, IM_ARRAYSIZE(view_modes))) {
        set_view_mode(cur_vm == 0 ? GraphViewMode::TableSchemaDSV : GraphViewMode::EntityNetwork);
    }

    // Selector de modo de coloración
    same_line_if_fits(160.0f);
    ImGui::SetNextItemWidth(150.0f);
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
        if (view_mode_ == GraphViewMode::TableSchemaDSV) {
            arrange_schema_layout();
            const ImVec2 sz = ImGui::GetContentRegionAvail();
            fit_to_screen(sz.x > 10.0f ? sz.x : last_canvas_sz_.x, sz.y > 10.0f ? sz.y : last_canvas_sz_.y);
        } else {
            std::mt19937 rng(1337);
            std::uniform_real_distribution<Float32> dist(-60.0f, 60.0f);
            for (auto& n : nodes_) {
                n.x += dist(rng);
                n.y += dist(rng);
            }
            temperature_ = 80.0f;
        }
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

    // Ajustar pan si la ventana cambió de tamaño o primera inicialización
    if (last_canvas_sz_.x <= 1.0f) {
        fit_to_screen(canvas_sz.x, canvas_sz.y);
    } else {
        const Float32 delta_w = canvas_sz.x - last_canvas_sz_.x;
        const Float32 delta_h = canvas_sz.y - last_canvas_sz_.y;
        if (std::abs(delta_w) > 0.5f || std::abs(delta_h) > 0.5f) {
            pan_offset_.x += delta_w * 0.5f;
            pan_offset_.y += delta_h * 0.5f;
        }
    }
    last_canvas_sz_ = canvas_sz;

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

    // Comprobar si algún nodo está bajo el cursor o siendo arrastrado
    bool any_node_hovered = false;
    bool any_node_dragging = false;
    for (const auto& node : nodes_) {
        if (node.is_dragging) any_node_dragging = true;
        if (view_mode_ == GraphViewMode::TableSchemaDSV) {
            const ImVec2 tl = world_to_screen(node.x - node.card_width * 0.5f, node.y - node.card_height * 0.5f);
            const ImVec2 br = ImVec2(tl.x + node.card_width * zoom_, tl.y + node.card_height * zoom_);
            if (mouse_pos.x >= tl.x && mouse_pos.x <= br.x && mouse_pos.y >= tl.y && mouse_pos.y <= br.y) {
                any_node_hovered = true;
            }
        } else {
            const ImVec2 center = world_to_screen(node.x, node.y);
            const Float32 radius = node.radius * zoom_;
            const Float32 dist_sq = (mouse_pos.x - center.x) * (mouse_pos.x - center.x) +
                                    (mouse_pos.y - center.y) * (mouse_pos.y - center.y);
            if (dist_sq <= radius * radius) {
                any_node_hovered = true;
            }
        }
    }

    // Gestión de Pan
    const bool can_pan_left = !any_node_hovered && !any_node_dragging;
    if (is_hovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Right) ||
                       ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
                       (can_pan_left && ImGui::IsMouseDragging(ImGuiMouseButton_Left)))) {
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
    update_physics(io.DeltaTime, 1000.0f, 800.0f);

    // Preparar filtro de búsqueda
    std::string filter_str = search_filter_;
    std::transform(filter_str.begin(), filter_str.end(), filter_str.begin(), ::tolower);

    // ==========================================
    // 1. DIBUJAR ARISTAS Y CONECTORES RELACIONALES
    // ==========================================
    for (const auto& edge : edges_) {
        if (edge.source_idx >= nodes_.size() || edge.target_idx >= nodes_.size()) continue;
        const auto& n1 = nodes_[edge.source_idx];
        const auto& n2 = nodes_[edge.target_idx];

        ImVec2 p1, p2, cp1, cp2;

        if (view_mode_ == GraphViewMode::TableSchemaDSV) {
            const Float32 header_h = 28.0f * zoom_;
            const Float32 row_h = 19.0f * zoom_;

            const ImVec2 b1_min = world_to_screen(n1.x - n1.card_width * 0.5f, n1.y - n1.card_height * 0.5f);
            const ImVec2 b1_max = ImVec2(b1_min.x + n1.card_width * zoom_, b1_min.y + n1.card_height * zoom_);

            const ImVec2 b2_min = world_to_screen(n2.x - n2.card_width * 0.5f, n2.y - n2.card_height * 0.5f);
            const ImVec2 b2_max = ImVec2(b2_min.x + n2.card_width * zoom_, b2_min.y + n2.card_height * zoom_);

            // Localizar fila de columna origen (FK)
            Float32 y1 = (b1_min.y + b1_max.y) * 0.5f;
            for (size_t c = 0; c < std::min<size_t>(n1.columns.size(), 8); ++c) {
                if (n1.columns[c].name == edge.source_col) {
                    y1 = b1_min.y + header_h + 4.0f * zoom_ + (static_cast<Float32>(c) + 0.5f) * row_h;
                    break;
                }
            }

            // Localizar fila de columna destino (PK)
            Float32 y2 = (b2_min.y + b2_max.y) * 0.5f;
            for (size_t c = 0; c < std::min<size_t>(n2.columns.size(), 8); ++c) {
                if (n2.columns[c].name == edge.target_col) {
                    y2 = b2_min.y + header_h + 4.0f * zoom_ + (static_cast<Float32>(c) + 0.5f) * row_h;
                    break;
                }
            }

            const bool source_is_left = (b1_max.x <= b2_min.x + 20.0f);
            const bool source_is_right = (b1_min.x >= b2_max.x - 20.0f);

            if (source_is_left) {
                p1 = ImVec2(b1_max.x, y1);
                p2 = ImVec2(b2_min.x, y2);
                const Float32 dx = std::max(40.0f * zoom_, (p2.x - p1.x) * 0.45f);
                cp1 = ImVec2(p1.x + dx, p1.y);
                cp2 = ImVec2(p2.x - dx, p2.y);
            } else if (source_is_right) {
                p1 = ImVec2(b1_min.x, y1);
                p2 = ImVec2(b2_max.x, y2);
                const Float32 dx = std::max(40.0f * zoom_, (p1.x - p2.x) * 0.45f);
                cp1 = ImVec2(p1.x - dx, p1.y);
                cp2 = ImVec2(p2.x + dx, p2.y);
            } else {
                const ImVec2 c2((b2_min.x + b2_max.x) * 0.5f, y2);
                const ImVec2 c1((b1_min.x + b1_max.x) * 0.5f, y1);
                p1 = get_box_intersection(b1_min, b1_max, c2);
                p2 = get_box_intersection(b2_min, b2_max, c1);
                const Float32 dy = (p2.y - p1.y) * 0.35f;
                cp1 = ImVec2(p1.x + 40.0f * zoom_, p1.y + dy);
                cp2 = ImVec2(p2.x + 40.0f * zoom_, p2.y - dy);
            }

            const Float32 edge_thickness = std::clamp(2.0f * zoom_, 1.5f, 3.5f);
            draw_list->AddBezierCubic(p1, cp1, cp2, p2, IM_COL32(85, 160, 235, 210), edge_thickness);

            // Pin conector en el origen (FK)
            draw_list->AddCircleFilled(p1, 3.5f * zoom_, IM_COL32(70, 205, 255, 255), 10);

            // Flecha en el destino (PK)
            const Float32 arr_sz = 7.0f * zoom_;
            const Float32 arr_dir = source_is_left ? 1.0f : (source_is_right ? -1.0f : 0.0f);
            if (std::abs(arr_dir) > 0.1f) {
                const ImVec2 tip = p2;
                const ImVec2 base1(tip.x - arr_dir * arr_sz, tip.y - arr_sz * 0.6f);
                const ImVec2 base2(tip.x - arr_dir * arr_sz, tip.y + arr_sz * 0.6f);
                draw_list->AddTriangleFilled(tip, base1, base2, IM_COL32(255, 215, 60, 255));
            } else {
                draw_list->AddCircleFilled(p2, 3.5f * zoom_, IM_COL32(255, 215, 60, 255), 10);
            }
        } else {
            const ImVec2 c1 = world_to_screen(n1.x, n1.y);
            const ImVec2 c2 = world_to_screen(n2.x, n2.y);
            p1 = c1;
            p2 = c2;
            const ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
            cp1 = ImVec2(mid.x - (p2.y - p1.y) * 0.15f, mid.y + (p2.x - p1.x) * 0.15f);

            const Float32 edge_thickness = std::clamp(2.0f * zoom_, 1.5f, 4.0f);
            draw_list->AddBezierQuadratic(p1, cp1, p2, IM_COL32(140, 165, 205, 180), edge_thickness);

            const Float32 dx = p2.x - cp1.x;
            const Float32 dy = p2.y - cp1.y;
            const Float32 len = std::sqrt(dx * dx + dy * dy);

            if (len > 1.0f) {
                const ImVec2 dir(dx / len, dy / len);
                const Float32 target_radius = n2.radius * zoom_;
                const ImVec2 tip = ImVec2(p2.x - dir.x * target_radius, p2.y - dir.y * target_radius);
                const Float32 arrow_sz = 9.0f * zoom_;
                const ImVec2 left(tip.x - dir.x * arrow_sz - dir.y * (arrow_sz * 0.45f), tip.y - dir.y * arrow_sz + dir.x * (arrow_sz * 0.45f));
                const ImVec2 right(tip.x - dir.x * arrow_sz + dir.y * (arrow_sz * 0.45f), tip.y - dir.y * arrow_sz - dir.x * (arrow_sz * 0.45f));
                draw_list->AddTriangleFilled(tip, left, right, IM_COL32(220, 235, 255, 240));
            }
        }

        // Etiquetas opcionales sobre las aristas
        if (show_edge_labels_) {
            const ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
            const std::string label = edge.source_col + " -> " + edge.target_col;
            draw_list->AddText(ImVec2(mid.x - 20, mid.y - 10), IM_COL32(200, 215, 235, 200), label.c_str());
        }

        // Tooltip al pasar cursor cerca del punto medio del conector
        const ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
        const Float32 dist_edge = std::sqrt((mouse_pos.x - mid.x) * (mouse_pos.x - mid.x) +
                                            (mouse_pos.y - mid.y) * (mouse_pos.y - mid.y));
        if (is_hovered && dist_edge < 16.0f) {
            ImGui::BeginTooltip();
            ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "Relacion Foranea (FK -> PK):");
            ImGui::Text("%s.%s -> %s.%s", edge.source_tbl.c_str(), edge.source_col.c_str(), edge.target_tbl.c_str(), edge.target_col.c_str());
            ImGui::Separator();
            ImGui::Text("Inclusividad: %.1f%% | Confianza: %.1f%%", edge.inclusion_ratio * 100.0, edge.confidence * 100.0);
            ImGui::EndTooltip();
        }
    }

    // ==========================================
    // 2. DIBUJAR NODOS / TABLAS E INTERACCIÓN
    // ==========================================
    for (auto& node : nodes_) {
        bool hovered = false;
        ImVec2 card_tl(0.0f, 0.0f);
        ImVec2 card_br(0.0f, 0.0f);
        ImVec2 center(0.0f, 0.0f);
        Float32 radius = 0.0f;

        if (view_mode_ == GraphViewMode::TableSchemaDSV) {
            card_tl = world_to_screen(node.x - node.card_width * 0.5f, node.y - node.card_height * 0.5f);
            card_br = ImVec2(card_tl.x + node.card_width * zoom_, card_tl.y + node.card_height * zoom_);
            hovered = is_hovered && (mouse_pos.x >= card_tl.x && mouse_pos.x <= card_br.x &&
                                     mouse_pos.y >= card_tl.y && mouse_pos.y <= card_br.y);
        } else {
            center = world_to_screen(node.x, node.y);
            radius = node.radius * zoom_;
            const Float32 dist_sq = (mouse_pos.x - center.x) * (mouse_pos.x - center.x) +
                                    (mouse_pos.y - center.y) * (mouse_pos.y - center.y);
            hovered = is_hovered && (dist_sq <= radius * radius);
        }

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

        const bool is_selected = (&node == selected_node_);

        if (view_mode_ == GraphViewMode::TableSchemaDSV) {
            // ===============================================
            // RENDERIZADO MODO TABLA RELACIONAL (ESTILO DSV)
            // ===============================================
            const Float32 rounding = 6.0f * zoom_;
            const Float32 header_h = 28.0f * zoom_;

            // Fondo del cuerpo de la tarjeta
            ImU32 body_bg = IM_COL32(22, 26, 36, matches_search ? 245 : 70);
            draw_list->AddRectFilled(card_tl, card_br, body_bg, rounding);

            // Fondo de la cabecera (color según rol o spectral)
            ImU32 header_bg = node.color;
            if (!matches_search) {
                header_bg = (header_bg & 0x00FFFFFF) | (0x35 << 24);
            }
            const ImVec2 hdr_br(card_br.x, card_tl.y + header_h);
            draw_list->AddRectFilled(card_tl, hdr_br, header_bg, rounding, ImDrawFlags_RoundCornersTop);

            // Borde de la tarjeta
            if (is_selected) {
                draw_list->AddRect(card_tl, card_br, IM_COL32(255, 215, 0, 255), rounding, 0, 3.0f * zoom_);
            } else if (hovered) {
                draw_list->AddRect(card_tl, card_br, IM_COL32(230, 240, 255, 230), rounding, 0, 2.0f * zoom_);
            } else {
                draw_list->AddRect(card_tl, card_br, IM_COL32(50, 60, 80, matches_search ? 200 : 60), rounding, 0, 1.5f * zoom_);
            }

            // Título de la tabla en cabecera
            if (zoom_ > 0.15f) {
                const ImVec2 title_pos(card_tl.x + 8.0f * zoom_, card_tl.y + 6.0f * zoom_);
                const std::string& display_title = node.original_name.empty() ? node.name : node.original_name;
                draw_list->AddText(title_pos, IM_COL32(255, 255, 255, 255), display_title.c_str());

                // Badge de rol a la derecha de la cabecera
                std::string role_tag = "[TBL]";
                if (node.inferred_role == "Fact_Hub_Transaction") role_tag = "[FACT]";
                else if (node.inferred_role == "Dimension_Master_Entity") role_tag = "[DIM]";
                else if (node.inferred_role == "Bridge_Lookup") role_tag = "[REL]";

                const ImVec2 tag_sz = ImGui::CalcTextSize(role_tag.c_str());
                const ImVec2 tag_pos(card_br.x - tag_sz.x - 8.0f * zoom_, card_tl.y + 6.0f * zoom_);
                draw_list->AddText(tag_pos, IM_COL32(240, 245, 255, 220), role_tag.c_str());
            }

            // Lista de columnas
            if (zoom_ > 0.20f) {
                const size_t visible_cols = std::min<size_t>(node.columns.size(), 8);
                const Float32 row_h = 19.0f * zoom_;
                Float32 cur_y = card_tl.y + header_h + 4.0f * zoom_;

                for (size_t c_idx = 0; c_idx < visible_cols; ++c_idx) {
                    const auto& col = node.columns[c_idx];

                    // Indicador PK / FK
                    Float32 badge_w = 0.0f;
                    if (col.is_pk && col.is_fk) {
                        draw_list->AddText(ImVec2(card_tl.x + 6.0f, cur_y), IM_COL32(255, 185, 40, 255), "[PK|FK]");
                        badge_w = ImGui::CalcTextSize("[PK|FK]").x + 6.0f;
                    } else if (col.is_pk) {
                        draw_list->AddText(ImVec2(card_tl.x + 6.0f, cur_y), IM_COL32(255, 215, 60, 255), "[PK]");
                        badge_w = ImGui::CalcTextSize("[PK]").x + 6.0f;
                    } else if (col.is_fk) {
                        draw_list->AddText(ImVec2(card_tl.x + 6.0f, cur_y), IM_COL32(70, 205, 255, 255), "[FK]");
                        badge_w = ImGui::CalcTextSize("[FK]").x + 6.0f;
                    } else {
                        draw_list->AddText(ImVec2(card_tl.x + 10.0f, cur_y), IM_COL32(110, 125, 145, 180), "•");
                        badge_w = ImGui::CalcTextSize("•").x + 10.0f;
                    }

                    // Nombre de columna
                    const ImVec2 col_name_pos(card_tl.x + 6.0f + badge_w, cur_y);
                    draw_list->AddText(col_name_pos, IM_COL32(230, 235, 245, 255), col.name.c_str());

                    // Tipo abreviado opcional solo si hay zoom amplio para no solaparse
                    if (zoom_ > 0.95f) {
                        std::string short_type = "TXT";
                        if (col.type.find("Key") != std::string::npos || col.type.find("INT") != std::string::npos || col.name.find("ID") != std::string::npos) short_type = "INT";
                        else if (col.type.find("Date") != std::string::npos || col.type.find("TIME") != std::string::npos) short_type = "DATE";
                        else if (col.type.find("Currency") != std::string::npos || col.type.find("Metric") != std::string::npos) short_type = "NUM";
                        const ImVec2 type_sz = ImGui::CalcTextSize(short_type.c_str());
                        const ImVec2 type_pos(card_br.x - type_sz.x - 8.0f * zoom_, cur_y);
                        draw_list->AddText(type_pos, IM_COL32(110, 130, 155, 170), short_type.c_str());
                    }

                    cur_y += row_h;
                }

                if (node.columns.size() > visible_cols) {
                    std::string extra_str = "+ " + std::to_string(node.columns.size() - visible_cols) + " cols...";
                    draw_list->AddText(ImVec2(card_tl.x + 20.0f * zoom_, cur_y), IM_COL32(150, 165, 185, 180), extra_str.c_str());
                }
            }
        } else {
            // ===============================================
            // RENDERIZADO MODO GRAFO DE RED (CIRCULAR 2D)
            // ===============================================
            ImU32 fill_color = node.color;
            if (!matches_search) {
                fill_color = (fill_color & 0x00FFFFFF) | (0x35 << 24);
            }

            draw_list->AddCircleFilled(center, radius, fill_color, 36);

            if (is_selected) {
                draw_list->AddCircle(center, radius + 4.0f * zoom_, IM_COL32(255, 230, 80, 255), 36, 3.5f * zoom_);
            } else if (hovered) {
                draw_list->AddCircle(center, radius + 2.5f * zoom_, IM_COL32(255, 255, 255, 230), 36, 2.5f * zoom_);
            } else {
                draw_list->AddCircle(center, radius, IM_COL32(25, 30, 42, 220), 36, 1.5f * zoom_);
            }

            if (zoom_ > 0.45f) {
                const ImVec2 text_sz = ImGui::CalcTextSize(node.name.c_str());
                const ImVec2 text_pos(center.x - text_sz.x * 0.5f, center.y - text_sz.y * 0.5f);
                draw_list->AddText(text_pos, matches_search ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 180, 180, 120), node.name.c_str());
            }
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
