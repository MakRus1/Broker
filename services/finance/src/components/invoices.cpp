#include <components/invoices.hpp>
#include <helpers/converters/invoice_print.hpp>
#include <helpers/finance_settings.hpp>
#include <helpers/invoice_registry.hpp>
#include <helpers/invoices.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/document_number.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace finance {
using namespace helpers;

namespace {
std::string ServiceChargeDirection(models::InvoiceDirection direction) {
    return direction == models::InvoiceDirection::Outgoing ? "income" : "expense";
}

std::vector<boost::uuids::uuid> InvoiceServiceChargeIds(const models::SaveInvoiceRequest& body) {
    std::set<boost::uuids::uuid> unique;
    for (const auto& id : body.service_charge_ids)
        unique.insert(boost::uuids::string_generator{}(id));
    if (unique.size() != body.service_charge_ids.size())
        throw ApiError<400>{"duplicate_service_charge", "Услуги не должны повторяться"};
    return {unique.begin(), unique.end()};
}
}  // namespace

namespace {
std::string GenerateInvoiceNumber(pg::Transaction& transaction, const std::string& organization,
    const pgtypes::InvoiceV1& invoice) {
    const auto settings =
        ConvertFinanceSettings(transaction.Execute(sql::kGetFinanceSettings, organization));
    const auto& mask = invoice.Direction == "outgoing" ? settings.outgoing_number_template
                                                       : settings.incoming_number_template;
    const auto period =
        platform::DocumentNumberPeriod(invoice.InvoiceDate, settings.reset_numbers_yearly);
    const auto sequence =
        transaction.Execute(sql::kNextDocumentNumber, organization, invoice.Direction, period)
            .AsSingleRow<std::int64_t>();
    const auto number = platform::FormatDocumentNumber(mask, invoice.InvoiceDate, sequence);
    if (number.size() > 100)
        throw ApiError<400>{"number_too_long", "Номер счёта длиннее 100 символов"};
    return number;
}
}  // namespace

InvoicesComponent::InvoicesComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      deps_{.contracts = context.FindComponent<clients::contracts::Component>().GetClient(),
          .counterparties = context.FindComponent<clients::counterparties::Component>().GetClient(),
          .pg_cluster = context.FindComponent<userver::components::Postgres>("postgres-finance")
                            .GetCluster()} {}

