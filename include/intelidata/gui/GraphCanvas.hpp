#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include "intelidata/model/SemanticAST.hpp"
#include <vector>
#include <string>
#include <string_view>
#include <imgui.h>

namespace intelidata::gui {

enum class NodeColorMode {
    SpectralCluster,
    OntologicalRole,
    PageRankGravity
};

struct NodeUI {
    Size id{0};
    std::string name;
    std::string original_name;
    std::string inferred_role;
    Float32 x{0.0f};
    Float32 y{0.0f};
    Float32 vx{0.0f};
    Float32 vy{0.0f};
    Float32 radius{25.0f};
    ImU32 color{IM_COL32(50, 150, 250, 255)};
    Float64 pagerank_gravity{0.0};
    Int32 spectral_cluster{0};
    Size column_count{0};
    std::string business_purpose;
    bool is_dragging{false};
    bool is_selected{false};
};

struct EdgeUI {
    Size source_idx{0};
    Size target_idx{0};
    std::string source_tbl;
    std::string target_tbl;
    std::string source_col;
    std::string target_col;
    Float64 confidence{0.0};
    Float64 inclusion_ratio{0.0};
};

/**
 * @brief Canvas interactivo profesional para el grafo ontológico:
 * Incluye transformación de cámara 2D (Pan & Zoom), controles interactivos de física,
 * filtrado por búsqueda en tiempo real, múltiples modos de coloración y tooltips enriquecidos.
 */
class GraphCanvas {
public:
    GraphCanvas();

    // Carga los datos del SemanticAST al canvas
    void load_from_ast(const model::SemanticAST& ast);

    // Ejecuta un paso de simulación de física de fuerzas Fruchterman-Reingold
    void update_physics(Float32 dt, Float32 width, Float32 height);

    // Renderiza el canvas interactivo
    void render(const char* title, Float32 width, Float32 height);

    [[nodiscard]] const NodeUI* selected_node() const noexcept { return selected_node_; }
    void select_node_by_name(std::string_view name);
    void clear_selection() noexcept { selected_node_ = nullptr; }

    // Centra y ajusta la cámara para ver todos los nodos
    void fit_to_screen(Float32 width, Float32 height);

    // Acceso a la lista de nodos
    [[nodiscard]] const std::vector<NodeUI>& nodes() const noexcept { return nodes_; }
    [[nodiscard]] const std::vector<EdgeUI>& edges() const noexcept { return edges_; }

private:
    void update_node_colors();

    std::vector<NodeUI> nodes_;
    std::vector<EdgeUI> edges_;
    const NodeUI* selected_node_{nullptr};

    // Física
    Float32 temperature_{100.0f};
    bool physics_enabled_{true};
    bool physics_paused_{false};
    Float32 physics_speed_{1.0f};
    Float32 repulsion_force_{1.0f};

    // Cámara 2D (Pan & Zoom)
    ImVec2 pan_offset_{0.0f, 0.0f};
    Float32 zoom_{1.0f};
    bool is_panning_{false};

    // Filtros y visualización
    char search_filter_[128]{""};
    NodeColorMode color_mode_{NodeColorMode::SpectralCluster};
    bool show_edge_labels_{false};
};

} // namespace intelidata::gui
