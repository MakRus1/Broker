#include <components/payments.hpp>
#include <helpers/invoices.hpp>
#include <helpers/payment_registry.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace finance {
using namespace helpers;

namespace {

models::InvoiceDirection PaymentInvoiceDirection(models::PaymentKind kind) {
    switch (kind) {
    case models::PaymentKind::Receipt:
        return models::InvoiceDirection::Outgoing;
    case models::PaymentKind::Disbursement:
        return models::InvoiceDirection::Incoming;
    }
    throw std::logic_error("Неизвестный вид платежа");
}

models::Payment Convert(const pgtypes::PaymentV1& value) {
    models::Payment result;
    result.id = boost::uuids::to_string(value.Id);
    result.kind = models::ParseEnumScalar(value.Kind, models::PaymentKind{});
    result.number = value.Number;
    result.payment_date = value.PaymentDate;
    result.own_company_id = boost::uuids::to_string(value.OwnCompanyId);
    result.own_company_name = value.OwnCompanyName;
    result.counterparty_id = boost::uuids::to_string(value.CounterpartyId);
    result.counterparty_name = value.CounterpartyName;
    result.own_bank_account_id = InvoiceId(value.OwnBankAccountId);
    result.counterparty_bank_account_id = InvoiceId(value.CounterpartyBankAccountId);
    result.currency = value.Currency;
    result.amount_minor = value.AmountMinor;
    result.purpose = value.Purpose;
    result.note = value.Note;
    result.archived = value.Archived;
    result.revision = value.Revision;
    return result;
}

models::PaymentDetails LoadPayment(
    pg::Transaction& transaction, const std::string& organization, const std::string& id) {
    const auto rows = transaction.Execute(sql::kGetPayment, organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"payment_not_found", "Платёж не найден"};
    models::PaymentDetails result;
    result.payment = Convert(rows.AsSingleRow<pgtypes::PaymentV1>());
    for (const auto& row : transaction.Execute(sql::kGetPaymentAllocations, organization, id)) {
        const auto [invoice, amount] = row.As<boost::uuids::uuid, std::int64_t>();
        result.allocations.push_back(
            {.amount_minor = amount, .invoice_id = boost::uuids::to_string(invoice)});
        if (!result.payment.archived)
            result.allocated_minor += amount;
    }
    result.unallocated_minor = result.payment.amount_minor - result.allocated_minor;
    return result;
}

models::PaymentInvoice LoadInvoiceBalance(
    pg::Transaction& transaction, const std::string& organization, const std::string& id) {
    const auto rows = transaction.Execute(sql::kGetInvoice, organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"invoice_not_found", "Счёт не найден"};
    const auto invoice = helpers::Convert(rows.AsSingleRow<pgtypes::InvoiceV1>());
    const auto paid =
        transaction.Execute(sql::kInvoicePaid, organization, id).AsSingleRow<std::int64_t>();
    std::vector<models::InvoicePayment> payments;
    for (const auto& row : transaction.Execute(sql::kInvoicePayments, organization, id)) {
        const auto [payment, amount] = row.As<pgtypes::PaymentV1, std::int64_t>();
        payments.push_back({.amount_minor = amount, .payment = Convert(payment)});
    }
    return {.invoice = invoice,
        .outstanding_minor = invoice.total_minor - paid,
        .paid_minor = paid,
        .payments = std::move(payments)};
}

std::vector<models::Payment> Convert(const std::vector<payment_registry::RegistryRow>& rows) {
    std::vector<models::Payment> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(Convert(row.Payment));
    return result;
}

}  // namespace

PaymentsComponent::PaymentsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      deps_{
          .counterparties = context.FindComponent<clients::counterparties::Component>().GetClient(),
          .pg_cluster = context.FindComponent<userver::components::Postgres>("postgres-finance")
                            .GetCluster()} {}

