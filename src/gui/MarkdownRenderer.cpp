#include "intelidata/gui/MarkdownRenderer.hpp"

#include <imgui.h>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace intelidata::gui {

namespace {

enum class TokenType {
    Normal,
    Bold,
    InlineCode,
    Link
};

struct Token {
    TokenType type;
    std::string text;
    std::string link_url;
};

std::vector<Token> tokenize_inline(std::string_view text) {
    std::vector<Token> tokens;
    size_t i = 0;
    size_t start = 0;

    auto flush_normal = [&](size_t end) {
        if (end > start) {
            tokens.push_back({TokenType::Normal, std::string(text.substr(start, end - start)), ""});
        }
        start = end;
    };

    while (i < text.size()) {
        if (text[i] == '*' && i + 1 < text.size() && text[i + 1] == '*') {
            // Negrita **texto**
            size_t close = text.find("**", i + 2);
            if (close != std::string_view::npos) {
                flush_normal(i);
                tokens.push_back({TokenType::Bold, std::string(text.substr(i + 2, close - (i + 2))), ""});
                i = close + 2;
                start = i;
                continue;
            }
        } else if (text[i] == '`') {
            // Código inline `codigo`
            size_t close = text.find('`', i + 1);
            if (close != std::string_view::npos) {
                flush_normal(i);
                tokens.push_back({TokenType::InlineCode, std::string(text.substr(i + 1, close - (i + 1))), ""});
                i = close + 1;
                start = i;
                continue;
            }
        } else if (text[i] == '[') {
            // Enlace [texto](url)
            size_t close_bracket = text.find(']', i + 1);
            if (close_bracket != std::string_view::npos &&
                close_bracket + 1 < text.size() && text[close_bracket + 1] == '(') {
                size_t close_paren = text.find(')', close_bracket + 2);
                if (close_paren != std::string_view::npos) {
                    flush_normal(i);
                    tokens.push_back({
                        TokenType::Link,
                        std::string(text.substr(i + 1, close_bracket - (i + 1))),
                        std::string(text.substr(close_bracket + 2, close_paren - (close_bracket + 2)))
                    });
                    i = close_paren + 1;
                    start = i;
                    continue;
                }
            }
        }
        i++;
    }
    flush_normal(text.size());
    return tokens;
}

void render_single_token(const Token& token) {
    switch (token.type) {
        case TokenType::Normal:
            ImGui::TextUnformatted(token.text.c_str());
            break;
        case TokenType::Bold:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.95f, 0.65f, 1.0f));
            ImGui::TextUnformatted(token.text.c_str());
            ImGui::PopStyleColor();
            break;
        case TokenType::InlineCode:
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.24f, 0.35f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.24f, 0.35f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.24f, 0.35f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.85f, 1.0f, 1.0f));
            ImGui::SmallButton(token.text.c_str());
            ImGui::PopStyleColor(4);
            break;
        case TokenType::Link:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.75f, 1.0f, 1.0f));
            ImGui::TextUnformatted(token.text.c_str());
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", token.link_url.c_str());
            }
            break;
    }
}

std::vector<std::string> parse_table_row(std::string_view line) {
    std::vector<std::string> cells;
    size_t start = 0;
    while (start < line.size() && (line[start] == ' ' || line[start] == '|')) {
        start++;
    }
    while (start < line.size()) {
        size_t pipe = line.find('|', start);
        if (pipe == std::string_view::npos) {
            std::string_view cell = line.substr(start);
            while (!cell.empty() && (cell.back() == ' ' || cell.back() == '\r')) cell.remove_suffix(1);
            while (!cell.empty() && cell.front() == ' ') cell.remove_prefix(1);
            if (!cell.empty()) cells.emplace_back(cell);
            break;
        }
        std::string_view cell = line.substr(start, pipe - start);
        while (!cell.empty() && cell.back() == ' ') cell.remove_suffix(1);
        while (!cell.empty() && cell.front() == ' ') cell.remove_prefix(1);
        cells.emplace_back(cell);
        start = pipe + 1;
        while (start < line.size() && line[start] == ' ') start++;
    }
    return cells;
}

bool is_separator_row(const std::vector<std::string>& cells) {
    if (cells.empty()) return false;
    for (const auto& c : cells) {
        if (c.empty()) continue;
        for (char ch : c) {
            if (ch != '-' && ch != ':' && ch != ' ') return false;
        }
    }
    return true;
}

} // namespace

