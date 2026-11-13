#include <array>
#include <components/financial_history.hpp>
#include <converters/financial_label.hpp>
#include <finance/sql_queries.hpp>
#include <map>
#include <models/errors.hpp>
#include <models/financial_history_row.hpp>
#include <models/invoice_name_row.hpp>
#include <openapi/models.hpp>
#include <permissions/permissions.hpp>
#include <set>
#include <span>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/chrono.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
namespace models = finance::models;
namespace pg = userver::storages::postgres;
using finance::ApiError;

using Json = userver::formats::json::Value;
using Format = models::FinancialHistoryFormat;
using FieldKey = models::FinancialHistoryField;
using Source = models::FinancialHistorySource;
using Action = models::FinancialHistoryAction;
using Reference = models::FinancialHistoryReference;
using Value = models::FinancialHistoryValue;

struct Field {
    FieldKey Key;
    Format ValueFormat = Format::Text;
    const char* NameKey = nullptr;
};

constexpr std::array kChargeFields{Field{FieldKey::LabelId, Format::Reference, "label_name"},
    Field{FieldKey::Direction, Format::Enum},
    Field{FieldKey::CounterpartyId, Format::Reference, "counterparty_name"},
    Field{FieldKey::ContractId, Format::Reference, "contract_number"},
    Field{FieldKey::ShipmentId, Format::Reference},
    Field{FieldKey::TransportationId, Format::Reference},
    Field{FieldKey::QuantityMilli, Format::Quantity}, Field{FieldKey::Unit, Format::Enum},
    Field{FieldKey::PriceMinor, Format::Money}, Field{FieldKey::Currency},
    Field{FieldKey::ServiceDate, Format::Date}, Field{FieldKey::InvoiceCurrency},
    Field{FieldKey::InvoiceRateRateMillionths, Format::Rate},
    Field{FieldKey::InvoiceRateDate, Format::Date},
    Field{FieldKey::InvoiceRateSource, Format::Enum}, Field{FieldKey::PriceMode, Format::Enum},
    Field{FieldKey::VatMode, Format::Enum}, Field{FieldKey::VatRateBasisPoints, Format::Percent},
    Field{FieldKey::NetMinor, Format::Money}, Field{FieldKey::VatMinor, Format::Money},
    Field{FieldKey::TotalMinor, Format::Money}, Field{FieldKey::InvoiceNetMinor, Format::Money},
    Field{FieldKey::InvoiceVatMinor, Format::Money},
    Field{FieldKey::InvoiceTotalMinor, Format::Money}, Field{FieldKey::Note},
    Field{FieldKey::Archived, Format::Boolean}};
constexpr std::array kInvoiceFields{Field{FieldKey::Number},
    Field{FieldKey::Direction, Format::Enum}, Field{FieldKey::InvoiceDate, Format::Date},
    Field{FieldKey::DueDate, Format::Date},
    Field{FieldKey::OwnCompanyId, Format::Reference, "own_company_name"},
    Field{FieldKey::CounterpartyId, Format::Reference, "counterparty_name"},
    Field{FieldKey::OwnBankAccountId, Format::Reference},
    Field{FieldKey::CounterpartyBankAccountId, Format::Reference},
    Field{FieldKey::ContractId, Format::Reference, "contract_number"}, Field{FieldKey::Currency},
    Field{FieldKey::NetMinor, Format::Money}, Field{FieldKey::VatMinor, Format::Money},
    Field{FieldKey::TotalMinor, Format::Money}, Field{FieldKey::Note},
    Field{FieldKey::Archived, Format::Boolean}};
constexpr std::array kPaymentFields{Field{FieldKey::Number}, Field{FieldKey::Kind, Format::Enum},
    Field{FieldKey::PaymentDate, Format::Date},
    Field{FieldKey::OwnCompanyId, Format::Reference, "own_company_name"},
    Field{FieldKey::CounterpartyId, Format::Reference, "counterparty_name"},
    Field{FieldKey::OwnBankAccountId, Format::Reference},
    Field{FieldKey::CounterpartyBankAccountId, Format::Reference}, Field{FieldKey::Currency},
    Field{FieldKey::AmountMinor, Format::Money}, Field{FieldKey::Purpose}, Field{FieldKey::Note},
    Field{FieldKey::Archived, Format::Boolean}};

struct DocumentSnapshot {
    std::optional<std::string> Name;
    std::optional<std::string> Currency;
    bool Archived = false;
    std::map<FieldKey, Value> Fields;
};

