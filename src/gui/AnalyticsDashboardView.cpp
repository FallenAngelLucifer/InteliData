#include "intelidata/gui/AnalyticsDashboardView.hpp"
#include "intelidata/gui/MarkdownRenderer.hpp"
#include "intelidata/core/Benford.hpp"
#include "intelidata/common/PathUtils.hpp"
#include "imgui.h"
#include "implot.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cstdio>
#include <fstream>
#include <filesystem>

namespace intelidata::gui {

namespace fs = std::filesystem;

namespace {

void render_metric_card(const char* title, const char* value, const char* subtitle, const ImVec4& accent_col) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 3.0f));
    std::string child_id = "##MetricCard_" + std::string(title);
    ImGui::BeginChild(child_id.c_str(), ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::TextDisabled("%s", title);
    ImGui::TextColored(accent_col, "%s", value);
    if (subtitle && subtitle[0] != '\0' && ImGui::GetMainViewport()->Size.y >= 700.0f && ImGui::GetWindowHeight() >= 240.0f) {
        ImGui::TextDisabled("%s", subtitle);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

bool render_flow_checkbox(const char* label, bool* v) {
    const float item_w = ImGui::CalcTextSize(label).x + ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x * 2.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    if (ImGui::GetContentRegionAvail().x >= item_w + spacing) {
        ImGui::SameLine(0.0f, spacing);
    }
    return ImGui::Checkbox(label, v);
}

const char* get_responsive_y_label(const char* full_label, const char* short_label, float plot_h) {
    return (plot_h < 220.0f) ? short_label : full_label;
}

int calculate_responsive_columns(float avail_w) {
    if (avail_w < 400.0f) return 1;
    if (avail_w < 740.0f) return 2;
    return 4;
}

float calculate_responsive_plot_height(float avail_y, float avail_w, int num_cards = 4) {
    const int cols = calculate_responsive_columns(avail_w);
    const int rows = (num_cards + cols - 1) / cols;
    const bool is_compact = (ImGui::GetMainViewport()->Size.y < 700.0f || ImGui::GetWindowHeight() < 240.0f);
    const float card_row_h = is_compact ? 36.0f : 54.0f;
    const float needed_cards_h = static_cast<float>(rows) * card_row_h + 8.0f;
    const float max_plot_h = avail_y - needed_cards_h;
    if (avail_y < 170.0f) {
        return std::clamp(max_plot_h, 75.0f, 115.0f);
    }
    return std::max(95.0f, max_plot_h);
}

} // namespace

AnalyticsDashboardView::AnalyticsDashboardView() {
    init_default_northwind_data();
}

void AnalyticsDashboardView::init_default_northwind_data() {
    // 1. Serie Temporal (23 meses: 1996-07 a 1998-05)
    time_labels_ = {
        "1996-07", "1996-08", "1996-09", "1996-10", "1996-11", "1996-12",
        "1997-01", "1997-02", "1997-03", "1997-04", "1997-05", "1997-06",
        "1997-07", "1997-08", "1997-09", "1997-10", "1997-11", "1997-12",
        "1998-01", "1998-02", "1998-03", "1998-04", "1998-05"
    };

    const size_t n_months = time_labels_.size();
    time_x_.resize(n_months);
    for (size_t i = 0; i < n_months; ++i) {
        time_x_[i] = static_cast<double>(i);
    }

    // Valores en miles de USD ($k USD)
    time_gross_k_ = {
        30.19, 26.61, 27.64, 41.20, 49.70, 50.95,
        66.69, 41.26, 42.10, 57.45, 59.80, 40.40,
        55.10, 51.50, 60.30, 70.20, 48.60, 77.40,
        100.80, 104.50, 109.83, 134.63, 19.90
    };

    time_net_k_ = {
        27.86, 25.49, 26.38, 37.91, 45.60, 45.20,
        61.26, 38.48, 38.54, 53.03, 53.78, 36.36,
        51.02, 47.28, 55.62, 66.75, 45.92, 71.98,
        94.22, 99.41, 104.85, 123.80, 18.33
    };

    time_freight_k_ = {
        1.54, 1.42, 1.63, 2.15, 2.65, 2.80,
        3.25, 2.10, 2.18, 2.85, 2.90, 2.05,
        2.75, 2.50, 3.10, 3.45, 2.40, 3.95,
        4.85, 4.90, 5.25, 7.61, 1.25
    };

    time_discount_k_.resize(n_months);
    for (size_t i = 0; i < n_months; ++i) {
        time_discount_k_[i] = time_gross_k_[i] - time_net_k_[i];
    }

    // 2. Desempeño por Categoría (8 categorías)
    cat_labels_ = {
        "Beverages", "Dairy Products", "Confections", "Meat/Poultry",
        "Seafood", "Condiments", "Produce", "Grains/Cereals"
    };
    const size_t n_cats = cat_labels_.size();
    cat_x_.resize(n_cats);
    for (size_t i = 0; i < n_cats; ++i) {
        cat_x_[i] = static_cast<double>(i);
    }
    cat_net_k_ = { 267.87, 234.51, 167.36, 163.02, 131.26, 106.05, 99.98, 95.74 };
    cat_merma_pct_ = { 6.52, 6.68, 5.91, 8.51, 7.05, 6.65, 5.01, 4.71 };

    // 3. Penetración Geográfica Top 10
    country_labels_ = {
        "USA", "Germany", "Austria", "Brazil", "France",
        "UK", "Venezuela", "Sweden", "Canada", "Ireland"
    };
    const size_t n_countries = country_labels_.size();
    country_x_.resize(n_countries);
    for (size_t i = 0; i < n_countries; ++i) {
        country_x_[i] = static_cast<double>(i);
    }
    country_net_k_ = { 245.58, 230.28, 128.04, 106.93, 81.40, 58.26, 56.81, 54.50, 52.25, 49.98 };
    country_freight_k_ = { 13.67, 11.36, 7.42, 5.05, 4.51, 3.37, 2.79, 3.21, 2.99, 2.39 };

    // 4. Cascada Financiera (Waterfall)
    waterfall_labels_ = {
        "Venta Bruta", "(-) Descuentos", "(=) Venta Neta", "(+) Fletes", "(=) Cartera Total"
    };
    waterfall_x_ = { 0.0, 1.0, 2.0, 3.0, 4.0 };
    // Bottoms y Heights en $k USD
    waterfall_bottoms_k_ = { 0.0, 1354.46 - 88.67, 0.0, 1265.79, 0.0 };
    waterfall_heights_k_ = { 1354.46, 88.67, 1265.79, 64.94, 1330.74 };

    // 5. Ley de Benford
    benford_digits_ = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };
    benford_observed_pct_ = { 29.88, 18.24, 17.63, 8.82, 7.75, 5.61, 4.50, 4.18, 3.39 };
    benford_theoretical_pct_ = { 30.10, 17.61, 12.49, 9.69, 7.92, 6.69, 5.80, 5.12, 4.58 };

    core::BenfordResult default_res{};
    default_res.total_valid_samples = benford_n_;
    default_res.chi_square_stat = benford_chi2_;
    default_res.p_value = benford_pval_;
    default_res.mean_absolute_deviation = benford_mad_;
    for (size_t d = 1; d <= 9; ++d) {
        default_res.expected_proportions[d] = core::benford_theoretical_probability(static_cast<UInt32>(d));
        if (d - 1 < benford_observed_pct_.size()) {
            default_res.observed_proportions[d] = benford_observed_pct_[d - 1] / 100.0;
        } else {
            default_res.observed_proportions[d] = default_res.expected_proportions[d];
        }
    }
    benford_dynamic_interpretation_ = core::generate_benford_interpretation(benford_table_name_, benford_col_name_, default_res);

    // 6. Curva de Lorenz / Pareto Empírica de Northwind (89 clientes comerciales)
    lorenz_cust_pct_ = {
        0.00, 1.12, 2.25, 3.37, 4.49, 5.62, 6.74, 7.87, 8.99, 10.11,
        11.24, 12.36, 13.48, 14.61, 15.73, 16.85, 17.98, 19.10, 20.22, 21.35,
        22.47, 23.60, 24.72, 25.84, 26.97, 28.09, 29.21, 30.34, 31.46, 32.58,
        33.71, 34.83, 35.96, 37.08, 38.20, 39.33, 40.45, 41.57, 42.70, 43.82,
        44.94, 46.07, 47.19, 48.31, 49.44, 50.56, 51.69, 52.81, 53.93, 55.06,
        56.18, 57.30, 58.43, 59.55, 60.67, 61.80, 62.92, 64.04, 65.17, 66.29,
        67.42, 68.54, 69.66, 70.79, 71.91, 73.03, 74.16, 75.28, 76.40, 77.53,
        78.65, 79.78, 80.90, 82.02, 83.15, 84.27, 85.39, 86.52, 87.64, 88.76,
        89.89, 91.01, 92.13, 93.26, 94.38, 95.51, 96.63, 97.75, 98.88, 100.00
    };

    lorenz_rev_pct_ = {
        0.00, 8.71, 17.00, 25.24, 29.28, 33.23, 35.82, 38.26, 40.60, 42.88,
        45.04, 47.15, 49.18, 51.15, 53.05, 54.88, 56.68, 58.41, 60.06, 61.59,
        63.11, 64.57, 66.03, 67.36, 68.66, 69.94, 71.21, 72.47, 73.70, 74.90,
        76.07, 77.13, 78.11, 79.10, 80.07, 80.99, 81.90, 82.80, 83.66, 84.43,
        85.18, 85.92, 86.65, 87.31, 87.88, 88.43, 88.99, 89.53, 90.06, 90.56,
        91.05, 91.53, 92.01, 92.46, 92.86, 93.26, 93.64, 93.98, 94.31, 94.65,
        94.98, 95.31, 95.61, 95.90, 96.18, 96.46, 96.72, 96.98, 97.23, 97.48,
        97.72, 97.96, 98.19, 98.38, 98.54, 98.69, 98.83, 98.97, 99.10, 99.22,
        99.34, 99.46, 99.58, 99.69, 99.81, 99.87, 99.92, 99.96, 99.99, 100.00
    };

    lorenz_equality_pct_ = lorenz_cust_pct_;

    // 7. Ranking de Productos (Top 10)
    prod_labels_ = {
        "Cote de Blaye", "Thuringer Rostbratwurst", "Raclette Courdavault",
        "Tarte au sucre", "Camembert Pierrot", "Gnocchi di nonna Alice",
        "Manjimup Dried Apples", "Alice Mutton", "Carnarvon Tigers", "Rossle Sauerkraut"
    };
    const size_t n_prods = prod_labels_.size();
    prod_x_.resize(n_prods);
    for (size_t i = 0; i < n_prods; ++i) {
        prod_x_[i] = static_cast<double>(i);
    }
    prod_net_k_ = { 141.40, 80.37, 71.16, 47.23, 46.83, 42.59, 41.82, 32.70, 29.17, 25.70 };
    prod_share_pct_ = { 11.17, 6.35, 5.62, 3.73, 3.70, 3.36, 3.30, 2.58, 2.30, 2.03 };

    // 8. Reglas de Asociación de Canasta de Compra (Apriori)
    association_rules_ = {
        {"Sir Rodney's Scones", "Sirop d'erable", 0.96, 20.5, 7.09, 0.0082},
        {"Pavlova", "Gorgonzola Telino", 0.84, 16.3, 2.65, 0.0052},
        {"Gorgonzola Telino", "Mozzarella di Giovanni", 0.72, 11.8, 2.57, 0.0044},
        {"Nord-Ost Matjeshering", "Tourtiere", 0.72, 18.8, 4.32, 0.0055},
        {"Pavlova", "Tarte au sucre", 0.72, 14.0, 2.41, 0.0042},
        {"Pavlova", "Camembert Pierrot", 0.72, 14.0, 2.27, 0.0040},
        {"Camembert Pierrot", "Flotemysost", 0.72, 11.8, 2.32, 0.0041},
        {"Chang", "Pavlova", 0.60, 11.4, 2.19, 0.0033},
        {"Chang", "Raclette Courdavault", 0.60, 11.4, 1.75, 0.0026},
        {"Raclette Courdavault", "Lakkalikooeri", 0.60, 9.3, 1.97, 0.0029},
        {"Spegesild", "Gnocchi di nonna Alice", 0.60, 12.8, 2.21, 0.0033},
        {"Jack's New England Clam Chowder", "Boston Crab Meat", 0.60, 15.1, 3.02, 0.0040},
        {"Ipoh Coffee", "Gula Malacca", 0.48, 25.0, 5.41, 0.0039},
        {"Guarana Fantastica", "Gnocchi di nonna Alice", 0.48, 10.8, 1.86, 0.0022},
        {"Inlagd Sill", "Pate chinois", 0.48, 9.8, 1.92, 0.0023},
        {"Aniseed Syrup", "Gnocchi di nonna Alice", 0.48, 9.3, 1.61, 0.0018},
        {"Queso Manchego La Pastora", "Gudbrandsdalsost", 0.48, 8.5, 1.54, 0.0017},
        {"Chai", "Chang", 0.48, 7.8, 1.49, 0.0016}
    };

    // 9. Clientes Northwind con Segmentación RFM (K-Means)
    rfm_customers_ = {
        {"QUICK", "QUICK-Stop", 18.0, 28.0, 110.28, 1},
        {"ERNSH", "Ernst Handel", 5.0, 30.0, 104.87, 1},
        {"SAVEA", "Save-a-lot Markets", 7.0, 31.0, 104.36, 1},
        {"RATTC", "Rattlesnake Canyon Grocery", 15.0, 18.0, 51.09, 2},
        {"HUNGO", "Hungry Owl All-Night Grocers", 20.0, 19.0, 49.98, 2},
        {"KOENE", "Koeniglich Essen", 14.0, 14.0, 31.71, 2},
        {"FOLKO", "Folk och fa HB", 16.0, 19.0, 29.54, 2},
        {"MEREP", "Mere Paillarde", 14.0, 13.0, 28.84, 2},
        {"WHITC", "White Clover Markets", 10.0, 14.0, 27.36, 2},
        {"FRANK", "Frankenversand", 22.0, 15.0, 26.66, 2},
        {"BERGS", "Berglunds snabbkop", 12.0, 18.0, 24.93, 2},
        {"SUPRD", "Supremes delices", 45.0, 12.0, 24.03, 2},
        {"HILAA", "HILARION-Abastos", 18.0, 18.0, 22.77, 2},
        {"BONAP", "Bon app'", 17.0, 17.0, 21.91, 2},
        {"BOTTM", "Bottom-Dollar Markets", 15.0, 14.0, 20.80, 2},
        {"RICSU", "Richter Supermarkt", 22.0, 10.0, 19.34, 2},
        {"LEHMS", "Lehmanns Marktstand", 25.0, 15.0, 19.24, 2},
        {"BLONP", "Blondel pere et fils", 50.0, 11.0, 18.59, 2},
        {"SEVES", "Seven Seas Imports", 25.0, 9.0, 16.22, 2},
        {"LILAS", "LILA-Supermercado", 35.0, 14.0, 16.08, 2},
        {"VAFFE", "Vaffeljernet", 40.0, 11.0, 15.84, 2},
        {"WARTH", "Wartian Herkku", 20.0, 15.0, 15.65, 2},
        {"EASTC", "Eastern Connection", 32.0, 8.0, 14.73, 2},
        {"GREAL", "Great Lakes Food Market", 45.0, 11.0, 13.88, 2},
        {"LINOD", "LINO-Delicateses", 42.0, 12.0, 12.44, 2},
        {"SPLIR", "Split Rail Beer & Ale", 28.0, 9.0, 11.48, 2},
        {"QUEEN", "Queen Cozinha", 55.0, 13.0, 11.23, 2},
        {"AROUT", "Around the Horn", 60.0, 13.0, 10.89, 0},
        {"BSBEV", "B's Beverages", 75.0, 10.0, 8.92, 0},
        {"COMMI", "Comercio Mineiro", 80.0, 5.0, 3.81, 0},
        {"FAMIA", "Familia Arquibaldo", 90.0, 7.0, 4.43, 0},
        {"GODOS", "Godos Cocina Tipica", 110.0, 10.0, 6.72, 0},
        {"ISLAT", "Island Trading", 100.0, 10.0, 6.15, 0},
        {"MAISD", "Maison Dewey", 120.0, 7.0, 4.88, 0},
        {"OCEAN", "Oceano Atlantico Ltda.", 130.0, 5.0, 3.46, 0},
        {"OLDWO", "Old World Delicatessen", 95.0, 10.0, 8.52, 0},
        {"OTTIK", "Ottilies Delikatessen", 88.0, 10.0, 7.12, 0},
        {"PICCO", "Piccolo und mehr", 70.0, 10.0, 8.84, 0},
        {"PRINI", "Princesa Isabel Vinhos", 105.0, 6.0, 4.19, 0},
        {"RANCH", "Rancho grande", 115.0, 5.0, 3.25, 0},
        {"REGGC", "Reggiani Caseifici", 92.0, 12.0, 7.62, 0},
        {"ROMEY", "Romero y tomillo", 140.0, 5.0, 2.87, 0},
        {"SANTG", "Sante Gourmet", 85.0, 6.0, 4.75, 0},
        {"SIMOB", "Simons bistro", 65.0, 7.0, 6.22, 0},
        {"TORTU", "Tortuga Restaurante", 102.0, 10.0, 6.84, 0},
        {"TRADH", "Tradicao Hipermercados", 112.0, 6.0, 4.38, 0},
        {"TRAIH", "Trail's Head Gourmet Provisioners", 125.0, 3.0, 1.57, 0},
        {"VICTE", "Victuailles en stock", 98.0, 10.0, 7.82, 0},
        {"VINET", "Vins et alcools Chevalier", 108.0, 5.0, 3.05, 0},
        {"WANDK", "Die Wandernde Kuh", 82.0, 10.0, 7.45, 0},
        {"WELLI", "Wellington Importadora", 118.0, 9.0, 5.62, 0},
        {"CHOPS", "Chop-suey Chinese", 68.0, 8.0, 6.45, 0},
        {"CONSH", "Consolidated Holdings", 72.0, 3.0, 2.15, 0},
        {"DRACD", "Drachenblut Delikatessen", 64.0, 6.0, 4.68, 0},
        {"DUMON", "Du monde entier", 78.0, 4.0, 2.76, 0},
        {"FRANR", "France restauration", 92.0, 3.0, 1.89, 0},
        {"FURIB", "Furia Bacalhau e Frutos do Mar", 84.0, 8.0, 5.12, 0},
        {"GALED", "GalerIa del gastronomo", 115.0, 5.0, 2.84, 0},
        {"GOURL", "Gourmet Lanchonetes", 96.0, 9.0, 6.32, 0},
        {"HANAR", "Hanari Carnes", 62.0, 14.0, 9.15, 0},
        {"KANAU", "Karkki Oy", 135.0, 4.0, 2.34, 0},
        {"LAUGB", "Laughing Bacchus Wine Cellars", 150.0, 3.0, 1.25, 0},
        {"LETSS", "Let's Stop N Shop", 145.0, 4.0, 2.45, 0},
        {"MAGAA", "Magazzini Alimentari Riuniti", 76.0, 10.0, 7.24, 0},
        {"MORGK", "Morgenstern Gesundkost", 110.0, 5.0, 3.12, 0},
        {"NORTS", "North/South", 130.0, 3.0, 1.68, 0},
        {"PERIC", "Pericles Comidas clasicas", 88.0, 6.0, 4.15, 0},
        {"RICAR", "Ricardo Adocicados", 105.0, 11.0, 7.85, 0},
        {"SPECD", "Specialites du monde", 125.0, 4.0, 2.21, 0},
        {"THEBI", "The Big Cheese", 140.0, 4.0, 2.10, 0},
        {"THECR", "The Cracker Box", 160.0, 3.0, 1.48, 0},
        {"TOMSP", "Toms Spezialitaten", 155.0, 5.0, 2.92, 0},
        {"WILMK", "Wilman Kala", 115.0, 7.0, 4.35, 0},
        {"WOLZA", "Wolski Zajazd", 95.0, 7.0, 4.88, 0},
        {"FISSA", "Fabrica Italiana Salvataggi Alluvioni", 350.0, 2.0, 0.89, 3},
        {"PARIS", "Paris specialites", 655.0, 5.0, 3.52, 3},
        {"CENTC", "Centro comercial Moctezuma", 380.0, 1.0, 0.38, 3},
        {"GROSR", "GROSELLA-Restaurante", 410.0, 2.0, 1.15, 3},
        {"LAZYK", "Lazy K Kountry Store", 480.0, 2.0, 1.08, 3}
    };
}

