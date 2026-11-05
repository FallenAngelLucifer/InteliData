#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "intelidata/core/PIND.hpp"
#include <unordered_set>
#include <vector>
#include <string>

using namespace intelidata;
using namespace intelidata::core;
using Catch::Matchers::WithinRel;
using Catch::Matchers::WithinAbs;

TEST_CASE("PIND: Dependencia de inclusión probabilística estricta (FK válida)", "[pind]") {
    // Origen: Llave foránea en tabla de órdenes
    const std::unordered_set<std::string> fk_orders = {"C101", "C102", "C103", "C104"};
    // Destino: Llave primaria en tabla de clientes
    const std::unordered_set<std::string> pk_customers = {"C101", "C102", "C103", "C104", "C105", "C106"};

    const auto res = evaluate_inclusion(fk_orders, pk_customers, true, 0.02);

    REQUIRE(res.source_distinct_count == 4);
    REQUIRE(res.target_distinct_count == 6);
    REQUIRE(res.matched_count == 4);
    REQUIRE_THAT(res.inclusion_ratio, WithinAbs(1.0, 1e-12));
    REQUIRE(res.is_likely_foreign_key);
    REQUIRE_THAT(res.confidence, WithinAbs(1.0, 1e-12));

    // Dirección inversa: los clientes NO están incluidos en las órdenes
    const auto reverse_res = evaluate_inclusion(pk_customers, fk_orders, false, 0.02);
    REQUIRE_THAT(reverse_res.inclusion_ratio, WithinRel(4.0 / 6.0, 1e-12));
    REQUIRE_FALSE(reverse_res.is_likely_foreign_key);
}

TEST_CASE("PIND: Información Mutua Normalizada (NMI) en dependencia funcional perfecta", "[pind]") {
    // Mapeo biyectivo: X e Y contienen exactamente la misma información codificada
    const std::vector<std::string> x = {"A", "A", "B", "B", "C", "C", "D", "D"};
    const std::vector<std::string> y = {"1", "1", "2", "2", "3", "3", "4", "4"};

    const Float64 nmi_perfect = compute_nmi(x, y);
    REQUIRE_THAT(nmi_perfect, WithinAbs(1.0, 1e-12));

    // Variable independiente
    const std::vector<std::string> z = {"1", "2", "1", "2", "1", "2", "1", "2"};
    const Float64 nmi_independent = compute_nmi(x, z);
    REQUIRE_THAT(nmi_independent, WithinAbs(0.0, 1e-12));
}