struct Snapshot {
    DocumentSnapshot Document;
    std::optional<std::string> ActorName;
    std::optional<models::PaymentKind> PaymentKind;
    std::map<std::string, DocumentSnapshot> Charges;
    std::map<std::string, std::int64_t> Allocations;
};

// JSONB — формат хранения. Ниже этой границы история работает с типизированными снимками.
// Поля старых редакций могут отсутствовать: не подставляем значения текущей модели документа.
DocumentSnapshot ReadDocumentSnapshot(
    const Json& document, std::span<const Field> fields, const char* name_key) {
    if (!document.IsObject())
        return {};
    DocumentSnapshot result{.Name = document[name_key].As<std::optional<std::string>>(std::nullopt),
        .Currency = document["currency"].As<std::optional<std::string>>(std::nullopt),
        .Archived = document["archived"].As<bool>(false),
        .Fields = {}};
    for (const auto& field : fields) {
        const std::string key{models::EnumValue(field.Key)};
        const auto dot = key.find('.');
        const auto raw = dot == std::string::npos ? document[key]
            : document[key.substr(0, dot)].IsObject()
            ? document[key.substr(0, dot)][key.substr(dot + 1)]
            : Json{};
        if (raw.IsMissing() || raw.IsNull())
            continue;
        Value value{.format = field.ValueFormat};
        if (raw.IsString())
            value.value = raw.As<std::string>();
        else if (raw.IsBool())
            value.value = raw.As<bool>() ? "true" : "false";
        else if (raw.IsInt64())
            value.value = std::to_string(raw.As<std::int64_t>());
        else
            continue;
        if (field.NameKey != nullptr)
            value.display_name =
                document[field.NameKey].As<std::optional<std::string>>(std::nullopt);
        if (field.ValueFormat == Format::Money)
            value.currency = document[key.starts_with("invoice_") ? "invoice_currency" : "currency"]
                                 .As<std::optional<std::string>>(std::nullopt);
        result.Fields.emplace(field.Key, std::move(value));
    }
    return result;
}

Snapshot ReadSnapshot(const Json& snapshot, Source source) {
    Snapshot result;
    result.ActorName = snapshot["actor_name"].As<std::optional<std::string>>(std::nullopt);
    switch (source) {
    case Source::ServiceCharge:
        result.Document = ReadDocumentSnapshot(snapshot, kChargeFields, "label_name");
        break;
    case Source::Invoice:
        result.Document = ReadDocumentSnapshot(snapshot["invoice"], kInvoiceFields, "number");
        if (snapshot["charges"].IsArray())
            for (const auto& charge : snapshot["charges"])
                result.Charges.emplace(charge["id"].As<std::string>(""),
                    ReadDocumentSnapshot(charge, kChargeFields, "label_name"));
        break;
    case Source::Payment:
        result.Document = ReadDocumentSnapshot(snapshot["payment"], kPaymentFields, "number");
        if (snapshot["payment"].IsObject() && snapshot["payment"]["kind"].IsString())
            result.PaymentKind = models::ParseEnumScalar(
                snapshot["payment"]["kind"].As<std::string>(), models::PaymentKind{});
        if (snapshot["allocations"].IsArray())
            for (const auto& allocation : snapshot["allocations"])
                result.Allocations.emplace(allocation["invoice_id"].As<std::string>(""),
                    allocation["amount_minor"].As<std::int64_t>(0));
        break;
    }
    return result;
}

void AppendFieldChanges(std::vector<models::FinancialHistoryChange>& changes,
    std::span<const Field> fields, const DocumentSnapshot& before, const DocumentSnapshot& after,
    const std::optional<Reference>& subject = std::nullopt) {
    for (const auto& field : fields) {
        const auto old = before.Fields.find(field.Key);
        const auto current = after.Fields.find(field.Key);
        const bool had_value = old != before.Fields.end();
        const bool has_value = current != after.Fields.end();
        if ((!had_value && !has_value) ||
            (had_value && has_value && old->second.value == current->second.value &&
                old->second.display_name == current->second.display_name))
            continue;
        changes.push_back({.after = has_value ? std::optional{current->second} : std::nullopt,
            .before = had_value ? std::optional{old->second} : std::nullopt,
            .field = field.Key,
            .subject = subject});
    }
}

