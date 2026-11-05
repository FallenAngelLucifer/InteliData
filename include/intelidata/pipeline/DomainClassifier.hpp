#pragma once

#include "intelidata/common/Platform.hpp"
#include "intelidata/common/Types.hpp"
#include "intelidata/model/SemanticAST.hpp"
#include <string>
#include <vector>

namespace intelidata::pipeline {

enum class DomainType {
    EcommerceSales,          // 1. Comercio Electrónico, Facturación y Ventas
    BankingFinance,          // 2. Banca, Finanzas y Contabilidad
    SportsTournament,        // 3. Deportes y Torneos Competitivos
    GamingMatchmaking,       // 4. Videojuegos, Gaming y Plataformas Multijugador
    HealthcareClinical,      // 5. Salud, Hospitales y Expedientes Médicos
    EducationAcademic,       // 6. Educación, Universidades y Gestión Académica
    LogisticsFleet,          // 7. Logística, Distribución, Almacenes y Flotas
    HumanResources,          // 8. Recursos Humanos, Gestión de Personal y Nómina
    SocialCommunity,         // 9. Redes Sociales, Mensajería y Comunidades
    HospitalityTourism,      // 10. Hotelería, Turismo y Reservas
    RealEstate,              // 11. Bienes Raíces, Inmobiliaria y Gestión de Propiedades
    ManufacturingSupply,     // 12. Manufactura, Producción Industrial y Calidad
    TelemetryIoT,            // 13. Telecomunicaciones, Redes y Telemetría IoT
    PublicSectorGov,         // 14. Sector Público, Trámites Gubernamentales y Ciudadanía
    MediaStreaming,          // 15. Streaming, Medios Audiovisuales y Contenido Digital
    CustomerSupportCRM,      // 16. CRM, Soporte Técnico y Ticketing
    LegalJudicial,           // 17. Gestión Legal, Jurídica y Casos Procesales
    AdaptiveGeneral          // 18. Inferencia Adaptativa Inductiva (Fallback General)
};

struct DomainClassificationResult {
    DomainType type{DomainType::AdaptiveGeneral};
    std::string domain_name;
    Float64 confidence{0.0};
    std::string icon;
    std::string primary_focus;
    std::string narrative;
    std::vector<std::string> executive_insights;
    std::vector<std::string> industry_tags;
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
