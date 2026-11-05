#include "intelidata/ingestion/DuckDBAdapter.hpp"

#if defined(INTELIDATA_HAS_DUCKDB)
#include <duckdb.h>
#include <filesystem>
#include <algorithm>

namespace intelidata::ingestion {

struct DuckDBAdapter::Impl {
    duckdb_database db{nullptr};
    duckdb_connection conn{nullptr};
    bool valid{false};

    explicit Impl(const std::string& path) {
        if (path.empty() || path == ":memory:") {
            if (duckdb_open(nullptr, &db) == DuckDBSuccess) {
                if (duckdb_connect(db, &conn) == DuckDBSuccess) {
                    valid = true;
                }
            }
            return;
        }

        std::filesystem::path fs_path(path);
        std::string ext = fs_path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".sqlite" || ext == ".db") {
            if (duckdb_open(nullptr, &db) == DuckDBSuccess && duckdb_connect(db, &conn) == DuckDBSuccess) {
                valid = true;
                duckdb_result res;
                std::string q = "ATTACH '" + path + "' AS sqlite_db (TYPE SQLITE);";
                if (duckdb_query(conn, q.c_str(), &res) == DuckDBSuccess) {
                    duckdb_destroy_result(&res);
                    duckdb_query(conn, "USE sqlite_db;", &res);
                    duckdb_destroy_result(&res);
                } else {
                    duckdb_destroy_result(&res);
                }
            }
        } else if (ext == ".csv") {
            if (duckdb_open(nullptr, &db) == DuckDBSuccess && duckdb_connect(db, &conn) == DuckDBSuccess) {
                valid = true;
                duckdb_result res;
                std::string q = "CREATE TABLE \"" + fs_path.stem().string() + "\" AS SELECT * FROM read_csv_auto('" + path + "');";
                duckdb_query(conn, q.c_str(), &res);
                duckdb_destroy_result(&res);
            }
        } else if (ext == ".parquet") {
            if (duckdb_open(nullptr, &db) == DuckDBSuccess && duckdb_connect(db, &conn) == DuckDBSuccess) {
                valid = true;
                duckdb_result res;
                std::string q = "CREATE TABLE \"" + fs_path.stem().string() + "\" AS SELECT * FROM read_parquet('" + path + "');";
                duckdb_query(conn, q.c_str(), &res);
                duckdb_destroy_result(&res);
            }
        } else {
            if (duckdb_open(path.c_str(), &db) == DuckDBSuccess) {
                if (duckdb_connect(db, &conn) == DuckDBSuccess) {
                    valid = true;
                }
            }
        }
    }

    ~Impl() {
        if (conn) {
            duckdb_disconnect(&conn);
            conn = nullptr;
        }
        if (db) {
            duckdb_close(&db);
            db = nullptr;
        }
    }
};

DuckDBAdapter::DuckDBAdapter(const std::string& db_path)
    : impl_(std::make_unique<Impl>(db_path)) {}

DuckDBAdapter::~DuckDBAdapter() = default;
DuckDBAdapter::DuckDBAdapter(DuckDBAdapter&&) noexcept = default;
DuckDBAdapter& DuckDBAdapter::operator=(DuckDBAdapter&&) noexcept = default;

bool DuckDBAdapter::is_valid() const noexcept {
    return impl_ && impl_->valid;
}

bool DuckDBAdapter::execute(const std::string& query) {
    if (!impl_ || !impl_->valid) return false;
    duckdb_result result;
    const duckdb_state state = duckdb_query(impl_->conn, query.c_str(), &result);
    const bool ok = (state == DuckDBSuccess);
    duckdb_destroy_result(&result);
    return ok;
}

std::vector<std::string> DuckDBAdapter::list_tables() {
    std::vector<std::string> tables;
    if (!impl_ || !impl_->valid) return tables;

    duckdb_result result;
    const duckdb_state state = duckdb_query(impl_->conn, "SHOW TABLES;", &result);

    if (state == DuckDBSuccess) {
        const idx_t rows = duckdb_row_count(&result);
        tables.reserve(rows);
        for (idx_t i = 0; i < rows; ++i) {
            char* val = duckdb_value_varchar(&result, 0, i);
            if (val) {
                tables.emplace_back(val);
                duckdb_free(val);
            }
        }
    }
    duckdb_destroy_result(&result);
    return tables;
}

TableSchema DuckDBAdapter::get_table_schema(std::string_view table_name) {
    TableSchema schema{std::string(table_name), {}};
    if (!impl_ || !impl_->valid) return schema;

    const std::string q = "DESCRIBE \"" + std::string(table_name) + "\";";
    duckdb_result result;
    const duckdb_state state = duckdb_query(impl_->conn, q.c_str(), &result);

    if (state == DuckDBSuccess) {
        const idx_t rows = duckdb_row_count(&result);
        schema.columns.reserve(rows);
        for (idx_t i = 0; i < rows; ++i) {
            char* col_name = duckdb_value_varchar(&result, 0, i);
            char* data_type = duckdb_value_varchar(&result, 1, i);
            char* is_nullable = duckdb_value_varchar(&result, 2, i);

            ColumnSchema col;
            if (col_name) {
                col.name = col_name;
                duckdb_free(col_name);
            }
            if (data_type) {
                col.data_type = data_type;
                duckdb_free(data_type);
            }
            if (is_nullable) {
                col.is_nullable = (std::string(is_nullable) == "YES");
                duckdb_free(is_nullable);
            }
            schema.columns.push_back(std::move(col));
        }
    }
    duckdb_destroy_result(&result);
    return schema;
}

