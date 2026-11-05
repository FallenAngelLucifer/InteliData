#include "intelidata/model/DependencyGraph.hpp"
#include <cmath>
#include <algorithm>

namespace intelidata::model {

const ColumnMetadata* TableNode::find_column(std::string_view col_name) const noexcept {
    for (const auto& col : columns) {
        if (col.name == col_name) {
            return &col;
        }
    }
    return nullptr;
}

Size DependencyGraph::add_table(std::string_view table_name) {
    const std::string name_str(table_name);
    const auto it = table_name_to_index_.find(name_str);
    if (it != table_name_to_index_.end()) {
        return it->second;
    }

    const Size new_id = tables_.size();
    TableNode node;
    node.id = new_id;
    node.name = name_str;
    tables_.push_back(std::move(node));
    table_name_to_index_[name_str] = new_id;

    return new_id;
}

void DependencyGraph::add_column(std::string_view table_name, ColumnMetadata metadata) {
    const Size table_idx = add_table(table_name);
    tables_[table_idx].columns.push_back(std::move(metadata));
}

void DependencyGraph::add_edge(RelationshipEdge edge) {
    // Asegurar que las tablas existan en el grafo
    add_table(edge.source_table);
    add_table(edge.target_table);
    edges_.push_back(std::move(edge));
}

const TableNode* DependencyGraph::find_table(std::string_view table_name) const noexcept {
    const auto it = table_name_to_index_.find(std::string(table_name));
    if (it != table_name_to_index_.end()) {
        return &tables_[it->second];
    }
    return nullptr;
}

TableNode* DependencyGraph::find_table_mut(std::string_view table_name) noexcept {
    const auto it = table_name_to_index_.find(std::string(table_name));
    if (it != table_name_to_index_.end()) {
        return &tables_[it->second];
    }
    return nullptr;
}

Eigen::MatrixXd DependencyGraph::build_adjacency_matrix() const {
    const Size n = tables_.size();
    Eigen::MatrixXd adj = Eigen::MatrixXd::Zero(static_cast<Eigen::Index>(n), static_cast<Eigen::Index>(n));

    for (const auto& edge : edges_) {
        const auto src_it = table_name_to_index_.find(edge.source_table);
        const auto tgt_it = table_name_to_index_.find(edge.target_table);

        if (src_it != table_name_to_index_.end() && tgt_it != table_name_to_index_.end()) {
            const auto i = static_cast<Eigen::Index>(src_it->second);
            const auto j = static_cast<Eigen::Index>(tgt_it->second);
            if (i != j) { // Evitar auto-bucles para la matriz estocástica
                const Float64 weight = edge.confidence > 0.0 ? edge.confidence : (edge.inclusion_ratio > 0.0 ? edge.inclusion_ratio : 1.0);
                adj(i, j) += weight;
            }
        }
    }

    return adj;
}

void DependencyGraph::compute_pagerank(Float64 damping, UInt32 max_iterations, Float64 tol) {
    const Size n_size = tables_.size();
    if (n_size == 0) return;
    if (n_size == 1) {
        tables_[0].pagerank_gravity = 1.0;
        return;
    }

    const auto n = static_cast<Eigen::Index>(n_size);
    const Float64 n_f = static_cast<Float64>(n);
    const Eigen::MatrixXd adj = build_adjacency_matrix();

    // Construir la matriz estocástica de transición P (por filas)
    Eigen::MatrixXd p = Eigen::MatrixXd::Zero(n, n);
    for (Eigen::Index i = 0; i < n; ++i) {
        const Float64 row_sum = adj.row(i).sum();
        if (row_sum > 0.0) {
            p.row(i) = adj.row(i) / row_sum;
        } else {
            // Nodo sumidero (sin FK salientes): distribución uniforme
            p.row(i).setConstant(1.0 / n_f);
        }
    }

    // Inicializar vector de probabilidades estacionarias pi (1 x n)
    Eigen::RowVectorXd pi = Eigen::RowVectorXd::Constant(n, 1.0 / n_f);
    const Eigen::RowVectorXd teleport = Eigen::RowVectorXd::Constant(n, (1.0 - damping) / n_f);

    for (UInt32 iter = 0; iter < max_iterations; ++iter) {
        const Eigen::RowVectorXd next_pi = damping * (pi * p) + teleport;
        const Float64 diff = (next_pi - pi).cwiseAbs().sum();
        pi = next_pi;

        if (diff < tol) {
            break;
        }
    }

    // Normalizar suma exactamente a 1.0 y asignar a cada tabla
    const Float64 total_sum = pi.sum();
    for (Eigen::Index i = 0; i < n; ++i) {
        tables_[static_cast<Size>(i)].pagerank_gravity = (total_sum > 0.0) ? (pi(i) / total_sum) : (1.0 / n_f);
    }
}

void DependencyGraph::clear() noexcept {
    tables_.clear();
    table_name_to_index_.clear();
    edges_.clear();
}

} // namespace intelidata::model
