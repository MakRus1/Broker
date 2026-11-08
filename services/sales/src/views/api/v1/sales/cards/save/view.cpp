#include <helpers/settings.hpp>
#include <views/api/v1/sales/cards/save/view.hpp>

namespace sales::views::api::v1::sales::cards::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto& value = request.body;
    ValidateReferences(deps, context, value.customer_id, value.manager_subject);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockSettings, organization);
    const auto previous = transaction.Execute(sql::kLockCards, organization, value.id);
    const auto previous_stage =
        previous.IsEmpty() ? std::nullopt : std::optional{previous.AsSingleRow<std::string>()};
    ValidateStage(Settings(transaction.Execute(sql::kGetSettings, organization)), value.stage_id,
        previous_stage);
    const auto document = json::ValueBuilder(value).ExtractValue();
    transaction.Execute(sql::kClearChangedActionSource, organization, document);
    const auto changed = value.expected_revision == 0
        ? transaction.Execute(sql::kCreateCards, organization, document)
        : transaction.Execute(sql::kUpdateCards, organization, document, value.expected_revision);
    if (changed.IsEmpty())
        throw ApiError<409>{
            "revision_conflict", "Запись уже существует, изменена или передана в поставку"};
    const auto rows = transaction.Execute(sql::kGetCards, organization, value.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    const auto result = models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::SalesCard>{});
    transaction.Execute(sql::kWriteHistory, organization, std::string{"cards"}, result.id,
        result.revision, platform::auth::GetIdentity(context).ActorId(),
        json::ValueBuilder(result).ExtractValue());
    transaction.Commit();
    return Response200{.item = result};
}
}  // namespace sales::views::api::v1::sales::cards::save
