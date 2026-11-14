#include <catch2/catch_test_macros.hpp>
#include "intelidata/gui/MarkdownRenderer.hpp"
#include <imgui.h>

TEST_CASE("MarkdownRenderer: strip_markdown elimina marcas de formato", "[gui][markdown]") {
    using intelidata::gui::MarkdownRenderer;

    SECTION("Texto simple sin formato") {
        REQUIRE(MarkdownRenderer::strip_markdown("Hola Mundo") == "Hola Mundo");
    }

    SECTION("Eliminacion de negrita (**)") {
        REQUIRE(MarkdownRenderer::strip_markdown("Este es un **texto en negrita** destacado") == "Este es un texto en negrita destacado");
    }

    SECTION("Eliminacion de codigo inline (`)") {
        REQUIRE(MarkdownRenderer::strip_markdown("La columna `customer_id` es clave") == "La columna customer_id es clave");
    }

    SECTION("Eliminacion de enlaces [texto](url)") {
        REQUIRE(MarkdownRenderer::strip_markdown("Consulte [la documentacion](https://example.com) para mas detalles") == "Consulte la documentacion para mas detalles");
    }

    SECTION("Combinacion multiple de tokens") {
        std::string raw = "**Dominio:** `E-Commerce` con [enlace](https://api.com) y `precio`";
        std::string stripped = MarkdownRenderer::strip_markdown(raw);
        REQUIRE(stripped == "Dominio: E-Commerce con enlace y precio");
    }
}

TEST_CASE("MarkdownRenderer: render_formatted_text y render_document ejecutan sin fallos en contexto ImGui", "[gui][markdown]") {
    using intelidata::gui::MarkdownRenderer;

    // Inicializar contexto headless de ImGui para renderizado sin ventana
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.DisplaySize = ImVec2(1024, 768);

    ImGui::NewFrame();

    SECTION("render_formatted_text con tokens variados") {
        MarkdownRenderer::render_formatted_text("Texto plano simple");
        MarkdownRenderer::render_formatted_text("**Texto en negrita**", false);
        MarkdownRenderer::render_formatted_text("Columna `id_orden` con [enlace](http://test.com)", true);
    }

    SECTION("render_document con estructura markdown completa") {
        const std::string doc =
            "# Reconstruccion Ontologica de Negocio\n"
            "> **Dominio Inferido:** [VENTAS] E-Commerce Comercial\n"
            "\n"
            "## 1. Subdominios Funcionales\n"
            "* Modulo de Ventas y Facturacion\n"
            "* Modulo de Inventario\n"
            "\n"
            "## 2. Dependencias Foraneas\n"
            "| Origen (FK) | Destino (PK) | Confianza |\n"
            "| :--- | :--- | :--- |\n"
            "| `Orders.CustomerID` | `Customers.CustomerID` | 100.0% |\n"
            "| `OrderDetails.OrderID` | `Orders.OrderID` | 98.5% |\n"
            "\n"
            "```sql\n"
            "SELECT * FROM Orders WHERE total > 100;\n"
            "```\n"
            "\n"
            "1. Primer hallazgo de integridad\n"
            "2. Segundo hallazgo de integridad\n"
            "---\n";

        MarkdownRenderer::render_document(doc);
    }

    SECTION("render_document y render_formatted_text con reporte forense Benford") {
        const std::string benford_md =
            "### Dictamen de Auditoria Forense (Conformidad Aceptable)\n\n"
            "La prueba valida que la columna `Freight` cumple la Ley de Benford.\n\n"
            "#### Analisis Forense de Concentracion:\n\n"
            "* **Sobrerrepresentacion Critica en Digito 4:** Frecuencia observada **11.81%**.\n"
            "* **Indice MAD de Nigrini:** `MAD = 0.0094`.\n\n"
            "#### Recomendaciones para Control Interno:\n\n"
            "1. Realizar muestreo estratificado.\n"
            "2. Validar politicas comerciales.\n";

        MarkdownRenderer::render_document(benford_md);
        MarkdownRenderer::render_formatted_text(benford_md, true);
    }

    ImGui::Render();
    ImGui::DestroyContext(ctx);
}
