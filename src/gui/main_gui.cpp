#include "intelidata/gui/GraphCanvas.hpp"
#include "intelidata/gui/InspectorView.hpp"
#include "intelidata/gui/DataHubView.hpp"
#include "intelidata/gui/DataPreviewView.hpp"
#include "intelidata/gui/AnalyticsDashboardView.hpp"
#include "intelidata/pipeline/Engine.hpp"
#include "intelidata/pipeline/DDLGenerator.hpp"
#include "intelidata/ingestion/DuckDBAdapter.hpp"
#include "intelidata/common/PathUtils.hpp"

#include <iostream>
#include <string>
#include <memory>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <algorithm>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/gl.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

namespace fs = std::filesystem;

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

namespace {

void render_badge(const char* text, const ImVec4& bg_color, const ImVec4& text_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f)) {
    const float pad_x = 6.0f;
    const float pad_y = 2.0f;
    const ImVec2 txt_sz = ImGui::CalcTextSize(text);
    const ImVec2 badge_sz(txt_sz.x + pad_x * 2.0f, txt_sz.y + pad_y * 2.0f);

    const ImVec2 cur = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    const ImU32 col_bg = ImGui::ColorConvertFloat4ToU32(bg_color);
    const ImU32 col_txt = ImGui::ColorConvertFloat4ToU32(text_color);

    draw_list->AddRectFilled(cur, ImVec2(cur.x + badge_sz.x, cur.y + badge_sz.y), col_bg, 4.0f);
    draw_list->AddText(ImVec2(cur.x + pad_x, cur.y + pad_y), col_txt, text);

    ImGui::Dummy(badge_sz);
}

void render_responsive_badge(const char* text, const ImVec4& bg_color, const ImVec4& text_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f), bool try_same_line = true) {
    const float pad_x = 6.0f;
    const float badge_w = ImGui::CalcTextSize(text).x + pad_x * 2.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float max_x = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x - ImGui::GetStyle().ScrollbarSize - 6.0f;

    if (try_same_line && (ImGui::GetItemRectMax().x + spacing + badge_w <= max_x)) {
        ImGui::SameLine(0.0f, spacing);
    }
    render_badge(text, bg_color, text_color);
}

void setup_default_dock_layout(ImGuiID dockspace_id, const ImVec2& size, const std::string& solo_window = "") {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, size);

    if (!solo_window.empty()) {
        ImGui::DockBuilderDockWindow(solo_window.c_str(), dockspace_id);
        ImGui::DockBuilderFinish(dockspace_id);
        return;
    }

    ImGuiID dock_main = dockspace_id;
    ImGuiID dock_right = 0;
    ImGuiID dock_bottom = 0;
    ImGuiID dock_right_bottom = 0;

    // 1. División vertical derecha (42% de ancho para Inspector y Exportador)
    dock_right = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.42f, nullptr, &dock_main);

    // 2. División horizontal izquierda inferior (38% de alto para datos crudos)
    dock_bottom = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.38f, nullptr, &dock_main);

    // 3. División horizontal derecha inferior (45% de alto para DDL y exportación)
    dock_right_bottom = ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down, 0.45f, nullptr, &dock_right);

    // Acoplar ventanas fijas
    ImGui::DockBuilderDockWindow("Grafo Ontologico (Fisica 2D)", dock_main);
    ImGui::DockBuilderDockWindow("Analitica Interactiva y Finanzas (BI)", dock_bottom);
    ImGui::DockBuilderDockWindow("Previsualizacion de Datos Crudos", dock_bottom);
    ImGui::DockBuilderDockWindow("Inteligencia y Diagnostico de Negocio", dock_right);
    ImGui::DockBuilderDockWindow("Centro de Exportacion DDL / Markdown / JSON", dock_right_bottom);
    ImGui::DockBuilderFinish(dockspace_id);
}

void scale_dock_node_tree(ImGuiDockNode* node, float scale_x, float scale_y) {
    if (!node) return;
    if (scale_x > 0.001f && node->SizeRef.x > 0.0f) {
        node->SizeRef.x = std::max(100.0f, std::round(node->SizeRef.x * scale_x));
    }
    if (scale_y > 0.001f && node->SizeRef.y > 0.0f) {
        node->SizeRef.y = std::max(60.0f, std::round(node->SizeRef.y * scale_y));
    }
    if (node->ChildNodes[0]) scale_dock_node_tree(node->ChildNodes[0], scale_x, scale_y);
    if (node->ChildNodes[1]) scale_dock_node_tree(node->ChildNodes[1], scale_x, scale_y);
}

