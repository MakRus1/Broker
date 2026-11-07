#include <helpers/settings.hpp>
#include <views/api/v1/sales/cards/save/view.hpp>

namespace sales::views::api::v1::sales::cards::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    const auto organization = Organization(context);
    const auto& value = request.body;
    ValidateReferences(deps, context, value.customer_id, value.manager_subject);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockSettings, organization);
    const auto previous = transaction.Execute(sql::kLockCards, organization, value.id);
    const auto old = previous.IsEmpty() ? std::optional<models::SalesCard>{}
                                        : std::optional{Read<models::SalesCard>(previous)};
    ValidateStage(Settings(transaction.Execute(sql::kGetSettings, organization)), value.stage_id,
        old.has_value() ? std::optional{old->stage_id} : std::nullopt);
    const auto document = json::ValueBuilder(value).ExtractValue();
    const auto changed = value.expected_revision == 0
        ? transaction.Execute(sql::kCreateCards, organization, document)
        : transaction.Execute(sql::kUpdateCards, organization, document, value.expected_revision);
    if (changed.IsEmpty())
        throw ApiError<409>{
            "revision_conflict", "Запись уже существует, изменена или передана в поставку"};
    const auto result =
        Read<models::SalesCard>(transaction.Execute(sql::kGetCards, organization, value.id));
    Audit(transaction, organization, platform::auth::GetIdentity(context), std::string{"cards"},
        result);
    transaction.Commit();
    return Response200{.item = result};
}
}  // namespace sales::views::api::v1::sales::cards::save
