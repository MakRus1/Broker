#include <helpers/proposals.hpp>
#include <views/api/v1/sales/proposals/action/view.hpp>

namespace sales::views::api::v1::sales::proposals::action {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto before = ReadProposal(deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetProposal, organization, request.body.id));
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto inquiry = LockProposalInquiry(transaction, organization, before.input.inquiry_id);
    auto value =
        ReadProposal(transaction.Execute(sql::kGetProposal, organization, request.body.id));
    if (value.revision != request.body.expected_revision)
        throw ApiError<409>{"revision_conflict", "Предложение изменено"};
    const bool has_cargo_description = inquiry.cargo_description &&
        !userver::utils::text::Trim(*inquiry.cargo_description).empty();
    if (inquiry.revision != value.inquiry_revision)
        throw ApiError<409>{
            "inquiry_changed", "Запрос изменился после расчёта. Создайте копию предложения"};
    if (!has_cargo_description)
        throw ApiError<400>{
            "cargo_required", "Перед предложением клиенту опишите груз во входящем запросе"};
    if (request.body.action == models::ProposalActionRequest_action::Offer) {
        if (value.status != models::ProposalStatus::Draft)
            throw ApiError<409>{"invalid_status", "Предложение уже зафиксировано"};
        value.status = models::ProposalStatus::Offered;
    } else {
        if (value.status != models::ProposalStatus::Offered)
            throw ApiError<409>{"invalid_status", "Сначала зафиксируйте предложение клиенту"};
        for (const auto& line : value.input.lines)
            if (line.purchase && !line.supplier_id)
                throw ApiError<400>{"supplier_required", "Для переноса закупки укажите поставщика"};
        value.status = models::ProposalStatus::Accepted;
    }
    ++value.revision;
    StoreProposal(
        transaction, organization, value, request.body.expected_revision, identity.ActorId());
    if (value.status == models::ProposalStatus::Accepted) {
        transaction.Execute(sql::kAcceptInquiryProposal, organization, value.input.inquiry_id,
            value.input.id, value.revision);
        const auto current =
            transaction.Execute(sql::kGetInquiries, organization, value.input.inquiry_id)
                .AsSingleRow<json::Value>();
        transaction.Execute(sql::kWriteHistory, organization, std::string{"inquiries"}, inquiry.id,
            inquiry.revision + 1, identity.ActorId(), current);
    }
    transaction.Commit();
    return Response200{.item = value};
}
}  // namespace sales::views::api::v1::sales::proposals::action
