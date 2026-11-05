#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "intelidata/core/Markov.hpp"
#include <vector>
#include <string>
#include <random>

using namespace intelidata;
using namespace intelidata::core;
using Catch::Matchers::WithinRel;
using Catch::Matchers::WithinAbs;

TEST_CASE("MarkovChainModel: Detección de estados absorbentes en ciclo de vida", "[markov]") {
    MarkovChainModel model;

    // Transiciones de pedidos de e-commerce
    // Flujo normal: CREADO -> EN_PROCESO -> ENVIADO -> ENTREGADO -> ENTREGADO
    for (int i = 0; i < 20; ++i) {
        model.add_transition("CREADO", "EN_PROCESO");
        model.add_transition("EN_PROCESO", "ENVIADO");
        model.add_transition("ENVIADO", "ENTREGADO");
        model.add_transition("ENTREGADO", "ENTREGADO"); // Auto-bucle terminal
    }

    // Flujo de cancelación: CREADO -> CANCELADO -> CANCELADO
    for (int i = 0; i < 5; ++i) {
        model.add_transition("CREADO", "CANCELADO");
        model.add_transition("CANCELADO", "CANCELADO");
    }

    const auto res = model.analyze(0.98);

    REQUIRE(res.states.size() == 5);
    REQUIRE(res.is_discrete_state_machine);

    // Estados absorbentes esperados: ENTREGADO y CANCELADO
    REQUIRE(res.absorbing_states.size() == 2);
    const bool has_entregado = std::find(res.absorbing_states.begin(), res.absorbing_states.end(), "ENTREGADO") != res.absorbing_states.end();
    const bool has_cancelado = std::find(res.absorbing_states.begin(), res.absorbing_states.end(), "CANCELADO") != res.absorbing_states.end();
    REQUIRE(has_entregado);
    REQUIRE(has_cancelado);
}

TEST_CASE("CharacterMarkovModel: Discriminación entre Hash/UUID y Lenguaje Natural", "[markov]") {
    CharacterMarkovModel model_uuid;
    CharacterMarkovModel model_nlp;

    // Generar 50 hashes pseudoaleatorios hex de 32 caracteres (distribución uniforme de caracteres hex)
    std::mt19937_64 rng(1337);
    const char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < 50; ++i) {
        std::string h;
        h.reserve(32);
        for (int j = 0; j < 32; ++j) {
            h.push_back(hex_chars[rng() % 16]);
        }
        model_uuid.update(h);
    }

    // Texto en lenguaje natural (baja entropía de transición por patrones vocálicos y consonánticos)
    const std::vector<std::string> natural_texts = {
        "esta es una descripcion detallada del producto para el cliente residencial en la base de datos",
        "direccion de entrega en avenida de los insurgentes sur numero quinientos interior cuatro",
        "observaciones sobre el envio de mercancia perecedera al almacen central de distribucion",
        "el cliente solicita confirmacion por correo electronico antes de proceder con el cobro total",
        "reporte mensual de inventario y facturacion correspondiente al periodo fiscal concluido"
    };
    for (const auto& text : natural_texts) {
        model_nlp.update(text);
    }

    REQUIRE(model_uuid.transition_entropy() > model_nlp.transition_entropy());
    REQUIRE(model_uuid.is_high_entropy_hash_or_uuid());
    REQUIRE(model_nlp.is_natural_language());
}
