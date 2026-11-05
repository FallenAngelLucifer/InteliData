#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include "intelidata/model/SemanticAST.hpp"
#include <vector>
#include <string>
#include <imgui.h>

namespace intelidata::gui {

struct NodeUI {
    Size id{0};
    std::string name;
    std::string inferred_role;
    Float32 x{0.0f};
    Float32 y{0.0f};
    Float32 vx{0.0f};
    Float32 vy{0.0f};
    Float32 radius{25.0f};
    ImU32 color{IM_COL32(50, 150, 250, 255)};
    Float64 pagerank_gravity{0.0};
    Int32 spectral_cluster{0};
    bool is_dragging{false};
    bool is_selected{false};
};

struct EdgeUI {
    Size source_idx{0};
    Size target_idx{0};
    std::string source_col;
    std::string target_col;
    Float64 confidence{0.0};
};

/**
 * @brief Canvas interactivo para el grafo ontológico con simulación física Fruchterman-Reingold
 * y renderizado de nodos y aristas dirigidas mediante curvas de Bézier cúbicas a 144 FPS.
 */
class GraphCanvas {
public:
    GraphCanvas();

    // Carga los datos del SemanticAST al canvas
    void load_from_ast(const model::SemanticAST& ast);

    // Ejecuta un paso de simulación de física de fuerzas Fruchterman-Reingold
    void update_physics(Float32 dt, Float32 width, Float32 height);

    // Renderiza el grafo interactivo sobre la ventana ImGui
    void render(const char* title, Float32 width, Float32 height);

    [[nodiscard]] const NodeUI* selected_node() const noexcept { return selected_node_; }
    void clear_selection() noexcept { selected_node_ = nullptr; }

private:
    std::vector<NodeUI> nodes_;
    std::vector<EdgeUI> edges_;
    const NodeUI* selected_node_{nullptr};
    Float32 temperature_{100.0f};
    bool physics_enabled_{true};
    ImVec2 pan_offset_{0.0f, 0.0f};
    Float32 zoom_{1.0f};
};

} // namespace intelidata::gui
