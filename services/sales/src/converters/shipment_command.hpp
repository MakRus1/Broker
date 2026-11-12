#pragma once
#include <clients/shipments/client.hpp>
#include <models/shipment_creation_context.hpp>
#include <platform/auth/identity.hpp>

namespace sales::converters {
inline clients::shipments::models::EnsureShipmentRequest ToShipmentCommand(
    const std::string& organization, const platform::auth::Identity& actor,
    const models::Inquiry& inquiry, const models::ShipmentCreationContext& creation) {
    namespace json = userver::formats::json;
    namespace shipment_models = clients::shipments::models;
    constexpr auto kInquirySourceType = "sales.inquiry";
    shipment_models::CreateShipmentRequest shipment;
    shipment.customer_id = inquiry.customer_id;
    shipment.idempotency_key =
        std::string{kInquirySourceType} + ":" + inquiry.id + ":" + std::to_string(inquiry.revision);
    shipment.reference = inquiry.number;
    shipment.cargo_description = inquiry.cargo_description;
    shipment.carriage_terms = inquiry.carriage_terms;
    shipment.delivery_address = inquiry.delivery_address;
    shipment.delivery_city = inquiry.delivery_city;
    shipment.delivery_country = inquiry.delivery_country;
    shipment.incoterms = inquiry.incoterms;
    shipment.loading = inquiry.loading;
    shipment.logist = creation.Logist;
    shipment.route_segments.emplace();
    for (const auto& segment : creation.RouteSegments)
        shipment.route_segments->push_back(
            shipment_models::ParseModelJson(json::ValueBuilder(segment).ExtractValue(),
                std::type_identity<shipment_models::ShipmentRouteSegment>{}));
    shipment_models::EnsureShipmentRequest command;
    command.organization_id = organization;
    command.source.type = kInquirySourceType;
    command.source.id = inquiry.id;
    command.actor_subject = actor.Subject;
    command.actor_issuer = actor.Issuer;
    command.shipment = std::move(shipment);
    return command;
}
}  // namespace sales::converters