void AppendInvoiceChargeChanges(std::vector<models::FinancialHistoryChange>& changes,
    const Snapshot& before, const Snapshot& after) {
    std::set<std::string> ids;
    for (const auto& [id, charge] : before.Charges)
        ids.insert(id);
    for (const auto& [id, charge] : after.Charges)
        ids.insert(id);
    for (const auto& id : ids) {
        const auto old = before.Charges.find(id);
        const auto current = after.Charges.find(id);
        const bool was = old != before.Charges.end();
        const bool included = current != after.Charges.end();
        const Reference subject{.id = id,
            .name = included ? current->second.Name : old->second.Name,
            .source = Source::ServiceCharge};
        if (was && included) {
            AppendFieldChanges(changes, kChargeFields, old->second, current->second, subject);
        } else {
            const Value linked{.format = Format::Boolean, .value = "true"};
            changes.push_back({.after = included ? std::optional{linked} : std::nullopt,
                .before = was ? std::optional{linked} : std::nullopt,
                .field = FieldKey::Charges,
                .subject = subject});
        }
    }
}

std::optional<Value> PaymentAllocationValue(std::int64_t amount, const Snapshot& snapshot) {
    if (amount == 0)
        return std::nullopt;
    return Value{.currency = snapshot.Document.Currency,
        .format = Format::Money,
        .value = std::to_string(amount)};
}

struct HistoryRecord {
    models::FinancialHistoryCursor Cursor;
    std::string Actor;
    Snapshot Current;
    std::optional<Snapshot> Previous;
};

models::FinancialHistoryEvent BuildEvent(const HistoryRecord& record, Source requested,
    const std::string& requested_id, const std::map<std::string, std::string>& invoice_names) {
    const auto& after = record.Current;
    const Snapshot empty;
    const auto& before = record.Previous.has_value() ? record.Previous.value() : empty;
    const auto source = record.Cursor.source;
    const auto document_type = source == Source::ServiceCharge
        ? models::FinancialHistoryDocument::ServiceCharge
        : source == Source::Invoice ? models::FinancialHistoryDocument::Invoice
        : after.PaymentKind.value() == models::PaymentKind::Receipt
        ? models::FinancialHistoryDocument::Receipt
        : models::FinancialHistoryDocument::Disbursement;
    const auto action = record.Cursor.revision == 1            ? Action::Created
        : !record.Previous.has_value()                         ? Action::Updated
        : after.Document.Archived && !before.Document.Archived ? Action::Archived
        : !after.Document.Archived && before.Document.Archived ? Action::Restored
                                                               : Action::Updated;
    models::FinancialHistoryEvent event{.action = action,
        .actor = record.Actor,
        .cursor = record.Cursor,
        .document = document_type,
        .has_previous = record.Previous.has_value()};
    event.actor_name = after.ActorName;
    if (source == Source::Invoice && requested == Source::ServiceCharge) {
        const bool was = before.Charges.contains(requested_id) && !before.Document.Archived;
        const bool included = after.Charges.contains(requested_id) && !after.Document.Archived;
        event.action = included ? Action::InvoiceLinked : Action::InvoiceUnlinked;
        event.reference = Reference{
            .id = record.Cursor.entity_id, .name = after.Document.Name, .source = Source::Invoice};
        event.changes.push_back(
            {.after = included ? std::optional{Value{.display_name = after.Document.Name,
                                     .format = Format::Reference,
                                     .value = record.Cursor.entity_id}}
                               : std::nullopt,
                .before = was ? std::optional{Value{.display_name = before.Document.Name,
                                    .format = Format::Reference,
                                    .value = record.Cursor.entity_id}}
                              : std::nullopt,
                .field = FieldKey::Invoice});
        return event;
    }
    // В архивном платеже распределения сохранены в снимке, но уже не оплачивают счета.
    const auto allocated = [](const Snapshot& snapshot, const std::string& invoice) {
        return !snapshot.Document.Archived && snapshot.Allocations.contains(invoice)
            ? snapshot.Allocations.at(invoice)
            : std::int64_t{0};
    };
    if (source == Source::Payment && requested == Source::Invoice) {
        const auto old = allocated(before, requested_id);
        const auto current = allocated(after, requested_id);
        event.action = current == 0 ? Action::PaymentUnallocated
            : old == 0              ? Action::PaymentAllocated
                                    : Action::PaymentReallocated;
        event.reference = Reference{
            .id = record.Cursor.entity_id, .name = after.Document.Name, .source = Source::Payment};
        event.changes.push_back({.after = PaymentAllocationValue(current, after),
            .before = PaymentAllocationValue(old, before),
            .field = FieldKey::Allocation});
        return event;
    }
    if (source == Source::ServiceCharge)
        AppendFieldChanges(event.changes, kChargeFields, before.Document, after.Document);
    if (source == Source::Invoice) {
        AppendFieldChanges(event.changes, kInvoiceFields, before.Document, after.Document);
        AppendInvoiceChargeChanges(event.changes, before, after);
    }
    if (source == Source::Payment) {
        AppendFieldChanges(event.changes, kPaymentFields, before.Document, after.Document);
        std::set<std::string> ids;
        for (const auto& [id, amount] : before.Allocations)
            ids.insert(id);
        for (const auto& [id, amount] : after.Allocations)
            ids.insert(id);
        for (const auto& id : ids) {
            const auto old = allocated(before, id);
            const auto current = allocated(after, id);
            if (old == current)
                continue;
            const auto name = invoice_names.find(id);
            event.changes.push_back({.after = PaymentAllocationValue(current, after),
                .before = PaymentAllocationValue(old, before),
                .field = FieldKey::Allocation,
                .subject = Reference{.id = id,
                    .name =
                        name == invoice_names.end() ? std::nullopt : std::optional{name->second},
                    .source = Source::Invoice}});
        }
    }
    return event;
}

}  // namespace

