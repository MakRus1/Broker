#include <components/inquiries.hpp>
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
    const auto creation = deps.inquiries->PrepareShipmentCreation(organization, token, before);

    return Response200{
        .item = deps.inquiries->StartShipmentCreation(organization, actor, request.body, creation)};
}

}  // namespace sales::views::api::v1::sales::inquiries::create_shipment
