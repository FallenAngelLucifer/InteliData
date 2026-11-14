#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include "intelidata/model/SemanticAST.hpp"
#include "intelidata/ingestion/DuckDBAdapter.hpp"
#include <string>
#include <vector>

namespace intelidata::gui {

/**
 * @brief Tablero de Analítica Interactiva, Cuadro de Mando y Finanzas (BI).
 * Proporciona gráficos dinámicos interactivos con zoom in/out, desplazamientos (pan),
 * inspección por cursor (crosshairs), selección de rangos y restablecimiento de vistas
 * mediante ImPlot, organizados en pestañas temáticas de alta dirección.
 */
class AnalyticsDashboardView {
public:
    AnalyticsDashboardView();

    // Renderiza el panel analítico interactivo desacoplable
    void render(const char* title, const model::SemanticAST& ast, ingestion::DuckDBAdapter* adapter);

    // Notifica la carga o cambio de dataset para actualizar series de datos
    void on_dataset_loaded(const std::string& label, ingestion::DuckDBAdapter* adapter);

    // Guarda las 6 figuras analíticas en el directorio de salida y genera reporte complementario
    void save_report_charts(const std::string& target_dir = "");

    void set_selected_tab(int tab) noexcept { selected_tab_ = tab; requested_tab_ = tab; }

private:
    void render_toolbar();
    void render_time_series_tab();
    void render_categories_tab();
    void render_geography_tab();
    void render_waterfall_tab();
    void render_benford_tab();
    void render_lorenz_pareto_tab();
    void render_products_tab();
    void render_association_rules_tab();
    void render_rfm_clustering_tab();

    // Inicialización y actualización de datasets analíticos
    void init_default_northwind_data();

    // Estado de filtros y visualización interactiva
    bool auto_fit_requested_{true};
    bool show_gross_{true};
    bool show_net_{true};
    bool show_freight_{true};
    bool show_discount_{true};
    bool show_area_fill_{true};
    bool show_crosshairs_{true};
    int selected_tab_{0};
    int requested_tab_{-1};
    int category_sort_mode_{0}; // 0 = Venta Neta Desc, 1 = Merma Desc, 2 = Alfabético

    // Estructuras de datos para las series de tiempo (23 meses)
    std::vector<std::string> time_labels_;
    std::vector<double> time_x_;
    std::vector<double> time_gross_k_;
    std::vector<double> time_net_k_;
    std::vector<double> time_freight_k_;
    std::vector<double> time_discount_k_;

    // Estructuras de datos para categorías (8 categorías)
    std::vector<std::string> cat_labels_;
    std::vector<double> cat_x_;
    std::vector<double> cat_net_k_;
    std::vector<double> cat_merma_pct_;

    // Estructuras de datos para países (Top 10)
    std::vector<std::string> country_labels_;
    std::vector<double> country_x_;
    std::vector<double> country_net_k_;
    std::vector<double> country_freight_k_;

    // Estructuras de datos para Cascada Financiera (Waterfall)
    std::vector<std::string> waterfall_labels_;
    std::vector<double> waterfall_x_;
    std::vector<double> waterfall_bottoms_k_;
    std::vector<double> waterfall_heights_k_;

    // Estructuras de datos para Ley de Benford
    std::vector<double> benford_digits_;
    std::vector<double> benford_observed_pct_;
    std::vector<double> benford_theoretical_pct_;
    double benford_chi2_{21.07};
    double benford_pval_{0.006957};
    double benford_mad_{0.0123};
    uint64_t benford_n_{2155};
    std::string benford_col_name_{"UnitPrice"};
    std::string benford_table_name_{"OrderDetails"};
    std::string benford_dynamic_interpretation_;
    int selected_benford_col_idx_{0};
    std::vector<std::pair<std::string, const model::ColumnAST*>> available_benford_cols_;

    // Estructuras de datos para Curva de Lorenz (Pareto)
    std::vector<double> lorenz_cust_pct_;
    std::vector<double> lorenz_rev_pct_;
    std::vector<double> lorenz_equality_pct_;

    // Estructuras de datos para Ranking de Productos (Top 10)
    std::vector<std::string> prod_labels_;
    std::vector<double> prod_x_;
    std::vector<double> prod_net_k_;
    std::vector<double> prod_share_pct_;

    // Estructuras de datos para Minería de Datos: Reglas de Asociación (Apriori)
    struct AssociationRuleUI {
        std::string antecedent;
        std::string consequent;
        double support_pct;
        double confidence_pct;
        double lift;
        double leverage;
    };
    std::vector<AssociationRuleUI> association_rules_;
    float min_support_filter_{0.4f};
    float min_confidence_filter_{8.0f};
    float min_lift_filter_{1.5f};
    int selected_rule_idx_{-1};
    char rule_search_filter_[64]{""};

    // Estructuras de datos para Minería de Datos: Clustering y Segmentación RFM (K-Means)
    struct CustomerRFMUI {
        std::string customer_id;
        std::string company_name;
        double recency_days;
        double frequency_orders;
        double monetary_net_k;
        int cluster_id; // 0: Ocasionales, 1: VIP, 2: Crecimiento, 3: Riesgo
    };
    std::vector<CustomerRFMUI> rfm_customers_;
    bool show_cluster_[4]{true, true, true, true};
    int selected_customer_idx_{-1};
    char customer_search_filter_[64]{""};

    // Mensaje de estado para notificaciones de exportación
    std::string status_message_;
};

} // namespace intelidata::gui
