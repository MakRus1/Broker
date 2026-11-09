#include <components/internal/database.hpp>
#include <components/internal/editable_inquiry.hpp>
#include <components/internal/proposal_storage.hpp>
#include <components/internal/references.hpp>
#include <components/proposals.hpp>
#include <map>
#include <platform/charge_amounts.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/decimal64/decimal64.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/text_light.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace sales {
using namespace detail;

namespace {
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

models::ProposalAmounts Amounts(std::int64_t quantity, const models::ProposalPrice& value) {
    const auto a = platform::CalculateChargeAmounts(quantity, value.price_minor,
        value.price_mode == models::PriceMode::Gross, value.vat_mode == models::VatMode::Exempt,
        value.vat_rate_basis_points);
    return {.net_minor = a.Net, .total_minor = a.Total, .vat_minor = a.Vat};
}

void AddAmounts(models::ProposalAmounts& target, const models::ProposalAmounts& value) {
    target.net_minor += value.net_minor;
    target.vat_minor += value.vat_minor;
    target.total_minor += value.total_minor;
}

std::int64_t ConvertProposalAmount(std::int64_t amount, std::int64_t rate) {
    using Amount = userver::decimal64::Decimal<0, userver::decimal64::HalfUpRoundPolicy>;
    using Rate = userver::decimal64::Decimal<6, userver::decimal64::HalfUpRoundPolicy>;
    const auto result = (Amount{amount} * Rate::FromUnbiased(rate)).AsUnbiased();
    // Ограничиваем сумму для точного представления целых чисел в JSON у потребителей.
    if (result > 90000000000000LL)
        throw ApiError<400>{"amount_overflow", "Сумма предложения слишком велика"};
    return result;
}

}  // namespace

ProposalsComponent::ProposalsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      deps_{
          .counterparties = context.FindComponent<clients::counterparties::Component>().GetClient(),
          .finance = context.FindComponent<clients::finance::Component>().GetClient(),
          .organizations = context.FindComponent<clients::organizations::Component>().GetClient(),
          .rfq = &context.FindComponent<RfqComponent>(),
          .pg_cluster =
              context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()} {
}

userver::yaml_config::Schema ProposalsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::Proposal ProposalsComponent::Save(const models::SaveProposalRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    auto document = json::ValueBuilder(request);
    document.Remove("expected_revision");
    auto value = Calculate(context,
        models::ParseModelJson(
            document.ExtractValue(), std::type_identity<models::ProposalInput>{}));
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto inquiry = LockEditableInquiry(transaction, organization, value.input.inquiry_id);
    if (inquiry.revision != value.inquiry_revision)
        throw ApiError<409>{"inquiry_changed", "Входящий запрос изменился во время расчёта"};
    for (const auto& line : value.input.lines) {
        if (line.route_segment_id &&
            std::none_of(inquiry.route_segments.begin(), inquiry.route_segments.end(),
                [&](const auto& segment) { return segment.id == *line.route_segment_id; }))
            throw ApiError<400>{"wrong_segment", "Участок не принадлежит запросу"};
    }
    if (request.expected_revision != 0) {
        const auto previous =
            ReadProposal(transaction.Execute(sql::kGetProposal, organization, value.input.id));
        if (previous.status != models::ProposalStatus::Draft)
            throw ApiError<409>{
                "proposal_frozen", "Создайте новый вариант на основе согласованной версии"};
    } else if (transaction.Execute(sql::kListProposals, organization, value.input.inquiry_id)
                   .Size() >= 100)
        throw ApiError<409>{"proposal_limit", "В запросе уже 100 вариантов предложения"};
    value.revision = request.expected_revision + 1;
    StoreProposal(transaction, organization, value, request.expected_revision, identity.ActorId());
    transaction.Commit();
    return value;
}

std::vector<models::Proposal> ProposalsComponent::List(
    const models::IdRequest& request, userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto rows = deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kListProposals, organization, request.id);
    std::vector<models::Proposal> result;
    for (const auto& row : rows)
        result.push_back(models::ParseModelJson(
            row[0].As<json::Value>(), std::type_identity<models::Proposal>{}));
    return result;
}

models::Proposal ProposalsComponent::Action(const models::ProposalActionRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto before = ReadProposal(deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetProposal, organization, request.id));
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto inquiry = LockEditableInquiry(transaction, organization, before.input.inquiry_id);
    auto value = ReadProposal(transaction.Execute(sql::kGetProposal, organization, request.id));
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

std::vector<models::ProposalHistoryEntry> ProposalsComponent::History(
    const models::HistoryRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kHistoryProposals,
        organization, request.id, request.after_revision.value_or(0));
    std::vector<models::ProposalHistoryEntry> result;
    for (const auto& row : rows)
        result.push_back(models::ParseModelJson(
            row[0].As<json::Value>(), std::type_identity<models::ProposalHistoryEntry>{}));
    return result;
}

