#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include "intelidata/pipeline/Engine.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <functional>

namespace intelidata::gui {

enum class IngestionSourceType {
    None,
    FilePath,
    MemorySQL,
    PresetDemo
};

struct IngestionRequest {
    IngestionSourceType type{IngestionSourceType::None};
    std::string path;
    std::string sql_code;
    pipeline::EngineOptions options;
};

/**
 * @brief Centro de Ingesta y Conexión de Datos:
 * Proporciona un explorador de archivos visual del sistema de archivos,
 * presets de datasets demostrativos con un solo clic, un scratchpad SQL
 * para análisis directo en memoria y ajuste interactivo de los parámetros del motor.
 */
class DataHubView {
public:
    DataHubView();

    // Renderiza la ventana o modal del Data Hub
    void render(bool* p_open = nullptr);

    // Consulta si hay una solicitud de análisis pendiente
    [[nodiscard]] bool has_pending_request() const noexcept { return has_request_; }

    // Obtiene la solicitud y limpia la bandera pendiente
    [[nodiscard]] IngestionRequest consume_request();

    // Actualiza la ruta activa actual
    void set_active_path(const std::string& path);

    // Ajusta las opciones actuales del motor
    [[nodiscard]] const pipeline::EngineOptions& options() const noexcept { return options_; }

private:
    void render_file_explorer();
    void render_presets();
    void render_sql_scratchpad();
    void render_engine_settings();

    void refresh_file_list();

    std::filesystem::path current_dir_;
    struct FileEntry {
        std::string filename;
        std::string full_path;
        bool is_directory{false};
        std::uintmax_t size{0};
    };
    std::vector<FileEntry> file_entries_;
    char path_input_buffer_[512]{""};
    char sql_scratchpad_buffer_[4096]{""};

    pipeline::EngineOptions options_;
    int selected_tab_{0};
    bool has_request_{false};
    IngestionRequest pending_request_;
    std::string active_path_;
};

} // namespace intelidata::gui
