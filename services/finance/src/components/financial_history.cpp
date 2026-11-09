#include <array>
#include <components/financial_history.hpp>
#include <helpers/financial_labels.hpp>
#include <map>
#include <openapi/models.hpp>
#include <permissions/permissions.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/chrono.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace finance {
using namespace helpers;

namespace {

using Json = userver::formats::json::Value;
using Format = models::FinancialHistoryFormat;

struct Field {
    const char* Key;
    Format ValueFormat = Format::Text;
    const char* NameKey = nullptr;
};

Json At(const Json& value, std::string_view key) {
    if (!value.IsObject())
        return Json{};
    const auto dot = key.find('.');
    if (dot == std::string_view::npos)
        return value[std::string{key}];
    return At(value[std::string{key.substr(0, dot)}], key.substr(dot + 1));
}

std::optional<std::string> Scalar(const Json& value) {
    if (value.IsMissing() || value.IsNull())
        return std::nullopt;
    if (value.IsString())
        return value.As<std::string>();
    if (value.IsBool())
        return value.As<bool>() ? "true" : "false";
    if (value.IsInt64())
        return std::to_string(value.As<std::int64_t>());
    return std::nullopt;
}

std::string Name(const Json& value, const char* key) {
    return Scalar(At(value, key)).value_or("");
}

std::optional<models::FinancialHistoryValue> Value(const Json& document, const Field& field) {
    auto value = Scalar(At(document, field.Key));
    if (!value.has_value())
        return std::nullopt;
    models::FinancialHistoryValue result{.format = field.ValueFormat, .value = value.value()};
    if (field.ValueFormat == Format::Reference && field.NameKey)
        result.display_name = Scalar(At(document, field.NameKey));
    if (field.ValueFormat == Format::Money) {
        const std::string key = field.Key;
        result.currency =
            Scalar(At(document, key.starts_with("invoice_") ? "invoice_currency" : "currency"));
    }
    return result;
}

void AddChange(std::vector<models::FinancialHistoryChange>& changes, const Field& field,
    const Json& before, const Json& after,
    const std::optional<models::FinancialHistoryReference>& subject = std::nullopt) {
    const auto old = Scalar(At(before, field.Key));
    const auto current = Scalar(At(after, field.Key));
    // Старые снимки могут не содержать недавно добавленных полей.
    if (old == current &&
        (!field.NameKey || Scalar(At(before, field.NameKey)) == Scalar(At(after, field.NameKey))))
        return;
    changes.push_back({.after = Value(after, field),
        .before = Value(before, field),
        .field = models::ParseEnumScalar(field.Key, models::FinancialHistoryField{}),
        .subject = subject});
}

constexpr std::array kChargeFields{Field{"label_id", Format::Reference, "label_name"},
    Field{"direction", Format::Enum},
    Field{"counterparty_id", Format::Reference, "counterparty_name"},
    Field{"contract_id", Format::Reference, "contract_number"},
    Field{"shipment_id", Format::Reference}, Field{"transportation_id", Format::Reference},
    Field{"quantity_milli", Format::Quantity}, Field{"unit", Format::Enum},
    Field{"price_minor", Format::Money}, Field{"currency"}, Field{"service_date", Format::Date},
    Field{"invoice_currency"}, Field{"invoice_rate.rate_millionths", Format::Rate},
    Field{"invoice_rate.date", Format::Date}, Field{"invoice_rate.source", Format::Enum},
    Field{"price_mode", Format::Enum}, Field{"vat_mode", Format::Enum},
    Field{"vat_rate_basis_points", Format::Percent}, Field{"net_minor", Format::Money},
    Field{"vat_minor", Format::Money}, Field{"total_minor", Format::Money},
    Field{"invoice_net_minor", Format::Money}, Field{"invoice_vat_minor", Format::Money},
    Field{"invoice_total_minor", Format::Money}, Field{"note"}, Field{"archived", Format::Boolean}};
constexpr std::array kInvoiceFields{Field{"number"}, Field{"direction", Format::Enum},
    Field{"invoice_date", Format::Date}, Field{"due_date", Format::Date},
    Field{"own_company_id", Format::Reference, "own_company_name"},
    Field{"counterparty_id", Format::Reference, "counterparty_name"},
    Field{"own_bank_account_id", Format::Reference},
    Field{"counterparty_bank_account_id", Format::Reference},
    Field{"contract_id", Format::Reference, "contract_number"}, Field{"currency"},
    Field{"net_minor", Format::Money}, Field{"vat_minor", Format::Money},
    Field{"total_minor", Format::Money}, Field{"note"}, Field{"archived", Format::Boolean}};
constexpr std::array kPaymentFields{Field{"number"}, Field{"kind", Format::Enum},
    Field{"payment_date", Format::Date},
    Field{"own_company_id", Format::Reference, "own_company_name"},
    Field{"counterparty_id", Format::Reference, "counterparty_name"},
    Field{"own_bank_account_id", Format::Reference},
    Field{"counterparty_bank_account_id", Format::Reference}, Field{"currency"},
    Field{"amount_minor", Format::Money}, Field{"purpose"}, Field{"note"},
    Field{"archived", Format::Boolean}};

std::map<std::string, Json> Indexed(const Json& rows, const char* key) {
    std::map<std::string, Json> result;
    if (!rows.IsArray())
        return result;
    for (const auto& row : rows)
        result.emplace(Name(row, key), row);
    return result;
}

Json Empty() {
    return userver::formats::json::Value{};
}

Json Find(const std::map<std::string, Json>& rows, const std::string& id) {
    const auto found = rows.find(id);
    return found == rows.end() ? Empty() : found->second;
}

using Source = models::FinancialHistorySource;
using Action = models::FinancialHistoryAction;
using Document = models::FinancialHistoryDocument;
using FieldKey = models::FinancialHistoryField;
using Reference = models::FinancialHistoryReference;

bool Archived(const Json& doc) {
    return At(doc, "archived").As<bool>(false);
}

void Fields(std::vector<models::FinancialHistoryChange>& changes, const auto& fields,
    const Json& before, const Json& after, const std::optional<Reference>& subject = std::nullopt) {
    for (const auto& field : fields)
        AddChange(changes, field, before, after, subject);
}

void InvoiceLines(
    std::vector<models::FinancialHistoryChange>& changes, const Json& before, const Json& after) {
    const auto old = Indexed(At(before, "charges"), "id");
    const auto current = Indexed(At(after, "charges"), "id");
    std::set<std::string> ids;
    for (const auto& [id, row] : old)
        ids.insert(id);
    for (const auto& [id, row] : current)
        ids.insert(id);
    for (const auto& id : ids) {
        const auto previous = Find(old, id);
        const auto value = Find(current, id);
        const Reference subject{.id = id,
            .name = Scalar(At(current.contains(id) ? value : previous, "label_name")),
            .source = Source::ServiceCharge};
        if (!old.contains(id) || !current.contains(id)) {
            const models::FinancialHistoryValue included{
                .format = Format::Boolean, .value = "true"};
            changes.push_back(
                {.after = current.contains(id) ? std::optional{included} : std::nullopt,
                    .before = old.contains(id) ? std::optional{included} : std::nullopt,
                    .field = FieldKey::Charges,
                    .subject = subject});
        } else {
            Fields(changes, kChargeFields, previous, value, subject);
        }
    }
}

std::int64_t Allocated(const Json& snapshot, const std::string& invoice) {
    if (Archived(At(snapshot, "payment")))
        return 0;
    const auto allocations = Indexed(At(snapshot, "allocations"), "invoice_id");
    return At(Find(allocations, invoice), "amount_minor").As<std::int64_t>(0);
}

std::optional<models::FinancialHistoryValue> Money(std::int64_t value, const Json& doc) {
    if (value == 0)
        return std::nullopt;
    return models::FinancialHistoryValue{.currency = Scalar(At(doc, "payment.currency")),
        .format = Format::Money,
        .value = std::to_string(value)};
}

Document DocumentType(Source source, const Json& document) {
    if (source == Source::ServiceCharge)
        return Document::ServiceCharge;
    if (source == Source::Invoice)
        return Document::Invoice;
    return models::ParseEnumScalar(Name(document, "kind"), models::PaymentKind{}) ==
            models::PaymentKind::Receipt
        ? Document::Receipt
        : Document::Disbursement;
}

Action DocumentAction(std::int64_t revision, const Json& before, const Json& after) {
    if (revision == 1)
        return Action::Created;
    if (before.IsMissing() || before.IsNull())
        return Action::Updated;
    if (Archived(after) && !Archived(before))
        return Action::Archived;
    if (!Archived(after) && Archived(before))
        return Action::Restored;
    return Action::Updated;
}

models::FinancialHistoryEvent Event(models::FinancialHistoryCursor cursor, const std::string& actor,
    const Json& before, const Json& after, Source requested, const std::string& requested_id,
    const std::map<std::string, std::string>& invoices) {
    const auto document = cursor.source == Source::Invoice ? At(after, "invoice")
        : cursor.source == Source::Payment                 ? At(after, "payment")
                                                           : after;
    const auto previous = cursor.source == Source::Invoice ? At(before, "invoice")
        : cursor.source == Source::Payment                 ? At(before, "payment")
                                                           : before;
    models::FinancialHistoryEvent event{
        .action = DocumentAction(cursor.revision, previous, document),
        .actor = actor,
        .cursor = cursor,
        .document = DocumentType(cursor.source, document),
        .has_previous = !before.IsNull() && !before.IsMissing()};
    event.actor_name = Scalar(At(after, "actor_name"));
    if (cursor.source == Source::Invoice && requested == Source::ServiceCharge) {
        const auto old = Indexed(At(before, "charges"), "id");
        const auto current = Indexed(At(after, "charges"), "id");
        const bool was = old.contains(requested_id) && !Archived(At(before, "invoice"));
        const bool included = current.contains(requested_id) && !Archived(At(after, "invoice"));
        event.action = included ? Action::InvoiceLinked : Action::InvoiceUnlinked;
        event.reference = Reference{.id = cursor.entity_id,
            .name = Scalar(At(document, "number")),
            .source = Source::Invoice};
        event.changes.push_back(
            {.after = included ? std::optional{models::FinancialHistoryValue{
                                     .display_name = Scalar(At(document, "number")),
                                     .format = Format::Reference,
                                     .value = cursor.entity_id}}
                               : std::nullopt,
                .before = was ? std::optional{models::FinancialHistoryValue{
                                    .display_name = Scalar(At(previous, "number")),
                                    .format = Format::Reference,
                                    .value = cursor.entity_id}}
                              : std::nullopt,
                .field = FieldKey::Invoice});
        return event;
    }
    if (cursor.source == Source::Payment && requested == Source::Invoice) {
        const auto old = Allocated(before, requested_id);
        const auto current = Allocated(after, requested_id);
        event.action = current == 0 ? Action::PaymentUnallocated
            : old == 0              ? Action::PaymentAllocated
                                    : Action::PaymentReallocated;
        event.reference = Reference{.id = cursor.entity_id,
            .name = Scalar(At(document, "number")),
            .source = Source::Payment};
        event.changes.push_back({.after = Money(current, after),
            .before = Money(old, before),
            .field = FieldKey::Allocation});
        return event;
    }
    if (cursor.source == Source::ServiceCharge)
        Fields(event.changes, kChargeFields, previous, document);
    if (cursor.source == Source::Invoice) {
        Fields(event.changes, kInvoiceFields, previous, document);
        InvoiceLines(event.changes, before, after);
    }
    if (cursor.source == Source::Payment) {
        Fields(event.changes, kPaymentFields, previous, document);
        const auto old = Indexed(At(before, "allocations"), "invoice_id");
        const auto current = Indexed(At(after, "allocations"), "invoice_id");
        std::set<std::string> ids;
        for (const auto& [id, row] : old)
            ids.insert(id);
        for (const auto& [id, row] : current)
            ids.insert(id);
        for (const auto& id : ids) {
            const auto was = Allocated(before, id);
            const auto amount = Allocated(after, id);
            if (was == amount)
                continue;
            const auto name = invoices.find(id);
            event.changes.push_back({.after = Money(amount, after),
                .before = Money(was, before),
                .field = FieldKey::Allocation,
                .subject = Reference{.id = id,
                    .name = name == invoices.end() ? std::nullopt : std::optional{name->second},
                    .source = Source::Invoice}});
        }
    }
    return event;
}

}  // namespace