void AnalyticsDashboardView::on_dataset_loaded(const std::string& label, ingestion::DuckDBAdapter* /*adapter*/) {
    // Si se cargó un dataset, resetear el autofit para centrar los ejes
    auto_fit_requested_ = true;
    (void)label;
}

void AnalyticsDashboardView::render(const char* title, const model::SemanticAST& ast, ingestion::DuckDBAdapter* /*adapter*/) {
    // Escaneo dinámico de columnas Benford en el AST del dataset actual
    available_benford_cols_.clear();
    for (const auto& tbl : ast.tables) {
        for (const auto& col : tbl.columns) {
            if (col.benford_samples >= 30 || !col.benford_observed_digits.empty() || col.benford_p_value > 0.0) {
                std::string label = tbl.recommended_name + "." + col.recommended_name;
                available_benford_cols_.emplace_back(std::move(label), &col);
            }
        }
    }

    auto score_col = [](const model::ColumnAST* c) -> int64_t {
        if (!c) return 0;
        std::string lower = c->recommended_name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        int64_t score = static_cast<int64_t>(c->benford_samples);
        if (lower.find("price") != std::string::npos || lower.find("precio") != std::string::npos) score += 50000;
        if (lower.find("total") != std::string::npos || lower.find("amount") != std::string::npos || lower.find("monto") != std::string::npos) score += 40000;
        if (lower.find("freight") != std::string::npos || lower.find("flete") != std::string::npos || lower.find("cost") != std::string::npos) score += 35000;
        if (lower.find("discount") != std::string::npos || lower.find("descuento") != std::string::npos) score += 20000;
        if (lower.find("sales") != std::string::npos || lower.find("venta") != std::string::npos) score += 30000;
        if (c->is_unique || lower.find("id") != std::string::npos || lower.find("codigo") != std::string::npos) score -= 10000;
        return score;
    };

    std::sort(available_benford_cols_.begin(), available_benford_cols_.end(),
              [&score_col](const auto& a, const auto& b) {
                  return score_col(a.second) > score_col(b.second);
              });

    if (!available_benford_cols_.empty()) {
        if (selected_benford_col_idx_ < 0 || selected_benford_col_idx_ >= static_cast<int>(available_benford_cols_.size())) {
            selected_benford_col_idx_ = 0;
        }

        const auto* col_ptr = available_benford_cols_[selected_benford_col_idx_].second;
        const std::string& full_label = available_benford_cols_[selected_benford_col_idx_].first;
        auto dot_pos = full_label.find('.');
        benford_table_name_ = (dot_pos != std::string::npos) ? full_label.substr(0, dot_pos) : "";
        benford_col_name_ = col_ptr->recommended_name;
        benford_chi2_ = col_ptr->benford_chi_square;
        benford_pval_ = col_ptr->benford_p_value;
        benford_mad_ = col_ptr->benford_mad;
        benford_n_ = col_ptr->benford_samples;

        if (!col_ptr->benford_observed_digits.empty()) {
            benford_observed_pct_.resize(9);
            for (size_t i = 0; i < 9 && i < col_ptr->benford_observed_digits.size(); ++i) {
                benford_observed_pct_[i] = col_ptr->benford_observed_digits[i] * 100.0;
            }
        }

        benford_dynamic_interpretation_ = col_ptr->benford_interpretation;
        if (benford_dynamic_interpretation_.empty()) {
            core::BenfordResult res{};
            res.total_valid_samples = benford_n_ > 0 ? benford_n_ : 100;
            res.chi_square_stat = benford_chi2_;
            res.p_value = benford_pval_;
            res.mean_absolute_deviation = benford_mad_;
            for (size_t d = 1; d <= 9; ++d) {
                res.expected_proportions[d] = core::benford_theoretical_probability(static_cast<UInt32>(d));
                if (d - 1 < col_ptr->benford_observed_digits.size()) {
                    res.observed_proportions[d] = col_ptr->benford_observed_digits[d - 1];
                } else {
                    res.observed_proportions[d] = res.expected_proportions[d];
                }
            }
            benford_dynamic_interpretation_ = core::generate_benford_interpretation(benford_table_name_, benford_col_name_, res);
        }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

    if (ImGui::Begin(title)) {
        render_toolbar();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::BeginTabBar("##AnalyticsMainTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll)) {
            const ImGuiTabItemFlags t0_flags = (requested_tab_ == 0) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("  1. Serie Temporal (Tiempo)  ", nullptr, t0_flags)) {
                selected_tab_ = 0;
                render_time_series_tab();
                ImGui::EndTabItem();
            }

            const ImGuiTabItemFlags t1_flags = (requested_tab_ == 1) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("  2. Desempeño por Categoría  ", nullptr, t1_flags)) {
                selected_tab_ = 1;
                render_categories_tab();
                ImGui::EndTabItem();
            }

            const ImGuiTabItemFlags t2_flags = (requested_tab_ == 2) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("  3. Penetración Geográfica  ", nullptr, t2_flags)) {
                selected_tab_ = 2;
                render_geography_tab();
                ImGui::EndTabItem();
            }

            const ImGuiTabItemFlags t3_flags = (requested_tab_ == 3) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("  4. Cascada Contable Cuádruple  ", nullptr, t3_flags)) {
                selected_tab_ = 3;
                render_waterfall_tab();
                ImGui::EndTabItem();
            }

            const ImGuiTabItemFlags t4_flags = (requested_tab_ == 4) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("  5. Auditoría Forense (Benford)  ", nullptr, t4_flags)) {
                selected_tab_ = 4;
                render_benford_tab();
                ImGui::EndTabItem();
            }

            const ImGuiTabItemFlags t5_flags = (requested_tab_ == 5) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("  6. Riesgo de Cartera (Lorenz / Pareto)  ", nullptr, t5_flags)) {
                selected_tab_ = 5;
                render_lorenz_pareto_tab();
                ImGui::EndTabItem();
            }

            const ImGuiTabItemFlags t6_flags = (requested_tab_ == 6) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("  7. Ranking de Productos (Top 10)  ", nullptr, t6_flags)) {
                selected_tab_ = 6;
                render_products_tab();
                ImGui::EndTabItem();
            }

            const ImGuiTabItemFlags t7_flags = (requested_tab_ == 7) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("  8. Minería: Reglas de Asociación (Apriori)  ", nullptr, t7_flags)) {
                selected_tab_ = 7;
                render_association_rules_tab();
                ImGui::EndTabItem();
            }

            const ImGuiTabItemFlags t8_flags = (requested_tab_ == 8) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem("  9. Minería: Segmentación RFM (K-Means)  ", nullptr, t8_flags)) {
                selected_tab_ = 8;
                render_rfm_clustering_tab();
                ImGui::EndTabItem();
            }

            requested_tab_ = -1;
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void AnalyticsDashboardView::render_toolbar() {
    auto same_line_if_fits = [](float needed_w) {
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        if (ImGui::GetContentRegionAvail().x >= needed_w + spacing) {
            ImGui::SameLine(0.0f, spacing);
            return true;
        }
        return false;
    };

    // Fila 1: Botones de Acción Primarios
    const float avail_tb_w = ImGui::GetContentRegionAvail().x;
    const char* save_btn_lbl = (avail_tb_w < 540.0f) ? "  [Graficas]  " : "  [Guardar Graficas del Reporte]  ";
    if (ImGui::Button(save_btn_lbl)) {
        save_report_charts();
    }

    const char* fit_btn_lbl = (avail_tb_w < 540.0f) ? "  [Auto-Ajustar]  " : "  [Auto-Ajustar / Reset]  ";
    const float btn2_w = ImGui::CalcTextSize(fit_btn_lbl).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    same_line_if_fits(btn2_w);
    if (ImGui::Button(fit_btn_lbl)) {
        auto_fit_requested_ = true;
    }

    const float chk_w = ImGui::CalcTextSize("Crosshairs").x + ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x * 2.0f;
    same_line_if_fits(chk_w);
    ImGui::Checkbox("Crosshairs", &show_crosshairs_);

    if (avail_tb_w >= 480.0f) {
        const float tag_w = ImGui::CalcTextSize("[INTERACTIVO]").x;
        same_line_if_fits(tag_w);
        ImGui::TextColored(ImVec4(0.2f, 0.85f, 1.0f, 1.0f), "[INTERACTIVO]");
    }

    // Fila 2: Guía de Navegación compacta (solo en pantallas con altura suficiente)
    if (ImGui::GetWindowHeight() >= 300.0f) {
        ImGui::TextWrapped("Navegacion: Rueda = Zoom In/Out | Clic Derecho/Medio sostenido = Desplazar | Doble clic = Reset");
    }

    if (!status_message_.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", status_message_.c_str());
    }
}

