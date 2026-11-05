#include "intelidata/gui/GraphCanvas.hpp"
#include "intelidata/gui/InspectorView.hpp"
#include "intelidata/pipeline/Engine.hpp"
#include "intelidata/ingestion/DuckDBAdapter.hpp"
#include <iostream>
#include <string>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/gl.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
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
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Habilitar sistema de acoplamiento

    ImGui::StyleColorsDark();

    // Personalización moderna del estilo oscuro
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.GrabRounding = 3.0f;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Componentes de la Interfaz
    intelidata::gui::GraphCanvas canvas;
    intelidata::gui::InspectorView inspector;
    intelidata::model::SemanticAST current_ast;
    char input_file_buffer[512] = "";
    std::string status_message = "Listo.";
    int max_frames = -1; // -1 = infinito, > 0 = modo prueba / CI

    auto load_dataset = [&](const std::string& path) {
        try {
            intelidata::ingestion::DuckDBAdapter adapter(path);
            intelidata::pipeline::Engine engine;
            current_ast = engine.analyze(adapter);
            canvas.load_from_ast(current_ast);
            status_message = "Cargado con exito: " + path + " (" +
                             std::to_string(current_ast.tables.size()) + " tablas analizadas)";
        } catch (const std::exception& e) {
            status_message = std::string("Error al cargar: ") + e.what();
            std::cerr << status_message << std::endl;
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

    if (!initial_input.empty()) {
        std::snprintf(input_file_buffer, sizeof(input_file_buffer), "%s", initial_input.c_str());
        load_dataset(initial_input);
    } else {
        // Cargar un esquema de demostración inicial en memoria
        try {
            intelidata::ingestion::DuckDBAdapter demo_adapter(":memory:");
            demo_adapter.execute("CREATE TABLE clientes (id_cliente BIGINT, nombre VARCHAR);");
            demo_adapter.execute("INSERT INTO clientes VALUES (1, 'Acme Corp'), (2, 'Globex'), (3, 'Initech');");

            demo_adapter.execute("CREATE TABLE ordenes (id_orden BIGINT, id_cliente BIGINT, total DOUBLE, estado VARCHAR);");
            demo_adapter.execute("INSERT INTO ordenes VALUES (101, 1, 1499.50, 'ENTREGADO'), (102, 2, 299.00, 'EN_PROCESO'), (103, 1, 45.00, 'CANCELADO');");

            intelidata::pipeline::Engine engine;
            current_ast = engine.analyze(demo_adapter);
            canvas.load_from_ast(current_ast);
            status_message = "Esquema demo en memoria cargado correctamente.";
        } catch (const std::exception& e) {
            status_message = std::string("Error demo: ") + e.what();
        }
    }

    bool show_open_dialog = false;
    int frame_count = 0;

    // Bucle Principal de Renderizado
    while (!glfwWindowShouldClose(window)) {
        if (max_frames > 0 && frame_count >= max_frames) {
            break;
        }
        frame_count++;

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Ventana principal DockSpace a pantalla completa
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // Barra de menú superior
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Archivo")) {
                if (ImGui::MenuItem("Abrir Origen de Datos... (CSV/Parquet/SQLite)", "Ctrl+O")) {
                    show_open_dialog = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Salir", "Alt+F4")) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Ayuda")) {
                ImGui::MenuItem("InteliData v0.1.0 - Motor Ontologico C++20", nullptr, false, false);
                ImGui::EndMenu();
            }
            ImGui::SameLine(ImGui::GetWindowWidth() - 400);
            ImGui::TextDisabled("%s", status_message.c_str());
            ImGui::EndMainMenuBar();
        }

        // Modal o Diálogo para Abrir Archivo
        if (show_open_dialog) {
            ImGui::OpenPopup("Abrir Origen de Datos");
        }
        if (ImGui::BeginPopupModal("Abrir Origen de Datos", &show_open_dialog, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Ruta del archivo (CSV, Parquet o base SQLite):");
            ImGui::InputText("##filepath", input_file_buffer, sizeof(input_file_buffer));
            ImGui::Separator();
            if (ImGui::Button("Cargar y Analizar", ImVec2(150, 0))) {
                load_dataset(input_file_buffer);
                show_open_dialog = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancelar", ImVec2(100, 0))) {
                show_open_dialog = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // 1. Renderizar Canvas del Grafo Ontológico
        canvas.render("Grafo Ontologico (Fisica GPU)", 800.0f, 600.0f);

        // 2. Renderizar Inspector Estadístico (ImPlot)
        inspector.render_inspector("Inspector Estadistico (ImPlot)", current_ast, canvas.selected_node());

        // 3. Renderizar Visor de DDL y Documentación
        inspector.render_ddl_viewer("Visor de Desofuscacion DDL / Markdown", current_ast);

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