userver::yaml_config::Schema InvoicesComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::InvoicePrintData InvoicesComponent::PrintData(const models::InvoicePrintRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto& body = request;
    auto read = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto details = LoadInvoice(read, organization, body.id);
    if (details.invoice.revision != body.revision)
        throw ApiError<409>{
            "revision_conflict", "Счёт изменён. Обновите карточку перед скачиванием"};
    if (details.invoice.direction != models::InvoiceDirection::Outgoing)
        throw ApiError<400>{"not_outgoing_invoice", "Печатная форма доступна для исходящего счёта"};
    const auto cached =
        read.Execute(sql::kGetInvoicePrintSnapshot, organization, body.id, body.revision);
    read.Commit();
    if (!cached.IsEmpty())
        return broker_codegen::ParseJson<models::InvoicePrintData>(
            cached.AsSingleRow<userver::formats::json::Value>());

    // Справочники нужны только для первого снимка редакции. Транзакция на время HTTP не открыта.
    models::InvoicePrintData data;
    data.invoice = details.invoice;
    data.charges = details.charges;
    const auto token = platform::auth::GetAccessToken(context);
    try {
        data.supplier = ConvertPrintParty(
            deps.counterparties
                .ApiV1CounterpartiesGet({.body = {.id = data.invoice.own_company_id}}, token)
                .counterparty);
        data.customer = ConvertPrintParty(
            deps.counterparties
                .ApiV1CounterpartiesGet({.body = {.id = data.invoice.counterparty_id}}, token)
                .counterparty);
        if (data.invoice.own_bank_account_id.has_value()) {
            const auto accounts = deps.counterparties.ApiV1CounterpartiesBankAccountsList(
                {.body = {.counterparty_id = data.invoice.own_company_id}}, token);
            const auto found = std::find_if(
                accounts.items.begin(), accounts.items.end(), [&](const auto& account) {
                    return SameId(account.id, data.invoice.own_bank_account_id.value());
                });
            if (found == accounts.items.end() || found->currency != data.invoice.currency)
                throw ApiError<409>{"invalid_print_account",
                    "Банковские реквизиты счёта недоступны или имеют другую валюту"};
            data.supplier_bank = ConvertPrintBank(*found);
        }
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"forbidden_reference", "Нет доступа к реквизитам для печатной формы"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<404>{"reference_not_found", "Реквизиты для печатной формы не найдены"};
    } catch (const platform::http::ApiError&) {
        throw ApiError<502>{"reference_unavailable", "Не удалось получить реквизиты счёта"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::TransportError&) {
        throw ApiError<502>{"reference_unavailable", "Сервис реквизитов недоступен"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса реквизитов"};
    }
    auto write = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto current = write.Execute(sql::kLockInvoicePrintRevision, organization, body.id);
    if (current.IsEmpty() || current.AsSingleRow<std::int64_t>() != body.revision)
        throw ApiError<409>{
            "revision_conflict", "Счёт изменён. Обновите карточку перед скачиванием"};
    auto saved = write.Execute(sql::kSaveInvoicePrintSnapshot, organization, body.id, body.revision,
        userver::formats::json::ValueBuilder(data).ExtractValue());
    if (saved.IsEmpty())
        saved = write.Execute(sql::kGetInvoicePrintSnapshot, organization, body.id, body.revision);
    const auto result = broker_codegen::ParseJson<models::InvoicePrintData>(
        saved.AsSingleRow<userver::formats::json::Value>());
    write.Commit();
    return result;
}

models::InvoiceDetails InvoicesComponent::Save(const models::SaveInvoiceRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_INVOICES);
    const auto organization = platform::auth::GetOrganization(identity);
    auto value = Prepare(context, request);
    const auto ids = InvoiceServiceChargeIds(request);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    if (value.Number.empty())
        value.Number = GenerateInvoiceNumber(transaction, organization, value);
    // Общий порядок блокировок защищает от одновременного редактирования и включения услуг в
    // счета.
    const auto charges = transaction.Execute(sql::kLockServiceCharges, organization, ids);
    if (charges.Size() != ids.size())
        throw ApiError<404>{"service_charge_not_found", "Услуга не найдена в организации"};
    if (request.previous_invoice_id.has_value()) {
        if (request.expected_revision != 0 || ids.size() != 1)
            throw ApiError<400>{
                "invalid_reassignment", "Новый счёт при перепривязке создаётся из одной услуги"};
        CheckInvoiceAssignment(transaction, organization, ids.front(), request.previous_invoice_id);
        RemoveInvoiceServiceCharge(transaction, organization,
            boost::uuids::string_generator{}(request.previous_invoice_id.value()), ids.front(),
            identity);
    }
    if (!transaction.Execute(sql::kConflictingInvoiceServiceCharges, organization, ids, value.Id)
             .IsEmpty())
        throw ApiError<409>{"service_charge_invoiced", "Услуга уже включена в другой счёт"};
    std::vector<pgtypes::ServiceChargeV1> snapshots;
    snapshots.reserve(charges.Size());
    for (const auto& charge : charges.AsSetOf<pgtypes::ServiceChargeV1>()) {
        ValidateInvoiceServiceCharge(charge, value);
        snapshots.push_back(charge);
        value.NetMinor += charge.InvoiceNetMinor;
        value.VatMinor += charge.InvoiceVatMinor;
        value.TotalMinor += charge.InvoiceTotalMinor;
    }
    if (request.expected_revision != 0)
        transaction.Execute(sql::kLockInvoices, organization, std::vector{value.Id});
    ValidateInvoiceSettlement(transaction, organization, value);
    const auto saved = request.expected_revision == 0
        ? transaction.Execute(sql::kCreateInvoice, organization, value)
        : transaction.Execute(sql::kUpdateInvoice, organization, value, request.expected_revision);
    if (saved.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Счёт изменён. Обновите карточку"};
    transaction.Execute(sql::kDeleteInvoiceServiceCharges, organization, value.Id,
        std::optional<boost::uuids::uuid>{});
    transaction.Execute(
        sql::kCreateInvoiceServiceCharge, organization, value.Id, snapshots, value.Archived);
    const auto result = LoadInvoice(transaction, organization, request.id);
    transaction.Execute(sql::kWriteInvoiceHistory, organization, value.Id, value.Revision,
        identity.ActorId(), HistorySnapshot(result, identity));
    transaction.Commit();
    return models::InvoiceDetails{.charges = result.charges, .invoice = result.invoice};
}

models::InvoiceDetails InvoicesComponent::Get(const models::GetInvoiceRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);
    const auto organization = platform::auth::GetOrganization(identity);
    auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kSlave,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto result = LoadInvoice(transaction, organization, request.id);
    transaction.Commit();
    return models::InvoiceDetails{.charges = result.charges, .invoice = result.invoice};
}

InvoicesComponent::ListResult InvoicesComponent::List(const models::ListInvoicesRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    using namespace ::finance::helpers::invoice_registry;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);
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
                    .id = boost::uuids::to_string(last.Invoice.Id),
                    .scope = scope,
                    .text_value = date_sort ? std::nullopt : std::optional{last.TextKey}};
        }
        transaction.Commit();
        return ListResult{.items = Convert(rows), .next_cursor = cursor};
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

