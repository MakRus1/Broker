#include <helpers/numbering.hpp>
#include <views/api/admin/contracts/numbering/update/view.hpp>

namespace contracts::views::api::admin::contracts::numbering::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& body = request.body;
    helpers::RequireNumberingAdministrator(deps, context, body.organization_id);
    platform::ValidateNumberTemplate(body.number_template);
    if (body.expected_revision < 0 ||
        body.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция настроек"};
    const auto rows = deps.pg_cluster->Execute(helpers::pg::ClusterHostType::kMaster,
        sql::kSaveNumbering, body.organization_id, body.number_template, body.reset_number_yearly,
        body.expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Настройки изменены. Обновите форму"};
    const auto result = helpers::ConvertNumbering(rows);
    return Response200{.number_template = result.number_template,
        .reset_number_yearly = result.reset_number_yearly,
        .revision = result.revision};
}
}  // namespace contracts::views::api::admin::contracts::numbering::update
