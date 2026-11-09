#include <algorithm>
#include <components/internal/editable_inquiry.hpp>
#include <components/internal/references.hpp>
#include <components/rfq.hpp>
#include <events/shipment_command.hpp>
#include <map>
#include <platform/charge_amounts.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace sales {
using namespace detail;

namespace {
models::Rfq Read(const pg::ResultSet& rows) {
    if (rows.IsEmpty())
        throw ApiError<404>{"rfq_not_found", "Запрос ставок не найден в организации"};
    return models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::Rfq>{});
}
}  // namespace

RfqComponent::RfqComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      deps_{
          .counterparties = context.FindComponent<clients::counterparties::Component>().GetClient(),
          .finance = context.FindComponent<clients::finance::Component>().GetClient(),
          .organizations = context.FindComponent<clients::organizations::Component>().GetClient(),
          .pg_cluster =
              context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()} {
}

userver::yaml_config::Schema RfqComponent::GetStaticConfigSchema() {
    return userver::components::ComponentBase::GetStaticConfigSchema();
}

models::Rfq RfqComponent::Get(const std::string& organization, const std::string& id) const {
    return Read(
        deps_.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kGetRfq, organization, id));
}

std::vector<models::Rfq> RfqComponent::List(
    const std::string& organization, const std::string& inquiry_id) const {
    const auto rows = deps_.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kListRfqs, organization, inquiry_id);
    std::vector<models::Rfq> result;
    for (const auto& row : rows)
        result.push_back(
            models::ParseModelJson(row[0].As<json::Value>(), std::type_identity<models::Rfq>{}));
    return result;
}

std::vector<models::RfqHistoryEntry> RfqComponent::History(
    const std::string& organization, const models::HistoryRequest& request) const {
    const auto rows = deps_.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kHistoryRfqs,
        organization, request.id, request.after_revision.value_or(0));
    std::vector<models::RfqHistoryEntry> result;
    for (const auto& row : rows)
        result.push_back(models::ParseModelJson(
            row[0].As<json::Value>(), std::type_identity<models::RfqHistoryEntry>{}));
    return result;
}