FinancialHistoryComponent::FinancialHistoryComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-finance").GetCluster()) {}

userver::yaml_config::Schema FinancialHistoryComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::FinancialHistoryResponse FinancialHistoryComponent::Load(
    const userver::server::request::RequestContext& context,
    const models::FinancialHistoryRequest& body, models::FinancialHistorySource source) const {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto permission = source == Source::ServiceCharge
        ? permissions::FINANCE_READ_SERVICE_CHARGES
        : source == Source::Invoice ? permissions::FINANCE_READ_INVOICES
                                    : permissions::FINANCE_READ_PAYMENTS;
    platform::auth::CheckPermission(context, permission);
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
    const bool related = source == Source::ServiceCharge
        ? platform::auth::HasPermission(context, permissions::FINANCE_READ_INVOICES)
        : source == Source::Invoice
        ? platform::auth::HasPermission(context, permissions::FINANCE_READ_PAYMENTS)
        : true;
    const auto query = source == Source::ServiceCharge ? sql::kListServiceChargeHistory
        : source == Source::Invoice                    ? sql::kListInvoiceHistory
                                                       : sql::kListPaymentHistory;
    const auto rows = transaction.Execute(query, organization, body.id, before_time, before_source,
        before_id, before_revision, body.limit + 1, related);
    using Row = std::tuple<std::string, boost::uuids::uuid, std::int64_t, std::string,
        pg::TimePointTz, Json, std::optional<Json>>;
    const auto history = rows.AsContainer<std::vector<Row>>(pg::kRowTag);
    std::set<boost::uuids::uuid> invoice_ids;
    if (source == Source::Payment &&
        platform::auth::HasPermission(context, permissions::FINANCE_READ_INVOICES)) {
        for (const auto& row : history) {
            for (const auto& doc : {std::get<5>(row), std::get<6>(row).value_or(Json{})}) {
                for (const auto& [id, allocation] : Indexed(At(doc, "allocations"), "invoice_id"))
                    invoice_ids.insert(boost::uuids::string_generator{}(id));
            }
        }
    }
    std::map<std::string, std::string> invoice_names;
    if (!invoice_ids.empty()) {
        const auto names = transaction.Execute(sql::kHistoryInvoiceNames, organization,
            std::vector<boost::uuids::uuid>{invoice_ids.begin(), invoice_ids.end()});
        for (const auto& row : names) {
            const auto [id, name] = row.As<boost::uuids::uuid, std::string>();
            invoice_names.emplace(boost::uuids::to_string(id), name);
        }
    }
    transaction.Commit();
    models::FinancialHistoryResponse result;
    for (const auto& [type, id, revision, actor, time, snapshot, previous] : history) {
        if (result.items.size() == static_cast<std::size_t>(body.limit))
            break;
        result.items.push_back(Event({.entity_id = boost::uuids::to_string(id),
                                         .occurred_at = time.GetUnderlying(),
                                         .revision = revision,
                                         .source = models::ParseEnumScalar(type, Source{})},
            actor, previous.value_or(Json{}), snapshot, source, body.id, invoice_names));
    }
    if (history.size() > static_cast<std::size_t>(body.limit))
        result.next_before = result.items.back().cursor;
    return result;
}
}  // namespace finance
