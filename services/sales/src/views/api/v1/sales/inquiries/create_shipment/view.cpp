#include <helpers/shipment_references.hpp>
#include <platform/outbox/component.hpp>
#include <views/api/v1/sales/inquiries/create_shipment/view.hpp>

namespace sales::views::api::v1::sales::inquiries::create_shipment {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    platform::auth::CheckPermission(context, "shipments.write");
    const auto organization = Organization(context);
    const auto& actor = platform::auth::GetIdentity(context);
    // Справочник читаем до транзакции: назначение логиста фиксируется при постановке.
    const auto before = Read<models::Inquiry>(deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetInquiries, organization, request.body.id));
    if (before.shipment_id.has_value() || before.converting)
        return Response200{.item = before};
    const auto party = Reference([&] {
        return deps.counterparties.ApiV1CounterpartiesGet(
            {.body = {.id = before.customer_id}}, platform::auth::GetAccessToken(context));
    });
    if (party.counterparty.archived)
        throw ApiError<409>{"archived_customer", "Клиент находится в архиве"};
    auto segments = before.route_segments;
    ResolveShipmentReferences(
        deps, context, segments, party.counterparty.logist.value_or(std::nullopt));
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto inquiry = Read<models::Inquiry>(
        transaction.Execute(sql::kLockInquiries, organization, request.body.id));
    if (inquiry.shipment_id.has_value() || inquiry.converting) {
        transaction.Commit();
        return Response200{.item = inquiry};
    }
    if (inquiry.revision != request.body.expected_revision || inquiry.revision != before.revision)
        throw ApiError<409>{"revision_conflict", "Запрос изменён. Обновите карточку"};
    if (inquiry.status != models::InquiryStatus::Agreed || inquiry.conversion_error.has_value())
        throw ApiError<409>{"not_agreed", "Сначала согласуйте запрос"};
    json::ValueBuilder shipment(json::ValueBuilder(inquiry).ExtractValue());
    for (const auto* field : {"id", "revision", "number", "manager_subject", "status", "ready_date",
             "desired_delivery_date", "note", "converting", "conversion_error"})
        shipment.Remove(field);
    shipment["idempotency_key"] =
        "sales-inquiry:" + inquiry.id + ":" + std::to_string(inquiry.revision);
    shipment["reference"] = inquiry.number;
    shipment["route_segments"] = segments;
    shipment["logist"] = party.counterparty.logist.value_or(std::nullopt);
    json::ValueBuilder command;
    command["organization_id"] = organization;
    command["source"]["type"] = "sales.inquiry";
    command["source"]["id"] = inquiry.id;
    command["actor_subject"] = actor.Subject;
    command["actor_issuer"] = actor.Issuer;
    command["shipment"] = shipment.ExtractValue();
    const auto document = command.ExtractValue();
    transaction.Execute(sql::kStartConversion, organization, inquiry.id, document);
    platform::outbox::Enqueue(transaction, "shipments.create", inquiry.id, document);
    const auto result =
        Read<models::Inquiry>(transaction.Execute(sql::kGetInquiries, organization, inquiry.id));
    transaction.Commit();
    return Response200{.item = result};
}
}  // namespace sales::views::api::v1::sales::inquiries::create_shipment
