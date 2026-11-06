#pragma once

#include "intelidata/common/Platform.hpp"
#include <string_view>
#include <string>

namespace intelidata::gui {

class MarkdownRenderer {
public:
    /// Renderiza un documento Markdown completo utilizando widgets nativos de ImGui (encabezados, citas, listas y tablas)
    static void render_document(std::string_view markdown);

    /// Renderiza un párrafo o texto corto interpretando negrita (**texto**), código (`codigo`) y citas
    static void render_formatted_text(std::string_view text, bool wrapped = true);

    /// Elimina sintaxis markdown cruda (** `) para visualización en texto simple
    [[nodiscard]] static std::string strip_markdown(std::string_view text);
};

} // namespace intelidata::gui
