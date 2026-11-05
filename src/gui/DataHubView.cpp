#include "intelidata/gui/DataHubView.hpp"
#include <imgui.h>
#include <cstring>
#include <algorithm>
#include <iostream>

namespace intelidata::gui {

namespace fs = std::filesystem;

DataHubView::DataHubView() {
    try {
        current_dir_ = fs::current_path();
        if (fs::exists(current_dir_ / "samples")) {
            current_dir_ = current_dir_ / "samples";
        }
    } catch (...) {
        current_dir_ = ".";
    }

    refresh_file_list();

    const char* default_sql =
        "CREATE TABLE t_clientes (id_cliente BIGINT, nombre VARCHAR, email VARCHAR, nivel VARCHAR);\n"
        "INSERT INTO t_clientes VALUES\n"
        "  (1, 'Acme Industrial', 'contacto@acme.corp', 'ENTERPRISE'),\n"
        "  (2, 'Globex Internacional', 'ventas@globex.com', 'PREMIUM'),\n"
        "  (3, 'Initech Software', 'soporte@initech.io', 'STANDARD');\n\n"
        "CREATE TABLE t_facturas (id_factura BIGINT, id_cliente BIGINT, monto DOUBLE, estado VARCHAR);\n"
        "INSERT INTO t_facturas VALUES\n"
        "  (1001, 1, 4590.80, 'PAGADO'),\n"
        "  (1002, 2, 1250.00, 'PENDIENTE'),\n"
        "  (1003, 1, 340.50, 'PAGADO'),\n"
        "  (1004, 3, 89.00, 'CANCELADO');\n";

    std::snprintf(sql_scratchpad_buffer_, sizeof(sql_scratchpad_buffer_), "%s", default_sql);
}

void DataHubView::set_active_path(const std::string& path) {
    active_path_ = path;
    std::snprintf(path_input_buffer_, sizeof(path_input_buffer_), "%s", path.c_str());
}

IngestionRequest DataHubView::consume_request() {
    has_request_ = false;
    return std::move(pending_request_);
}

void DataHubView::refresh_file_list() {
    file_entries_.clear();
    try {
        if (!fs::exists(current_dir_)) return;

        // Añadir entrada para subir de directorio
        if (current_dir_.has_parent_path()) {
            file_entries_.push_back({
                ".. (Subir directorio)",
                current_dir_.parent_path().string(),
                true,
                0
            });
        }

        std::vector<FileEntry> dirs;
        std::vector<FileEntry> files;

        for (const auto& entry : fs::directory_iterator(current_dir_, fs::directory_options::skip_permission_denied)) {
            const auto& p = entry.path();
            const std::string filename = p.filename().string();
            if (filename.starts_with(".")) continue; // Ignorar ocultos

            if (entry.is_directory()) {
                dirs.push_back({filename, p.string(), true, 0});
            } else if (entry.is_regular_file()) {
                const std::string ext = p.extension().string();
                if (ext == ".csv" || ext == ".parquet" || ext == ".sqlite" || ext == ".db") {
                    files.push_back({filename, p.string(), false, entry.file_size()});
                }
            }
        }

        std::sort(dirs.begin(), dirs.end(), [](const auto& a, const auto& b) { return a.filename < b.filename; });
        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.filename < b.filename; });

        for (auto& d : dirs) file_entries_.push_back(std::move(d));
        for (auto& f : files) file_entries_.push_back(std::move(f));
    } catch (const std::exception& e) {
        std::cerr << "Error navegando archivos: " << e.what() << "\n";
    }
}

