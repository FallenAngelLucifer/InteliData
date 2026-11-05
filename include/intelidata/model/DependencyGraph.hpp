#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <Eigen/Dense>

namespace intelidata::model {

struct ColumnMetadata {
    std::string name;
    std::string inferred_type;
    Float64 normalized_entropy{0.0};
    Float64 benford_p_value{0.0};
    bool is_candidate_key{false};
    bool is_financial{false};
    UInt64 distinct_count{0};
    UInt64 total_count{0};
};

struct RelationshipEdge {
    std::string source_table;
    std::string source_column;
    std::string target_table;
    std::string target_column;
    Float64 inclusion_ratio{0.0};
    Float64 confidence{0.0};
    Float64 nmi{0.0};
    bool is_foreign_key{false};
};

struct TableNode {
    Size id{0};
    std::string name;
    std::string inferred_role;
    Float64 pagerank_gravity{0.0};
    Int32 spectral_cluster{0};
    std::vector<ColumnMetadata> columns;

    [[nodiscard]] const ColumnMetadata* find_column(std::string_view col_name) const noexcept;
};

/**
 * @brief Multigrafo en memoria de dependencias relacionales, inclusiones y dependencias funcionales.
 */
class DependencyGraph {
public:
    DependencyGraph() = default;

    // Gestión de nodos
    Size add_table(std::string_view table_name);
    void add_column(std::string_view table_name, ColumnMetadata metadata);

    // Gestión de aristas
    void add_edge(RelationshipEdge edge);

    // Cálculo de PageRank relacional (Gravedad de Entidades en el esquema)
    void compute_pagerank(Float64 damping = 0.85, UInt32 max_iterations = 200, Float64 tol = 1e-8);

    // Acceso al grafo
    [[nodiscard]] Size table_count() const noexcept { return tables_.size(); }
    [[nodiscard]] Size edge_count() const noexcept { return edges_.size(); }
    [[nodiscard]] const std::vector<TableNode>& tables() const noexcept { return tables_; }
    [[nodiscard]] std::vector<TableNode>& tables() noexcept { return tables_; }
    [[nodiscard]] const std::vector<RelationshipEdge>& edges() const noexcept { return edges_; }
    [[nodiscard]] const TableNode* find_table(std::string_view table_name) const noexcept;
    [[nodiscard]] TableNode* find_table_mut(std::string_view table_name) noexcept;

    // Matriz de adyacencia dirigida de tablas (pesada por confianza de inclusión)
    [[nodiscard]] Eigen::MatrixXd build_adjacency_matrix() const;

    void clear() noexcept;

private:
    std::vector<TableNode> tables_;
    std::unordered_map<std::string, Size> table_name_to_index_;
    std::vector<RelationshipEdge> edges_;
};

} // namespace intelidata::model