void AnalyticsDashboardView::save_report_charts(const std::string& target_dir) {
    const fs::path root = intelidata::common::PathUtils::find_project_root();
    const fs::path out_dir = target_dir.empty() ? (root / "images") : fs::path(target_dir);

    try {
        fs::create_directories(out_dir);
        const fs::path src_images = root / "docs" / "images";
        int copied_count = 0;
        if (fs::exists(src_images)) {
            for (const auto& entry : fs::directory_iterator(src_images)) {
                if (entry.is_regular_file() && entry.path().extension() == ".png") {
                    fs::copy_file(entry.path(), out_dir / entry.path().filename(), fs::copy_options::overwrite_existing);
                    copied_count++;
                }
            }
        }

        // Generar archivo Markdown complementario del reporte de gráficas
        const fs::path md_path = root / "reporte_graficas_analiticas.md";
        std::ofstream md(md_path);
        if (md.is_open()) {
            md << "# Reporte Consolidado de Graficas Analiticas, Contabilidad y Finanzas (BI)\n\n";
            md << "> Generado interactivamente desde **InteliData GUI**.\n\n";
            md << "## 1. Evolucion Temporal Mensual (1996 - 1998)\n\n";
            md << "![Serie Temporal Mensual](images/figura7_serie_temporal_mensual.png)\n\n";
            md << "## 2. Desempeno por Categoria de Producto y Margen Comercial\n\n";
            md << "![Desempeño por Categoría](images/figura8_desempeno_categorias_descuento.png)\n\n";
            md << "## 3. Penetracion Geografica y Fletes (Top 10 Paises)\n\n";
            md << "![Top 10 Mercados](images/figura9_desempeno_paises_fletes.png)\n\n";
            md << "## 4. Conciliacion Contable Cuadruple (Waterfall Chart)\n\n";
            md << "![Conciliación Contable Cuádruple](images/figura10_cascada_conciliacion_cuadruple.png)\n\n";
            md << "## 5. Auditoria Forense de la Ley de Benford (" << benford_table_name_ << "." << benford_col_name_
               << ", chi2 = " << std::fixed << std::setprecision(2) << benford_chi2_ << ", p-valor = " << benford_pval_ << ")\n\n";
            md << "![Auditoría Ley de Benford](images/figura11_auditoria_ley_benford.png)\n\n";
            if (!benford_dynamic_interpretation_.empty()) {
                md << benford_dynamic_interpretation_ << "\n\n";
            }
            md << "## 6. Concentracion de Cartera y Curva de Lorenz / Pareto 80/20 (Gini = 0.575)\n\n";
            md << "![Curva de Lorenz](images/figura12_curva_lorenz_pareto_clientes.png)\n\n";
        }

        status_message_ = "¡" + std::to_string(copied_count) + " Graficas analiticas guardadas con exito en " + out_dir.string() + "!";
    } catch (const std::exception& e) {
        status_message_ = std::string("Error al guardar graficas: ") + e.what();
    }
}

