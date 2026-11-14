#include "intelidata/pipeline/Engine.hpp"
#include "intelidata/pipeline/DDLGenerator.hpp"
#include "intelidata/ingestion/DuckDBAdapter.hpp"
#include "intelidata/common/PathUtils.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

void print_usage(const char* prog_name) {
    std::cout << "==============================================================================\n";
    std::cout << " InteliData CLI - Motor Analitico y Ontologico Determinista\n";
    std::cout << "==============================================================================\n";
    std::cout << "Uso: " << prog_name << " [opciones]\n\n";
    std::cout << "Opciones Requeridas:\n";
    std::cout << "  --input <ruta>         Archivo de entrada (.parquet, .csv, .sqlite, .db)\n\n";
    std::cout << "Opciones de Salida:\n";
    std::cout << "  --output-json <ruta>   Ruta para guardar el AST semantico JSON (default: schema.json)\n";
    std::cout << "  --output-sql <ruta>    Ruta para guardar las vistas SQL DDL (alias: --output-ddl)\n";
    std::cout << "  --output-docs <ruta>   Ruta para guardar el Diccionario en Markdown con graficas\n";
    std::cout << "  --help, -h             Muestra este mensaje de ayuda\n";
}

int main(int argc, char* argv[]) {
    std::string input_path;
    std::string output_json = "schema.json";
    std::string output_sql;
    std::string output_docs;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--input" && i + 1 < argc) {
            input_path = argv[++i];
        } else if (arg == "--output-json" && i + 1 < argc) {
            output_json = argv[++i];
        } else if ((arg == "--output-sql" || arg == "--output-ddl") && i + 1 < argc) {
            output_sql = argv[++i];
        } else if (arg == "--output-docs" && i + 1 < argc) {
            output_docs = argv[++i];
        }
    }

    if (input_path.empty()) {
        std::cerr << "Error: Se requiere el argumento --input.\n\n";
        print_usage(argv[0]);
        return 1;
    }

    const fs::path in_path = intelidata::common::PathUtils::resolve_path(input_path);
    if (!fs::exists(in_path)) {
        std::cerr << "Error: El archivo '" << input_path << "' no existe (busqueda universal: " << in_path.string() << ").\n";
        return 1;
    }

    std::cout << "[InteliData] Inicializando motor de ingesta universal DuckDB...\n";
    std::cout << "[InteliData] Cargando archivo: " << in_path.filename().string() << " (Ruta: " << in_path.string() << ")...\n";
    intelidata::ingestion::DuckDBAdapter adapter(in_path.string());
    if (!adapter.is_valid()) {
        std::cerr << "Error al abrir origen de datos: " << in_path.string() << "\n";
        return 1;
    }

    intelidata::pipeline::Engine engine;
    std::cout << "[InteliData] Iniciando analisis ontologico determinista...\n";

    const auto ast = engine.analyze(adapter, [](std::string_view phase, intelidata::Float32 progress) {
        std::cout << "  [" << static_cast<int>(progress * 100.0f) << "%] " << phase << "\n";
    });

    std::cout << "\n[InteliData] Analisis concluido con exito:\n";
    std::cout << "  * Dominio Detectado: " << ast.system_summary.detected_domain << "\n";
    std::cout << "  * Nivel de Confianza: " << (ast.system_summary.confidence * 100.0) << "%\n";
    std::cout << "  * Conectividad Fiedler: " << ast.system_summary.fiedler_connectivity << "\n";
    std::cout << "  * Tablas Reconstruidas: " << ast.tables.size() << "\n";
    std::cout << "  * Relaciones Descubiertas: " << ast.relationships.size() << "\n\n";

    // Guardar AST JSON
    std::cout << "[InteliData] Exportando JSON semantico a: " << output_json << "\n";
    fs::path json_p(output_json);
    if (json_p.has_parent_path()) {
        fs::create_directories(json_p.parent_path());
    }
    std::ofstream json_file(json_p);
    if (json_file.is_open()) {
        json_file << ast.to_json_string(2);
    } else {
        std::cerr << "Error: No se pudo escribir en '" << output_json << "'.\n";
    }

    // Guardar Vistas SQL DDL si fue solicitado
    if (!output_sql.empty()) {
        std::cout << "[InteliData] Exportando vistas SQL DDL a: " << output_sql << "\n";
        fs::path sql_p(output_sql);
        if (sql_p.has_parent_path()) {
            fs::create_directories(sql_p.parent_path());
        }
        std::ofstream sql_file(sql_p);
        if (sql_file.is_open()) {
            sql_file << intelidata::pipeline::DDLGenerator::generate_sql_views(ast);
        } else {
            std::cerr << "Error: No se pudo escribir en '" << output_sql << "'.\n";
        }
    }

    // Guardar Diccionario Markdown y empaquetar gráficas si fue solicitado
    if (!output_docs.empty()) {
        std::cout << "[InteliData] Exportando diccionario en Markdown y graficas analiticas a: " << output_docs << "\n";
        intelidata::pipeline::DDLGenerator::export_report_with_assets(ast, output_docs);
    }

    std::cout << "[InteliData] Proceso completado exitosamente.\n";
    return 0;
}
