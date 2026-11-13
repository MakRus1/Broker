#include <components/proposals.hpp>
#include <components/rfq.hpp>
#include <components/rfq_rules.hpp>
#include <helpers/editable_inquiry.hpp>
#include <helpers/proposal_storage.hpp>
#include <map>
#include <models/errors.hpp>
#include <platform/http_client.hpp>
#include <sales/sql_queries.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/utils/text_light.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
namespace models = sales::models;
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;
namespace sql = sales::sql;
using sales::ApiError;

void StoreProposal(pg::Transaction& transaction, const std::string& organization,
    const models::Proposal& value, std::int64_t expected, const std::string& actor) {

    const auto document = json::ValueBuilder(value).ExtractValue();
    const auto changed =
        transaction.Execute(expected == 0 ? sql::kSaveProposal : sql::kUpdateProposal, organization,
            document, expected);
    if (changed.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Предложение изменено. Откройте его заново"};
    transaction.Execute(sql::kWriteProposalHistory, organization, document, actor);
}

}  // namespace

namespace sales {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;

ProposalsComponent::ProposalsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()),
      rfq_(context.FindComponent<RfqComponent>()),
      finance_(context.FindComponent<clients::finance::Component>().GetClient()),
      counterparties_(context.FindComponent<clients::counterparties::Component>().GetClient()) {}

userver::yaml_config::Schema ProposalsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::ProposalCalculation ProposalsComponent::PrepareProposalCalculation(
    const std::string& organization, std::string_view token, const models::Inquiry& inquiry,
    models::ProposalInput& input) const try {
    const auto settings = finance_.ApiV1FinanceSettingsGet({}, token);
    const auto customer =
        counterparties_.ApiV1CounterpartiesGet({.body = {.id = inquiry.customer_id}}, token)
            .counterparty;
    models::ProposalCalculation data{.InquiryRevision = inquiry.revision,
        .CustomerName = customer.name,
        .ReportingCurrency = settings.reporting_currency};
    std::map<std::pair<std::string, std::int64_t>, models::Rfq> rfq_revisions;
    std::map<std::string, clients::finance::models::FinancialLabel> labels;
    for (auto& line : input.lines) {
        if (line.quote_source) {
            const auto& source = *line.quote_source;
            const auto key = std::pair{source.rfq_id, source.revision};
            auto revision = rfq_revisions.find(key);
            if (revision == rfq_revisions.end())
                revision =
                    rfq_revisions
                        .emplace(
                            key, rfq_.FindRfqRevision(organization, source.rfq_id, source.revision))
                        .first;

            rfq_rules::ApplyQuote(input, inquiry, revision->second, line);
        }
        auto label_position = labels.find(line.label_id);
        if (label_position == labels.end())
            label_position =
                labels
                    .emplace(line.label_id,
                        finance_.ApiV1FinancialLabelsGet({.body = {.id = line.label_id}}, token)
                            .label)
                    .first;

        const auto& label = label_position->second;
        if (label.archived ||
            label.direction == clients::finance::models::FinancialLabelDirection::Expense ||
            (line.purchase &&
                label.direction != clients::finance::models::FinancialLabelDirection::Both))
            throw ApiError<400>{
                "invalid_label", "Выберите действующую метку, подходящую для дохода и расхода"};
        data.LabelNames.emplace(line.label_id, label.name);
        if (line.supplier_id && !data.SupplierNames.contains(*line.supplier_id)) {
            const auto supplier =
                counterparties_.ApiV1CounterpartiesGet({.body = {.id = *line.supplier_id}}, token)
                    .counterparty;
            if (supplier.archived)
                throw ApiError<400>{"archived_supplier", "Поставщик находится в архиве"};
            data.SupplierNames.emplace(*line.supplier_id, supplier.name);
        }
    }

    std::set<std::string> currencies;
    for (const auto& line : input.lines) {
        currencies.insert(line.sale.currency);
        if (line.purchase)
            currencies.insert(line.purchase->currency);
    }
    for (const auto& currency : currencies) {
        data.Rates[currency] = currency == data.ReportingCurrency
            ? 1000000
            : finance_
                  .ApiV1ExchangeRatesGet({.body = {.date = input.calculation_date,
                                              .from_currency = currency,
                                              .to_currency = data.ReportingCurrency}},
                      token)
                  .rate_millionths;
    }
    return data;
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к справочникам расчёта"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<502>{"reference_unavailable", "Не удалось получить данные для расчёта"};
} catch (const platform::http::TransportError&) {
    throw ApiError<502>{"reference_unavailable", "Сервис справочников недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса справочников"};
}