void MarkdownRenderer::render_formatted_text(std::string_view text, bool wrapped) {
    if (text.empty()) return;

    auto tokens = tokenize_inline(text);
    if (tokens.empty()) return;

    // Optimización rápida para párrafos planos
    if (tokens.size() == 1 && tokens[0].type == TokenType::Normal) {
        if (wrapped) {
            ImGui::TextWrapped("%s", tokens[0].text.c_str());
        } else {
            ImGui::TextUnformatted(tokens[0].text.c_str());
        }
        return;
    }

    if (!wrapped) {
        for (size_t idx = 0; idx < tokens.size(); ++idx) {
            if (idx > 0) {
                ImGui::SameLine(0.0f, 0.0f);
            }
            render_single_token(tokens[idx]);
        }
        return;
    }

    // Renderizado con control de saltos de línea adaptativo para texto enriquecido
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float cur_start_x = ImGui::GetCursorScreenPos().x;
    const float window_right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    const float right_bound = (avail_w > 40.0f) ? std::min(cur_start_x + avail_w, window_right) : window_right;
    const float space_width = ImGui::CalcTextSize(" ").x;

    for (size_t t_idx = 0; t_idx < tokens.size(); ++t_idx) {
        const auto& tok = tokens[t_idx];

        if (tok.type == TokenType::InlineCode) {
            const float item_width = ImGui::CalcTextSize(tok.text.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
            const float cur_x = ImGui::GetCursorScreenPos().x;
            if (t_idx > 0 && cur_x + item_width > right_bound && cur_x > cur_start_x + 5.0f) {
                // Siguiente línea
            } else if (t_idx > 0) {
                ImGui::SameLine(0.0f, 3.0f);
            }
            render_single_token(tok);
            continue;
        }

        // Para texto normal, negrita o enlaces, separamos palabras para ajuste dinámico
        std::istringstream iss(tok.text);
        std::string word;
        bool first_word = true;

        while (iss >> word) {
            Token word_tok = tok;
            word_tok.text = word;
            const float word_width = ImGui::CalcTextSize(word.c_str()).x;
            const float cur_x = ImGui::GetCursorScreenPos().x;

            if (t_idx == 0 && first_word) {
                // Primer token de la línea
            } else if (cur_x + word_width + space_width > right_bound && cur_x > cur_start_x + 5.0f) {
                // Salto a nueva línea
            } else {
                ImGui::SameLine(0.0f, space_width);
            }

            render_single_token(word_tok);
            first_word = false;
        }
    }
}

std::string MarkdownRenderer::strip_markdown(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '*' || text[i] == '`') {
            i++;
            continue;
        }
        if (text[i] == '_' && i + 1 < text.size() && text[i + 1] == '_') {
            i += 2;
            continue;
        }
        if (text[i] == '[' && i + 1 < text.size()) {
            size_t close_bracket = text.find(']', i + 1);
            if (close_bracket != std::string_view::npos &&
                close_bracket + 1 < text.size() && text[close_bracket + 1] == '(') {
                size_t close_paren = text.find(')', close_bracket + 2);
                if (close_paren != std::string_view::npos) {
                    result.append(text.substr(i + 1, close_bracket - (i + 1)));
                    i = close_paren + 1;
                    continue;
                }
            }
        }
        result.push_back(text[i++]);
    }
    return result;
}