namespace finance {

FinancialHistoryComponent::FinancialHistoryComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-finance").GetCluster()) {}

userver::yaml_config::Schema FinancialHistoryComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::FinancialHistoryResponse FinancialHistoryComponent::GetHistory(
    const std::string& organization, const models::FinancialHistoryRequest& body,
    models::FinancialHistorySource source, bool related, bool can_read_invoices) const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto exists = source == Source::ServiceCharge ? sql::kGetServiceCharge
        : source == Source::Invoice                     ? sql::kGetInvoice
                                                        : sql::kGetPayment;
    if (transaction.Execute(exists, organization, body.id).IsEmpty())
        throw ApiError<404>{"not_found", "Документ не найден в организации"};
    std::optional<pg::TimePointTz> before_time;
    std::optional<std::string> before_source, before_id;
    std::optional<std::int64_t> before_revision;
    if (body.before.has_value()) {
        const auto& before = body.before.value();
        before_time = pg::TimePointTz{before.occurred_at};
        before_source = models::EnumValue(before.source);
        before_id = before.entity_id;
        before_revision = before.revision;
    }

    const auto query = source == Source::ServiceCharge ? sql::kListServiceChargeHistory
        : source == Source::Invoice                    ? sql::kListInvoiceHistory
                                                       : sql::kListPaymentHistory;
    const auto rows = transaction.Execute(query, organization, body.id, before_time, before_source,
        before_id, before_revision, body.limit + 1, related);
    const auto stored_rows =
        rows.AsContainer<std::vector<models::FinancialHistoryRow>>(pg::kRowTag);
    std::vector<HistoryRecord> history;
    history.reserve(stored_rows.size());
    std::set<boost::uuids::uuid> invoice_ids;
    const bool load_invoice_names = source == Source::Payment && can_read_invoices;
    for (const auto& row : stored_rows) {
        const auto event_source = models::ParseEnumScalar(row.Source, Source{});
        HistoryRecord record{.Cursor = {.entity_id = boost::uuids::to_string(row.EntityId),
                                 .occurred_at = row.OccurredAt.GetUnderlying(),
                                 .revision = row.Revision,
                                 .source = event_source},
            .Actor = row.Actor,
            .Current = ReadSnapshot(row.Snapshot, event_source),
            .Previous = std::nullopt};
        if (row.Previous.has_value() && !row.Previous.value().IsNull())
            record.Previous = ReadSnapshot(row.Previous.value(), event_source);
        if (load_invoice_names) {
            for (const auto& [id, amount] : record.Current.Allocations)
                invoice_ids.insert(boost::uuids::string_generator{}(id));
            if (record.Previous.has_value())
                for (const auto& [id, amount] : record.Previous.value().Allocations)
                    invoice_ids.insert(boost::uuids::string_generator{}(id));
        }
        history.push_back(std::move(record));
    }
    std::map<std::string, std::string> invoice_names;
    if (!invoice_ids.empty()) {
        const auto names = transaction.Execute(sql::kHistoryInvoiceNames, organization,
            std::vector<boost::uuids::uuid>{invoice_ids.begin(), invoice_ids.end()});
        for (const auto& row : names) {
            const auto invoice = row.As<models::InvoiceNameRow>(pg::kRowTag);
            invoice_names.emplace(boost::uuids::to_string(invoice.Id), invoice.Number);
        }
    }
    transaction.Commit();
    models::FinancialHistoryResponse result;
    for (const auto& record : history) {
        if (result.items.size() == static_cast<std::size_t>(body.limit))
            break;
        result.items.push_back(BuildEvent(record, source, body.id, invoice_names));
    }
    if (history.size() > static_cast<std::size_t>(body.limit))
        result.next_before = result.items.back().cursor;
    return result;
}
}  // namespace finance
