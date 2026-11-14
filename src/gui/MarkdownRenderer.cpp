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
        case TokenType::InlineCode: {
            const ImVec2 cur = ImGui::GetCursorScreenPos();
            const ImVec2 txt_sz = ImGui::CalcTextSize(token.text.c_str());
            const float pad_x = 4.0f;
            const float pad_y = 1.0f;
            const ImVec2 badge_sz(txt_sz.x + pad_x * 2.0f, txt_sz.y + pad_y * 2.0f);

            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(cur, ImVec2(cur.x + badge_sz.x, cur.y + badge_sz.y),
                              IM_COL32(36, 48, 72, 220), 3.0f);
            dl->AddRect(cur, ImVec2(cur.x + badge_sz.x, cur.y + badge_sz.y),
                        IM_COL32(75, 120, 180, 180), 3.0f);
            dl->AddText(ImVec2(cur.x + pad_x, cur.y + pad_y),
                        IM_COL32(102, 224, 255, 255), token.text.c_str());

            ImGui::Dummy(badge_sz);
            break;
        }
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

    // Si el texto contiene saltos de línea (\n), contiene estructura multi-línea o bloques
    // Markdown (encabezados ###, listas *, tablas, etc.), delegar a render_document
    // para preservar fielmente los saltos de línea, jerarquías y listas.
    if (text.find('\n') != std::string_view::npos) {
        render_document(text);
        return;
    }

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
    const float max_x = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x - 4.0f;
    const float space_width = ImGui::CalcTextSize(" ").x;
    bool is_first_item = true;

    for (size_t t_idx = 0; t_idx < tokens.size(); ++t_idx) {
        const auto& tok = tokens[t_idx];

        if (tok.type == TokenType::InlineCode) {
            const float item_width = ImGui::CalcTextSize(tok.text.c_str()).x + 8.0f;
            const float next_x = is_first_item ? ImGui::GetCursorScreenPos().x : (ImGui::GetItemRectMax().x + 3.0f);

            if (is_first_item) {
                if (next_x + item_width > max_x && ImGui::GetCursorPosX() > ImGui::GetStyle().WindowPadding.x + 2.0f) {
                    ImGui::NewLine();
                }
            } else {
                if (next_x + item_width <= max_x) {
                    ImGui::SameLine(0.0f, 3.0f);
                }
            }
            render_single_token(tok);
            is_first_item = false;
            continue;
        }

        // Para texto normal, negrita o enlaces, separamos palabras para ajuste dinámico
        std::istringstream iss(tok.text);
        std::string word;

        while (iss >> word) {
            Token word_tok = tok;
            word_tok.text = word;
            const float word_width = ImGui::CalcTextSize(word.c_str()).x;
            const float next_x = is_first_item ? ImGui::GetCursorScreenPos().x : (ImGui::GetItemRectMax().x + space_width);

            if (is_first_item) {
                if (next_x + word_width > max_x && ImGui::GetCursorPosX() > ImGui::GetStyle().WindowPadding.x + 2.0f) {
                    ImGui::NewLine();
                }
            } else {
                if (next_x + word_width <= max_x) {
                    ImGui::SameLine(0.0f, space_width);
                }
            }

            render_single_token(word_tok);
            is_first_item = false;
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

        std::string_view trimmed = line;
        while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t')) {
            trimmed.remove_prefix(1);
        }

        // 1. Líneas vacías
        if (trimmed.empty()) {
            ImGui::Spacing();
            line_idx++;
            continue;
        }

        // 2. Bloque de código multi-línea (```)
        if (trimmed.starts_with("```")) {
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
            if (ImGui::BeginChild(child_id.c_str(), ImVec2(0, box_height), ImGuiChildFlags_Borders)) {
                ImGui::PushTextWrapPos(0.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.85f, 0.95f, 1.0f));
                ImGui::TextUnformatted(code_content.c_str());
                ImGui::PopStyleColor();
                ImGui::PopTextWrapPos();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::Spacing();
            continue;
        }

        // 3. Tablas Markdown (| ... |)
        if (trimmed.starts_with("|")) {
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

                    ImGuiTableFlags t_flags = ImGuiTableFlags_Borders |
                                              ImGuiTableFlags_RowBg |
                                              ImGuiTableFlags_Resizable |
                                              ImGuiTableFlags_SizingStretchProp;

                    if (ImGui::BeginTable(t_id.c_str(), col_count, t_flags)) {
                        for (int c = 0; c < col_count; ++c) {
                            std::string col_title = strip_markdown(headers[c]);
                            ImGui::TableSetupColumn(col_title.c_str());
                        }
                        ImGui::TableHeadersRow();

                        for (size_t r = first_data_row; r < table_rows.size(); ++r) {
                            const auto& row = table_rows[r];
                            ImGui::TableNextRow();
                            ImGui::PushID(static_cast<int>(r));
                            for (int c = 0; c < col_count; ++c) {
                                ImGui::TableNextColumn();
                                ImGui::PushID(c);
                                if (c < static_cast<int>(row.size())) {
                                    ImGui::PushTextWrapPos(0.0f);
                                    render_formatted_text(row[c], true);
                                    ImGui::PopTextWrapPos();
                                }
                                ImGui::PopID();
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndTable();
                        ImGui::Spacing();
                    }
                }
            }
            continue;
        }

        // 4. Citas en bloque (> ...)
        if (trimmed.starts_with(">")) {
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
        if (trimmed == "---" || trimmed == "***" || trimmed == "___") {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            line_idx++;
            continue;
        }

        // 6. Encabezados (#, ##, ###, ####)
        if (trimmed.starts_with("#### ")) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.65f, 0.95f, 1.0f));
            ImGui::TextWrapped("%s", strip_markdown(trimmed.substr(5)).c_str());
            ImGui::PopStyleColor();
            line_idx++;
            continue;
        }
        if (trimmed.starts_with("### ")) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.80f, 0.35f, 1.0f));
            ImGui::TextWrapped("%s", strip_markdown(trimmed.substr(4)).c_str());
            ImGui::PopStyleColor();
            line_idx++;
            continue;
        }
        if (trimmed.starts_with("## ")) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.90f, 0.60f, 1.0f));
            ImGui::TextWrapped("%s", strip_markdown(trimmed.substr(3)).c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
            line_idx++;
            continue;
        }
        if (trimmed.starts_with("# ")) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.85f, 1.0f, 1.0f));
            ImGui::TextWrapped("%s", strip_markdown(trimmed.substr(2)).c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Spacing();
            line_idx++;
            continue;
        }

        // 7. Listas con viñetas (* o - o +)
        if (trimmed.starts_with("* ") || trimmed.starts_with("- ") || trimmed.starts_with("+ ")) {
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::BeginGroup();
            render_formatted_text(trimmed.substr(2), true);
            ImGui::EndGroup();
            line_idx++;
            continue;
        }

        // 8. Listas numeradas (1. , 2. , etc.)
        const size_t dot = trimmed.find(". ");
        if (dot != std::string_view::npos && dot <= 3 &&
            std::all_of(trimmed.begin(), trimmed.begin() + static_cast<long>(dot), ::isdigit)) {
            const std::string num = std::string(trimmed.substr(0, dot + 1));
            ImGui::TextDisabled("%s", num.c_str());
            ImGui::SameLine();
            ImGui::BeginGroup();
            render_formatted_text(trimmed.substr(dot + 2), true);
            ImGui::EndGroup();
            line_idx++;
            continue;
        }

        // 9. Párrafo estándar
        render_formatted_text(trimmed, true);
        line_idx++;
    }
}

} // namespace intelidata::gui
