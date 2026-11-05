#include <catch2/catch_test_macros.hpp>
#include "intelidata/ingestion/DuckDBAdapter.hpp"

using namespace intelidata::ingestion;

TEST_CASE("DuckDBAdapter: Operaciones en memoria y muestreo SQL", "[duckdb]") {
#if defined(INTELIDATA_HAS_DUCKDB)
    DuckDBAdapter adapter(":memory:");

    const bool created = adapter.execute(
        "CREATE TABLE productos (id_producto BIGINT, nombre VARCHAR, precio DOUBLE);"
    );
    REQUIRE(created);

    const bool inserted = adapter.execute(
        "INSERT INTO productos VALUES "
        "(101, 'Teclado Mecanico', 149.99), "
        "(102, 'Monitor 4K', 499.50), "
        "(103, 'Raton Optico', 45.00);"
    );
    REQUIRE(inserted);

    const auto tables = adapter.list_tables();
    REQUIRE(tables.size() == 1);
    REQUIRE(tables[0] == "productos");

    const auto schema = adapter.get_table_schema("productos");
    REQUIRE(schema.columns.size() == 3);
    REQUIRE(schema.find_column("id_producto") != nullptr);
    REQUIRE(schema.find_column("precio") != nullptr);

    const auto sampled_prices = adapter.sample_column("productos", "precio", 10);
    REQUIRE(sampled_prices.size() == 3);
#else
    SUCCEED("DuckDB no habilitado en este build, prueba omitida");
#endif
}