void MarkdownRenderer::render_document(std::string_view markdown) {
    if (markdown.empty()) return;

    std::vector<std::string_view> lines;
    size_t start = 0;
    while (start < markdown.size()) {
        size_t end = markdown.find('\n', start);
        if (end == std::string_view::npos) {
            std::string_view line = markdown.substr(start);
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            lines.push_back(line);
            break;
        }
        std::string_view line = markdown.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        lines.push_back(line);
        start = end + 1;
    }

    int table_counter = 0;
    int block_counter = 0;
    int quote_counter = 0;

    size_t line_idx = 0;
    while (line_idx < lines.size()) {
        const auto& line = lines[line_idx];

        // 1. Líneas vacías
        if (line.empty()) {
            ImGui::Spacing();
            line_idx++;
            continue;
        }

        // 2. Bloque de código multi-línea (```)
        if (line.starts_with("```")) {
            std::string code_content;
            line_idx++;
            while (line_idx < lines.size() && !lines[line_idx].starts_with("```")) {
                code_content.append(lines[line_idx]);
                code_content.push_back('\n');
                line_idx++;
            }
            if (line_idx < lines.size()) line_idx++; // Consumir ``` de cierre

            const float line_count = static_cast<float>(std::count(code_content.begin(), code_content.end(), '\n') + 1);
            const float box_height = std::min(260.0f, std::max(40.0f, line_count * ImGui::GetTextLineHeightWithSpacing() + 16.0f));

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.09f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.25f, 0.35f, 0.7f));
            const std::string child_id = "##code_block_" + std::to_string(block_counter++);
            if (ImGui::BeginChild(child_id.c_str(), ImVec2(0, box_height), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar)) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.85f, 0.95f, 1.0f));
                ImGui::TextUnformatted(code_content.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::Spacing();
            continue;
        }

        // 3. Tablas Markdown (| ... |)
        if (line.starts_with("|")) {
            std::vector<std::vector<std::string>> table_rows;
            while (line_idx < lines.size() && lines[line_idx].starts_with("|")) {
                table_rows.push_back(parse_table_row(lines[line_idx]));
                line_idx++;
            }

            if (!table_rows.empty()) {
                const auto& headers = table_rows[0];
                size_t first_data_row = 1;
                if (table_rows.size() > 1 && is_separator_row(table_rows[1])) {
                    first_data_row = 2;
                }

                const int col_count = static_cast<int>(headers.size());
                if (col_count > 0) {
                    const std::string t_id = "##md_table_" + std::to_string(table_counter++);
                    const float avail_table_w = ImGui::GetContentRegionAvail().x;

                    ImGuiTableFlags t_flags = ImGuiTableFlags_Borders |
                                              ImGuiTableFlags_RowBg |
                                              ImGuiTableFlags_Resizable;

                    // Si hay pocas columnas o suficiente espacio horizontal, ajustar al ancho exacto del panel
                    if (col_count <= 3 || avail_table_w >= static_cast<float>(col_count) * 110.0f) {
                        t_flags |= ImGuiTableFlags_SizingStretchProp;
                    } else {
                        // En paneles estrechos con muchas columnas, permitir scroll horizontal limpio interno
                        t_flags |= ImGuiTableFlags_ScrollX;
                    }

                    if (ImGui::BeginTable(t_id.c_str(), col_count, t_flags)) {
                        for (int c = 0; c < col_count; ++c) {
                            std::string col_title = strip_markdown(headers[c]);
                            ImGui::TableSetupColumn(col_title.c_str());
                        }
                        ImGui::TableHeadersRow();

                        for (size_t r = first_data_row; r < table_rows.size(); ++r) {
                            const auto& row = table_rows[r];
                            ImGui::TableNextRow();
                            for (int c = 0; c < col_count; ++c) {
                                ImGui::TableNextColumn();
                                if (c < static_cast<int>(row.size())) {
                                    ImGui::PushTextWrapPos(0.0f);
                                    render_formatted_text(row[c], true);
                                    ImGui::PopTextWrapPos();
                                }
                            }
                        }
                        ImGui::EndTable();
                        ImGui::Spacing();
                    }
                }
            }
            continue;
        }

        // 4. Citas en bloque (> ...)
        if (line.starts_with(">")) {
            std::vector<std::string> quote_lines;
            while (line_idx < lines.size() && lines[line_idx].starts_with(">")) {
                std::string_view q = lines[line_idx].substr(1);
                while (!q.empty() && q.front() == ' ') q.remove_prefix(1);
                quote_lines.emplace_back(q);
                line_idx++;
            }

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.16f, 0.22f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.55f, 0.85f, 1.0f));
            const std::string q_id = "##quote_" + std::to_string(quote_counter++);
            if (ImGui::BeginChild(q_id.c_str(), ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY)) {
                for (const auto& q_line : quote_lines) {
                    render_formatted_text(q_line, true);
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::Spacing();
            continue;
        }

        // 5. Separadores horizontales (--- o ***)
        if (line == "---" || line == "***" || line == "___") {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            line_idx++;
            continue;
        }

        // 6. Encabezados (#, ##, ###, ####)
        if (line.starts_with("#### ")) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.95f, 1.0f), "%s", strip_markdown(line.substr(5)).c_str());
            line_idx++;
            continue;
        }
        if (line.starts_with("### ")) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.35f, 1.0f), "%s", strip_markdown(line.substr(4)).c_str());
            line_idx++;
            continue;
        }
        if (line.starts_with("## ")) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.60f, 1.0f), "%s", strip_markdown(line.substr(3)).c_str());
            ImGui::Spacing();
            line_idx++;
            continue;
        }
        if (line.starts_with("# ")) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.30f, 0.85f, 1.0f, 1.0f), "%s", strip_markdown(line.substr(2)).c_str());
            ImGui::Separator();
            ImGui::Spacing();
            line_idx++;
            continue;
        }

        // 7. Listas con viñetas (* o - o +)
        if ((line.starts_with("* ") || line.starts_with("- ") || line.starts_with("+ "))) {
            ImGui::Bullet();
            ImGui::SameLine();
            render_formatted_text(line.substr(2), true);
            line_idx++;
            continue;
        }

        // 8. Listas numeradas (1. , 2. , etc.)
        const size_t dot = line.find(". ");
        if (dot != std::string_view::npos && dot <= 3 &&
            std::all_of(line.begin(), line.begin() + static_cast<long>(dot), ::isdigit)) {
            const std::string num = std::string(line.substr(0, dot + 1));
            ImGui::TextDisabled("%s", num.c_str());
            ImGui::SameLine();
            render_formatted_text(line.substr(dot + 2), true);
            line_idx++;
            continue;
        }

        // 9. Párrafo estándar
        render_formatted_text(line, true);
        line_idx++;
    }
}

} // namespace intelidata::gui
