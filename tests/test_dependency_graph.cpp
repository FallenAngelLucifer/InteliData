#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "intelidata/model/DependencyGraph.hpp"

using namespace intelidata;
using namespace intelidata::model;
using Catch::Matchers::WithinRel;
using Catch::Matchers::WithinAbs;

TEST_CASE("DependencyGraph: Construcción de nodos y búsqueda", "[graph]") {
    DependencyGraph graph;
    const Size id1 = graph.add_table("clientes");
    const Size id2 = graph.add_table("ordenes");

    REQUIRE(id1 == 0);
    REQUIRE(id2 == 1);
    REQUIRE(graph.table_count() == 2);

    ColumnMetadata col;
    col.name = "id_cliente";
    col.is_candidate_key = true;
    col.normalized_entropy = 1.0;
    graph.add_column("clientes", std::move(col));

    const auto* table = graph.find_table("clientes");
    REQUIRE(table != nullptr);
    REQUIRE(table->columns.size() == 1);
    REQUIRE(table->find_column("id_cliente") != nullptr);
    REQUIRE(table->find_column("no_existe") == nullptr);
}

TEST_CASE("DependencyGraph: PageRank estocástico (Gravedad de Entidades)", "[graph]") {
    DependencyGraph graph;

    // Estructura clásica relacional:
    // ordenes -> clientes (FK)
    // item_orden -> ordenes (FK)
    // item_orden -> productos (FK)
    // resenas -> productos (FK)

    graph.add_edge(RelationshipEdge{"ordenes", "id_cliente", "clientes", "id", 1.0, 1.0, 0.0, true});
    graph.add_edge(RelationshipEdge{"item_orden", "id_orden", "ordenes", "id", 1.0, 1.0, 0.0, true});
    graph.add_edge(RelationshipEdge{"item_orden", "id_producto", "productos", "id", 1.0, 1.0, 0.0, true});
    graph.add_edge(RelationshipEdge{"resenas", "id_prod", "productos", "id", 1.0, 1.0, 0.0, true});

    graph.compute_pagerank(0.85);

    const auto* clientes = graph.find_table("clientes");
    const auto* productos = graph.find_table("productos");
    const auto* ordenes = graph.find_table("ordenes");
    const auto* item_orden = graph.find_table("item_orden");
    const auto* resenas = graph.find_table("resenas");

    REQUIRE(clientes != nullptr);
    REQUIRE(productos != nullptr);
    REQUIRE(ordenes != nullptr);
    REQUIRE(item_orden != nullptr);
    REQUIRE(resenas != nullptr);

    // La suma total de probabilidades de PageRank debe ser exactamente 1.0
    Float64 total_gravity = 0.0;
    for (const auto& t : graph.tables()) {
        total_gravity += t.pagerank_gravity;
    }
    REQUIRE_THAT(total_gravity, WithinAbs(1.0, 1e-12));

    // 'productos' es referenciada por dos tablas clave (item_orden y resenas),
    // debe tener una gravedad relacional significativamente mayor que las hojas (item_orden, resenas)
    REQUIRE(productos->pagerank_gravity > item_orden->pagerank_gravity);
    REQUIRE(productos->pagerank_gravity > resenas->pagerank_gravity);
    REQUIRE(clientes->pagerank_gravity > item_orden->pagerank_gravity);
}