userver::yaml_config::Schema PaymentsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::PaymentDetails PaymentsComponent::Save(const models::SavePaymentRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_PAYMENTS);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto& body = request;
    auto value = Prepare(context, body);
    std::map<boost::uuids::uuid, std::int64_t> allocations;
    std::int64_t allocated = 0;
    for (const auto& item : body.allocations) {
        if (item.amount_minor > value.AmountMinor - allocated)
            throw ApiError<400>{"payment_overallocated", "Распределение превышает сумму платежа"};
        allocated += item.amount_minor;
        if (!allocations
                 .emplace(boost::uuids::string_generator{}(item.invoice_id), item.amount_minor)
                 .second)
            throw ApiError<400>{"duplicate_invoice", "Счёт указан несколько раз"};
    }
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    if (body.expected_revision == 0) {
        if (transaction.Execute(sql::kCreatePayment, organization, value).IsEmpty())
            throw ApiError<409>{"revision_conflict", "Платёж уже существует. Обновите карточку"};
    } else {
        if (transaction.Execute(sql::kLockPayments, organization, value.Id).IsEmpty())
            throw ApiError<404>{"payment_not_found", "Платёж не найден"};
        if (transaction.Execute(sql::kUpdatePayment, organization, value, body.expected_revision)
                .IsEmpty())
            throw ApiError<409>{"revision_conflict", "Платёж изменён. Обновите карточку"};
    }
    std::set<boost::uuids::uuid> ids;
    for (const auto& [id, amount] : allocations)
        ids.insert(id);
    for (const auto& row : transaction.Execute(sql::kGetPaymentAllocations, organization, value.Id))
        ids.insert(row[0].As<boost::uuids::uuid>());
    // Платёж блокируется первым; все счета — в едином порядке. HTTP уже завершён.
    const auto invoices = transaction.Execute(
        sql::kLockInvoices, organization, std::vector<boost::uuids::uuid>{ids.begin(), ids.end()});
    if (invoices.Size() != ids.size())
        throw ApiError<404>{"invoice_not_found", "Счёт не найден в организации"};
    for (const auto& invoice : invoices.AsSetOf<pgtypes::InvoiceV1>()) {
        const auto found = allocations.find(invoice.Id);
        if (value.Archived || found == allocations.end())
            continue;
        const auto direction = models::EnumValue(PaymentInvoiceDirection(body.kind));
        if (invoice.Archived || invoice.Direction != direction ||
            invoice.Currency != value.Currency || invoice.OwnCompanyId != value.OwnCompanyId ||
            invoice.CounterpartyId != value.CounterpartyId)
            throw ApiError<400>{"incompatible_invoice",
                "Выберите действующий счёт с теми же сторонами, валютой и направлением"};
        const auto paid =
            transaction.Execute(sql::kInvoiceOtherPayments, organization, invoice.Id, value.Id)
                .AsSingleRow<std::int64_t>();
        if (found->second > invoice.TotalMinor - paid)
            throw ApiError<409>{
                "invoice_overpaid", "Распределение превышает остаток счёта. Обновите данные"};
    }
    transaction.Execute(sql::kDeletePaymentAllocations, organization, value.Id);
    for (const auto& [id, amount] : allocations)
        transaction.Execute(sql::kCreatePaymentAllocation, organization, value.Id, id, amount);
    const auto result = LoadPayment(transaction, organization, body.id);
    transaction.Execute(sql::kWritePaymentHistory, organization, value.Id, value.Revision,
        identity.ActorId(), HistorySnapshot(result, identity));
    transaction.Commit();
    return models::PaymentDetails{.allocated_minor = result.allocated_minor,
        .allocations = result.allocations,
        .payment = result.payment,
        .unallocated_minor = result.unallocated_minor};
}

models::PaymentDetails PaymentsComponent::Get(const models::InvoiceBalanceRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_PAYMENTS);
    const auto organization = platform::auth::GetOrganization(identity);
    auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto result = LoadPayment(transaction, organization, request.id);
    transaction.Commit();
    return models::PaymentDetails{.allocated_minor = result.allocated_minor,
        .allocations = result.allocations,
        .payment = result.payment,
        .unallocated_minor = result.unallocated_minor};
}