models::Rfq RfqComponent::Save(const models::SaveRfqRequest& request,
    userver::server::request::RequestContext& context) const try {
    const auto& deps = deps_;
    auto document_input = json::ValueBuilder(request);
    document_input.Remove("expected_revision");
    auto input = models::ParseModelJson(
        document_input.ExtractValue(), std::type_identity<models::RfqInput>{});
    const auto expected = request.expected_revision;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    if (userver::utils::text::Trim(input.title).empty())
        throw ApiError<400>{"empty_title", "Укажите название запроса ставок"};
    const auto inquiry_rows = deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetInquiries, organization, input.inquiry_id);
    if (inquiry_rows.IsEmpty())
        throw ApiError<404>{"not_found", "Входящий запрос не найден"};
    const auto initial = models::ParseModelJson(
        inquiry_rows.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});
    ValidateReferences(deps, context, initial.customer_id, input.responsible_subject);
    if (input.status == models::RfqStatus::Ready && input.quotes.empty())
        throw ApiError<400>{"quotes_required", "Добавьте хотя бы одну котировку"};
    std::map<std::string, std::string> suppliers;
    std::map<std::string, std::string> labels;
    std::set<std::string> quote_ids;
    for (auto& quote : input.quotes) {
        if (!quote_ids.insert(quote.id).second)
            throw ApiError<400>{"duplicate_quote", "У котировок должны быть разные идентификаторы"};
        if (userver::utils::text::Trim(quote.title).empty())
            throw ApiError<400>{"empty_quote_title", "Укажите название варианта поставщика"};
        if (!suppliers.contains(quote.supplier_id)) {
            const auto supplier = GetCounterparty(deps, context, quote.supplier_id);
            if (supplier.archived)
                throw ApiError<400>{"archived_supplier", "Поставщик в архиве"};
            suppliers.emplace(quote.supplier_id, supplier.name);
        }
        quote.supplier_name = suppliers.at(quote.supplier_id);
        std::set<std::string> lines;
        for (auto& line : quote.lines) {
            if (!lines.insert(line.id).second)
                throw ApiError<400>{"duplicate_line", "У строк должны быть разные идентификаторы"};
            if (!labels.contains(line.label_id)) {
                const auto label = deps.finance
                                       .ApiV1FinancialLabelsGet({.body = {.id = line.label_id}},
                                           platform::auth::GetAccessToken(context))
                                       .label;
                if (label.archived ||
                    label.direction == clients::finance::models::FinancialLabelDirection::Income)
                    throw ApiError<400>{"invalid_label", "Выберите метку для расхода"};
                labels.emplace(line.label_id, label.name);
            }
            line.label_name = labels.at(line.label_id);
            platform::CalculateChargeAmounts(line.quantity_milli, line.price.price_minor,
                line.price.price_mode == models::PriceMode::Gross,
                line.price.vat_mode == models::VatMode::Exempt, line.price.vat_rate_basis_points);
        }
    }
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto inquiry = LockEditableInquiry(transaction, organization, input.inquiry_id);
    const auto segment = std::find_if(inquiry.route_segments.begin(), inquiry.route_segments.end(),
        [&](const auto& value) { return value.id == input.route_segment_id; });
    if (segment == inquiry.route_segments.end())
        throw ApiError<400>{"wrong_segment", "Участок не принадлежит входящему запросу"};
    const auto rows = transaction.Execute(sql::kGetRfq, organization, input.id);
    std::optional<models::Rfq> previous;
    if (!rows.IsEmpty())
        previous = Read(rows);
    if ((previous ? previous->revision : 0) != expected ||
        (previous && previous->input.inquiry_id != input.inquiry_id))
        throw ApiError<409>{"revision_conflict", "Запрос ставок изменён. Откройте его заново"};
    if (!previous &&
        transaction.Execute(sql::kListRfqs, organization, input.inquiry_id).Size() >= 100)
        throw ApiError<409>{"rfq_limit", "Во входящем запросе уже 100 запросов ставок"};
    models::Rfq value{.input = std::move(input),
        .inquiry_number = inquiry.number,
        .revision = expected + 1,
        .route_segment = *segment};
    const auto document = json::ValueBuilder(value).ExtractValue();
    if (transaction.Execute(sql::kSaveRfq, organization, document, expected).IsEmpty())
        throw ApiError<409>{"revision_conflict", "Запрос ставок изменён"};
    transaction.Execute(sql::kWriteRfqHistory, organization, document, identity.ActorId());
    namespace events = clients::notifications::models;
    const auto payload = [&](const std::string& recipient) {
        return events::RfqNotificationPayload{.actor_subject = identity.Subject,
            .recipient_issuer = identity.Issuer,
            .recipient_subject = recipient,
            .revision = value.revision,
            .rfq_id = value.input.id,
            .rfq_title = value.input.title};
    };
    if (!previous || previous->input.responsible_subject != value.input.responsible_subject) {
        const auto id = "rfq-assigned:" + value.input.id + ":" + std::to_string(value.revision);
        const events::PublishedEvent event{
            .value = events::RfqAssigned{.id = id,
                .organization_id = organization,
                .payload = payload(value.input.responsible_subject)}};
        platform::outbox::Enqueue(transaction, kPublishNotification, id, event);
    }
    if (value.input.status == models::RfqStatus::Ready &&
        (!previous || previous->input.status != models::RfqStatus::Ready)) {
        const auto id = "rfq-ready:" + value.input.id + ":" + std::to_string(value.revision);
        const events::PublishedEvent event{.value = events::RfqReady{.id = id,
                                               .organization_id = organization,
                                               .payload = payload(inquiry.manager_subject)}};
        platform::outbox::Enqueue(transaction, kPublishNotification, id, event);
    }
    transaction.Commit();
    return value;
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к справочникам котировок"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<502>{"reference_unavailable", "Не удалось получить справочники котировок"};
} catch (const platform::http::TransportError&) {
    throw ApiError<502>{"reference_unavailable", "Сервис справочников недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса справочников"};
}

void RfqComponent::ResolveQuote(const std::string& organization,
    const models::ProposalInput& proposal, const models::Inquiry& inquiry,
    models::ProposalLine& line) const {
    const auto& deps = deps_;
    if (!line.quote_source)
        return;
    const auto& source = *line.quote_source;
    const auto value = Read(deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
        sql::kGetRfqRevision, organization, source.rfq_id, source.revision));
    if (value.input.inquiry_id != proposal.inquiry_id ||
        value.input.status != models::RfqStatus::Ready)
        throw ApiError<400>{"wrong_quote_source", "Выберите готовую котировку этого запроса"};
    const auto segment = std::find_if(inquiry.route_segments.begin(), inquiry.route_segments.end(),
        [&](const auto& candidate) { return candidate.id == value.input.route_segment_id; });
    if (segment == inquiry.route_segments.end() ||
        json::ValueBuilder(*segment).ExtractValue() !=
            json::ValueBuilder(value.route_segment).ExtractValue())
        throw ApiError<400>{
            "quote_route_changed", "Участок изменился. Уточните котировку у поставщика"};
    for (const auto& quote : value.input.quotes) {
        if (quote.id != source.quote_id)
            continue;
        if (quote.valid_until && *quote.valid_until < proposal.calculation_date)
            throw ApiError<400>{"quote_expired", "Срок действия котировки истёк"};
        for (const auto& quoted : quote.lines) {
            if (quoted.id != source.line_id)
                continue;
            line.label_id = quoted.label_id;
            line.unit = quoted.unit;
            line.quantity_milli = quoted.quantity_milli;
            line.purchase = quoted.price;
            line.supplier_id = quote.supplier_id;
            line.route_segment_id = value.input.route_segment_id;
            return;
        }
    }
    throw ApiError<400>{"quote_not_found", "Строка котировки не найдена"};
}

}  // namespace sales