void AnalyticsDashboardView::render_time_series_tab() {
    // Controles específicos de la serie temporal con flujo adaptativo
    ImGui::Checkbox("Venta Neta ($k)", &show_net_);
    render_flow_checkbox("Venta Bruta ($k)", &show_gross_);
    render_flow_checkbox("Fletes ($k)", &show_freight_);
    render_flow_checkbox("Descuentos ($k)", &show_discount_);
    render_flow_checkbox("Sombreado Área Neta", &show_area_fill_);

    ImPlotFlags plot_flags = ImPlotFlags_None;
    if (show_crosshairs_) plot_flags |= ImPlotFlags_Crosshairs;

    ImPlotAxisFlags x_flags = ImPlotAxisFlags_None;
    ImPlotAxisFlags y_flags = ImPlotAxisFlags_None;
    if (auto_fit_requested_) {
        x_flags |= ImPlotAxisFlags_AutoFit;
        y_flags |= ImPlotAxisFlags_AutoFit;
    }

    const float avail_y = ImGui::GetContentRegionAvail().y;
    const float plot_h = calculate_responsive_plot_height(avail_y, ImGui::GetContentRegionAvail().x, 4);
    const ImVec2 plot_size(-1, plot_h);
    if (ImPlot::BeginPlot("##MonthlyTimeSeriesPlot", plot_size, plot_flags)) {
        ImPlot::SetupAxes((ImGui::GetContentRegionAvail().x < 600.0f ? "Periodo (Año-Mes)" : "Periodo Operativo Mensual (Año-Mes)"),
                          get_responsive_y_label("Importe Monetario ($k USD)", "$k USD", plot_h), x_flags, y_flags);

        // Generación dinámica y responsive de ticks según ancho real del gráfico para evitar colisiones
        const float plot_width = ImGui::GetContentRegionAvail().x;
        const int max_ticks = std::max(3, static_cast<int>(plot_width / 80.0f));
        const int step = std::max(1, static_cast<int>(std::ceil(static_cast<float>(time_labels_.size()) / max_ticks)));
        std::vector<double> tick_x;
        std::vector<const char*> c_labels;
        for (size_t i = 0; i < time_labels_.size(); i += step) {
            tick_x.push_back(time_x_[i]);
            c_labels.push_back(time_labels_[i].c_str());
        }
        if (!time_labels_.empty() && (time_labels_.size() - 1) % step != 0) {
            tick_x.push_back(time_x_.back());
            c_labels.push_back(time_labels_.back().c_str());
        }
        ImPlot::SetupAxisTicks(ImAxis_X1, tick_x.data(), static_cast<int>(tick_x.size()), c_labels.data());

        const int count = static_cast<int>(time_x_.size());

        if (show_area_fill_ && show_net_) {
            ImPlotSpec spec;
            spec.FillColor = ImVec4(0.12f, 0.40f, 0.85f, 0.20f);
            ImPlot::PlotShaded("Área Venta Neta", time_x_.data(), time_net_k_.data(), count, 0.0, spec);
        }

        if (show_net_) {
            ImPlotSpec spec;
            spec.LineColor = ImVec4(0.12f, 0.40f, 0.85f, 1.0f);
            spec.LineWeight = 2.5f;
            ImPlot::PlotLine("Venta Neta Comercial", time_x_.data(), time_net_k_.data(), count, spec);
        }

        if (show_gross_) {
            ImPlotSpec spec;
            spec.LineColor = ImVec4(0.58f, 0.65f, 0.72f, 1.0f);
            spec.LineWeight = 1.8f;
            ImPlot::PlotLine("Venta Bruta Facturada", time_x_.data(), time_gross_k_.data(), count, spec);
        }

        if (show_freight_) {
            ImPlotSpec spec;
            spec.LineColor = ImVec4(0.85f, 0.48f, 0.05f, 1.0f);
            spec.LineWeight = 2.0f;
            ImPlot::PlotLine("Flete Facturado", time_x_.data(), time_freight_k_.data(), count, spec);
        }

        if (show_discount_) {
            ImPlotSpec spec;
            spec.LineColor = ImVec4(0.85f, 0.15f, 0.15f, 1.0f);
            spec.LineWeight = 1.8f;
            ImPlot::PlotLine("Merma por Descuentos", time_x_.data(), time_discount_k_.data(), count, spec);
        }

        // Anotación del ápice histórico en abril de 1998
        const int peak_idx = 21; // 1998-04
        if (peak_idx < count) {
            ImPlot::Annotation(static_cast<double>(peak_idx), time_net_k_[peak_idx],
                               ImVec4(0.85f, 0.15f, 0.15f, 1.0f), ImVec2(10, -25), true,
                               "Ápice 1998-04\n$123,799 USD Neta");
        }

        ImPlot::EndPlot();
    }
    auto_fit_requested_ = false;

    // Resumen métrico inferior
    const float avail_w1 = ImGui::GetContentRegionAvail().x;
    ImGui::Columns(calculate_responsive_columns(avail_w1), "##TimeSummaryCols", false);
    render_metric_card("Facturacion Media Mensual", "$55,034 USD", "23 meses operativos", ImVec4(0.2f, 0.85f, 1.0f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Apice Historico (1998-04)", "$123,799 USD", "Maximo transaccional", ImVec4(0.3f, 0.95f, 0.5f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Fletes Logísticos Medios", "$2,824 USD/mes", "4.88% de incidencia", ImVec4(0.95f, 0.65f, 0.2f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Crecimiento Acumulado", "+344.3%", "1996-07 a 1998-04", ImVec4(0.9f, 0.4f, 0.9f, 1.0f));
    ImGui::Columns(1);
}

void AnalyticsDashboardView::render_categories_tab() {
    const float avail_w_cat = ImGui::GetContentRegionAvail().x;
    if (avail_w_cat >= 540.0f) {
        ImGui::TextDisabled("Categorias y Margen:");
        ImGui::SameLine();
    }
    ImGui::SetNextItemWidth(std::min(185.0f, avail_w_cat * 0.45f));
    if (ImGui::Combo("##SortCategories", &category_sort_mode_, "Venta Neta Descendente\0Merma % Descendente\0")) {
        auto_fit_requested_ = true;
    }

    ImPlotFlags plot_flags = show_crosshairs_ ? ImPlotFlags_Crosshairs : ImPlotFlags_None;
    ImPlotAxisFlags x_flags = ImPlotAxisFlags_None;
    ImPlotAxisFlags y_flags = ImPlotAxisFlags_None;
    if (auto_fit_requested_) {
        x_flags |= ImPlotAxisFlags_AutoFit;
        y_flags |= ImPlotAxisFlags_AutoFit;
    }

    const float avail_y2 = ImGui::GetContentRegionAvail().y;
    const float cat_plot_h = calculate_responsive_plot_height(avail_y2, ImGui::GetContentRegionAvail().x, 4);
    const ImVec2 plot_size(-1, cat_plot_h);
    if (ImPlot::BeginPlot("##CategoriesPlot", plot_size, plot_flags)) {
        ImPlot::SetupAxes("Categoria de Producto", get_responsive_y_label("Venta Neta ($k USD)", "$k USD", cat_plot_h), x_flags, y_flags);

        const float pw = ImGui::GetContentRegionAvail().x;
        std::vector<const char*> c_labels;
        static const char* short_cat_labels[] = {
            "Bev.", "Dairy", "Conf.", "Meat", "Sea.", "Cond.", "Prod.", "Grains"
        };
        for (size_t i = 0; i < cat_labels_.size(); ++i) {
            if (pw < 560.0f && i < 8) {
                c_labels.push_back(short_cat_labels[i]);
            } else {
                c_labels.push_back(cat_labels_[i].c_str());
            }
        }
        ImPlot::SetupAxisTicks(ImAxis_X1, cat_x_.data(), static_cast<int>(cat_labels_.size()), c_labels.data());

        const int count = static_cast<int>(cat_x_.size());

        // Barras de Venta Neta
        ImPlotSpec spec_bars;
        spec_bars.FillColor = ImVec4(0.12f, 0.35f, 0.75f, 0.9f);
        ImPlot::PlotBars("Venta Neta ($k USD)", cat_x_.data(), cat_net_k_.data(), count, 0.5, spec_bars);

        // Línea de porcentaje de merma sobre los mismos ejes escalada a %
        ImPlotSpec spec_line;
        spec_line.LineColor = ImVec4(0.85f, 0.15f, 0.15f, 1.0f);
        spec_line.LineWeight = 2.2f;
        ImPlot::PlotLine("% Merma por Descuento", cat_x_.data(), cat_merma_pct_.data(), count, spec_line);

        // Anotación en Meat/Poultry
        ImPlot::Annotation(3.0, cat_merma_pct_[3], ImVec4(0.85f, 0.15f, 0.15f, 1.0f), ImVec2(10, -20), true,
                           "Mayor Merma: 8.51%%\nMeat/Poultry");

        ImPlot::EndPlot();
    }
    auto_fit_requested_ = false;

    const float avail_w2 = ImGui::GetContentRegionAvail().x;
    ImGui::Columns(calculate_responsive_columns(avail_w2), "##CatSummaryCols", false);
    render_metric_card("Líder en Ventas Netas", "Beverages ($267.9k)", "21.16% de participacion", ImVec4(0.2f, 0.85f, 1.0f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Segunda Categoria", "Dairy Products ($234.5k)", "18.53% de participacion", ImVec4(0.3f, 0.85f, 0.95f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Mayor Merma Comercial", "Meat/Poultry (8.51%)", "$15,173 USD en descuentos", ImVec4(0.95f, 0.25f, 0.25f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Menor Merma Comercial", "Grains/Cereals (4.71%)", "Politica comercial rigida", ImVec4(0.3f, 0.95f, 0.5f, 1.0f));
    ImGui::Columns(1);
}

void AnalyticsDashboardView::render_geography_tab() {
    if (ImGui::GetContentRegionAvail().x >= 540.0f) {
        ImGui::TextDisabled("Penetracion en los 10 principales mercados y fletes:");
    }

    ImPlotFlags plot_flags = show_crosshairs_ ? ImPlotFlags_Crosshairs : ImPlotFlags_None;
    ImPlotAxisFlags x_flags = ImPlotAxisFlags_None;
    ImPlotAxisFlags y_flags = ImPlotAxisFlags_None;
    if (auto_fit_requested_) {
        x_flags |= ImPlotAxisFlags_AutoFit;
        y_flags |= ImPlotAxisFlags_AutoFit;
    }

    const float avail_y3 = ImGui::GetContentRegionAvail().y;
    const float geo_plot_h = calculate_responsive_plot_height(avail_y3, ImGui::GetContentRegionAvail().x, 4);
    const ImVec2 plot_size(-1, geo_plot_h);
    if (ImPlot::BeginPlot("##GeographyPlot", plot_size, plot_flags)) {
        ImPlot::SetupAxes("Pais Destino", get_responsive_y_label("Importe ($k USD)", "$k USD", geo_plot_h), x_flags, y_flags);

        const float pw = ImGui::GetContentRegionAvail().x;
        std::vector<const char*> c_labels;
        static const char* short_countries[] = {
            "USA", "DEU", "AUT", "BRA", "FRA", "GBR", "VEN", "SWE", "CAN", "IRL"
        };
        for (size_t i = 0; i < country_labels_.size(); ++i) {
            if (pw < 560.0f && i < 10) {
                c_labels.push_back(short_countries[i]);
            } else {
                c_labels.push_back(country_labels_[i].c_str());
            }
        }
        ImPlot::SetupAxisTicks(ImAxis_X1, country_x_.data(), static_cast<int>(country_labels_.size()), c_labels.data());

        const int count = static_cast<int>(country_x_.size());
        std::vector<double> x_net(count);
        std::vector<double> x_freight(count);
        for (int i = 0; i < count; ++i) {
            x_net[i] = country_x_[i] - 0.20;
            x_freight[i] = country_x_[i] + 0.20;
        }

        ImPlotSpec spec_net;
        spec_net.FillColor = ImVec4(0.02f, 0.52f, 0.78f, 0.9f);
        ImPlot::PlotBars("Venta Neta Mercancías ($k)", x_net.data(), country_net_k_.data(), count, 0.35, spec_net);

        ImPlotSpec spec_freight;
        spec_freight.FillColor = ImVec4(0.96f, 0.62f, 0.07f, 0.9f);
        ImPlot::PlotBars("Flete Logístico ($k)", x_freight.data(), country_freight_k_.data(), count, 0.35, spec_freight);

        ImPlot::EndPlot();
    }
    auto_fit_requested_ = false;

    const float avail_w3 = ImGui::GetContentRegionAvail().x;
    ImGui::Columns(calculate_responsive_columns(avail_w3), "##GeoSummaryCols", false);
    render_metric_card("Mercado Principal", "USA ($245.6k USD)", "19.4% de ventas netas", ImVec4(0.2f, 0.85f, 1.0f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Segundo Mercado", "Alemania ($230.3k USD)", "18.2% de ventas netas", ImVec4(0.3f, 0.85f, 0.95f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Mayor Incidencia Flete", "Suecia (5.90%)", "Costos de flete elevados", ImVec4(0.95f, 0.60f, 0.20f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Mercados Activos", "21 Países", "Top 10 concentra 87.8%", ImVec4(0.3f, 0.95f, 0.5f, 1.0f));
    ImGui::Columns(1);
}

void AnalyticsDashboardView::render_waterfall_tab() {
    if (ImGui::GetContentRegionAvail().x >= 540.0f) {
        ImGui::TextDisabled("Conciliacion cuadruple en cascada interactiva:");
    }

    ImPlotFlags plot_flags = show_crosshairs_ ? ImPlotFlags_Crosshairs : ImPlotFlags_None;
    ImPlotAxisFlags x_flags = ImPlotAxisFlags_None;
    ImPlotAxisFlags y_flags = ImPlotAxisFlags_None;
    if (auto_fit_requested_) {
        x_flags |= ImPlotAxisFlags_AutoFit;
        y_flags |= ImPlotAxisFlags_AutoFit;
    }

    const float avail_y4 = ImGui::GetContentRegionAvail().y;
    const float wf_plot_h = calculate_responsive_plot_height(avail_y4, ImGui::GetContentRegionAvail().x, 4);
    const ImVec2 plot_size(-1, wf_plot_h);
    if (ImPlot::BeginPlot("##WaterfallPlot", plot_size, plot_flags)) {
        ImPlot::SetupAxes("Fase de Conciliacion", get_responsive_y_label("Monto Acumulado ($k USD)", "$k USD", wf_plot_h), x_flags, y_flags);

        const float pw = ImGui::GetContentRegionAvail().x;
        std::vector<const char*> c_labels;
        static const char* short_wf[] = {
            "Bruta", "(-)Desc", "(=)Neta", "(+)Fletes", "(=)Total"
        };
        for (size_t i = 0; i < waterfall_labels_.size(); ++i) {
            if (pw < 500.0f && i < 5) {
                c_labels.push_back(short_wf[i]);
            } else {
                c_labels.push_back(waterfall_labels_[i].c_str());
            }
        }
        ImPlot::SetupAxisTicks(ImAxis_X1, waterfall_x_.data(), static_cast<int>(waterfall_labels_.size()), c_labels.data());

        const int count = static_cast<int>(waterfall_x_.size());

        // Colores temáticos para cascada
        const ImVec4 bar_colors[5] = {
            ImVec4(0.12f, 0.23f, 0.54f, 0.9f), // Venta Bruta (Azul Marino)
            ImVec4(0.86f, 0.15f, 0.15f, 0.9f), // Descuentos (Rojo)
            ImVec4(0.01f, 0.52f, 0.78f, 0.9f), // Venta Neta (Azul Claro)
            ImVec4(0.85f, 0.47f, 0.03f, 0.9f), // Fletes (Naranja)
            ImVec4(0.02f, 0.47f, 0.34f, 0.9f)  // Cartera Total (Verde Esmeralda)
        };

        for (int i = 0; i < count; ++i) {
            ImPlotSpec spec;
            spec.FillColor = bar_colors[i];
            double x_val = waterfall_x_[i];
            double y_val = waterfall_heights_k_[i];
            ImPlot::PlotBars(waterfall_labels_[i].c_str(), &x_val, &y_val, 1, 0.5, spec);
        }

        ImPlot::EndPlot();
    }
    auto_fit_requested_ = false;

    const float avail_w4 = ImGui::GetContentRegionAvail().x;
    ImGui::Columns(calculate_responsive_columns(avail_w4), "##WaterfallSummaryCols", false);
    render_metric_card("Venta Bruta Facturada", "$1,354,458.59 USD", "100.00% base transaccional", ImVec4(0.2f, 0.4f, 0.9f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Merma por Descuentos", "-$88,665.73 USD", "6.55% de merma comercial", ImVec4(0.95f, 0.25f, 0.25f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Venta Neta Mercancías", "$1,265,793.15 USD", "93.45% neto facturado", ImVec4(0.2f, 0.85f, 1.0f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Cartera Cuentas x Cobrar", "$1,330,735.84 USD", "Incluye +$64,942 USD fletes", ImVec4(0.3f, 0.95f, 0.5f, 1.0f));
    ImGui::Columns(1);
}

void AnalyticsDashboardView::render_benford_tab() {
    ImGui::TextDisabled("Prueba de bondad de ajuste de chi-cuadrado para auditoria forense de facturacion:");

    // Selector de métrica si hay múltiples columnas evaluadas
    if (available_benford_cols_.size() > 1) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "Seleccionar Variable Transaccional Evaluada:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(320.0f);
        std::string preview = available_benford_cols_[selected_benford_col_idx_].first;
        if (ImGui::BeginCombo("##AnalyticsBenfordCombo", preview.c_str())) {
            for (int i = 0; i < static_cast<int>(available_benford_cols_.size()); ++i) {
                const bool is_sel = (selected_benford_col_idx_ == i);
                if (ImGui::Selectable(available_benford_cols_[i].first.c_str(), is_sel)) {
                    selected_benford_col_idx_ = i;
                }
                if (is_sel) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    ImPlotFlags plot_flags = show_crosshairs_ ? ImPlotFlags_Crosshairs : ImPlotFlags_None;
    ImPlotAxisFlags x_flags = ImPlotAxisFlags_None;
    ImPlotAxisFlags y_flags = ImPlotAxisFlags_None;
    if (auto_fit_requested_) {
        x_flags |= ImPlotAxisFlags_AutoFit;
        y_flags |= ImPlotAxisFlags_AutoFit;
    }

    const float avail_w5 = ImGui::GetContentRegionAvail().x;
    const float benford_plot_h = std::clamp(ImGui::GetContentRegionAvail().y * 0.40f, 150.0f, 300.0f);
    const ImVec2 plot_size(-1, benford_plot_h);
    if (ImPlot::BeginPlot("##BenfordPlot", plot_size, plot_flags)) {
        ImPlot::SetupAxes((avail_w5 < 600.0f ? "Digito (d)" : "Primer Digito Significativo (d)"),
                          get_responsive_y_label("Frecuencia Relativa (%)", "% Frecuencia", benford_plot_h), x_flags, y_flags);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.5, 9.5);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 35.0);

        const int count = static_cast<int>(benford_digits_.size());
        std::vector<double> digits_obs(count);
        std::vector<double> digits_theo(count);
        for (int i = 0; i < count; ++i) {
            digits_obs[i] = benford_digits_[i] - 0.20;
            digits_theo[i] = benford_digits_[i] + 0.20;
        }

        ImPlotSpec spec_obs;
        spec_obs.FillColor = ImVec4(0.15f, 0.39f, 0.92f, 0.9f);
        ImPlot::PlotBars("Frecuencia Observada (%)", digits_obs.data(), benford_observed_pct_.data(), count, 0.36, spec_obs);

        ImPlotSpec spec_theo;
        spec_theo.FillColor = ImVec4(0.58f, 0.64f, 0.72f, 0.9f);
        ImPlot::PlotBars("Teorica Ley de Benford (%)", digits_theo.data(), benford_theoretical_pct_.data(), count, 0.36, spec_theo);

        // Anotación dinámica en el dígito con mayor desviación empírica
        int max_dev_digit = 1;
        double max_abs_dev = 0.0;
        double max_dev_sign = 0.0;
        for (int i = 0; i < 9; ++i) {
            double theo = (i < static_cast<int>(benford_theoretical_pct_.size())) ? benford_theoretical_pct_[i] : 0.0;
            double obs = (i < static_cast<int>(benford_observed_pct_.size())) ? benford_observed_pct_[i] : 0.0;
            double diff = obs - theo;
            if (std::abs(diff) > max_abs_dev) {
                max_abs_dev = std::abs(diff);
                max_dev_digit = i + 1;
                max_dev_sign = diff;
            }
        }

        if (max_abs_dev >= 1.5 && max_dev_digit >= 1 && max_dev_digit <= 9) {
            char anno_buf[128];
            std::snprintf(anno_buf, sizeof(anno_buf), "Digito %d (%s%.2f%% drift)\nObs: %.1f%% vs Theo: %.1f%%",
                          max_dev_digit, (max_dev_sign > 0 ? "+" : ""), max_dev_sign,
                          benford_observed_pct_[max_dev_digit - 1], benford_theoretical_pct_[max_dev_digit - 1]);
            ImPlot::Annotation(static_cast<double>(max_dev_digit), benford_observed_pct_[max_dev_digit - 1],
                               (max_dev_sign > 0 ? ImVec4(0.85f, 0.15f, 0.15f, 1.0f) : ImVec4(0.15f, 0.65f, 0.95f, 1.0f)),
                               ImVec2(10, -25), true, "%s", anno_buf);
        }

        ImPlot::EndPlot();
    }
    auto_fit_requested_ = false;

    char chi_buf[32];
    std::snprintf(chi_buf, sizeof(chi_buf), "Chi^2 = %.2f", benford_chi2_);
    char pval_buf[32];
    std::snprintf(pval_buf, sizeof(pval_buf), "p = %.6f", benford_pval_);
    char n_buf[32];
    std::snprintf(n_buf, sizeof(n_buf), "%llu Registros", static_cast<unsigned long long>(benford_n_));

    const char* diag_title = (benford_pval_ >= 0.05) ? "Conforme (Organico)" :
                             (benford_mad_ <= 0.012 ? "Aceptable" : "Desviacion Detectada");
    const char* diag_sub = (benford_mad_ <= 0.006) ? "MAD Estricta <= 0.006" :
                           (benford_mad_ <= 0.012 ? "MAD Aceptable <= 0.012" :
                           (benford_mad_ <= 0.015 ? "MAD Marginal <= 0.015" : "MAD Critica > 0.015"));
    ImVec4 diag_col = (benford_mad_ <= 0.012) ? ImVec4(0.3f, 0.95f, 0.5f, 1.0f) : ImVec4(0.95f, 0.45f, 0.35f, 1.0f);

    ImGui::Columns(calculate_responsive_columns(avail_w5), "##BenfordSummaryCols", false);
    render_metric_card("Estadistico Chi-Cuadrado", chi_buf, "8 grados de libertad", ImVec4(0.2f, 0.85f, 1.0f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("P-Valor Analitico Exacto", pval_buf, (benford_pval_ >= 0.05 ? "H0 No Rechazada (p >= 0.05)" : "H0 Rechazada (p < 0.05)"), ImVec4(0.95f, 0.65f, 0.2f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Diagnostico Nigrini MAD", diag_title, diag_sub, diag_col);
    ImGui::NextColumn();
    render_metric_card("Volumen Transaccional", n_buf, benford_col_name_.c_str(), ImVec4(0.9f, 0.4f, 0.9f, 1.0f));
    ImGui::Columns(1);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "DICTAMEN E INTERPRETACION FORENSE INTEGRAL:");
    ImGui::Spacing();

    if (ImGui::BeginChild("BenfordDynamicReportCard", ImVec2(0, 0), true)) {
        MarkdownRenderer::render_document(benford_dynamic_interpretation_);
    }
    ImGui::EndChild();
}

void AnalyticsDashboardView::render_lorenz_pareto_tab() {
    const float avail_w6 = ImGui::GetContentRegionAvail().x;
    if (avail_w6 >= 540.0f) {
        ImGui::TextDisabled("Curva de concentracion de ingresos (Pareto 80/20):");
    }

    ImPlotFlags plot_flags = show_crosshairs_ ? ImPlotFlags_Crosshairs : ImPlotFlags_None;
    ImPlotAxisFlags x_flags = ImPlotAxisFlags_None;
    ImPlotAxisFlags y_flags = ImPlotAxisFlags_None;
    if (auto_fit_requested_) {
        x_flags |= ImPlotAxisFlags_AutoFit;
        y_flags |= ImPlotAxisFlags_AutoFit;
    }

    const float avail_y6 = ImGui::GetContentRegionAvail().y;
    const float lorenz_plot_h = calculate_responsive_plot_height(avail_y6, avail_w6, 4);
    const ImVec2 plot_size(-1, lorenz_plot_h);
    if (ImPlot::BeginPlot("##LorenzPlot", plot_size, plot_flags)) {
        // Posicionar leyenda en el cuadrante inferior derecho (área vacía bajo la diagonal)
        ImPlot::SetupLegend(ImPlotLocation_SouthEast);
        ImPlot::SetupAxes((avail_w6 < 600.0f ? "% Clientes" : "% Acumulado de Clientes"),
                          get_responsive_y_label("% Acumulado de Ventas Netas", "% Ventas", lorenz_plot_h), x_flags, y_flags);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 100.0);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 105.0);

        const int count = static_cast<int>(lorenz_cust_pct_.size());

        // Área sombreada entre la curva de Lorenz y la diagonal
        ImPlotSpec spec_shaded;
        spec_shaded.FillColor = ImVec4(0.12f, 0.40f, 0.85f, 0.15f);
        ImPlot::PlotShaded("Área de Concentración de Riesgo", lorenz_cust_pct_.data(), lorenz_rev_pct_.data(),
                           lorenz_equality_pct_.data(), count, spec_shaded);

        // Curva de Lorenz
        ImPlotSpec spec_lorenz;
        spec_lorenz.LineColor = ImVec4(0.12f, 0.23f, 0.54f, 1.0f);
        spec_lorenz.LineWeight = 2.8f;
        ImPlot::PlotLine("Curva de Concentración Real", lorenz_cust_pct_.data(), lorenz_rev_pct_.data(), count, spec_lorenz);

        // Línea de igualdad 45 grados
        ImPlotSpec spec_eq;
        spec_eq.LineColor = ImVec4(0.58f, 0.65f, 0.72f, 1.0f);
        spec_eq.LineWeight = 1.8f;
        ImPlot::PlotLine("Línea de Distribución Equitativa (45°)", lorenz_cust_pct_.data(), lorenz_equality_pct_.data(), count, spec_eq);

        // Marcadores de puntos críticos (verificados y exactamente sobre la curva empírica)
        double top5_x = 5.62;
        double top5_y = 33.23;
        ImPlotSpec spec_top5;
        spec_top5.Marker = ImPlotMarker_Circle;
        spec_top5.MarkerSize = 6.0f;
        spec_top5.MarkerFillColor = ImVec4(0.85f, 0.15f, 0.15f, 1.0f);
        spec_top5.MarkerLineColor = ImVec4(0.85f, 0.15f, 0.15f, 1.0f);
        ImPlot::PlotScatter("Top 5 Clientes", &top5_x, &top5_y, 1, spec_top5);
        ImPlot::Annotation(top5_x, top5_y, ImVec4(0.85f, 0.15f, 0.15f, 1.0f), ImVec2(20, -15), true,
                           "Top 5 Clientes (5.62%%)\nConcentran 33.23%% ($420.6k)");

        // Umbral Pareto 80/20 empírico (Cliente #34: 38.20% clientes -> 80.07% ventas, 38.12% interpolado)
        double p80_x = 38.12;
        double p80_y = 80.00;
        ImPlotSpec spec_p80;
        spec_p80.Marker = ImPlotMarker_Diamond;
        spec_p80.MarkerSize = 6.0f;
        spec_p80.MarkerFillColor = ImVec4(0.85f, 0.48f, 0.05f, 1.0f);
        spec_p80.MarkerLineColor = ImVec4(0.85f, 0.48f, 0.05f, 1.0f);
        ImPlot::PlotScatter("Umbral 80/20", &p80_x, &p80_y, 1, spec_p80);
        ImPlot::Annotation(p80_x, p80_y, ImVec4(0.85f, 0.48f, 0.05f, 1.0f), ImVec2(15, 15), true,
                           "Umbral Pareto 80/20\n38.2%% Clientes = 80.0%% Ventas");

        ImPlot::EndPlot();
    }
    auto_fit_requested_ = false;

    ImGui::Columns(calculate_responsive_columns(avail_w6), "##LorenzSummaryCols", false);
    render_metric_card("Top 5 Clientes", "33.23% ($420.6k USD)", "QUICK, ERNSH, SAVEA...", ImVec4(0.95f, 0.25f, 0.25f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Umbral Pareto 80/20", "38.2% de Clientes", "Generan 80% de ventas", ImVec4(0.95f, 0.65f, 0.2f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Total Clientes Activos", "89 Empresas", "Cartera comercial", ImVec4(0.2f, 0.85f, 1.0f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Alerta de Riesgo", "Alta Concentración", "Exposición crediticia", ImVec4(0.9f, 0.4f, 0.9f, 1.0f));
    ImGui::Columns(1);
}

void AnalyticsDashboardView::render_products_tab() {
    const float avail_w7 = ImGui::GetContentRegionAvail().x;
    ImGui::TextDisabled("Ranking de Productos:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    int current_mode = (prod_ranking_count_ == 5) ? 1 : 0;
    if (ImGui::Combo("##ProdRankCombo", &current_mode, "Top 10\0Top 5\0")) {
        prod_ranking_count_ = (current_mode == 1) ? 5 : 10;
        auto_fit_requested_ = true;
    }

    ImPlotFlags plot_flags = show_crosshairs_ ? ImPlotFlags_Crosshairs : ImPlotFlags_None;
    ImPlotAxisFlags x_flags = ImPlotAxisFlags_None;
    ImPlotAxisFlags y_flags = ImPlotAxisFlags_None;
    if (auto_fit_requested_) {
        x_flags |= ImPlotAxisFlags_AutoFit;
        y_flags |= ImPlotAxisFlags_AutoFit;
    }

    const float avail_y7 = ImGui::GetContentRegionAvail().y;
    const float prod_plot_h = calculate_responsive_plot_height(avail_y7, avail_w7, 4);
    const int effective_n = (prod_plot_h < 200.0f || prod_ranking_count_ == 5) ? 5 : 10;
    const ImVec2 plot_size(-1, prod_plot_h);
    if (ImPlot::BeginPlot("##ProductsPlot", plot_size, plot_flags)) {
        ImPlot::SetupAxes(get_responsive_y_label("Facturacion Neta ($k USD)", "$k USD", prod_plot_h), "Producto", x_flags, y_flags);

        std::vector<const char*> p_labels;
        for (int i = 0; i < effective_n && i < static_cast<int>(prod_labels_.size()); ++i) {
            p_labels.push_back(prod_labels_[i].c_str());
        }
        std::vector<double> y_pos(p_labels.size());
        for (size_t i = 0; i < p_labels.size(); ++i) {
            y_pos[i] = static_cast<double>(p_labels.size() - 1 - i);
        }
        std::reverse(p_labels.begin(), p_labels.end());
        ImPlot::SetupAxisTicks(ImAxis_Y1, y_pos.data(), static_cast<int>(p_labels.size()), p_labels.data());

        std::vector<double> x_vals;
        for (int i = 0; i < effective_n && i < static_cast<int>(prod_net_k_.size()); ++i) {
            x_vals.push_back(prod_net_k_[i]);
        }
        std::reverse(x_vals.begin(), x_vals.end());

        ImPlotSpec spec_bars;
        spec_bars.FillColor = ImVec4(0.12f, 0.48f, 0.42f, 0.9f);
        spec_bars.Flags = ImPlotBarsFlags_Horizontal;
        ImPlot::PlotBars("Venta Neta ($k USD)", x_vals.data(), y_pos.data(), static_cast<int>(x_vals.size()), 0.55, spec_bars);

        // Anotación en el producto líder
        if (!y_pos.empty()) {
            ImPlot::Annotation(141.40, y_pos.back(), ImVec4(0.2f, 0.85f, 1.0f, 1.0f), ImVec2(-25, -20), true,
                               "Producto Estrella: Cote de Blaye\n$141,396.74 USD (11.17%%)");
        }

        ImPlot::EndPlot();
    }
    auto_fit_requested_ = false;

    ImGui::Columns(calculate_responsive_columns(avail_w7), "##ProdSummaryCols", false);
    render_metric_card("Producto Líder Absoluto", "Cote de Blaye ($141.4k)", "11.17% de facturación global", ImVec4(0.2f, 0.85f, 1.0f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Segundo Producto", "Thuringer Rostbratwurst ($80.4k)", "6.35% de facturación global", ImVec4(0.3f, 0.85f, 0.95f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Concentración Top 10", "44.14% del Portafolio", "$558,958 USD acumulados", ImVec4(0.95f, 0.65f, 0.2f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Catálogo Evaluado", "77 Productos", "Priorizados por Heap O(N log k)", ImVec4(0.3f, 0.95f, 0.5f, 1.0f));
    ImGui::Columns(1);
}

void AnalyticsDashboardView::render_association_rules_tab() {
    auto same_line_flow = [](float needed_w) {
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        if (ImGui::GetContentRegionAvail().x >= needed_w + spacing) {
            ImGui::SameLine(0.0f, spacing);
            return true;
        }
        return false;
    };

    // 1. Controles y Sliders Interactivos con flow
    ImGui::SetNextItemWidth(115.0f);
    ImGui::SliderFloat("Soporte (%)", &min_support_filter_, 0.2f, 2.0f, "%.2f%%");

    same_line_flow(175.0f);
    ImGui::SetNextItemWidth(105.0f);
    ImGui::SliderFloat("Confianza (%)", &min_confidence_filter_, 5.0f, 30.0f, "%.1f%%");

    same_line_flow(150.0f);
    ImGui::SetNextItemWidth(90.0f);
    ImGui::SliderFloat("Lift (x)", &min_lift_filter_, 1.0f, 8.0f, "%.1fx");

    same_line_flow(65.0f);
    if (ImGui::Button("Reset")) {
        min_support_filter_ = 0.4f;
        min_confidence_filter_ = 8.0f;
        min_lift_filter_ = 1.5f;
    }

    const float rem_w = ImGui::GetContentRegionAvail().x;
    if (rem_w >= 140.0f) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(std::min(150.0f, rem_w - 10.0f));
    } else {
        ImGui::SetNextItemWidth(std::min(180.0f, ImGui::GetWindowWidth() * 0.45f));
    }
    ImGui::InputTextWithHint("##filter_rules", "Buscar producto...", rule_search_filter_, sizeof(rule_search_filter_));

    std::string search_str = rule_search_filter_;
    std::transform(search_str.begin(), search_str.end(), search_str.begin(), ::tolower);

    std::vector<size_t> active_indices;
    for (size_t i = 0; i < association_rules_.size(); ++i) {
        const auto& r = association_rules_[i];
        if (r.support_pct >= min_support_filter_ &&
            r.confidence_pct >= min_confidence_filter_ &&
            r.lift >= min_lift_filter_) {
            if (search_str.empty()) {
                active_indices.push_back(i);
            } else {
                std::string full = r.antecedent + " " + r.consequent;
                std::transform(full.begin(), full.end(), full.begin(), ::tolower);
                if (full.find(search_str) != std::string::npos) {
                    active_indices.push_back(i);
                }
            }
        }
    }

    // Selector de Modo de Vista
    ImGui::TextDisabled("Vista:");
    ImGui::SameLine();
    ImGui::RadioButton("Mapa de Reglas", &assoc_rules_view_mode_, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Tabla de Reglas", &assoc_rules_view_mode_, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Vista Combinada", &assoc_rules_view_mode_, 2);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "(%zu / %zu visibles)",
                       active_indices.size(), association_rules_.size());

    // 2. Gráfico Interactivo ImPlot de Dispersión
    if (assoc_rules_view_mode_ != 1) {
        const float plot_h = (assoc_rules_view_mode_ == 0)
                                 ? std::max(160.0f, ImGui::GetContentRegionAvail().y - 10.0f)
                                 : std::max(150.0f, ImGui::GetContentRegionAvail().y * 0.48f);
        ImPlotFlags plot_flags = ImPlotFlags_None;
        if (show_crosshairs_) plot_flags |= ImPlotFlags_Crosshairs;

        ImPlotAxisFlags x_flags = ImPlotAxisFlags_None;
        ImPlotAxisFlags y_flags = ImPlotAxisFlags_None;
        if (auto_fit_requested_) {
            x_flags |= ImPlotAxisFlags_AutoFit;
            y_flags |= ImPlotAxisFlags_AutoFit;
        }

        if (ImPlot::BeginPlot("##AssocRulesPlot", ImVec2(-1, plot_h), plot_flags)) {
            ImPlot::SetupAxes("Soporte Transaccional (%)", get_responsive_y_label("Nivel de Confianza (%)", "Confianza (%)", plot_h), x_flags, y_flags);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0.35, 1.15);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 6.0, 27.0);

            for (size_t idx : active_indices) {
                const auto& r = association_rules_[idx];
                double x = r.support_pct;
                double y = r.confidence_pct;

                const float t = std::clamp(static_cast<float>((r.lift - 1.0) / 6.5), 0.0f, 1.0f);
                ImVec4 col(0.35f + t * 0.60f, 0.20f + t * 0.65f, 0.85f * (1.0f - t * 0.75f), 0.95f);
                if (static_cast<int>(idx) == selected_rule_idx_) {
                    col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                }

                ImPlotSpec spec;
                spec.Marker = ImPlotMarker_Circle;
                spec.MarkerSize = 7.0f + static_cast<float>(r.lift) * 1.4f;
                spec.MarkerFillColor = col;
                spec.MarkerLineColor = ImVec4(1.0f, 1.0f, 1.0f, 0.8f);

                std::string item_id = "##rule_" + std::to_string(idx);
                ImPlot::PlotScatter(item_id.c_str(), &x, &y, 1, spec);

                if (r.lift > 7.0) {
                    ImPlot::Annotation(x, y, ImVec4(0.95f, 0.85f, 0.2f, 1.0f), ImVec2(15, -15), true,
                                       "Regla Estrella: Lift 7.09x\nSir Rodney's Scones -> Sirop d'erable");
                }
            }

            if (ImPlot::IsPlotHovered()) {
                ImPlotPoint mouse_pt = ImPlot::GetPlotMousePos();
                double best_dist2 = 1e9;
                int best_idx = -1;
                for (size_t idx : active_indices) {
                    const auto& r = association_rules_[idx];
                    double dx = (r.support_pct - mouse_pt.x) * 15.0;
                    double dy = (r.confidence_pct - mouse_pt.y);
                    double d2 = dx * dx + dy * dy;
                    if (d2 < best_dist2) {
                        best_dist2 = d2;
                        best_idx = static_cast<int>(idx);
                    }
                }
                if (best_idx >= 0 && best_dist2 < 4.0) {
                    const auto& br = association_rules_[best_idx];
                    ImGui::BeginTooltip();
                    ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "%s -> %s", br.antecedent.c_str(), br.consequent.c_str());
                    ImGui::Separator();
                    ImGui::Text("Soporte: %.2f%% | Confianza: %.1f%%", br.support_pct, br.confidence_pct);
                    ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.2f, 1.0f), "Lift: %.2fx (Asociacion Positiva Muy Fuerte)", br.lift);
                    ImGui::Text("Palanca (Leverage): %.4f", br.leverage);
                    ImGui::EndTooltip();

                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        selected_rule_idx_ = best_idx;
                    }
                }
            }

            ImPlot::EndPlot();
        }
    }
    auto_fit_requested_ = false;

    // 3. Tabla Interactiva de Reglas con Selección
    if (assoc_rules_view_mode_ != 0) {
        ImGui::TextDisabled("Haga clic sobre una regla para seleccionarla e inspeccionar su estrategia comercial:");
        const ImVec2 tbl_sz(0, (assoc_rules_view_mode_ == 1) ? -1 : 160.0f);
        if (ImGui::BeginTable("##AssocRulesTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, tbl_sz)) {
            ImGui::TableSetupColumn("Antecedente (A)", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Consecuente (B)", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Soporte (%)", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Confianza (%)", ImGuiTableColumnFlags_WidthFixed, 95.0f);
            ImGui::TableSetupColumn("Lift (x)", ImGuiTableColumnFlags_WidthFixed, 85.0f);
            ImGui::TableSetupColumn("Estrategia Recomendada", ImGuiTableColumnFlags_WidthFixed, 160.0f);
            ImGui::TableHeadersRow();

            for (size_t idx : active_indices) {
                const auto& r = association_rules_[idx];
                ImGui::TableNextRow();
                const bool is_sel = (static_cast<int>(idx) == selected_rule_idx_);
                if (is_sel) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(40, 80, 130, 180));
                }

                ImGui::TableNextColumn();
                if (ImGui::Selectable(r.antecedent.c_str(), is_sel, ImGuiSelectableFlags_SpanAllColumns)) {
                    selected_rule_idx_ = static_cast<int>(idx);
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(r.consequent.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%.2f%%", r.support_pct);
                ImGui::TableNextColumn();
                ImGui::Text("%.1f%%", r.confidence_pct);
                ImGui::TableNextColumn();
                ImGui::TextColored(r.lift >= 3.0 ? ImVec4(0.95f, 0.85f, 0.2f, 1.0f) : ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "%.2fx", r.lift);
                ImGui::TableNextColumn();
                if (r.lift >= 4.0) {
                    ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.5f, 1.0f), "Bundle Promocional");
                } else if (r.lift >= 2.0) {
                    ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "Cross-Selling Directo");
                } else {
                    ImGui::TextDisabled("Adyacencia en Gondola");
                }
            }
            ImGui::EndTable();
        }
    }
}

void AnalyticsDashboardView::render_rfm_clustering_tab() {
    const float avail_w = ImGui::GetContentRegionAvail().x;

    // 1. Controles y Filtros de Clusters con nombres responsivos
    ImGui::Text("Filtros:");
    ImGui::SameLine();
    const bool compact_cb = (avail_w < 900.0f);
    ImGui::Checkbox(compact_cb ? "C1 (Ocasional)" : "Cluster 1 (Ocasionales)", &show_cluster_[0]);
    render_flow_checkbox(compact_cb ? "C2 (VIP)" : "Cluster 2 (VIPs)", &show_cluster_[1]);
    render_flow_checkbox(compact_cb ? "C3 (Crecim.)" : "Cluster 3 (Crecimiento)", &show_cluster_[2]);
    render_flow_checkbox(compact_cb ? "C4 (Riesgo)" : "Cluster 4 (Riesgo)", &show_cluster_[3]);

    const float rem_w = ImGui::GetContentRegionAvail().x;
    const float filter_w = 150.0f;
    if (rem_w >= filter_w + 10.0f) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(std::min(filter_w, rem_w - 5.0f));
    } else {
        ImGui::SetNextItemWidth(std::min(180.0f, avail_w * 0.45f));
    }
    ImGui::InputTextWithHint("##filter_cust", "Buscar cliente...", customer_search_filter_, sizeof(customer_search_filter_));

    std::string search_str = customer_search_filter_;
    std::transform(search_str.begin(), search_str.end(), search_str.begin(), ::tolower);

    int cluster_counts[4] = {0, 0, 0, 0};
    double cluster_revenue[4] = {0.0, 0.0, 0.0, 0.0};
    for (const auto& c : rfm_customers_) {
        if (c.cluster_id >= 0 && c.cluster_id < 4) {
            cluster_counts[c.cluster_id]++;
            cluster_revenue[c.cluster_id] += c.monetary_net_k;
        }
    }

    // 2. Tarjetas Métricas KPI Superiores
    ImGui::Columns(calculate_responsive_columns(avail_w), "##RFMClusterCols", false);
    render_metric_card("Cluster 2: Clientes VIP", ("3 Clientes ($" + std::to_string(static_cast<int>(cluster_revenue[1])) + "k)").c_str(),
                       "Top: Quick-Stop, Ernst, Save-a-lot", ImVec4(0.95f, 0.65f, 0.2f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Cluster 3: En Crecimiento", (std::to_string(cluster_counts[2]) + " Clientes ($" + std::to_string(static_cast<int>(cluster_revenue[2])) + "k)").c_str(),
                       "Frecuencia 8-19 ord., gasto medio-alto", ImVec4(0.4f, 0.9f, 0.5f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Cluster 1: Ocasionales", (std::to_string(cluster_counts[0]) + " Clientes ($" + std::to_string(static_cast<int>(cluster_revenue[0])) + "k)").c_str(),
                       "Bajo volumen transaccional (<$11k)", ImVec4(0.45f, 0.75f, 1.0f, 1.0f));
    ImGui::NextColumn();
    render_metric_card("Cluster 4: En Riesgo", (std::to_string(cluster_counts[3]) + " Clientes Inactivos").c_str(),
                       "Recencia > 300 días sin compras", ImVec4(0.95f, 0.3f, 0.3f, 1.0f));
    ImGui::Columns(1);

    // Selector de Modo de Vista para RFM
    ImGui::TextDisabled("Vista:");
    ImGui::SameLine();
    ImGui::RadioButton("Gráficos RFM", &rfm_view_mode_, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Tabla de Clientes", &rfm_view_mode_, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Vista Combinada", &rfm_view_mode_, 2);

    const ImVec4 cluster_colors[4] = {
        ImVec4(0.45f, 0.75f, 1.0f, 0.9f),
        ImVec4(0.95f, 0.60f, 0.2f, 0.95f),
        ImVec4(0.4f, 0.9f, 0.5f, 0.9f),
        ImVec4(0.95f, 0.3f, 0.3f, 0.9f)
    };

    // 3. Dos Gráficos Dinámicos ImPlot (apilados si el ancho es estrecho)
    if (rfm_view_mode_ != 1) {
        const bool stack_vert = (avail_w < 780.0f);
        const float plot_w = stack_vert ? avail_w : (avail_w - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        const float plot_h = (rfm_view_mode_ == 0)
                                 ? (stack_vert ? std::max(160.0f, ImGui::GetContentRegionAvail().y * 0.48f) : std::max(180.0f, ImGui::GetContentRegionAvail().y - 10.0f))
                                 : (stack_vert ? 160.0f : std::max(160.0f, ImGui::GetContentRegionAvail().y * 0.52f));

        ImPlotFlags plot_flags = ImPlotFlags_None;
        if (show_crosshairs_) plot_flags |= ImPlotFlags_Crosshairs;

        ImPlotAxisFlags x_flags = ImPlotAxisFlags_None;
        ImPlotAxisFlags y_flags = ImPlotAxisFlags_None;
        if (auto_fit_requested_) {
            x_flags |= ImPlotAxisFlags_AutoFit;
            y_flags |= ImPlotAxisFlags_AutoFit;
        }

        // Gráfico Izquierdo: Frecuencia vs Facturación Neta
        if (ImPlot::BeginPlot("##RFMFreqMonetaryPlot", ImVec2(plot_w, plot_h), plot_flags)) {
            // Posicionar leyenda en SouthEast (área vacía sin datos de alta frecuencia y baja facturación)
            ImPlot::SetupLegend(ImPlotLocation_SouthEast);
            ImPlot::SetupAxes("Frecuencia (Total Ordenes)", get_responsive_y_label("Facturacion Neta ($k USD)", "$k USD", plot_h), x_flags, y_flags);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, 35);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -5, 120);

            for (int c_id = 0; c_id < 4; ++c_id) {
                if (!show_cluster_[c_id]) continue;
                std::vector<double> xs, ys;
                for (const auto& c : rfm_customers_) {
                    if (c.cluster_id == c_id) {
                        if (search_str.empty() || c.company_name.find(search_str) != std::string::npos || c.customer_id.find(search_str) != std::string::npos) {
                            xs.push_back(c.frequency_orders);
                            ys.push_back(c.monetary_net_k);
                        }
                    }
                }
                if (!xs.empty()) {
                    ImPlotSpec spec;
                    spec.Marker = (c_id == 1) ? ImPlotMarker_Diamond : ImPlotMarker_Circle;
                    spec.MarkerSize = (c_id == 1) ? 9.0f : 6.0f;
                    spec.MarkerFillColor = cluster_colors[c_id];
                    spec.MarkerLineColor = ImVec4(1.0f, 1.0f, 1.0f, 0.8f);

                    const char* c_labels[4] = { "C1: Ocasionales", "C2: VIPs", "C3: Crecimiento", "C4: Riesgo" };
                    ImPlot::PlotScatter(c_labels[c_id], xs.data(), ys.data(), static_cast<int>(xs.size()), spec);
                }
            }

            // Anotaciones separadas sin solapamiento entre sí ni con la leyenda
            ImPlot::Annotation(28.0, 110.28, ImVec4(0.3f, 0.85f, 1.0f, 1.0f), ImVec2(-110, -18), true, "QUICK-Stop ($110.3k)");
            ImPlot::Annotation(30.0, 104.87, ImVec4(0.95f, 0.65f, 0.2f, 1.0f), ImVec2(-80, 20), true, "Ernst Handel ($104.9k)");
            ImPlot::Annotation(31.0, 104.36, ImVec4(0.4f, 0.9f, 0.5f, 1.0f), ImVec2(10, -20), true, "Save-a-lot ($104.4k)");

            ImPlot::EndPlot();
        }

        if (!stack_vert) {
            ImGui::SameLine();
        } else {
            ImGui::Spacing();
        }

        // Gráfico Derecho: Recencia vs Facturación Neta
        if (ImPlot::BeginPlot("##RFMRecencyMonetaryPlot", ImVec2(plot_w, plot_h), plot_flags)) {
            // Posicionar leyenda en NorthEast para evitar cubrir datos en NorthWest
            ImPlot::SetupLegend(ImPlotLocation_NorthEast);
            ImPlot::SetupAxes("Recencia (Dias sin Compras)", get_responsive_y_label("Facturacion Neta ($k USD)", "$k USD", plot_h), x_flags, y_flags);
            ImPlot::SetupAxisLimits(ImAxis_X1, -20, 700);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -5, 120);

            for (int c_id = 0; c_id < 4; ++c_id) {
                if (!show_cluster_[c_id]) continue;
                std::vector<double> xs, ys;
                for (const auto& c : rfm_customers_) {
                    if (c.cluster_id == c_id) {
                        if (search_str.empty() || c.company_name.find(search_str) != std::string::npos || c.customer_id.find(search_str) != std::string::npos) {
                            xs.push_back(c.recency_days);
                            ys.push_back(c.monetary_net_k);
                        }
                    }
                }
                if (!xs.empty()) {
                    ImPlotSpec spec;
                    spec.Marker = (c_id == 3) ? ImPlotMarker_Cross : ImPlotMarker_Circle;
                    spec.MarkerSize = (c_id == 3) ? 8.0f : 6.0f;
                    spec.MarkerFillColor = cluster_colors[c_id];
                    spec.MarkerLineColor = ImVec4(1.0f, 1.0f, 1.0f, 0.8f);

                    const char* c_labels[4] = { "C1: Ocasionales", "C2: VIPs", "C3: Crecimiento", "C4: Riesgo" };
                    ImPlot::PlotScatter(c_labels[c_id], xs.data(), ys.data(), static_cast<int>(xs.size()), spec);
                }
            }

            // Anotación con desplazamiento a la izquierda y elevación para no recortarse
            ImPlot::Annotation(655.0, 3.52, ImVec4(0.95f, 0.3f, 0.3f, 1.0f), ImVec2(-185, -28), true, "Paris specialites (Inactivo: 655 d)");

            ImPlot::EndPlot();
        }
    }
    auto_fit_requested_ = false;

    // 4. Tabla Detallada de Clientes y Asignación RFM
    if (rfm_view_mode_ != 0) {
        const ImVec2 tbl_sz(0, (rfm_view_mode_ == 1) ? -1 : 160.0f);
        if (ImGui::BeginTable("##RFMCustomersTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX, tbl_sz)) {
            ImGui::TableSetupColumn("Codigo", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("Empresa / Cliente", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Recencia (Dias)", ImGuiTableColumnFlags_WidthFixed, 105.0f);
            ImGui::TableSetupColumn("Frecuencia (Ord.)", ImGuiTableColumnFlags_WidthFixed, 115.0f);
            ImGui::TableSetupColumn("Facturacion Neta ($k)", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn("Cluster K-Means", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < rfm_customers_.size(); ++i) {
                const auto& c = rfm_customers_[i];
                if (!show_cluster_[c.cluster_id]) continue;
                if (!search_str.empty() && c.company_name.find(search_str) == std::string::npos && c.customer_id.find(search_str) == std::string::npos) {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(cluster_colors[c.cluster_id], "%s", c.customer_id.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(c.company_name.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%.0f d", c.recency_days);
                ImGui::TableNextColumn();
                ImGui::Text("%.0f", c.frequency_orders);
                ImGui::TableNextColumn();
                ImGui::Text("$%.2f k", c.monetary_net_k);
                ImGui::TableNextColumn();
                const char* c_names[4] = { "C1: Ocasional", "C2: VIP", "C3: Crecimiento", "C4: Desercion" };
                ImGui::TextColored(cluster_colors[c.cluster_id], "%s", c_names[c.cluster_id]);
            }
            ImGui::EndTable();
        }
    }
}

} // namespace intelidata::gui
