#include <helpers/finance_settings.hpp>
#include <views/api/admin/finance/settings/update/view.hpp>

namespace finance::views::api::admin::finance::settings::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& body = request.body;
    helpers::RequireFinanceAdministrator(deps, context, body.organization_id);
    helpers::ValidateCurrency(body.reporting_currency);
    platform::ValidateNumberTemplate(body.outgoing_number_template, 100);
    platform::ValidateNumberTemplate(body.incoming_number_template, 100);
    if (body.expected_revision < 0 ||
        body.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция настроек"};
    const auto rows = deps.pg_cluster->Execute(helpers::pg::ClusterHostType::kMaster,
        sql::kSaveFinanceSettings, body.organization_id, body.reporting_currency,
        body.expected_revision, body.outgoing_number_template, body.incoming_number_template,
        body.reset_numbers_yearly);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Настройки изменены. Обновите форму"};
    const auto result = helpers::ConvertFinanceSettings(rows);
    return Response200{.incoming_number_template = result.incoming_number_template,
        .outgoing_number_template = result.outgoing_number_template,
        .reporting_currency = result.reporting_currency,
        .reset_numbers_yearly = result.reset_numbers_yearly,
        .revision = result.revision};
}
}  // namespace finance::views::api::admin::finance::settings::update