PaymentsComponent::ListResult PaymentsComponent::List(const models::ListPaymentsRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    using namespace ::finance::helpers::payment_registry;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::FINANCE_READ_PAYMENTS);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto& body = request;
        const auto scope = platform::registry::CursorScope(body, organization);
        const auto order = registry::RegistryOrderBy(registry_fields::kFields, body.sort);
        const bool date_sort = order.CursorType == registry::CursorKeyType::Date;
        if (body.cursor.has_value()) {
            const auto& cursor = body.cursor.value();
            if (cursor.scope != scope || date_sort != cursor.date_value.has_value() ||
                date_sort == cursor.text_value.has_value())
                throw ApiError<400>{
                    "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        }
        const auto query = platform::registry::BuildPage(kRegistry, body, organization);
        // Транзакция ограничивает SET LOCAL; HTTP-вызовов и записи внутри нет.
        auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster,
            pg::TransactionOptions{
                pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
        transaction.Execute(sql::kRegistryPlan);
        auto rows = transaction.Execute(query.Statement, query.Parameters)
                        .AsContainer<std::vector<RegistryRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(body.limit);
        if (more)
            rows.pop_back();
        std::optional<models::InvoiceRegistryCursor> cursor;
        if (more) {
            const auto& last = rows.back();
            cursor =
                models::InvoiceRegistryCursor{.date_value = date_sort ? last.DateKey : std::nullopt,
                    .id = boost::uuids::to_string(last.Payment.Id),
                    .scope = scope,
                    .text_value = date_sort ? std::nullopt : std::optional{last.TextKey}};
        }
        transaction.Commit();
        return ListResult{.items = Convert(rows), .next_cursor = cursor};
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

PaymentsComponent::FilterValuesResult PaymentsComponent::FilterValues(
    const models::PaymentFilterValuesRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    using namespace ::finance::helpers::payment_registry;
    try {
        const auto organization =
            platform::auth::GetOrganization(platform::auth::GetIdentity(context));
        FilterValuesResult result;
        result.fields = registry::DescribeFields<models::RegistryField>(registry_fields::kFields);
        if (!request.field.has_value())
            return result;
        const auto query = platform::registry::BuildFilterValues(kRegistry, request, organization);
        const auto rows =
            deps.pg_cluster->Execute(
                               pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(request.limit);
        for (std::size_t index = 0;
             index < rows.size() && index < static_cast<std::size_t>(request.limit); ++index)
            result.values.push_back({.label = rows[index].Label, .value = rows[index].Value});
        if (more)
            result.next_after = result.values.back().value;
        return result;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

PaymentsComponent::InvoiceCandidatesResult PaymentsComponent::InvoiceCandidates(
    const models::PaymentInvoiceCandidatesRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_PAYMENTS);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto& body = request;
    const auto rows =
        deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kPaymentInvoiceCandidates,
            organization, models::EnumValue(PaymentInvoiceDirection(body.kind)),
            body.own_company_id, body.counterparty_id, body.currency, body.query, body.after,
            body.limit + 1, body.payment_id);
    InvoiceCandidatesResult result;
    for (std::size_t index = 0; index < rows.Size() && index < static_cast<std::size_t>(body.limit);
         ++index) {
        const auto [row, paid] = rows[index].As<pgtypes::InvoiceV1, std::int64_t>();
        const auto invoice = Convert(row);
        result.items.push_back({.invoice = invoice,
            .outstanding_minor = invoice.total_minor - paid,
            .paid_minor = paid,
            .payments = {}});
    }
    if (rows.Size() > static_cast<std::size_t>(body.limit))
        result.next_after = result.items.back().invoice.id;
    return result;
}

models::PaymentInvoice PaymentsComponent::InvoiceBalance(
    const models::InvoiceBalanceRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);
    const auto organization = platform::auth::GetOrganization(identity);
    auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto result = LoadInvoiceBalance(transaction, organization, request.id);
    transaction.Commit();
    return models::PaymentInvoice{.invoice = result.invoice,
        .outstanding_minor = result.outstanding_minor,
        .paid_minor = result.paid_minor,
        .payments = result.payments};
}

PaymentsComponent::PaymentCandidatesResult PaymentsComponent::PaymentCandidates(
    const models::PaymentCandidatesRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_PAYMENTS);
    const auto& body = request;
    auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto invoice = LoadInvoice(transaction, organization, body.invoice_id).invoice;
    if (invoice.archived)
        throw ApiError<409>{"invoice_archived", "Счёт находится в архиве"};
    const auto kind = invoice.direction == models::InvoiceDirection::Outgoing
        ? models::PaymentKind::Receipt
        : models::PaymentKind::Disbursement;
    const auto rows = transaction.Execute(sql::kPaymentCandidates, organization,
        models::EnumValue(kind), invoice.own_company_id, invoice.counterparty_id, invoice.currency,
        body.query, body.after, body.limit + 1);
    PaymentCandidatesResult response;
    for (std::size_t index = 0; index < rows.Size() && index < static_cast<std::size_t>(body.limit);
         ++index) {
        const auto [payment, available] = rows[index].As<pgtypes::PaymentV1, std::int64_t>();
        response.items.push_back({.payment = Convert(payment), .unallocated_minor = available});
    }
    if (rows.Size() > static_cast<std::size_t>(body.limit))
        response.next_after = response.items.back().payment.id;
    transaction.Commit();
    return response;
}

pgtypes::PaymentV1 PaymentsComponent::Prepare(
    const userver::server::request::RequestContext& context,
    const models::SavePaymentRequest& body) const {
    const auto& deps = deps_;
    if (body.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция платежа"};
    if (!platform::IsCalendarDateInRange(
            body.payment_date, std::chrono::year{1900} / 1 / 1, std::chrono::year{9999} / 12 / 31))
        throw ApiError<400>{"invalid_date", "Дата должна быть не раньше 01.01.1900"};
    if (SameId(body.own_company_id, body.counterparty_id))
        throw ApiError<400>{"same_parties", "Выберите разные стороны платежа"};
    pgtypes::PaymentV1 value{.Id = boost::uuids::string_generator{}(body.id),
        .Kind = models::EnumValue(body.kind),
        .Number = userver::utils::text::Trim(body.number),
        .PaymentDate = body.payment_date,
        .OwnCompanyId = boost::uuids::string_generator{}(body.own_company_id),
        .OwnCompanyName = {},
        .CounterpartyId = boost::uuids::string_generator{}(body.counterparty_id),
        .CounterpartyName = {},
        .OwnBankAccountId = InvoiceId(body.own_bank_account_id),
        .CounterpartyBankAccountId = InvoiceId(body.counterparty_bank_account_id),
        .Currency = body.currency,
        .AmountMinor = body.amount_minor,
        .Purpose = body.purpose,
        .Note = body.note,
        .Archived = body.archived,
        .Revision = body.expected_revision + 1};
    if (value.Number.empty())
        throw ApiError<400>{"empty_number", "Укажите номер платежа"};
    ValidateDocumentParties(deps, context, body, value);
    return value;
}
}  // namespace finance