void DataHubView::render(bool* p_open) {
    ImGui::SetNextWindowSize(ImVec2(780, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Centro de Ingesta y Conexion de Datos", p_open)) {
        ImGui::End();
        return;
    }

    if (!active_path_.empty()) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Origen Activo:");
        ImGui::SameLine();
        ImGui::TextUnformatted(active_path_.c_str());
        ImGui::Separator();
    }

    if (ImGui::BeginTabBar("DataHubTabs")) {
        if (ImGui::BeginTabItem("Explorador de Archivos")) {
            render_file_explorer();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Datasets de Demostracion")) {
            render_presets();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("SQL Scratchpad")) {
            render_sql_scratchpad();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Ajustes del Motor")) {
            render_engine_settings();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void DataHubView::render_file_explorer() {
    ImGui::TextDisabled("Directorio actual: %s", current_dir_.string().c_str());
    ImGui::Spacing();

    // Barra de navegación rápida
    if (ImGui::Button("Ir a Proyecto")) {
        current_dir_ = fs::current_path();
        refresh_file_list();
    }
    ImGui::SameLine();
    if (ImGui::Button("Ir a samples/")) {
        current_dir_ = fs::current_path() / "samples";
        refresh_file_list();
    }
    ImGui::SameLine();
    if (ImGui::Button("Actualizar Lista")) {
        refresh_file_list();
    }

    ImGui::Separator();

    // Tabla de archivos
    const ImVec2 table_size(0, ImGui::GetContentRegionAvail().y - 80);
    if (ImGui::BeginTable("FileTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, table_size)) {
        ImGui::TableSetupColumn("Nombre de Archivo / Carpeta", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Tipo", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Tamano", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableHeadersRow();

        for (const auto& item : file_entries_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            if (item.is_directory) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.4f, 1.0f));
                if (ImGui::Selectable(item.filename.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                    current_dir_ = fs::path(item.full_path);
                    refresh_file_list();
                    ImGui::PopStyleColor();
                    break;
                }
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("Carpeta");
                ImGui::TableSetColumnIndex(2);
                ImGui::TextDisabled("-");
            } else {
                const bool is_selected = (path_input_buffer_ == item.full_path);
                if (ImGui::Selectable(item.filename.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    std::snprintf(path_input_buffer_, sizeof(path_input_buffer_), "%s", item.full_path.c_str());
                }

                ImGui::TableSetColumnIndex(1);
                const std::string ext = fs::path(item.full_path).extension().string();
                ImGui::TextUnformatted(ext.c_str());

                ImGui::TableSetColumnIndex(2);
                if (item.size < 1024) {
                    ImGui::Text("%lu B", item.size);
                } else if (item.size < 1024 * 1024) {
                    ImGui::Text("%.1f KB", item.size / 1024.0f);
                } else {
                    ImGui::Text("%.2f MB", item.size / (1024.0f * 1024.0f));
                }
            }
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::Text("Ruta del archivo seleccionado:");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 170);
    ImGui::InputText("##selpath", path_input_buffer_, sizeof(path_input_buffer_));
    ImGui::SameLine();

    ImGui::BeginDisabled(std::strlen(path_input_buffer_) == 0);
    if (ImGui::Button("Cargar y Analizar", ImVec2(160, 0))) {
        pending_request_.type = IngestionSourceType::FilePath;
        pending_request_.path = path_input_buffer_;
        pending_request_.options = options_;
        has_request_ = true;
    }
    ImGui::EndDisabled();
}

void DataHubView::render_presets() {
    ImGui::TextWrapped("Selecciona un conjunto de datos preconfigurado para evaluar las capacidades matematicas de InteliData de forma instantanea:");
    ImGui::Spacing();
    ImGui::Separator();

    auto render_preset_card = [&](const char* title, const char* desc, const char* path, const char* btn_text) {
        ImGui::BeginGroup();
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.6f, 1.0f), "%s", title);
        ImGui::TextWrapped("%s", desc);
        ImGui::TextDisabled("Ruta: %s", path);
        if (ImGui::Button(btn_text, ImVec2(220, 30))) {
            pending_request_.type = IngestionSourceType::FilePath;
            pending_request_.path = path;
            pending_request_.options = options_;
            has_request_ = true;
        }
        ImGui::EndGroup();
        ImGui::Separator();
    };

    render_preset_card(
        "1. E-Commerce Multitabla Ofuscado (SQLite)",
        "Base de datos con 4 tablas con nombres ofuscados (tbl_001_usr, tbl_002_ord, tbl_003_itm, tbl_004_log). Contiene dependencias PIND exactas, montos bajo Ley de Benford, Cadenas de Markov y particion espectral en 2 subdominios.",
        "samples/ecommerce_obfuscated.db",
        "Cargar E-Commerce Ofuscado"
    );

    render_preset_card(
        "2. Transacciones Financieras (CSV)",
        "Montos monetarios distribuidos logaritmicamente que verifican la Ley de Benford, acompanados de estados de ciclo de vida.",
        "samples/samples_transacciones.csv",
        "Cargar Transacciones CSV"
    );

    render_preset_card(
        "3. Muestra Rapida de Clientes y Pedidos (CSV)",
        "Dataset plano y compacto para verificacion rapida de unicidad, calculo de Welford y tiempo de respuesta.",
        "samples/sample.csv",
        "Cargar Muestra Ligera CSV"
    );
}

void DataHubView::render_sql_scratchpad() {
    ImGui::TextWrapped("Escribe o pega sentencias SQL DDL y DML para crear un esquema interactivo en memoria RAM y analizarlo inmediatamente:");
    ImGui::Spacing();

    const ImVec2 text_size(0, ImGui::GetContentRegionAvail().y - 50);
    ImGui::InputTextMultiline("##sql_pad", sql_scratchpad_buffer_, sizeof(sql_scratchpad_buffer_), text_size, ImGuiInputTextFlags_AllowTabInput);

    if (ImGui::Button("Ejecutar y Analizar Esquema en RAM", ImVec2(280, 32))) {
        pending_request_.type = IngestionSourceType::MemorySQL;
        pending_request_.sql_code = sql_scratchpad_buffer_;
        pending_request_.options = options_;
        has_request_ = true;
    }
}

void DataHubView::render_engine_settings() {
    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Parametros de Calibracion del Motor Ontologico:");
    ImGui::Spacing();

    int sample_size = static_cast<int>(options_.max_sample_size);
    if (ImGui::SliderInt("Tamano Maximo de Muestra (Filas)", &sample_size, 500, 50000, "%d registros")) {
        options_.max_sample_size = static_cast<Size>(sample_size);
    }
    ImGui::TextDisabled("Capacidad del Reservoir Sampler por columna para garantizar tiempo interactivo.");

    float fk_threshold = static_cast<float>(options_.fk_inclusion_threshold);
    if (ImGui::SliderFloat("Umbral de Inclusion PIND (FK)", &fk_threshold, 0.80f, 1.00f, "%.2f")) {
        options_.fk_inclusion_threshold = static_cast<Float64>(fk_threshold);
    }
    ImGui::TextDisabled("Tolerancia minima de inclusividad de conjuntos para inferir una clave foranea valida.");

    float benford_a = static_cast<float>(options_.benford_alpha);
    if (ImGui::SliderFloat("Nivel de Significancia Benford (Alpha)", &benford_a, 0.01f, 0.15f, "%.2f")) {
        options_.benford_alpha = static_cast<Float64>(benford_a);
    }
    ImGui::TextDisabled("Umbral para el p-value de chi-cuadrado para declarar conformidad con la Ley de Benford.");

    float damping = static_cast<float>(options_.pagerank_damping);
    if (ImGui::SliderFloat("Factor de Amortiguacion PageRank (Damping)", &damping, 0.50f, 0.95f, "%.2f")) {
        options_.pagerank_damping = static_cast<Float64>(damping);
    }
    ImGui::TextDisabled("Probabilidad estocastica de teletransportacion en la gravedad relacional del esquema.");

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Restablecer Valores Predeterminados", ImVec2(260, 0))) {
        options_ = pipeline::EngineOptions{};
    }
}

} // namespace intelidata::gui