std::vector<std::string> DuckDBAdapter::sample_column(
    std::string_view table_name,
    std::string_view column_name,
    Size max_samples) {
    std::vector<std::string> samples;
    if (!impl_ || !impl_->valid) return samples;

    std::string q = "SELECT \"" + std::string(column_name) + "\" FROM \"" + std::string(table_name) +
                    "\" WHERE \"" + std::string(column_name) + "\" IS NOT NULL USING SAMPLE " +
                    std::to_string(max_samples) + " (reservoir);";

    duckdb_result result;
    duckdb_state state = duckdb_query(impl_->conn, q.c_str(), &result);

    if (state != DuckDBSuccess) {
        duckdb_destroy_result(&result);
        q = "SELECT \"" + std::string(column_name) + "\" FROM \"" + std::string(table_name) +
            "\" WHERE \"" + std::string(column_name) + "\" IS NOT NULL LIMIT " + std::to_string(max_samples) + ";";
        state = duckdb_query(impl_->conn, q.c_str(), &result);
    }

    if (state == DuckDBSuccess) {
        const idx_t rows = duckdb_row_count(&result);
        samples.reserve(rows);
        for (idx_t i = 0; i < rows; ++i) {
            char* val = duckdb_value_varchar(&result, 0, i);
            if (val) {
                samples.emplace_back(val);
                duckdb_free(val);
            }
        }
    }
    duckdb_destroy_result(&result);
    return samples;
}

bool DuckDBAdapter::load_parquet(const std::string& table_name, const std::string& parquet_path) {
    const std::string q = "CREATE TABLE \"" + table_name + "\" AS SELECT * FROM read_parquet('" + parquet_path + "');";
    return execute(q);
}

bool DuckDBAdapter::load_csv(const std::string& table_name, const std::string& csv_path) {
    const std::string q = "CREATE TABLE \"" + table_name + "\" AS SELECT * FROM read_csv_auto('" + csv_path + "');";
    return execute(q);
}

bool DuckDBAdapter::attach_sqlite(const std::string& sqlite_path, const std::string& alias) {
    const std::string q = "ATTACH '" + sqlite_path + "' AS " + alias + " (TYPE SQLITE);";
    if (!execute(q)) return false;
    return execute("USE " + alias + ";");
}

TablePreviewData DuckDBAdapter::fetch_table_preview(std::string_view table_name, Size max_rows) {
    TablePreviewData preview{std::string(table_name), {}, {}};
    if (!impl_ || !impl_->valid) return preview;

    const std::string q = "SELECT * FROM \"" + std::string(table_name) + "\" LIMIT " + std::to_string(max_rows) + ";";
    duckdb_result result;
    const duckdb_state state = duckdb_query(impl_->conn, q.c_str(), &result);

    if (state == DuckDBSuccess) {
        const idx_t cols = duckdb_column_count(&result);
        const idx_t rows = duckdb_row_count(&result);
        preview.column_names.reserve(cols);
        for (idx_t c = 0; c < cols; ++c) {
            const char* name = duckdb_column_name(&result, c);
            preview.column_names.emplace_back(name ? name : "");
        }
        preview.rows.resize(rows);
        for (idx_t r = 0; r < rows; ++r) {
            preview.rows[r].resize(cols);
            for (idx_t c = 0; c < cols; ++c) {
                char* val = duckdb_value_varchar(&result, c, r);
                preview.rows[r][c] = (val ? val : "NULL");
                if (val) duckdb_free(val);
            }
        }
    }
    duckdb_destroy_result(&result);
    return preview;
}

} // namespace intelidata::ingestion

#else

namespace intelidata::ingestion {

struct DuckDBAdapter::Impl {};

DuckDBAdapter::DuckDBAdapter(const std::string&) : impl_(nullptr) {}
DuckDBAdapter::~DuckDBAdapter() = default;
DuckDBAdapter::DuckDBAdapter(DuckDBAdapter&&) noexcept = default;
DuckDBAdapter& DuckDBAdapter::operator=(DuckDBAdapter&&) noexcept = default;

bool DuckDBAdapter::is_valid() const noexcept { return false; }
bool DuckDBAdapter::execute(const std::string&) { return false; }
std::vector<std::string> DuckDBAdapter::list_tables() { return {}; }
TableSchema DuckDBAdapter::get_table_schema(std::string_view table_name) { return {std::string(table_name), {}}; }
std::vector<std::string> DuckDBAdapter::sample_column(std::string_view, std::string_view, Size) { return {}; }
TablePreviewData DuckDBAdapter::fetch_table_preview(std::string_view table_name, Size) { return {std::string(table_name), {}, {}}; }
bool DuckDBAdapter::load_parquet(const std::string&, const std::string&) { return false; }
bool DuckDBAdapter::load_csv(const std::string&, const std::string&) { return false; }
bool DuckDBAdapter::attach_sqlite(const std::string&, const std::string&) { return false; }

} // namespace intelidata::ingestion

#endif
