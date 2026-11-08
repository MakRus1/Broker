#include <helpers/shipment_command.hpp>
#include <helpers/shipment_references.hpp>
#include <platform/outbox/component.hpp>
#include <views/api/v1/sales/inquiries/create_shipment/view.hpp>

namespace sales::views::api::v1::sales::inquiries::create_shipment {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    platform::auth::CheckPermission(context, "shipments.write");
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto& actor = platform::auth::GetIdentity(context);
    // Справочник читаем до транзакции: назначение логиста фиксируется при постановке.
    const auto before_rows = deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetInquiries, organization, request.body.id);
    if (before_rows.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    const auto before = models::ParseModelJson(
        before_rows.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});
    if (before.shipment_id.has_value() || before.converting)
        return Response200{.item = before};
    const auto party = GetCounterparty(deps, context, before.customer_id);
    if (party.archived)
        throw ApiError<409>{"archived_customer", "Клиент находится в архиве"};
    auto segments = before.route_segments;
    ResolveShipmentReferences(deps, context, segments, party.logist.value_or(std::nullopt));
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto locked = transaction.Execute(sql::kLockInquiries, organization, request.body.id);
    if (locked.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    const auto inquiry = models::ParseModelJson(
        locked.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});
    if (inquiry.shipment_id.has_value() || inquiry.converting) {
        transaction.Commit();
        return Response200{.item = inquiry};
    }
    if (inquiry.revision != request.body.expected_revision || inquiry.revision != before.revision)
        throw ApiError<409>{"revision_conflict", "Запрос изменён. Обновите карточку"};
    if (inquiry.status != models::InquiryStatus::Agreed || inquiry.conversion_error.has_value())
        throw ApiError<409>{"not_agreed", "Сначала согласуйте запрос"};
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
    shipment.logist = party.logist.value_or(std::nullopt);
    shipment.route_segments.emplace();
    for (const auto& segment : segments)
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
    const auto document = json::ValueBuilder(command).ExtractValue();
    transaction.Execute(sql::kStartConversion, organization, inquiry.id, document);
    platform::outbox::Enqueue(transaction, kCreateShipment, inquiry.id, command);
    const auto rows = transaction.Execute(sql::kGetInquiries, organization, inquiry.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    const auto result = models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});
    transaction.Commit();
    return Response200{.item = result};
}
}  // namespace sales::views::api::v1::sales::inquiries::create_shipment
