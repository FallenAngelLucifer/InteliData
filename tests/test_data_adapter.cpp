#include <catch2/catch_test_macros.hpp>
#include "intelidata/ingestion/DataAdapter.hpp"

using namespace intelidata::ingestion;

TEST_CASE("MemoryDataAdapter: Registro de esquemas y muestreo de columnas", "[ingestion]") {
    MemoryDataAdapter adapter;

    std::vector<ColumnSchema> clientes_schema = {
        {"id", "BIGINT", false},
        {"nombre", "VARCHAR", true},
        {"saldo", "DOUBLE", true}
    };
    adapter.add_table("clientes", std::move(clientes_schema));

    adapter.add_column_data("clientes", "id", {"1", "2", "3", "4", "5"});
    adapter.add_column_data("clientes", "nombre", {"Alice", "Bob", "Charlie", "Diana", "Evan"});

    const auto tables = adapter.list_tables();
    REQUIRE(tables.size() == 1);
    REQUIRE(tables[0] == "clientes");

    const auto schema = adapter.get_table_schema("clientes");
    REQUIRE(schema.columns.size() == 3);
    REQUIRE(schema.find_column("saldo") != nullptr);
    REQUIRE(schema.find_column("inexistente") == nullptr);

    const auto id_sample = adapter.sample_column("clientes", "id", 3);
    REQUIRE(id_sample.size() == 3);
    REQUIRE(id_sample[0] == "1");
    REQUIRE(id_sample[1] == "2");
    REQUIRE(id_sample[2] == "3");
}
