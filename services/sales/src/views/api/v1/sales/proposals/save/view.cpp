#include <helpers/proposals.hpp>
#include <views/api/v1/sales/proposals/save/view.hpp>

namespace sales::views::api::v1::sales::proposals::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    auto document = json::ValueBuilder(request.body);
    document.Remove("expected_revision");
    auto value = CalculateProposal(deps, context,
        models::ParseModelJson(
            document.ExtractValue(), std::type_identity<models::ProposalInput>{}));
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto inquiry = LockProposalInquiry(transaction, organization, value.input.inquiry_id);
    if (inquiry.revision != value.inquiry_revision)
        throw ApiError<409>{"inquiry_changed", "Входящий запрос изменился во время расчёта"};
    for (const auto& line : value.input.lines) {
        if (line.route_segment_id &&
            std::none_of(inquiry.route_segments.begin(), inquiry.route_segments.end(),
                [&](const auto& segment) { return segment.id == *line.route_segment_id; }))
            throw ApiError<400>{"wrong_segment", "Участок не принадлежит запросу"};
    }
    if (request.body.expected_revision != 0) {
        const auto previous =
            ReadProposal(transaction.Execute(sql::kGetProposal, organization, value.input.id));
        if (previous.status != models::ProposalStatus::Draft)
            throw ApiError<409>{
                "proposal_frozen", "Создайте новый вариант на основе согласованной версии"};
    } else if (transaction.Execute(sql::kListProposals, organization, value.input.inquiry_id)
                   .Size() >= 100)
        throw ApiError<409>{"proposal_limit", "В запросе уже 100 вариантов предложения"};
    value.revision = request.body.expected_revision + 1;
    StoreProposal(
        transaction, organization, value, request.body.expected_revision, identity.ActorId());
    transaction.Commit();
    return Response200{.item = value};
}
}  // namespace sales::views::api::v1::sales::proposals::save
