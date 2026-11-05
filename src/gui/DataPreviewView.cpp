#include "intelidata/gui/DataPreviewView.hpp"
#include <imgui.h>
#include <algorithm>
#include <cctype>

namespace intelidata::gui {

void DataPreviewView::set_preview_data(ingestion::TablePreviewData data) {
    preview_data_ = std::move(data);
}

void DataPreviewView::render(const char* title, const std::vector<std::string>& available_tables, std::string& current_selected_table) {
    if (!ImGui::Begin(title)) {
        ImGui::End();
        return;
    }

    // Cabecera con selector de tabla
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Tabla:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220.0f);

    if (ImGui::BeginCombo("##table_preview_combo", current_selected_table.empty() ? "(Seleccionar tabla)" : current_selected_table.c_str())) {
        for (const auto& t_name : available_tables) {
            const bool is_selected = (current_selected_table == t_name);
            if (ImGui::Selectable(t_name.c_str(), is_selected)) {
                current_selected_table = t_name;
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("| %zu columnas | %zu filas cargadas", preview_data_.column_names.size(), preview_data_.rows.size());

    ImGui::SameLine(ImGui::GetWindowWidth() - 250);
    ImGui::SetNextItemWidth(230.0f);
    ImGui::InputTextWithHint("##filter_preview", "Filtrar en filas...", filter_buffer_, sizeof(filter_buffer_));

    ImGui::Separator();

    if (preview_data_.column_names.empty() || preview_data_.rows.empty()) {
        ImGui::TextDisabled("No hay datos de previsualizacion disponibles para la tabla seleccionada.");
        ImGui::End();
        return;
    }

    // Preparar filtro insensible a mayúsculas
    std::string filter_str = filter_buffer_;
    std::transform(filter_str.begin(), filter_str.end(), filter_str.begin(), ::tolower);

    // Grid tabular con barra de desplazamiento horizontal y vertical
    const int total_columns = static_cast<int>(preview_data_.column_names.size()) + 1; // +1 para índice de fila
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders |
                                  ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_ScrollX |
                                  ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_Reorderable;

    if (ImGui::BeginTable("DataGridTable", total_columns, flags, ImVec2(0, 0))) {
        // Columna de índice
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 45.0f);

        // Columnas de datos
        for (const auto& col_name : preview_data_.column_names) {
            ImGui::TableSetupColumn(col_name.c_str(), ImGuiTableColumnFlags_WidthStretch);
        }
        ImGui::TableHeadersRow();

        int row_num = 1;
        for (const auto& row : preview_data_.rows) {
            // Comprobar filtro si está activo
            if (!filter_str.empty()) {
                bool matches = false;
                for (const auto& cell : row) {
                    std::string lower_cell = cell;
                    std::transform(lower_cell.begin(), lower_cell.end(), lower_cell.begin(), ::tolower);
                    if (lower_cell.find(filter_str) != std::string::npos) {
                        matches = true;
                        break;
                    }
                }
                if (!matches) {
                    row_num++;
                    continue;
                }
            }

            ImGui::TableNextRow();

            // Columna de índice
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("%d", row_num);

            // Celdas de datos
            for (size_t c = 0; c < row.size(); ++c) {
                ImGui::TableSetColumnIndex(static_cast<int>(c + 1));
                const auto& val = row[c];
                if (val == "NULL" || val.empty()) {
                    ImGui::TextColored(ImVec4(0.85f, 0.35f, 0.35f, 0.8f), "NULL");
                } else {
                    ImGui::TextUnformatted(val.c_str());
                }
            }
            row_num++;
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace intelidata::gui
