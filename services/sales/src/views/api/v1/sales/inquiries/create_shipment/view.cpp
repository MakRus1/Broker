#include <components/inquiries.hpp>
#include <components/sales_directory.hpp>
#include <models/errors.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/inquiries/create_shipment/view.hpp>

namespace sales::views::api::v1::sales::inquiries::create_shipment {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, "shipments.write");
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);
    const auto before = deps.inquiries->FindInquiry(organization, request.body.id);
    if (before.shipment_id || before.converting)
        return Response200{.item = before};

    const auto& token = platform::auth::GetAccessToken(context);
    const auto party = deps.directory->GetCounterparty(token, before.customer_id);
    if (party.archived)
        throw ApiError<409>{"archived_customer", "Клиент находится в архиве"};
    models::ShipmentCreationContext creation{.InquiryRevision = before.revision,
        .Logist = party.logist.value_or(std::nullopt),
        .RouteSegments = before.route_segments};
    deps.directory->ResolveShipmentRoute(
        organization, token, creation.RouteSegments, creation.Logist);

    return Response200{
        .item = deps.inquiries->StartShipmentCreation(organization, actor, request.body, creation)};
}
}  // namespace sales::views::api::v1::sales::inquiries::create_shipment