models::Proposal ProposalsComponent::Calculate(
    const userver::server::request::RequestContext& context, models::ProposalInput input) const
    try {
    const auto& deps = deps_;
    if (userver::utils::text::Trim(input.title).empty())
        throw ApiError<400>{"empty_title", "Укажите название предложения"};
    if (input.valid_until && *input.valid_until < input.calculation_date)
        throw ApiError<400>{"invalid_dates", "Срок действия не может быть раньше даты расчёта"};
    const auto token = platform::auth::GetAccessToken(context);
    const auto settings = deps.finance.ApiV1FinanceSettingsGet({}, token);
    models::Proposal result;
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto inquiry_rows = deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetInquiries, organization, input.inquiry_id);
    if (inquiry_rows.IsEmpty())
        throw ApiError<404>{"not_found", "Входящий запрос не найден"};
    const auto inquiry = models::ParseModelJson(
        inquiry_rows.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});
    if (!inquiry.cargo_description ||
        userver::utils::text::Trim(*inquiry.cargo_description).empty())
        throw ApiError<400>{
            "cargo_required", "Перед расчётом предложения опишите груз во входящем запросе"};
    result.inquiry_revision = inquiry.revision;
    result.customer_name = GetCounterparty(deps, context, inquiry.customer_id).name;
    result.input = std::move(input);
    result.status = models::ProposalStatus::Draft;
    result.reporting_currency = settings.reporting_currency;
    result.reporting.currency = result.reporting_currency;
    std::map<std::string, models::ProposalTotal> totals;
    std::map<std::string, std::int64_t> rates;
    std::set<std::string> ids;
    const auto accumulate = [&](const models::ProposalPrice& price,
                                const models::ProposalAmounts& amount, bool sale) {
        auto& total = totals[price.currency];
        total.currency = price.currency;
        AddAmounts(sale ? total.sale : total.purchase, amount);
        if (!rates.contains(price.currency)) {
            rates[price.currency] = price.currency == result.reporting_currency
                ? 1000000
                : deps.finance
                      .ApiV1ExchangeRatesGet({.body = {.date = result.input.calculation_date,
                                                  .from_currency = price.currency,
                                                  .to_currency = result.reporting_currency}},
                          token)
                      .rate_millionths;
        }
        const auto rate = rates.at(price.currency);
        const auto net = ConvertProposalAmount(amount.net_minor, rate);
        const auto gross = ConvertProposalAmount(amount.total_minor, rate);
        AddAmounts(sale ? result.reporting.sale : result.reporting.purchase,
            {.net_minor = net, .total_minor = gross, .vat_minor = gross - net});
    };
    for (auto& line : result.input.lines) {
        deps.rfq->ResolveQuote(organization, result.input, inquiry, line);
        if (!ids.insert(line.id).second)
            throw ApiError<400>{
                "duplicate_line", "Строки предложения должны иметь разные идентификаторы"};
        const auto label =
            deps.finance.ApiV1FinancialLabelsGet({.body = {.id = line.label_id}}, token).label;
        if (label.archived ||
            label.direction == clients::finance::models::FinancialLabelDirection::Expense ||
            (line.purchase &&
                label.direction != clients::finance::models::FinancialLabelDirection::Both))
            throw ApiError<400>{
                "invalid_label", "Выберите действующую метку, подходящую для дохода и расхода"};
        models::CalculatedProposalLine row;
        row.input = line;
        row.label_name = label.name;
        row.sale = Amounts(line.quantity_milli, line.sale);
        accumulate(line.sale, row.sale, true);
        if (line.purchase) {
            row.purchase = Amounts(line.quantity_milli, *line.purchase);
            accumulate(*line.purchase, *row.purchase, false);
        }
        if (line.supplier_id) {
            const auto party = GetCounterparty(deps, context, *line.supplier_id);
            if (party.archived)
                throw ApiError<400>{"archived_supplier", "Поставщик находится в архиве"};
            row.supplier_name = party.name;
        }
        result.lines.push_back(std::move(row));
    }
    for (const auto& [_, total] : totals)
        result.totals.push_back(total);
    for (const auto& [currency, rate] : rates)
        result.rates.push_back({.currency = currency, .rate_millionths = rate});
    if (result.reporting.sale.total_minor > 9000000000000000LL ||
        result.reporting.purchase.total_minor > 9000000000000000LL)
        throw ApiError<400>{"amount_overflow", "Сумма предложения слишком велика"};
    return result;
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

}  // namespace sales
