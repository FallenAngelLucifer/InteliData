#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "intelidata/pipeline/Engine.hpp"
#include "intelidata/pipeline/DDLGenerator.hpp"
#include "intelidata/ingestion/DataAdapter.hpp"
#include <string>
#include <vector>

using namespace intelidata;
using namespace intelidata::pipeline;
using namespace intelidata::ingestion;

TEST_CASE("Engine & DDLGenerator: Pipeline integral end-to-end de inferencia ontologica", "[engine]") {
    MemoryDataAdapter adapter;

    // 1. Tabla de Clientes
    adapter.add_table("t_cli", {
        {"c_id", "BIGINT", false},
        {"c_nom", "VARCHAR", true}
    });
    std::vector<std::string> cli_ids;
    std::vector<std::string> cli_noms;
    for (int i = 1; i <= 50; ++i) {
        cli_ids.push_back(std::to_string(i));
        cli_noms.push_back("Cliente registrado de prueba numero " + std::to_string(i));
    }
    adapter.add_column_data("t_cli", "c_id", cli_ids);
    adapter.add_column_data("t_cli", "c_nom", cli_noms);

    // 2. Tabla de Productos
    adapter.add_table("t_prod", {
        {"p_id", "BIGINT", false},
        {"p_pre", "DOUBLE", true}
    });
    std::vector<std::string> prod_ids;
    std::vector<std::string> prod_precios;
    double precio = 1.0;
    for (int i = 1; i <= 30; ++i) {
        prod_ids.push_back(std::to_string(i + 100));
        prod_precios.push_back(std::to_string(precio));
        precio *= 2.0;
        if (precio > 1e12) precio /= 1e12;
    }
    adapter.add_column_data("t_prod", "p_id", prod_ids);
    adapter.add_column_data("t_prod", "p_pre", prod_precios);

    // 3. Tabla Transaccional de Ordenes de Compra
    adapter.add_table("t_ord", {
        {"o_id", "BIGINT", false},
        {"fk_c", "BIGINT", true},
        {"fk_p", "BIGINT", true},
        {"monto", "DOUBLE", true},
        {"estado", "VARCHAR", true}
    });
    std::vector<std::string> ord_ids;
    std::vector<std::string> ord_fks_c;
    std::vector<std::string> ord_fks_p;
    std::vector<std::string> ord_montos;
    std::vector<std::string> ord_estados;

    double monto = 2.0;
    for (int i = 1; i <= 40; ++i) {
        ord_ids.push_back(std::to_string(i + 1000));
        ord_fks_c.push_back(std::to_string((i % 50) + 1));      // Incluido en t_cli.c_id
        ord_fks_p.push_back(std::to_string((i % 30) + 101));    // Incluido en t_prod.p_id
        ord_montos.push_back(std::to_string(monto));
        monto *= 2.0;
        if (monto > 1e12) monto /= 1e12;

        ord_estados.push_back(i % 3 == 0 ? "ENTREGADO" : (i % 5 == 0 ? "CANCELADO" : "EN_PROCESO"));
    }
    adapter.add_column_data("t_ord", "o_id", ord_ids);
    adapter.add_column_data("t_ord", "fk_c", ord_fks_c);
    adapter.add_column_data("t_ord", "fk_p", ord_fks_p);
    adapter.add_column_data("t_ord", "monto", ord_montos);
    adapter.add_column_data("t_ord", "estado", ord_estados);

    // Ejecutar Engine
    Engine engine;
    const auto ast = engine.analyze(adapter);

    REQUIRE(ast.tables.size() == 3);
    REQUIRE(ast.relationships.size() >= 2);

    // Verificar descubrimiento de claves foráneas
    bool found_fk_c = false;
    bool found_fk_p = false;
    for (const auto& rel : ast.relationships) {
        if (rel.source_table == "t_ord" && rel.source_column == "fk_c" && rel.target_table == "t_cli") {
            found_fk_c = true;
        }
        if (rel.source_table == "t_ord" && rel.source_column == "fk_p" && rel.target_table == "t_prod") {
            found_fk_p = true;
        }
    }
    REQUIRE(found_fk_c);
    REQUIRE(found_fk_p);

    // Generar SQL Views
    const std::string sql = DDLGenerator::generate_sql_views(ast);
    REQUIRE(sql.find("CREATE OR REPLACE VIEW") != std::string::npos);
    REQUIRE(sql.find("FROM \"t_ord\"") != std::string::npos);

    // Generar Data Dictionary Markdown
    const std::string doc = DDLGenerator::generate_data_dictionary_markdown(ast);
    REQUIRE(doc.find("# Diccionario de Datos y Reconstruccion Ontologica") != std::string::npos);
    REQUIRE(doc.find("Dominio Inferido") != std::string::npos);
}
