#include <helpers/settings.hpp>
#include <views/api/admin/sales/settings/update/view.hpp>

namespace sales::views::api::admin::sales::settings::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    const auto& value = request.body;
    RequireAdministrator(deps, context, value.organization_id);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockSettings, value.organization_id);
    const auto previous = Settings(transaction.Execute(sql::kGetSettings, value.organization_id));
    if (previous.revision != value.expected_revision)
        throw ApiError<409>{"revision_conflict", "Настройки изменены. Обновите страницу"};
    ValidateStages(value, previous);
    platform::ValidateNumberTemplate(value.inquiry_number_template);
    const auto rows = transaction.Execute(sql::kSaveSettings, value.organization_id,
        value.expected_revision, json::ValueBuilder(value.stages).ExtractValue(),
        value.inquiry_number_template, value.reset_number_yearly);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Настройки изменены"};
    const auto result = Settings(transaction.Execute(sql::kGetSettings, value.organization_id));
    transaction.Commit();
    return Response200{.inquiry_number_template = result.inquiry_number_template,
        .reset_number_yearly = result.reset_number_yearly,
        .revision = result.revision,
        .stages = result.stages};
}
}  // namespace sales::views::api::admin::sales::settings::update