InvoicesComponent::CandidatesResult InvoicesComponent::Candidates(
    const models::ListInvoiceCandidatesRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto& body = request;
    const auto rows =
        deps.pg_cluster->Execute(pg::ClusterHostType::kSlave, sql::kListInvoiceCandidates,
            organization, ServiceChargeDirection(body.direction), body.counterparty_id,
            body.currency, InvoiceId(body.contract_id), InvoiceId(body.invoice_id),
            InvoiceId(body.after), static_cast<std::int32_t>(body.limit + 1));
    auto items = ConvertServiceCharges(rows);
    std::optional<std::string> next;
    if (items.size() > static_cast<std::size_t>(body.limit)) {
        items.pop_back();
        next = items.back().id;
    }
    return CandidatesResult{.items = std::move(items), .next_after = next};
}

models::InvoiceDetails InvoicesComponent::AddServiceCharge(
    const models::AddInvoiceServiceChargeRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_INVOICES);
    const auto organization = platform::auth::GetOrganization(identity);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto charges = transaction.Execute(sql::kLockServiceCharges, organization,
        std::vector{boost::uuids::string_generator{}(request.service_charge_id)});
    if (charges.IsEmpty())
        throw ApiError<404>{"service_charge_not_found", "Услуга не найдена"};
    const auto charge = charges.AsSingleRow<pgtypes::ServiceChargeV1>();
    const auto result = IncludeServiceCharge(transaction, organization, identity, charge,
        request.invoice_id, request.expected_revision, request.previous_invoice_id);
    transaction.Commit();
    return models::InvoiceDetails{.charges = result.charges, .invoice = result.invoice};
}

models::InvoiceDetails InvoicesComponent::DetachServiceCharge(
    const models::DetachInvoiceServiceChargeRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_INVOICES);
    const auto organization = platform::auth::GetOrganization(identity);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto charges = transaction.Execute(sql::kLockServiceCharges, organization,
        std::vector{boost::uuids::string_generator{}(request.service_charge_id)});
    if (charges.IsEmpty())
        throw ApiError<404>{"service_charge_not_found", "Услуга не найдена"};
    const auto charge = charges.AsSingleRow<pgtypes::ServiceChargeV1>();
    CheckInvoiceAssignment(transaction, organization, charge.Id, std::optional{request.invoice_id});
    RemoveInvoiceServiceCharge(transaction, organization,
        boost::uuids::string_generator{}(request.invoice_id), charge.Id, identity);
    const auto result = LoadInvoice(transaction, organization, request.invoice_id);
    transaction.Commit();
    return models::InvoiceDetails{.charges = result.charges, .invoice = result.invoice};
}

InvoicesComponent::FilterValuesResult InvoicesComponent::FilterValues(
    const models::InvoiceFilterValuesRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    using namespace ::finance::helpers::invoice_registry;
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

pgtypes::InvoiceV1 InvoicesComponent::Prepare(
    const userver::server::request::RequestContext& context,
    const models::SaveInvoiceRequest& body) const {
    const auto& deps = deps_;
    if (body.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция счёта"};
    const auto number = userver::utils::text::Trim(body.number.value_or(""));
    if (number.empty() && (body.number.has_value() || body.expected_revision != 0))
        throw ApiError<400>{"empty_number", "Укажите номер счёта"};
    for (const auto& date : {std::optional{body.invoice_date}, body.due_date})
        if (date.has_value() &&
            !platform::IsCalendarDateInRange(
                date.value(), std::chrono::year{1900} / 1 / 1, std::chrono::year{9999} / 12 / 31))
            throw ApiError<400>{"invalid_date", "Дата должна быть не раньше 01.01.1900"};
    if (SameId(body.own_company_id, body.counterparty_id))
        throw ApiError<400>{"same_parties", "Выберите разные стороны счёта"};
    auto result = pgtypes::InvoiceV1{.Id = boost::uuids::string_generator{}(body.id),
        .Direction = models::EnumValue(body.direction),
        .Number = number,
        .InvoiceDate = body.invoice_date,
        .DueDate = body.due_date,
        .OwnCompanyId = boost::uuids::string_generator{}(body.own_company_id),
        .OwnCompanyName = {},
        .CounterpartyId = boost::uuids::string_generator{}(body.counterparty_id),
        .CounterpartyName = {},
        .OwnBankAccountId = InvoiceId(body.own_bank_account_id),
        .CounterpartyBankAccountId = InvoiceId(body.counterparty_bank_account_id),
        .ContractId = InvoiceId(body.contract_id),
        .ContractNumber = std::nullopt,
        .Currency = body.currency,
        .Note = body.note,
        .Archived = body.archived,
        .Revision = body.expected_revision + 1,
        .NetMinor = 0,
        .VatMinor = 0,
        .TotalMinor = 0};
    if (result.Note.has_value()) {
        result.Note = userver::utils::text::Trim(result.Note.value());
        if (result.Note.value().empty())
            result.Note.reset();
    }
    ValidateDocumentParties(deps, context, body, result);
    return result;
}
}  // namespace finance
