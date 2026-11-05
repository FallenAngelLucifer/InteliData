#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include "intelidata/model/SemanticAST.hpp"
#include <string>
#include <vector>

namespace intelidata::pipeline {

enum class DomainType {
    SportsCombatGaming,      // Deportes de Combate, Torneos y Salas en Línea
    EcommerceSales,          // Comercio Electrónico, Facturación y Ventas
    BankingFinance,          // Banca, Finanzas y Contabilidad
    HealthcareClinical,      // Salud, Gestión Clínica y Expedientes Médicos
    EducationAcademic,       // Educación, Universidad y Gestión Académica
    LogisticsFleet,          // Logística, Flota y Distribución Física
    SocialCommunity,         // Redes Sociales, Mensajería y Comunidad
    HumanResources,          // Gestión de Personal y Recursos Humanos
    TelemetryIoT,            // Telecomunicaciones, Redes y Telemetría IoT
    AdaptiveGeneral          // Inferencia Adaptativa para Dominios No Catalogados
};

struct DomainClassificationResult {
    DomainType type{DomainType::AdaptiveGeneral};
    std::string domain_name;
    Float64 confidence{0.0};
    std::string icon;
    std::string primary_focus;
    std::string narrative;
    std::vector<std::string> executive_insights;
};

class DomainClassifier {
public:
    /// Clasifica el dominio del sistema a partir de evidencias estadísticas, topológicas y léxicas
    [[nodiscard]] static DomainClassificationResult classify(const model::SemanticAST& ast);

    /// Enriquece el AST con diagnósticos ejecutivos, narrativa, subdominios y fichas de negocio
    static void enrich_ast_with_business_intelligence(model::SemanticAST& ast);

    /// Formatea un nombre crudo/ofuscado a un nombre legible de presentación
    [[nodiscard]] static std::string prettify_identifier(const std::string& raw);
};

} // namespace intelidata::pipeline
