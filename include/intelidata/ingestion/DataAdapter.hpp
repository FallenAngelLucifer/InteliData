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

struct TablePreviewData {
    std::string table_name;
    std::vector<std::string> column_names;
    std::vector<std::vector<std::string>> rows;
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

    // Extrae una muestra de filas para previsualización tabular en la interfaz
    [[nodiscard]] virtual TablePreviewData fetch_table_preview(
        std::string_view table_name,
        [[maybe_unused]] Size max_rows = 50) {
        TablePreviewData preview{std::string(table_name), {}, {}};
        const auto schema = get_table_schema(table_name);
        for (const auto& col : schema.columns) {
            preview.column_names.push_back(col.name);
        }
        return preview;
    }
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

    [[nodiscard]] TablePreviewData fetch_table_preview(
        std::string_view table_name,
        Size max_rows = 50) override {
        TablePreviewData preview{std::string(table_name), {}, {}};
        const auto schema = get_table_schema(table_name);
        for (const auto& col : schema.columns) {
            preview.column_names.push_back(col.name);
        }
        if (preview.column_names.empty()) return preview;

        const auto t_it = data_.find(std::string(table_name));
        if (t_it != data_.end()) {
            Size num_rows = 0;
            for (const auto& col_name : preview.column_names) {
                const auto c_it = t_it->second.find(col_name);
                if (c_it != t_it->second.end()) {
                    num_rows = std::max(num_rows, c_it->second.size());
                }
            }
            num_rows = std::min(num_rows, max_rows);
            preview.rows.resize(num_rows);
            for (Size r = 0; r < num_rows; ++r) {
                preview.rows[r].resize(preview.column_names.size());
                for (Size c = 0; c < preview.column_names.size(); ++c) {
                    const auto c_it = t_it->second.find(preview.column_names[c]);
                    if (c_it != t_it->second.end() && r < c_it->second.size()) {
                        preview.rows[r][c] = c_it->second[r];
                    } else {
                        preview.rows[r][c] = "NULL";
                    }
                }
            }
        }
        return preview;
    }

private:
    std::unordered_map<std::string, TableSchema> tables_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>> data_;
};

} // namespace intelidata::ingestion