void setup_application_fonts(ImGuiIO& io, float dpi_scale = 1.0f) {
    const std::vector<std::string> candidate_fonts = {
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/carlito/Carlito-Regular.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\tahoma.ttf",
        "/System/Library/Fonts/SFCompact.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/Library/Fonts/Arial.ttf"
    };

    static const ImWchar glyph_ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin-1 Supplement (á, é, í, ó, ú, ñ, ¿, ¡, etc.)
        0x0100, 0x017F, // Latin Extended-A
        0x2000, 0x206F, // General Punctuation (• bullet, etc.)
        0x2190, 0x21FF, // Arrows (←, →, ↑, ↓)
        0x25A0, 0x25FF, // Geometric Shapes (●, ■, etc.)
        0
    };

    const float base_font_size = 16.0f * (dpi_scale > 0.5f ? dpi_scale : 1.0f);
    bool loaded = false;
    for (const auto& path : candidate_fonts) {
        if (fs::exists(path)) {
            ImFontConfig config;
            config.OversampleH = 2;
            config.OversampleV = 2;
            config.PixelSnapH = true;
            ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), base_font_size, &config, glyph_ranges);
            if (font != nullptr) {
                loaded = true;
                break;
            }
        }
    }

    if (!loaded) {
        io.Fonts->AddFontDefault();
    }
}

} // namespace

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

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--headless" || arg == "--screenshot") {
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            break;
        }
    }

    GLFWwindow* window = glfwCreateWindow(1600, 1000, "InteliData - Reconstruccion Ontologica Bare-Metal", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    // Limitar tamaño mínimo de la ventana para evitar colapso de componentes
    glfwSetWindowSizeLimits(window, 720, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // V-Sync

    // Detección de escala de contenido (DPI para pantallas de alta resolución)
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    const float dpi_scale = (xscale > 0.5f && xscale < 4.0f) ? xscale : 1.0f;

    // Inicializar ImGui e ImPlot
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Sistema de Docking
    io.ConfigDebugHighlightIdConflicts = false;       // Desactivar modal intrusivo de depuración de IDs

    // Cargar fuente TTF del sistema con cobertura completa Latin-1, símbolos y escala DPI
    setup_application_fonts(io, dpi_scale);

    ImGui::StyleColorsDark();

    // Estilo visual moderno y pulido
    ImGuiStyle& style = ImGui::GetStyle();
    if (dpi_scale > 1.05f) {
        style.ScaleAllSizes(dpi_scale);
    }
    style.WindowMinSize = ImVec2(100.0f, 60.0f);
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.11f, 0.14f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.24f, 0.28f, 0.36f, 0.6f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.35f, 0.8f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.35f, 0.48f, 0.8f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.45f, 0.60f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.26f, 0.36f, 0.9f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.36f, 0.50f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.45f, 0.62f, 1.0f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // ==========================================
    // Estado y Componentes de la Aplicación
    // ==========================================
    intelidata::gui::GraphCanvas canvas;
    intelidata::gui::InspectorView inspector;
    intelidata::gui::DataHubView data_hub;
    intelidata::gui::DataPreviewView preview_view;
    intelidata::gui::AnalyticsDashboardView analytics_view;
    intelidata::pipeline::Engine engine;

    std::unique_ptr<intelidata::ingestion::DuckDBAdapter> current_adapter;
    intelidata::model::SemanticAST current_ast;
    std::vector<std::string> available_tables;
    std::string current_selected_table;
    std::string current_dataset_label;
    std::string status_message = "A la espera de origen de datos.";
    double last_analysis_time_ms = 0.0;

    // Estado de flujo de la interfaz
    bool has_dataset_loaded = false;
    bool needs_dock_setup = true;
    bool show_data_hub_modal = false;
    bool show_about_dialog = false;

    // Buffers para la pantalla de bienvenida / entrada
    char welcome_file_path[512] = "";

    int max_frames = -1; // -1 = interactivo, > 0 = modo headless / CI

    // Helper para actualizar datos crudos
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
            analytics_view.on_dataset_loaded(label, current_adapter.get());

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

            has_dataset_loaded = true;
            needs_dock_setup = true;
            show_data_hub_modal = false;

            status_message = "Analisis exitoso: " + std::to_string(current_ast.tables.size()) +
                             " entidades, " + std::to_string(current_ast.relationships.size()) +
                             " relaciones PIND (" + std::to_string(static_cast<int>(last_analysis_time_ms)) + " ms)";
        } catch (const std::exception& e) {
            status_message = std::string("Error al analizar: ") + e.what();
            std::cerr << status_message << std::endl;
        }
    };

    // Helper para exportar archivos
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
    std::string screenshot_path;
    int screenshot_frame = 25;
    std::string initial_table;
    int initial_inspector_tab = -1;
    int initial_sheet_tab = -1;
    int initial_ddl_tab = -1;
    int initial_col_idx = -1;
    int initial_analytics_tab = -1;
    int initial_graph_mode = -1;
    bool exit_after_screenshot = false;
    std::string solo_window;
    bool force_show_data_hub = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test") {
            max_frames = 10;
        } else if (arg == "--frames" && i + 1 < argc) {
            max_frames = std::stoi(argv[++i]);
        } else if (arg == "--screenshot" && i + 1 < argc) {
            screenshot_path = argv[++i];
            exit_after_screenshot = true;
        } else if (arg == "--screenshot-frame" && i + 1 < argc) {
            screenshot_frame = std::stoi(argv[++i]);
        } else if (arg == "--table" && i + 1 < argc) {
            initial_table = argv[++i];
        } else if (arg == "--inspector-tab" && i + 1 < argc) {
            initial_inspector_tab = std::stoi(argv[++i]);
        } else if (arg == "--sheet-tab" && i + 1 < argc) {
            initial_sheet_tab = std::stoi(argv[++i]);
        } else if (arg == "--ddl-tab" && i + 1 < argc) {
            initial_ddl_tab = std::stoi(argv[++i]);
        } else if (arg == "--selected-col" && i + 1 < argc) {
            initial_col_idx = std::stoi(argv[++i]);
        } else if (arg == "--analytics-tab" && i + 1 < argc) {
            initial_analytics_tab = std::stoi(argv[++i]);
        } else if (arg == "--graph-mode" && i + 1 < argc) {
            initial_graph_mode = std::stoi(argv[++i]);
        } else if (arg == "--solo-window" && i + 1 < argc) {
            solo_window = argv[++i];
        } else if (arg == "--show-data-hub") {
            force_show_data_hub = true;
        } else if (arg == "--exit-after-screenshot") {
            exit_after_screenshot = true;
        } else if (arg == "--input" && i + 1 < argc) {
            initial_input = argv[++i];
        } else if (!arg.starts_with("--") && initial_input.empty()) {
            initial_input = arg;
        }
    }

    // Si se especificó explícitamente un archivo de entrada en la invocación CLI, cargarlo
    if (!initial_input.empty()) {
        load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>(initial_input),
                     initial_input, data_hub.options());
        if (initial_graph_mode >= 0) {
            canvas.set_view_mode(initial_graph_mode == 0 ? intelidata::gui::GraphViewMode::TableSchemaDSV : intelidata::gui::GraphViewMode::EntityNetwork);
        }
        if (!initial_table.empty()) {
            current_selected_table = initial_table;
            canvas.select_node_by_name(current_selected_table);
            update_preview_for_table(current_selected_table);
        }
    } else {
        // De lo contrario, iniciar esperando input de datos del usuario
        has_dataset_loaded = false;
    }

    if (force_show_data_hub) {
        show_data_hub_modal = true;
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

        // Procesar solicitudes del DataHub modal
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

        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        // ====================================================================
        // 1. Barra de Menú Principal (Siempre disponible en la parte superior)
        // ====================================================================
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Archivo")) {
                if (ImGui::MenuItem("Ingresar Datos / Conectar Origen...", "Ctrl+O")) {
                    show_data_hub_modal = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Cargar Northwind (SQLite Clásico)")) {
                    const auto p = intelidata::common::PathUtils::resolve_path("samples/northwind.db");
                    if (fs::exists(p)) {
                        load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>(p.string()),
                                     p.string(), data_hub.options());
                    }
                }
                if (ImGui::MenuItem("Cargar E-Commerce Ofuscado (SQLite)")) {
                    const auto p = intelidata::common::PathUtils::resolve_path("samples/ecommerce_obfuscated.db");
                    if (fs::exists(p)) {
                        load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>(p.string()),
                                     p.string(), data_hub.options());
                    }
                }
                if (ImGui::MenuItem("Cargar Transacciones Financieras (CSV)")) {
                    const auto p = intelidata::common::PathUtils::resolve_path("samples/samples_transacciones.csv");
                    if (fs::exists(p)) {
                        load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>(p.string()),
                                     p.string(), data_hub.options());
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Cerrar Esquema Activo")) {
                    has_dataset_loaded = false;
                    current_adapter.reset();
                    current_ast = intelidata::model::SemanticAST{};
                    available_tables.clear();
                    current_selected_table.clear();
                    current_dataset_label.clear();
                    status_message = "A la espera de origen de datos.";
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Salir", "Alt+F4")) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Vistas")) {
                if (ImGui::MenuItem("Restablecer Diseno Predeterminado")) {
                    needs_dock_setup = true;
                }
                ImGui::Separator();
                if (has_dataset_loaded && ImGui::MenuItem("Analitica Interactiva y Finanzas (BI)")) {
                    ImGui::SetWindowFocus("Analitica Interactiva y Finanzas (BI)");
                }
                if (has_dataset_loaded && ImGui::MenuItem("Previsualizacion de Datos Crudos")) {
                    ImGui::SetWindowFocus("Previsualizacion de Datos Crudos");
                }
                if (has_dataset_loaded && ImGui::MenuItem("Grafo Ontologico (Fisica 2D)")) {
                    ImGui::SetWindowFocus("Grafo Ontologico (Fisica 2D)");
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Abrir Centro de Ingesta (Data Hub)")) {
                    show_data_hub_modal = true;
                }
                ImGui::EndMenu();
            }

            if (has_dataset_loaded && ImGui::BeginMenu("Acciones")) {
                if (ImGui::MenuItem("Re-analizar Esquema Activo", "F5")) {
                    if (!current_dataset_label.empty() && current_adapter) {
                        if (current_dataset_label.starts_with("Memoria")) {
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

            if (has_dataset_loaded && ImGui::BeginMenu("Exportar")) {
                if (ImGui::MenuItem("Guardar Vistas SQL (schema_views.sql)")) {
                    export_file("schema_views.sql", intelidata::pipeline::DDLGenerator::generate_sql_views(current_ast));
                }
                if (ImGui::MenuItem("Guardar Diccionario Markdown con Graficas (data_dictionary.md)")) {
                    intelidata::pipeline::DDLGenerator::export_report_with_assets(current_ast, "data_dictionary.md");
                    status_message = "Diccionario y graficas guardadas en data_dictionary.md e images/.";
                }
                if (ImGui::MenuItem("Guardar Todas las Graficas Analiticas (images/)")) {
                    analytics_view.save_report_charts();
                    status_message = "Graficas analiticas guardadas con exito en images/.";
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

            // Indicador de estado en la barra de menú (adaptativo a la anchura)
            const float status_w = 340.0f;
            if (ImGui::GetContentRegionAvail().x >= status_w + 10.0f) {
                ImGui::SameLine(ImGui::GetWindowWidth() - status_w);
                ImGui::TextDisabled("%s", status_message.c_str());
            }

            ImGui::EndMainMenuBar();
        }

        // ====================================================================
        // ESTADO 1: ESPERANDO INPUT DE DATOS (PANTALLA DE BIENVENIDA E INGESTA)
        // ====================================================================
        if (!has_dataset_loaded) {
            // Fondo centrado adaptativo para la pantalla de bienvenida
            const float max_w = std::min(1150.0f, std::max(340.0f, viewport->WorkSize.x - 30.0f));
            const float max_h = std::min(780.0f, std::max(260.0f, viewport->WorkSize.y - 30.0f));
            const ImVec2 center_size(max_w, max_h);
            const ImVec2 center_pos(
                viewport->WorkPos.x + std::max(0.0f, (viewport->WorkSize.x - center_size.x) * 0.5f),
                viewport->WorkPos.y + std::max(0.0f, (viewport->WorkSize.y - center_size.y) * 0.5f)
            );

            ImGui::SetNextWindowPos(center_pos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(center_size, ImGuiCond_Always);

            ImGuiWindowFlags welcome_flags = ImGuiWindowFlags_NoCollapse |
                                             ImGuiWindowFlags_NoMove;

            if (ImGui::Begin("Bienvenido a InteliData - Esperando Datos", nullptr, welcome_flags)) {
                // Cabecera Principal
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.2f, 0.85f, 1.0f, 1.0f), "INTELIDATA - RECONSTRUCCION ONTOLOGICA BARE-METAL");
                ImGui::TextDisabled("Motor de desofuscacion y analisis semantico en C++20 (Sin ML / Cajas Negras)");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextWrapped("El sistema requiere el ingreso o seleccion de una fuente de datos antes de proceder al analisis. Elija una de las siguientes opciones:");
                ImGui::Spacing();

                if (ImGui::BeginTabBar("WelcomeInputTabs")) {
                    // Pestaña 1: Datasets de Demostración (Recomendado)
                    if (ImGui::BeginTabItem("  1. Datasets de Prueba (Recomendado)  ")) {
                        ImGui::Spacing();

                        // TARJETA 1: NORTHWIND (DESTACADA)
                        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.17f, 0.24f, 1.0f));
                        ImGui::BeginChild("NorthwindCard", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

                        ImGui::TextColored(ImVec4(0.3f, 0.95f, 0.6f, 1.0f), "Northwind - Base de Datos Relacional Comercial");
                        render_responsive_badge("RECOMENDADO", ImVec4(0.18f, 0.70f, 0.35f, 1.0f));
                        render_responsive_badge("12 TABLAS", ImVec4(0.25f, 0.45f, 0.75f, 1.0f));
                        render_responsive_badge("25 RELACIONES", ImVec4(0.55f, 0.30f, 0.75f, 1.0f));
                        render_responsive_badge("SQLITE", ImVec4(0.70f, 0.45f, 0.15f, 1.0f));

                        ImGui::Spacing();
                        ImGui::TextWrapped("Esquema relacional comercial estandar con Orders, Customers, OrderDetails, Products, Categories, Suppliers, Shippers y Employees. Ideal para verificar la inferencia PIND exacta, centralidad PageRank y perfiles de Benford.");

                        ImGui::Spacing();
                        const float nw_btn_w = std::min(320.0f, ImGui::GetContentRegionAvail().x);
                        if (ImGui::Button("  [Cargar] Analizar Northwind (SQLite)  ", ImVec2(nw_btn_w, 32))) {
                            const auto p = intelidata::common::PathUtils::resolve_path("samples/northwind.db");
                            if (fs::exists(p)) {
                                load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>(p.string()),
                                             p.string(), data_hub.options());
                            } else {
                                status_message = "No se encontro samples/northwind.db en el disco.";
                            }
                        }
                        ImGui::EndChild();
                        ImGui::PopStyleColor();

                        ImGui::Spacing();

                        // TARJETA 2: E-COMMERCE OFUSCADO
                        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.15f, 0.18f, 1.0f));
                        ImGui::BeginChild("ObfuscatedCard", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

                        ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.25f, 1.0f), "E-Commerce Multitabla Ofuscado");
                        render_responsive_badge("4 TABLAS OFUSCADAS", ImVec4(0.80f, 0.40f, 0.15f, 1.0f));
                        render_responsive_badge("DESOFUSCACION", ImVec4(0.30f, 0.50f, 0.80f, 1.0f));

                        ImGui::Spacing();
                        ImGui::TextWrapped("Tablas con nombres cripticos (tbl_001_usr a tbl_004_log). Demuestra el descubrimiento de nombres canonicos, estados absorbentes de Markov y particion espectral Fiedler en 2 subdominios.");

                        ImGui::Spacing();
                        const float obf_btn_w = std::min(300.0f, ImGui::GetContentRegionAvail().x);
                        if (ImGui::Button("  Cargar E-Commerce Ofuscado (SQLite)  ", ImVec2(obf_btn_w, 28))) {
                            const auto p = intelidata::common::PathUtils::resolve_path("samples/ecommerce_obfuscated.db");
                            if (fs::exists(p)) {
                                load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>(p.string()),
                                             p.string(), data_hub.options());
                            }
                        }
                        ImGui::EndChild();
                        ImGui::PopStyleColor();

                        ImGui::Spacing();

                        // TARJETA 3: TRANSACCIONES FINANCIERAS
                        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.15f, 0.18f, 1.0f));
                        ImGui::BeginChild("TransaccionesCard", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

                        ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.95f, 1.0f), "Transacciones Financieras (CSV)");
                        render_responsive_badge("BENFORD CHI-CUADRADO", ImVec4(0.20f, 0.65f, 0.35f, 1.0f));

                        ImGui::Spacing();
                        ImGui::TextWrapped("Registros con montos monetarios logaritmicos para comprobar la bondad de ajuste a la Ley de Benford.");

                        ImGui::Spacing();
                        const float tx_btn_w = std::min(240.0f, ImGui::GetContentRegionAvail().x);
                        if (ImGui::Button("  Cargar Transacciones (CSV)  ", ImVec2(tx_btn_w, 26))) {
                            const auto p = intelidata::common::PathUtils::resolve_path("samples/samples_transacciones.csv");
                            if (fs::exists(p)) {
                                load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>(p.string()),
                                             p.string(), data_hub.options());
                            }
                        }
                        ImGui::EndChild();
                        ImGui::PopStyleColor();

                        ImGui::EndTabItem();
                    }

                    // Pestaña 2: Archivo Local
                    if (ImGui::BeginTabItem("  2. Archivo Local (SQLite/CSV/Parquet)  ")) {
                        ImGui::Spacing();
                        ImGui::Text("Especifique la ruta de un archivo existente en su sistema:");
                        ImGui::Spacing();

                        ImGui::SetNextItemWidth(-1);
                        ImGui::InputTextWithHint("##welcome_path", "Ej: samples/northwind.db o /ruta/completa/archivo.parquet",
                                                 welcome_file_path, sizeof(welcome_file_path));

                        ImGui::Spacing();
                        const bool can_load_path = (std::strlen(welcome_file_path) > 0 && fs::exists(welcome_file_path));

                        if (!can_load_path && std::strlen(welcome_file_path) > 0) {
                            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "El archivo indicado no existe en la ruta especificada.");
                        }

                        ImGui::BeginDisabled(!can_load_path);
                        if (ImGui::Button("  [Cargar] Analizar Archivo  ", ImVec2(260, 32))) {
                            load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>(welcome_file_path),
                                         welcome_file_path, data_hub.options());
                        }
                        ImGui::EndDisabled();

                        ImGui::SameLine();
                        if (ImGui::Button("Abrir Explorador Completo...")) {
                            show_data_hub_modal = true;
                        }

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::TextDisabled("Archivos detectados en samples/:");
                        if (fs::exists("samples")) {
                            for (const auto& entry : fs::directory_iterator("samples")) {
                                if (entry.is_regular_file()) {
                                    const std::string ext = entry.path().extension().string();
                                    if (ext == ".db" || ext == ".sqlite" || ext == ".csv" || ext == ".parquet") {
                                        if (ImGui::SmallButton(entry.path().string().c_str())) {
                                            std::snprintf(welcome_file_path, sizeof(welcome_file_path), "%s", entry.path().string().c_str());
                                        }
                                    }
                                }
                            }
                        }

                        ImGui::EndTabItem();
                    }

                    // Pestaña 3: SQL Scratchpad
                    if (ImGui::BeginTabItem("  3. SQL Scratchpad en Memoria  ")) {
                        ImGui::Spacing();
                        ImGui::Text("Pegue o redacte sentencias DDL y DML para ejecutarlas en memoria RAM (:memory:):");
                        static char ram_sql_buf[2048] =
                            "CREATE TABLE productos (id_producto BIGINT, nombre VARCHAR, precio DOUBLE, stock INT);\n"
                            "INSERT INTO productos VALUES\n"
                            "  (101, 'Monitor 4K', 450.00, 25),\n"
                            "  (102, 'Teclado Mecanico', 85.50, 60),\n"
                            "  (103, 'Mouse Ergonomico', 45.00, 110);\n\n"
                            "CREATE TABLE ventas (id_venta BIGINT, id_producto BIGINT, cantidad INT, estado VARCHAR);\n"
                            "INSERT INTO ventas VALUES\n"
                            "  (1, 101, 2, 'PAGADO'),\n"
                            "  (2, 102, 1, 'PAGADO'),\n"
                            "  (3, 101, 5, 'ENTREGADO');\n";

                        const float sql_h = std::clamp(ImGui::GetContentRegionAvail().y - 65.0f, 130.0f, 380.0f);
                        ImGui::InputTextMultiline("##ram_sql", ram_sql_buf, sizeof(ram_sql_buf), ImVec2(-1, sql_h), ImGuiInputTextFlags_AllowTabInput);

                        if (ImGui::Button("  [Ejecutar] Analizar Esquema en RAM  ", ImVec2(300, 32))) {
                            try {
                                auto mem_adp = std::make_unique<intelidata::ingestion::DuckDBAdapter>(":memory:");
                                mem_adp->execute(ram_sql_buf);
                                load_adapter(std::move(mem_adp), "Memoria RAM (SQL Scratchpad)", data_hub.options());
                            } catch (const std::exception& e) {
                                status_message = std::string("Error SQL: ") + e.what();
                            }
                        }
                        ImGui::EndTabItem();
                    }

                    // Pestaña 4: Ajustes del Motor
                    if (ImGui::BeginTabItem("  4. Ajustes del Motor  ")) {
                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Parametros de Calibracion del Motor Ontologico:");
                        ImGui::Spacing();

                        auto opts = data_hub.options();
                        int sample_size = static_cast<int>(opts.max_sample_size);
                        if (ImGui::SliderInt("Muestra de Reservoir (Filas)", &sample_size, 500, 50000, "%d")) {
                            opts.max_sample_size = static_cast<intelidata::Size>(sample_size);
                        }

                        float fk_thresh = static_cast<float>(opts.fk_inclusion_threshold);
                        if (ImGui::SliderFloat("Umbral Inclusion PIND", &fk_thresh, 0.80f, 1.00f, "%.2f")) {
                            opts.fk_inclusion_threshold = static_cast<intelidata::Float64>(fk_thresh);
                        }

                        float benford_a = static_cast<float>(opts.benford_alpha);
                        if (ImGui::SliderFloat("Alpha Benford (Chi-cuadrado)", &benford_a, 0.01f, 0.15f, "%.2f")) {
                            opts.benford_alpha = static_cast<intelidata::Float64>(benford_a);
                        }
                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }
            }
            ImGui::End();
        }

        // ====================================================================
        // ESTADO 2: ESPACIO DE TRABAJO ACTIVO (DATASET CARGADO Y ACOPLADO)
        // ====================================================================
        if (has_dataset_loaded) {
            const float toolbar_height = 42.0f;
            const float statusbar_height = 26.0f;
            const ImVec2 dockspace_size(viewport->WorkSize.x, viewport->WorkSize.y - toolbar_height - statusbar_height);

            // ----------------------------------------------------------------
            // 2.1. Barra de Herramientas Superior Fija
            // ----------------------------------------------------------------
            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y));
            ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, toolbar_height));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.14f, 0.18f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            ImGuiWindowFlags fixed_toolbar_flags = ImGuiWindowFlags_NoDecoration |
                                                  ImGuiWindowFlags_NoMove |
                                                  ImGuiWindowFlags_NoScrollbar |
                                                  ImGuiWindowFlags_NoSavedSettings;

            const float win_w = viewport->WorkSize.x;
            const bool is_wide = (win_w >= 1200.0f);
            const bool is_mid = (win_w >= 880.0f);

            if (ImGui::Begin("##FixedTopToolbar", nullptr, fixed_toolbar_flags)) {
                if (ImGui::Button(is_wide ? "  [Origen] Cambiar Origen  " : (is_mid ? "  [Origen]  " : "Origen"))) {
                    show_data_hub_modal = true;
                }

                // Mostrar accesos directos de demo solo si hay suficiente espacio horizontal
                if (win_w >= 1260.0f) {
                    ImGui::SameLine();
                    if (ImGui::Button("  [Demo] Northwind  ")) {
                        const auto p = intelidata::common::PathUtils::resolve_path("samples/northwind.db");
                        if (fs::exists(p)) {
                            load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>(p.string()),
                                         p.string(), data_hub.options());
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("  [Demo] E-Commerce  ")) {
                        const auto p = intelidata::common::PathUtils::resolve_path("samples/ecommerce_obfuscated.db");
                        if (fs::exists(p)) {
                            load_adapter(std::make_unique<intelidata::ingestion::DuckDBAdapter>(p.string()),
                                         p.string(), data_hub.options());
                        }
                    }
                }

                ImGui::SameLine();
                ImGui::TextDisabled("|");
                ImGui::SameLine();

                if (ImGui::Button(is_wide ? "  [Re-analizar]  " : (is_mid ? "  [Re-analizar]  " : "Re-analizar"))) {
                    if (!current_dataset_label.empty() && current_adapter) {
                        if (current_dataset_label.starts_with("Memoria")) {
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
                if (ImGui::Button(is_wide ? "  [Centrar Grafo]  " : (is_mid ? "  [Centrar]  " : "Centrar"))) {
                    canvas.fit_to_screen(dockspace_size.x * 0.62f, dockspace_size.y * 0.62f);
                }

                ImGui::SameLine();
                if (ImGui::Button(is_wide ? "  [Analitica & Finanzas BI]  " : (is_mid ? "  [Analitica BI]  " : "BI"))) {
                    ImGui::SetWindowFocus("Analitica Interactiva y Finanzas (BI)");
                }

                ImGui::SameLine();
                ImGui::TextDisabled("|");
                ImGui::SameLine();

                if (ImGui::Button(is_wide ? "  [Guardar DDL]  " : (is_mid ? "  [DDL]  " : "DDL"))) {
                    export_file("schema_views.sql", intelidata::pipeline::DDLGenerator::generate_sql_views(current_ast));
                }
                ImGui::SameLine();
                if (ImGui::Button(is_wide ? "  [Guardar Docs]  " : (is_mid ? "  [Docs]  " : "Docs"))) {
                    intelidata::pipeline::DDLGenerator::export_report_with_assets(current_ast, "data_dictionary.md");
                    status_message = "Diccionario y graficas guardadas en data_dictionary.md e images/.";
                }

                // Información activa adaptativa (solo si cabe a la derecha sin solaparse)
                const float info_full_w = 340.0f;
                const float info_compact_w = 140.0f;
                const float avail_toolbar = ImGui::GetContentRegionAvail().x;
                if (avail_toolbar >= info_full_w + 10.0f) {
                    ImGui::SameLine(ImGui::GetWindowWidth() - info_full_w);
                    ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "Activo:");
                    ImGui::SameLine();
                    ImGui::TextUnformatted(current_dataset_label.c_str());
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%zu tablas, %zu rel.)", current_ast.tables.size(), current_ast.relationships.size());
                } else if (avail_toolbar >= info_compact_w + 10.0f) {
                    ImGui::SameLine(ImGui::GetWindowWidth() - info_compact_w);
                    ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "%s", current_dataset_label.c_str());
                }
            }
            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();

            // ----------------------------------------------------------------
            // 2.2. Barra de Estado Inferior Fija Adaptativa
            // ----------------------------------------------------------------
            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - statusbar_height));
            ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, statusbar_height));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.12f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 4.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            if (ImGui::Begin("##FixedStatusBar", nullptr, fixed_toolbar_flags)) {
                ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.5f, 1.0f), "● Activo: %s",
                                   current_dataset_label.c_str());

                const float avail_status = ImGui::GetContentRegionAvail().x;

                if (avail_status >= 520.0f) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("|");
                    ImGui::SameLine();
                    ImGui::Text("Entidades: %zu | Relaciones PIND: %zu",
                                current_ast.tables.size(), current_ast.relationships.size());
                }
                if (avail_status >= 360.0f) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("|");
                    ImGui::SameLine();
                    ImGui::Text("Fiedler: %.4f", current_ast.system_summary.fiedler_connectivity);
                }
                if (avail_status >= 200.0f) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("|");
                    ImGui::SameLine();
                    ImGui::Text("%.1f ms", last_analysis_time_ms);
                }
                if (ImGui::GetContentRegionAvail().x >= 75.0f) {
                    ImGui::SameLine(ImGui::GetWindowWidth() - 75.0f);
                    ImGui::Text("%.0f FPS", io.Framerate);
                }
            }
            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();

            // ----------------------------------------------------------------
            // 2.3. Espacio de Acoplamiento Central (DockSpace Host)
            // ----------------------------------------------------------------
            const ImVec2 dockspace_pos(viewport->WorkPos.x, viewport->WorkPos.y + toolbar_height);
            static ImVec2 last_dockspace_size(0.0f, 0.0f);

            ImGui::SetNextWindowPos(dockspace_pos);
            ImGui::SetNextWindowSize(dockspace_size);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            ImGuiWindowFlags host_flags = ImGuiWindowFlags_NoDecoration |
                                          ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoScrollbar |
                                          ImGuiWindowFlags_NoSavedSettings |
                                          ImGuiWindowFlags_NoBringToFrontOnFocus;

            if (ImGui::Begin("##CentralDockHost", nullptr, host_flags)) {
                ImGuiID dockspace_id = ImGui::GetID("InteliDataDockSpace");

                // Configurar el layout predeterminado por primera vez o si se solicitó reset
                if (needs_dock_setup) {
                    setup_default_dock_layout(dockspace_id, dockspace_size, solo_window);
                    needs_dock_setup = false;
                    last_dockspace_size = dockspace_size;
                    canvas.fit_to_screen(dockspace_size.x * 0.62f, dockspace_size.y * 0.62f);
                } else if (last_dockspace_size.x > 50.0f && last_dockspace_size.y > 50.0f) {
                    // Escalar proporcionalmente todos los paneles ante cualquier cambio de resolución
                    const float delta_x = std::abs(dockspace_size.x - last_dockspace_size.x);
                    const float delta_y = std::abs(dockspace_size.y - last_dockspace_size.y);
                    if (delta_x > 1.0f || delta_y > 1.0f) {
                        const float scale_x = dockspace_size.x / last_dockspace_size.x;
                        const float scale_y = dockspace_size.y / last_dockspace_size.y;
                        ImGuiDockNode* root = ImGui::DockBuilderGetNode(dockspace_id);
                        if (root) {
                            ImGui::DockBuilderSetNodeSize(dockspace_id, dockspace_size);
                            scale_dock_node_tree(root, scale_x, scale_y);
                        }
                        last_dockspace_size = dockspace_size;
                    }
                }

                ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
            }
            ImGui::End();
            ImGui::PopStyleVar(2);

            // ----------------------------------------------------------------
            // 2.4. Renderizado de Paneles Acoplados
            // ----------------------------------------------------------------
            if (initial_inspector_tab >= 0) inspector.set_inspector_tab(initial_inspector_tab);
            if (initial_sheet_tab >= 0) inspector.set_entity_sheet_tab(initial_sheet_tab);
            if (initial_ddl_tab >= 0) inspector.set_ddl_viewer_tab(initial_ddl_tab);
            if (initial_col_idx >= 0) inspector.set_selected_column_idx(initial_col_idx);
            if (initial_analytics_tab >= 0) analytics_view.set_selected_tab(initial_analytics_tab);

            if (!current_selected_table.empty() && !canvas.selected_node()) {
                canvas.select_node_by_name(current_selected_table);
            }

            if (!solo_window.empty()) {
                if (solo_window == "Grafo Ontologico (Fisica 2D)") {
                    canvas.render("Grafo Ontologico (Fisica 2D)", dockspace_size.x, dockspace_size.y);
                } else if (solo_window == "Previsualizacion de Datos Crudos") {
                    preview_view.render("Previsualizacion de Datos Crudos", available_tables, current_selected_table);
                } else if (solo_window == "Inteligencia y Diagnostico de Negocio") {
                    inspector.render_inspector("Inteligencia y Diagnostico de Negocio", current_ast, canvas.selected_node());
                } else if (solo_window == "Centro de Exportacion DDL / Markdown / JSON") {
                    inspector.render_ddl_viewer("Centro de Exportacion DDL / Markdown / JSON", current_ast);
                } else if (solo_window == "Analitica Interactiva y Finanzas (BI)") {
                    analytics_view.render("Analitica Interactiva y Finanzas (BI)", current_ast, current_adapter.get());
                }
            } else {
                // 1. Grafo Ontológico Interactivo 2D
                canvas.render("Grafo Ontologico (Fisica 2D)", dockspace_size.x * 0.62f, dockspace_size.y * 0.62f);

                // Sincronizar selección de nodo del canvas hacia previsualización
                const auto* selected_node = canvas.selected_node();
                if (selected_node && selected_node->name != current_selected_table) {
                    current_selected_table = selected_node->name;
                    update_preview_for_table(current_selected_table);
                }

                // 2. Visor de Datos Crudos (Grid Tabular)
                const std::string prev_table = current_selected_table;
                preview_view.render("Previsualizacion de Datos Crudos", available_tables, current_selected_table);
                if (current_selected_table != prev_table && !current_selected_table.empty()) {
                    canvas.select_node_by_name(current_selected_table);
                    update_preview_for_table(current_selected_table);
                }

                // 3. Inteligencia y Diagnóstico de Negocio
                inspector.render_inspector("Inteligencia y Diagnostico de Negocio", current_ast, canvas.selected_node());
                if (inspector.has_table_selection_request()) {
                    current_selected_table = inspector.consume_table_selection();
                    canvas.select_node_by_name(current_selected_table);
                    update_preview_for_table(current_selected_table);
                }

                // 4. Centro de Exportación DDL / Markdown / JSON
                inspector.render_ddl_viewer("Centro de Exportacion DDL / Markdown / JSON", current_ast);

                // 5. Analítica Interactiva y Finanzas (BI) - Tablero y Gráficos ImPlot
                analytics_view.render("Analitica Interactiva y Finanzas (BI)", current_ast, current_adapter.get());
            }
        }

        // ====================================================================
        // Diálogos Modales / Emergentes
        // ====================================================================

        // Modal del Centro de Ingesta (Data Hub)
        if (show_data_hub_modal) {
            data_hub.render(&show_data_hub_modal);
        }

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
        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (!screenshot_path.empty() && frame_count == screenshot_frame) {
            std::vector<unsigned char> pixels(display_w * display_h * 3);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, display_w, display_h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            std::vector<unsigned char> flipped(display_w * display_h * 3);
            for (int y = 0; y < display_h; ++y) {
                std::memcpy(&flipped[y * display_w * 3], &pixels[(display_h - 1 - y) * display_w * 3], display_w * 3);
            }
            std::ofstream ppm(screenshot_path, std::ios::binary);
            if (ppm.is_open()) {
                ppm << "P6\n" << display_w << " " << display_h << "\n255\n";
                ppm.write(reinterpret_cast<const char*>(flipped.data()), flipped.size());
                ppm.close();
                std::cout << "[Screenshot] Captura guardada con éxito: " << screenshot_path << " (" << display_w << "x" << display_h << ")\n";
            }
            if (exit_after_screenshot) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        }

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
