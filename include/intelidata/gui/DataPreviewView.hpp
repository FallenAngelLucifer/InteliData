#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include "intelidata/ingestion/DataAdapter.hpp"
#include <string>
#include <vector>

namespace intelidata::gui {

/**
 * @brief Visor de Datos Crudos:
 * Muestra las filas y columnas reales del origen de datos para la tabla seleccionada,
 * permitiendo validar y contrastar los datos brutos con las inferencias ontológicas.
 */
class DataPreviewView {
public:
    DataPreviewView() = default;

    // Actualiza los datos de la tabla a mostrar
    void set_preview_data(ingestion::TablePreviewData data);

    // Renderiza la vista de datos tabulares
    void render(const char* title, const std::vector<std::string>& available_tables, std::string& current_selected_table);

    // Consulta la tabla actualmente cargada en el visor
    [[nodiscard]] const std::string& loaded_table() const noexcept { return preview_data_.table_name; }

private:
    ingestion::TablePreviewData preview_data_;
    char filter_buffer_[128]{""};
};

} // namespace intelidata::gui
