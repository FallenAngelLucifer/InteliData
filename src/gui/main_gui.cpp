#include "intelidata/gui/GraphCanvas.hpp"
#include "intelidata/gui/InspectorView.hpp"
#include "intelidata/gui/DataHubView.hpp"
#include "intelidata/gui/DataPreviewView.hpp"
#include "intelidata/pipeline/Engine.hpp"
#include "intelidata/pipeline/DDLGenerator.hpp"
#include "intelidata/ingestion/DuckDBAdapter.hpp"

#include <iostream>
#include <string>
#include <memory>
#include <chrono>
#include <fstream>
#include <filesystem>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/gl.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

namespace fs = std::filesystem;

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main(int argc, char* argv[]) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::cerr << "Fallo al inicializar GLFW\n";
        return 1;
    }

    // Configuración de OpenGL 3.3 Core Profile
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1440, 900, "InteliData - Reconstruccion Ontologica Bare-Metal", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Habilitar V-Sync (60-144 FPS)

    // Inicializar ImGui e ImPlot
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Habilitar acoplamiento de ventanas

    ImGui::StyleColorsDark();

    // Personalización moderna del estilo oscuro
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.11f, 0.14f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.35f, 0.8f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.35f, 0.48f, 0.8f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.45f, 0.60f, 1.0f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // ==========================================
    // Estado y Componentes de la Aplicación
    // ==========================================
    intelidata::gui::GraphCanvas canvas;
    intelidata::gui::InspectorView inspector;
    intelidata::gui::DataHubView data_hub;
    intelidata::gui::DataPreviewView preview_view;
    intelidata::pipeline::Engine engine;

    std::unique_ptr<intelidata::ingestion::DuckDBAdapter> current_adapter;
    intelidata::model::SemanticAST current_ast;
    std::vector<std::string> available_tables;
    std::string current_selected_table;
    std::string current_dataset_label;
    std::string status_message = "Listo.";
    double last_analysis_time_ms = 0.0;

    // Visibilidad de Paneles
    bool show_data_hub = false;
    bool show_preview_view = true;
    bool show_inspector_view = true;
    bool show_ddl_view = true;
    bool show_canvas_view = true;
    bool show_about_dialog = false;

    int max_frames = -1; // -1 = interactivo, > 0 = modo headless / CI

    // Helper para actualizar los datos crudos en preview_view
    auto update_preview_for_table = [&](const std::string& table_name) {
        if (!current_adapter || table_name.empty()) return;
        std::string phys_name = table_name;
        for (const auto& tbl : current_ast.tables) {
            if (tbl.recommended_name == table_name || tbl.original_identifier == table_name) {
                phys_name = tbl.original_identifier;
                break;
            }
        }
        preview_view.set_preview_data(current_adapter->fetch_table_preview(phys_name, 200));
    };

    // Helper central para cargar y analizar un adaptador
    auto load_adapter = [&](std::unique_ptr<intelidata::ingestion::DuckDBAdapter> new_adp,
                            const std::string& label,
                            const intelidata::pipeline::EngineOptions& options) {
        try {
            current_adapter = std::move(new_adp);
            current_dataset_label = label;
            data_hub.set_active_path(label);

            engine.set_options(options);
            const auto t0 = std::chrono::high_resolution_clock::now();
            current_ast = engine.analyze(*current_adapter);
            const auto t1 = std::chrono::high_resolution_clock::now();
            last_analysis_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

            canvas.load_from_ast(current_ast);

            // Reconstruir lista de tablas disponibles
            available_tables.clear();
            for (const auto& t : current_ast.tables) {
                available_tables.push_back(t.recommended_name);
            }

            if (!available_tables.empty()) {
                current_selected_table = available_tables[0];
                canvas.select_node_by_name(current_selected_table);
                update_preview_for_table(current_selected_table);
            } else {
                current_selected_table.clear();
                preview_view.set_preview_data({});
            }

            status_message = "Analisis exitoso: " + std::to_string(current_ast.tables.size()) +
                             " entidades, " + std::to_string(current_ast.relationships.size()) +
                             " relaciones PIND (" + std::to_string(static_cast<int>(last_analysis_time_ms)) + " ms)";
        } catch (const std::exception& e) {
            status_message = std::string("Error al analizar: ") + e.what();
            std::cerr << status_message << std::endl;
        }
    };

    // Helper para guardar archivos a disco
    auto export_file = [&](const std::string& filename, const std::string& content) {
        std::ofstream out(filename);
        if (out.is_open()) {
            out << content;
            status_message = "Archivo exportado con exito: " + filename;
        } else {
            status_message = "Error al exportar archivo: " + filename;
        }
    };

    // Parseo de argumentos de línea de comandos
    std::string initial_input;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test") {
            max_frames = 10;
        } else if (arg == "--frames" && i + 1 < argc) {
            max_frames = std::stoi(argv[++i]);
        } else if (arg == "--input" && i + 1 < argc) {
            initial_input = argv[++i];
        } else if (!arg.starts_with("--") && initial_input.empty()) {
            initial_input = arg;
        }
    }

    // Inicialización del dataset inicial
    if (!initial_input.empty()) {
        load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>(initial_input),
                     initial_input, data_hub.options());
    } else if (fs::exists("samples/ecommerce_obfuscated.db")) {
        load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>("samples/ecommerce_obfuscated.db"),
                     "samples/ecommerce_obfuscated.db", data_hub.options());
    } else {
        // Fallback demostrativo en memoria RAM
        try {
            auto demo_adp = std::make_unique<intelidata::ingestion::DuckDBAdapter>(":memory:");
            demo_adp->execute("CREATE TABLE clientes (id_cliente BIGINT, nombre VARCHAR);");
            demo_adp->execute("INSERT INTO clientes VALUES (1, 'Acme Corp'), (2, 'Globex'), (3, 'Initech');");
            demo_adp->execute("CREATE TABLE ordenes (id_orden BIGINT, id_cliente BIGINT, total DOUBLE, estado VARCHAR);");
            demo_adp->execute("INSERT INTO ordenes VALUES (101, 1, 1499.50, 'ENTREGADO'), (102, 2, 299.00, 'EN_PROCESO'), (103, 1, 45.00, 'CANCELADO');");
            load_adapter(std::move(demo_adp), "Memoria RAM (Esquema Demo)", data_hub.options());
        } catch (const std::exception& e) {
            status_message = std::string("Error demo: ") + e.what();
        }
    }

    int frame_count = 0;

    // ==========================================
    // Bucle Principal de Renderizado
    // ==========================================
    while (!glfwWindowShouldClose(window)) {
        if (max_frames > 0 && frame_count >= max_frames) {
            break;
        }
        frame_count++;

        glfwPollEvents();

        // Procesar solicitudes pendientes del Centro de Ingesta (Data Hub)
        if (data_hub.has_pending_request()) {
            auto req = data_hub.consume_request();
            if (req.type == intelidata::gui::IngestionSourceType::FilePath) {
                load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>(req.path), req.path, req.options);
            } else if (req.type == intelidata::gui::IngestionSourceType::MemorySQL) {
                try {
                    auto mem_adp = std::make_unique<intelidata::ingestion::DuckDBAdapter>(":memory:");
                    mem_adp->execute(req.sql_code);
                    load_adapter(std::move(mem_adp), "Memoria RAM (SQL Scratchpad)", req.options);
                } catch (const std::exception& e) {
                    status_message = std::string("Error SQL Scratchpad: ") + e.what();
                }
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // DockSpace a pantalla completa
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // ==========================================
        // 1. Barra de Menú Principal
        // ==========================================
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Archivo")) {
                if (ImGui::MenuItem("Centro de Ingesta y Conexion... (Data Hub)", "Ctrl+O")) {
                    show_data_hub = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Cargar E-Commerce Ofuscado (SQLite)")) {
                    if (fs::exists("samples/ecommerce_obfuscated.db")) {
                        load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>("samples/ecommerce_obfuscated.db"),
                                     "samples/ecommerce_obfuscated.db", data_hub.options());
                    }
                }
                if (ImGui::MenuItem("Cargar Transacciones Financieras (CSV)")) {
                    if (fs::exists("samples/samples_transacciones.csv")) {
                        load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>("samples/samples_transacciones.csv"),
                                     "samples/samples_transacciones.csv", data_hub.options());
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Salir", "Alt+F4")) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Vistas")) {
                ImGui::MenuItem("Grafo Ontologico (Fisica 2D)", nullptr, &show_canvas_view);
                ImGui::MenuItem("Previsualizacion de Datos Crudos", nullptr, &show_preview_view);
                ImGui::MenuItem("Inspector Estadistico (ImPlot)", nullptr, &show_inspector_view);
                ImGui::MenuItem("Centro de Exportacion DDL / Markdown", nullptr, &show_ddl_view);
                ImGui::Separator();
                ImGui::MenuItem("Centro de Ingesta (Data Hub)", nullptr, &show_data_hub);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Acciones")) {
                if (ImGui::MenuItem("Re-analizar Esquema Activo", "F5")) {
                    if (!current_dataset_label.empty() && current_adapter) {
                        if (current_dataset_label.starts_with("Memoria")) {
                            // En memoria se re-analiza el adaptador existente
                            try {
                                engine.set_options(data_hub.options());
                                auto t0 = std::chrono::high_resolution_clock::now();
                                current_ast = engine.analyze(*current_adapter);
                                auto t1 = std::chrono::high_resolution_clock::now();
                                last_analysis_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                                canvas.load_from_ast(current_ast);
                                status_message = "Re-analisis completado (" + std::to_string(static_cast<int>(last_analysis_time_ms)) + " ms)";
                            } catch (const std::exception& e) {
                                status_message = std::string("Error re-analisis: ") + e.what();
                            }
                        } else {
                            load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>(current_dataset_label),
                                         current_dataset_label, data_hub.options());
                        }
                    }
                }
                if (ImGui::MenuItem("Centrar Grafo en Pantalla")) {
                    canvas.fit_to_screen(800.0f, 600.0f);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Exportar")) {
                if (ImGui::MenuItem("Guardar Vistas SQL (schema_views.sql)")) {
                    export_file("schema_views.sql", intelidata::pipeline::DDLGenerator::generate_sql_views(current_ast));
                }
                if (ImGui::MenuItem("Guardar Diccionario Markdown (data_dictionary.md)")) {
                    export_file("data_dictionary.md", intelidata::pipeline::DDLGenerator::generate_data_dictionary_markdown(current_ast));
                }
                if (ImGui::MenuItem("Guardar Modelo AST (semantic_ast.json)")) {
                    export_file("semantic_ast.json", current_ast.to_json_string(2));
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Ayuda")) {
                if (ImGui::MenuItem("Acerca de InteliData...")) {
                    show_about_dialog = true;
                }
                ImGui::EndMenu();
            }

            // Indicador de estado a la derecha del menú
            ImGui::SameLine(ImGui::GetWindowWidth() - 380);
            ImGui::TextDisabled("%s", status_message.c_str());

            ImGui::EndMainMenuBar();
        }

        // ==========================================
        // 2. Barra de Herramientas Rápida
        // ==========================================
        if (ImGui::Begin("Barra de Herramientas", nullptr, ImGuiWindowFlags_NoScrollbar)) {
            if (ImGui::Button("  Conectar / Explorar  ")) {
                show_data_hub = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("  Demo E-Commerce  ")) {
                if (fs::exists("samples/ecommerce_obfuscated.db")) {
                    load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>("samples/ecommerce_obfuscated.db"),
                                 "samples/ecommerce_obfuscated.db", data_hub.options());
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("  Re-analizar  ")) {
                if (!current_dataset_label.empty()) {
                    if (current_dataset_label.starts_with("Memoria") && current_adapter) {
                        try {
                            engine.set_options(data_hub.options());
                            auto t0 = std::chrono::high_resolution_clock::now();
                            current_ast = engine.analyze(*current_adapter);
                            auto t1 = std::chrono::high_resolution_clock::now();
                            last_analysis_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                            canvas.load_from_ast(current_ast);
                            status_message = "Re-analisis completado (" + std::to_string(static_cast<int>(last_analysis_time_ms)) + " ms)";
                        } catch (const std::exception& e) {
                            status_message = std::string("Error: ") + e.what();
                        }
                    } else {
                        load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>(current_dataset_label),
                                     current_dataset_label, data_hub.options());
                    }
                }
            }

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            if (ImGui::Button("  Datos Crudos  ")) {
                show_preview_view = !show_preview_view;
            }
            ImGui::SameLine();
            if (ImGui::Button("  Centrar Grafo  ")) {
                canvas.fit_to_screen(800.0f, 600.0f);
            }

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            if (ImGui::Button("  Guardar SQL  ")) {
                export_file("schema_views.sql", intelidata::pipeline::DDLGenerator::generate_sql_views(current_ast));
            }
            ImGui::SameLine();
            if (ImGui::Button("  Guardar Markdown  ")) {
                export_file("data_dictionary.md", intelidata::pipeline::DDLGenerator::generate_data_dictionary_markdown(current_ast));
            }

            ImGui::SameLine(ImGui::GetWindowWidth() - 320);
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Origen:");
            ImGui::SameLine();
            ImGui::TextUnformatted(current_dataset_label.empty() ? "(Sin origen)" : current_dataset_label.c_str());
        }
        ImGui::End();

        // ==========================================
        // 3. Renderizado de Componentes
        // ==========================================

        // Centro de Ingesta (Data Hub)
        if (show_data_hub) {
            data_hub.render(&show_data_hub);
        }

        // Visor de Datos Crudos
        if (show_preview_view) {
            const std::string prev_table = current_selected_table;
            preview_view.render("Previsualizacion de Datos Crudos", available_tables, current_selected_table);
            if (current_selected_table != prev_table && !current_selected_table.empty()) {
                canvas.select_node_by_name(current_selected_table);
                update_preview_for_table(current_selected_table);
            }
        }

        // Grafo Ontológico Interactivo 2D
        if (show_canvas_view) {
            canvas.render("Grafo Ontologico (Fisica 2D)", 800.0f, 600.0f);

            // Sincronizar selección del canvas hacia el visor de datos
            const auto* selected_node = canvas.selected_node();
            if (selected_node && selected_node->name != current_selected_table) {
                current_selected_table = selected_node->name;
                update_preview_for_table(current_selected_table);
            }
        }

        // Inspector Estadístico Avanzado
        if (show_inspector_view) {
            inspector.render_inspector("Inspector Estadistico (ImPlot)", current_ast, canvas.selected_node());
        }

        // Centro de Exportación DDL / Markdown / JSON
        if (show_ddl_view) {
            inspector.render_ddl_viewer("Centro de Exportacion DDL / Markdown / JSON", current_ast);
        }

        // Barra de Telemetría Inferior
        if (ImGui::Begin("Barra de Estado", nullptr, ImGuiWindowFlags_NoScrollbar)) {
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.5f, 1.0f), "● Activo: %s",
                               current_dataset_label.empty() ? "Ninguno" : current_dataset_label.c_str());
            ImGui::SameLine(280);
            ImGui::Text("Entidades: %zu | Relaciones PIND: %zu",
                        current_ast.tables.size(), current_ast.relationships.size());
            ImGui::SameLine(540);
            ImGui::Text("Conectividad Fiedler: %.4f", current_ast.system_summary.fiedler_connectivity);
            ImGui::SameLine(770);
            ImGui::Text("Tiempo Inferencia: %.1f ms", last_analysis_time_ms);
            ImGui::SameLine(ImGui::GetWindowWidth() - 140);
            ImGui::Text("%.1f FPS", io.Framerate);
        }
        ImGui::End();

        // Diálogo Acerca de
        if (show_about_dialog) {
            ImGui::OpenPopup("Acerca de InteliData");
        }
        if (ImGui::BeginPopupModal("Acerca de InteliData", &show_about_dialog, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "InteliData v0.1.0");
            ImGui::Text("Motor Ontologico Bare-Metal en C++20");
            ImGui::Spacing();
            ImGui::TextWrapped("Sistema de desofuscacion y reconstruccion semantica de esquemas relacionales sin ML/LLMs.");
            ImGui::BulletText("Inferencia Probabilistica PIND O(k log N)");
            ImGui::BulletText("Clustering Espectral por Laplaciano no dirigido");
            ImGui::BulletText("Prueba de Bondad de Ajuste Chi-cuadrado a Ley de Benford");
            ImGui::BulletText("Identificacion de Estados Absorbentes en Cadenas de Markov");
            ImGui::BulletText("Gravedad Relacional mediante Centralidad PageRank");
            ImGui::Separator();
            if (ImGui::Button("Cerrar", ImVec2(120, 0))) {
                show_about_dialog = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Renderizado Final de Frame
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.10f, 0.11f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Limpieza al cerrar
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