models::Proposal ProposalsComponent::SaveProposal(const std::string& organization,
    const platform::auth::Identity& identity, std::int64_t expected_revision,
    models::Proposal value) const {

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto inquiry =
        helpers::LockEditableInquiry(transaction, organization, value.input.inquiry_id);
    if (inquiry.revision != value.inquiry_revision)
        throw ApiError<409>{"inquiry_changed", "Входящий запрос изменился во время расчёта"};
    for (const auto& line : value.input.lines) {
        if (line.route_segment_id &&
            std::none_of(inquiry.route_segments.begin(), inquiry.route_segments.end(),
                [&](const auto& segment) { return segment.id == *line.route_segment_id; }))
            throw ApiError<400>{"wrong_segment", "Участок не принадлежит запросу"};
    }
    if (expected_revision != 0) {
        const auto previous = helpers::ReadProposal(
            transaction.Execute(sql::kGetProposal, organization, value.input.id));
        if (previous.status != models::ProposalStatus::Draft)
            throw ApiError<409>{
                "proposal_frozen", "Создайте новый вариант на основе согласованной версии"};
    } else if (transaction.Execute(sql::kListProposals, organization, value.input.inquiry_id)
                   .Size() >= 100)
        throw ApiError<409>{"proposal_limit", "В запросе уже 100 вариантов предложения"};
    value.revision = expected_revision + 1;
    StoreProposal(transaction, organization, value, expected_revision, identity.ActorId());

    transaction.Commit();
    return value;
}

std::vector<models::Proposal> ProposalsComponent::ListProposals(
    const std::string& organization, const std::string& inquiry_id) const {
    const auto rows = cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kListProposals, organization, inquiry_id);
    std::vector<models::Proposal> result;
    for (const auto& row : rows)
        result.push_back(models::ParseModelJson(
            row[0].As<json::Value>(), std::type_identity<models::Proposal>{}));

    return result;
}

models::Proposal ProposalsComponent::ApplyProposalAction(const std::string& organization,
    const platform::auth::Identity& identity, const models::ProposalActionRequest& request) const {
    const auto before = helpers::ReadProposal(cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kGetProposal, organization, request.id));

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto inquiry =
        helpers::LockEditableInquiry(transaction, organization, before.input.inquiry_id);
    auto value =
        helpers::ReadProposal(transaction.Execute(sql::kGetProposal, organization, request.id));
    if (value.revision != request.expected_revision)
        throw ApiError<409>{"revision_conflict", "Предложение изменено"};
    const bool has_cargo_description = inquiry.cargo_description &&
        !userver::utils::text::Trim(*inquiry.cargo_description).empty();
    if (inquiry.revision != value.inquiry_revision)
        throw ApiError<409>{
            "inquiry_changed", "Запрос изменился после расчёта. Создайте копию предложения"};
    if (!has_cargo_description)
        throw ApiError<400>{
            "cargo_required", "Перед предложением клиенту опишите груз во входящем запросе"};
    if (request.action == models::ProposalActionRequest_action::Offer) {
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
    StoreProposal(transaction, organization, value, request.expected_revision, identity.ActorId());
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
    return value;
}

std::vector<models::ProposalHistoryEntry> ProposalsComponent::GetProposalHistory(
    const std::string& organization, const models::HistoryRequest& request) const {
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kHistoryProposals,
        organization, request.id, request.after_revision.value_or(0));
    std::vector<models::ProposalHistoryEntry> result;
    for (const auto& row : rows)
        result.push_back(models::ParseModelJson(
            row[0].As<json::Value>(), std::type_identity<models::ProposalHistoryEntry>{}));

    return result;
}

}  // namespace sales
