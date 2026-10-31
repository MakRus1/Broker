#include <helpers/finance_settings.hpp>
#include <views/api/admin/finance/settings/update/view.hpp>

namespace finance::views::api::admin::finance::settings::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& body = request.body;
    helpers::RequireFinanceAdministrator(deps, context, body.organization_id);
    helpers::ValidateCurrency(body.reporting_currency);
    if (body.expected_revision < 0 ||
        body.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция настроек"};
    const auto rows =
        deps.pg_cluster->Execute(helpers::pg::ClusterHostType::kMaster, sql::kSaveFinanceSettings,
            body.organization_id, body.reporting_currency, body.expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Настройки изменены. Обновите форму"};
    const auto [currency, revision] =
        rows.AsSingleRow<std::tuple<std::string, std::int64_t>>(helpers::pg::kRowTag);
    return Response200{.reporting_currency = currency, .revision = revision};
}
}  // namespace finance::views::api::admin::finance::settings::update
