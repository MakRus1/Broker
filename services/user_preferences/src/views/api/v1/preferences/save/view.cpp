#include <helpers/preferences.hpp>
#include <userver/formats/json/serialize.hpp>
#include <views/api/v1/preferences/save/view.hpp>

namespace user_preferences::views::api::v1::preferences::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::user_preferences::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto& body = request.body;
    CheckOrganization(deps, context, body.organization_id);
    if (userver::formats::json::ToString(body.value).size() > 65536)
        throw ApiError<400>{"preference_too_large", "Настройка не должна превышать 64 КиБ"};
    const auto rows = body.expected_revision == 0
        ? deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kCreatePreference,
              body.organization_id, identity.Issuer, identity.Subject, body.key, body.value)
        : deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kUpdatePreference,
              body.organization_id, identity.Issuer, identity.Subject, body.key, body.value,
              body.expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"preference_changed", "Настройка изменена на другом устройстве"};
    return Response200{.preference = Convert(rows.AsSingleRow<PreferenceRow>(pg::kRowTag))};
}
}  // namespace user_preferences::views::api::v1::preferences::save
