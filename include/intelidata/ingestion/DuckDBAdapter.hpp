#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include "intelidata/ingestion/DataAdapter.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace intelidata::ingestion {

/**
 * @brief Adaptador universal basado en DuckDB para ingesta de Parquet, CSV, SQLite y bases en memoria.
 * Implementado bajo el patrón PIMPL con la C-API estable de DuckDB para garantizar
 * compatibilidad binaria total y cero conflictos de ABI C++ entre distribuciones Linux y Windows.
 */
class DuckDBAdapter : public DataAdapter {
public:
    explicit DuckDBAdapter(const std::string& db_path = ":memory:");
    ~DuckDBAdapter() override;

    DuckDBAdapter(const DuckDBAdapter&) = delete;
    DuckDBAdapter& operator=(const DuckDBAdapter&) = delete;
    DuckDBAdapter(DuckDBAdapter&&) noexcept;
    DuckDBAdapter& operator=(DuckDBAdapter&&) noexcept;

    // Ejecuta una consulta SQL arbitraria
    bool execute(const std::string& query);

    // Métodos de DataAdapter
    [[nodiscard]] std::vector<std::string> list_tables() override;
    [[nodiscard]] TableSchema get_table_schema(std::string_view table_name) override;
    [[nodiscard]] std::vector<std::string> sample_column(
        std::string_view table_name,
        std::string_view column_name,
        Size max_samples = 5000) override;

    // Carga de formatos analíticos
    bool load_parquet(const std::string& table_name, const std::string& parquet_path);
    bool load_csv(const std::string& table_name, const std::string& csv_path);
    bool attach_sqlite(const std::string& sqlite_path, const std::string& alias = "sqlite_db");

    [[nodiscard]] bool is_valid() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace intelidata::ingestion
