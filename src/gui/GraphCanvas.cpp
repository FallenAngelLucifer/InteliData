#include "intelidata/gui/GraphCanvas.hpp"
#include <cmath>
#include <algorithm>
#include <random>

namespace intelidata::gui {

GraphCanvas::GraphCanvas() = default;

void GraphCanvas::load_from_ast(const model::SemanticAST& ast) {
    nodes_.clear();
    edges_.clear();
    selected_node_ = nullptr;
    temperature_ = 100.0f;

    std::unordered_map<std::string, Size> name_to_idx;

    std::mt19937 rng(42);
    std::uniform_real_distribution<Float32> dist(-150.0f, 150.0f);

    const Size total = ast.tables.size();
    for (Size i = 0; i < total; ++i) {
        const auto& tbl = ast.tables[i];
        NodeUI node;
        node.id = i;
        node.name = tbl.recommended_name.empty() ? tbl.original_identifier : tbl.recommended_name;
        node.inferred_role = tbl.inferred_role;
        node.pagerank_gravity = tbl.pagerank_gravity;
        node.spectral_cluster = tbl.spectral_cluster;

        // Posicionamiento inicial en círculo con perturbación
        const Float32 angle = static_cast<Float32>(i) * (2.0f * 3.14159265f / static_cast<Float32>(std::max<Size>(1, total)));
        node.x = 400.0f + 250.0f * std::cos(angle) + dist(rng);
        node.y = 300.0f + 250.0f * std::sin(angle) + dist(rng);

        // Radio proporcional a la gravedad de PageRank
        node.radius = 20.0f + static_cast<Float32>(node.pagerank_gravity * 60.0f);

        // Color según cluster espectral
        if (node.spectral_cluster == 0) {
            node.color = IM_COL32(40, 160, 220, 240); // Azul cian
        } else if (node.spectral_cluster == 1) {
            node.color = IM_COL32(230, 90, 70, 240);  // Naranja coral
        } else {
            node.color = IM_COL32(140, 100, 210, 240); // Púrpura
        }

        name_to_idx[tbl.original_identifier] = i;
        nodes_.push_back(std::move(node));
    }

    for (const auto& rel : ast.relationships) {
        const auto s_it = name_to_idx.find(rel.source_table);
        const auto t_it = name_to_idx.find(rel.target_table);

        if (s_it != name_to_idx.end() && t_it != name_to_idx.end()) {
            EdgeUI edge;
            edge.source_idx = s_it->second;
            edge.target_idx = t_it->second;
            edge.source_col = rel.source_column;
            edge.target_col = rel.target_column;
            edge.confidence = rel.confidence;
            edges_.push_back(std::move(edge));
        }
    }
}

void GraphCanvas::update_physics(Float32 dt, Float32 width, Float32 height) {
    if (!physics_enabled_ || nodes_.empty() || temperature_ < 0.2f) {
        return;
    }

    const Size n = nodes_.size();
    const Float32 area = std::max(width * height, 100000.0f);
    const Float32 k = std::sqrt(area / static_cast<Float32>(n)) * 0.75f;
    const Float32 k2 = k * k;

    std::vector<ImVec2> disp(n, ImVec2(0.0f, 0.0f));

    // 1. Fuerzas de repulsión entre todos los pares de nodos: Fr = k^2 / d
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

    // 2. Fuerzas de atracción a lo largo de las aristas: Fa = d^2 / k
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

    // 3. Aplicar desplazamientos amortiguados por la temperatura cinética
    const Float32 margin = 50.0f;
    for (Size i = 0; i < n; ++i) {
        if (nodes_[i].is_dragging) continue;

        Float32 d_len = std::sqrt(disp[i].x * disp[i].x + disp[i].y * disp[i].y);
        if (d_len > 0.0f) {
            const Float32 step = std::min(d_len, temperature_);
            nodes_[i].x += (disp[i].x / d_len) * step * dt * 5.0f;
            nodes_[i].y += (disp[i].y / d_len) * step * dt * 5.0f;
        }

        // Confinar a los límites del canvas
        nodes_[i].x = std::clamp(nodes_[i].x, margin, width - margin);
        nodes_[i].y = std::clamp(nodes_[i].y, margin, height - margin);
    }

    // Enfriamiento gradual
    temperature_ = std::max(0.1f, temperature_ * 0.985f);
}

void GraphCanvas::render(const char* title, Float32 width, Float32 height) {
    ImGui::Begin(title);

    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    if (canvas_sz.x < 50.0f) canvas_sz.x = width;
    if (canvas_sz.y < 50.0f) canvas_sz.y = height;
    ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(22, 24, 30, 255));
    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(50, 55, 65, 255));

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mouse_pos = io.MousePos;

    // Actualizar física en cada frame
    update_physics(io.DeltaTime, canvas_sz.x, canvas_sz.y);

    // 1. Dibujar Aristas Dirigidas con Curvas de Bézier
    for (const auto& edge : edges_) {
        if (edge.source_idx >= nodes_.size() || edge.target_idx >= nodes_.size()) continue;
        const auto& n1 = nodes_[edge.source_idx];
        const auto& n2 = nodes_[edge.target_idx];

        const ImVec2 p1(canvas_p0.x + n1.x, canvas_p0.y + n1.y);
        const ImVec2 p2(canvas_p0.x + n2.x, canvas_p0.y + n2.y);

        const ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
        const ImVec2 cp1(mid.x - (p2.y - p1.y) * 0.15f, mid.y + (p2.x - p1.x) * 0.15f);

        draw_list->AddBezierQuadratic(p1, cp1, p2, IM_COL32(160, 180, 210, 180), 2.0f);

        // Flecha direccional
        const Float32 dx = p2.x - cp1.x;
        const Float32 dy = p2.y - cp1.y;
        const Float32 len = std::sqrt(dx * dx + dy * dy);
        if (len > 1.0f) {
            const ImVec2 dir(dx / len, dy / len);
            const ImVec2 tip(p2.x - dir.x * n2.radius, p2.y - dir.y * n2.radius);
            const ImVec2 left(tip.x - dir.x * 12.0f - dir.y * 6.0f, tip.y - dir.y * 12.0f + dir.x * 6.0f);
            const ImVec2 right(tip.x - dir.x * 12.0f + dir.y * 6.0f, tip.y - dir.y * 12.0f - dir.x * 6.0f);
            draw_list->AddTriangleFilled(tip, left, right, IM_COL32(220, 230, 245, 220));
        }
    }

    // 2. Dibujar Nodos e Interacción del Ratón
    for (auto& node : nodes_) {
        const ImVec2 center(canvas_p0.x + node.x, canvas_p0.y + node.y);
        const Float32 dist_sq = (mouse_pos.x - center.x) * (mouse_pos.x - center.x) +
                                (mouse_pos.y - center.y) * (mouse_pos.y - center.y);
        const bool hovered = (dist_sq <= node.radius * node.radius);

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            node.is_dragging = true;
            selected_node_ = &node;
            temperature_ = 30.0f; // Reactivar física levemente al mover
        }

        if (node.is_dragging) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                node.x = mouse_pos.x - canvas_p0.x;
                node.y = mouse_pos.y - canvas_p0.y;
            } else {
                node.is_dragging = false;
            }
        }

        // Círculo del nodo
        draw_list->AddCircleFilled(center, node.radius, node.color, 32);

        // Borde resaltado si está seleccionado o en hover
        if (&node == selected_node_) {
            draw_list->AddCircle(center, node.radius + 4.0f, IM_COL32(255, 255, 100, 255), 32, 3.0f);
        } else if (hovered) {
            draw_list->AddCircle(center, node.radius + 2.0f, IM_COL32(255, 255, 255, 200), 32, 2.0f);
        } else {
            draw_list->AddCircle(center, node.radius, IM_COL32(20, 25, 35, 200), 32, 1.5f);
        }

        // Etiqueta del nodo centrada
        const ImVec2 text_sz = ImGui::CalcTextSize(node.name.c_str());
        const ImVec2 text_pos(center.x - text_sz.x * 0.5f, center.y - text_sz.y * 0.5f);
        draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), node.name.c_str());
    }

    ImGui::End();
}

} // namespace intelidata::gui
