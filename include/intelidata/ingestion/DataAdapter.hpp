#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>

namespace intelidata::ingestion {

struct ColumnSchema {
    std::string name;
    std::string data_type; // e.g. "VARCHAR", "BIGINT", "DOUBLE", "DATE"
    bool is_nullable{true};
};

struct TableSchema {
    std::string name;
    std::vector<ColumnSchema> columns;

    [[nodiscard]] const ColumnSchema* find_column(std::string_view col_name) const noexcept {
        for (const auto& col : columns) {
            if (col.name == col_name) {
                return &col;
            }
        }
        return nullptr;
    }
};

/**
 * @brief Interfaz abstracta para adaptadores de ingesta (DuckDB, Conexión Viva, Archivos en Memoria).
 */
class DataAdapter {
public:
    virtual ~DataAdapter() = default;

    // Retorna la lista de tablas disponibles en el origen
    [[nodiscard]] virtual std::vector<std::string> list_tables() = 0;

    // Retorna el esquema formal de una tabla
    [[nodiscard]] virtual TableSchema get_table_schema(std::string_view table_name) = 0;

    // Extrae una muestra de valores como cadenas de texto para una columna específica
    [[nodiscard]] virtual std::vector<std::string> sample_column(
        std::string_view table_name,
        std::string_view column_name,
        Size max_samples = 5000) = 0;
};

/**
 * @brief Adaptador ligero en memoria para pruebas unitarias y conjuntos de datos tabulares estándar.
 */
class MemoryDataAdapter : public DataAdapter {
public:
    MemoryDataAdapter() = default;

    void add_table(std::string table_name, std::vector<ColumnSchema> schema) {
        tables_[table_name] = TableSchema{table_name, std::move(schema)};
    }

    void add_column_data(std::string table_name, std::string column_name, std::vector<std::string> values) {
        data_[table_name][column_name] = std::move(values);
    }

    [[nodiscard]] std::vector<std::string> list_tables() override {
        std::vector<std::string> result;
        result.reserve(tables_.size());
        for (const auto& [name, _] : tables_) {
            result.push_back(name);
        }
        return result;
    }

    [[nodiscard]] TableSchema get_table_schema(std::string_view table_name) override {
        const auto it = tables_.find(std::string(table_name));
        if (it != tables_.end()) {
            return it->second;
        }
        return TableSchema{std::string(table_name), {}};
    }

    [[nodiscard]] std::vector<std::string> sample_column(
        std::string_view table_name,
        std::string_view column_name,
        Size max_samples) override {
        const auto t_it = data_.find(std::string(table_name));
        if (t_it != data_.end()) {
            const auto c_it = t_it->second.find(std::string(column_name));
            if (c_it != t_it->second.end()) {
                const auto& vec = c_it->second;
                if (vec.size() <= max_samples) {
                    return vec;
                }
                return std::vector<std::string>(vec.begin(), vec.begin() + max_samples);
            }
        }
        return {};
    }

private:
    std::unordered_map<std::string, TableSchema> tables_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>> data_;
};

} // namespace intelidata::ingestion
