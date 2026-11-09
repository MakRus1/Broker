#include <components/service_charges.hpp>
#include <helpers/document_parties.hpp>
#include <helpers/exchange_amounts.hpp>
#include <helpers/finance_settings.hpp>
#include <helpers/history/snapshot.hpp>
#include <helpers/invoices.hpp>
#include <helpers/service_charge_registry.hpp>
#include <helpers/service_charges.hpp>
#include <limits>
#include <map>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/charge_amounts.hpp>
#include <platform/date_validation.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace finance {
using namespace helpers;

ServiceChargesComponent::ServiceChargesComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      deps_{.contracts = context.FindComponent<clients::contracts::Component>().GetClient(),
          .counterparties = context.FindComponent<clients::counterparties::Component>().GetClient(),
          .shipments = context.FindComponent<clients::shipments::Component>().GetClient(),
          .transportations =
              context.FindComponent<clients::transportations::Component>().GetClient(),
          .exchange_rates = context.FindComponent<ExchangeRates>(),
          .pg_cluster = context.FindComponent<userver::components::Postgres>("postgres-finance")
                            .GetCluster()} {}

userver::yaml_config::Schema ServiceChargesComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::ServiceCharge ServiceChargesComponent::Save(const models::SaveServiceChargeRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_SERVICE_CHARGES);
    const auto organization = platform::auth::GetOrganization(identity);
    ValidateServiceChargeScope(context,
        {.shipment_id = request.shipment_id, .transportation_id = request.transportation_id});
    const auto parties = ValidateServiceChargeParties(context, request);
    const auto& body = request;
    ValidateCurrency(body.currency);
    ValidateCurrency(body.invoice_currency);
    const Rate exchange_rate = body.manual_invoice_rate_millionths.has_value()
        ? Rate{.Millionths = body.manual_invoice_rate_millionths.value(),
              .Date = body.service_date,
              .Source = "manual"}
        : deps.exchange_rates.Get(body.currency, body.invoice_currency, body.service_date);
    if (body.currency == body.invoice_currency && exchange_rate.Millionths != 1000000)
        throw ApiError<400>{
            "identity_exchange_rate", "Для одинаковых валют курс должен быть равен 1"};
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    // Тот же порядок блокировок, что при включении услуги: услуга, затем счёт.
    if (request.expected_revision != 0)
        transaction.Execute(sql::kLockServiceCharges, organization,
            std::vector{boost::uuids::string_generator{}(request.id)});
    const auto labels =
        transaction.Execute(sql::kGetFinancialLabel, organization, request.label_id);
    if (labels.IsEmpty())
        throw ApiError<404>{"label_not_found", "Финансовая метка не найдена"};
    const auto value = PrepareServiceCharge(
        request, Convert(labels.AsSingleRow<pgtypes::FinancialLabelV1>()), parties, exchange_rate);
    const auto rows = request.expected_revision == 0
        ? transaction.Execute(sql::kCreateServiceCharge, organization, value)
        : transaction.Execute(
              sql::kUpdateServiceCharge, organization, value, request.expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Услуга изменена. Обновите список"};
    const auto result = Convert(rows.AsSingleRow<pgtypes::ServiceChargeV1>());
    transaction.Execute(sql::kWriteServiceChargeHistory, organization, result.id, result.revision,
        identity.ActorId(), HistorySnapshot(result, identity));
    const auto assignment =
        transaction.Execute(sql::kServiceChargeInvoiceAssignment, organization, value.Id);
    if (!assignment.IsEmpty()) {
        platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_INVOICES);
        const auto invoice_id = assignment.AsSingleRow<boost::uuids::uuid>();
        const auto locked =
            transaction.Execute(sql::kLockInvoices, organization, std::vector{invoice_id});
        auto invoice = locked.AsSingleRow<pgtypes::InvoiceV1>();
        if (invoice.Revision == std::numeric_limits<std::int64_t>::max())
            throw ApiError<409>{"invalid_revision", "Достигнут предел редакций счёта"};
        if (value.Archived)
            throw ApiError<409>{
                "service_charge_invoiced", "Сначала исключите услугу из счёта, затем архивируйте"};
        ValidateInvoiceServiceCharge(value, invoice);
        const auto previous =
            transaction.Execute(sql::kGetInvoiceServiceCharges, organization, invoice_id);
        invoice.NetMinor = 0;
        invoice.VatMinor = 0;
        invoice.TotalMinor = 0;
        for (const auto& line : previous.AsSetOf<pgtypes::ServiceChargeV1>()) {
            const auto& current = line.Id == value.Id ? value : line;
            invoice.NetMinor += current.InvoiceNetMinor;
            invoice.VatMinor += current.InvoiceVatMinor;
            invoice.TotalMinor += current.InvoiceTotalMinor;
        }
        const auto revision = invoice.Revision++;
        transaction.Execute(
            sql::kUpdateInvoiceServiceCharge, organization, invoice_id, value.Id, value);
        ValidateInvoiceSettlement(transaction, organization, invoice);
        transaction.Execute(sql::kUpdateInvoice, organization, invoice, revision);
        const auto details =
            LoadInvoice(transaction, organization, boost::uuids::to_string(invoice_id));
        transaction.Execute(sql::kWriteInvoiceHistory, organization, invoice_id, invoice.Revision,
            identity.ActorId(), HistorySnapshot(details, identity));
    }
    if (body.target_invoice_id.has_value()) {
        platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_INVOICES);
        if (body.expected_revision != 0 || !body.target_invoice_revision.has_value())
            throw ApiError<400>{
                "invalid_invoice_target", "Создавать услугу из счёта можно только с его редакцией"};
        IncludeServiceCharge(transaction, organization, identity, value,
            body.target_invoice_id.value(), body.target_invoice_revision.value());
    } else if (body.target_invoice_revision.has_value())
        throw ApiError<400>{"invalid_invoice_target", "Укажите счёт для новой услуги"};
    transaction.Commit();
    return result;
}

namespace {
// Значения source_group в service_charge_totals.sql.
enum class SummaryGroup : std::int32_t { Direct = 0, Transportations = 1, Combined = 2 };
}  // namespace

ServiceChargesComponent::ListResult ServiceChargesComponent::List(
    const models::ListServiceChargesRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_SERVICE_CHARGES);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto& scope = request.scope;
    ValidateServiceChargeScope(context, scope);
    auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto rows = transaction.Execute(sql::kListServiceCharges, organization, scope.shipment_id,
        scope.transportation_id, request.after, request.include_archived, request.limit + 1);
    ListResult response;
    response.items = ConvertServiceCharges(rows);
    if (response.items.size() > static_cast<std::size_t>(request.limit)) {
        response.items.pop_back();
        response.next_after = response.items.back().id;
    }
    response.invoices = LoadServiceChargeInvoices(transaction, organization, response.items);
    if (!scope.shipment_id.has_value()) {
        transaction.Commit();
        const auto settings = GetFinanceSettings(deps.pg_cluster, organization);
        response.reporting = {
            .currency = settings.reporting_currency, .missing_rates = {}, .summary = {}};
        return response;
    }
    const auto totals = transaction.Execute(
        sql::kServiceChargeTotals, organization, scope.shipment_id, scope.transportation_id);
    for (const auto& row : totals) {
        const auto [group, currency, income, expense] =
            row.As<std::int32_t, std::string, std::string, std::string>();
        models::CurrencyTotal total{
            .currency = currency, .expense_minor = expense, .income_minor = income};
        switch (static_cast<SummaryGroup>(group)) {
        case SummaryGroup::Direct:
            response.summary.own.push_back(total);
            break;
        case SummaryGroup::Transportations:
            response.summary.transportations.push_back(total);
            break;
        case SummaryGroup::Combined:
            response.summary.combined.push_back(total);
            break;
        default:
            throw std::runtime_error("Неизвестная группа финансовой сводки");
        }
    }
    const auto reporting_rows = transaction.Execute(
        sql::kReportingServiceCharges, organization, scope.shipment_id, scope.transportation_id);
    transaction.Commit();
    const auto settings = GetFinanceSettings(deps.pg_cluster, organization);
    response.reporting = BuildReporting(
        settings.reporting_currency, reporting_rows, scope.transportation_id.has_value());
    return response;
}

models::ServiceCharge ServiceChargesComponent::Get(const models::GetServiceChargeRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_SERVICE_CHARGES);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto rows = deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetServiceCharge, organization, request.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"service_charge_not_found", "Услуга не найдена"};
    const auto charge = Convert(rows.AsSingleRow<pgtypes::ServiceChargeV1>());
    ValidateServiceChargeScope(context,
        {.shipment_id = charge.shipment_id, .transportation_id = charge.transportation_id});
    return charge;
}

bool ServiceChargesComponent::ApplyImport(const models::ImportServiceChargesRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& command = request;
    auto actor = platform::auth::GetIdentity(context);
    if (actor.Issuer != command.actor_issuer)
        throw ApiError<400>{"invalid_actor", "Некорректный провайдер пользователя"};
    const auto producer = actor.ClientId;
    actor.Subject = command.actor_subject;
    const auto document = userver::formats::json::ValueBuilder(command).ExtractValue();
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto inserted = transaction.Execute(
        sql::kRegisterServiceChargeImport, command.organization_id, producer, command.id, document);
    if (inserted.IsEmpty()) {
        const auto old = transaction.Execute(
            sql::kGetServiceChargeImport, command.organization_id, producer, command.id);
        if (old.AsSingleRow<userver::formats::json::Value>() != document)
            throw ApiError<409>{
                "import_conflict", "Ключ переноса уже использован для другого расчёта"};
        transaction.Commit();
        return true;
    }
    std::set<std::string> ids;
    for (const auto& item : command.items) {
        const auto& charge = item.charge;
        if (!ids.insert(charge.id).second || charge.expected_revision != 0 || !charge.shipment_id ||
            charge.transportation_id || charge.target_invoice_id || charge.archived ||
            charge.currency != charge.invoice_currency || charge.contract_id ||
            (charge.manual_invoice_rate_millionths &&
                *charge.manual_invoice_rate_millionths != 1000000))
            throw ApiError<400>{"invalid_import", "Некорректные услуги для переноса в поставку"};
        const auto labels =
            transaction.Execute(sql::kGetFinancialLabel, command.organization_id, charge.label_id);
        if (labels.IsEmpty())
            throw ApiError<404>{"label_not_found", "Метка не найдена в организации"};
        auto label = Convert(labels.AsSingleRow<pgtypes::FinancialLabelV1>());
        // Названия, суммы и НДС берём из согласованного снимка, а не текущего справочника.
        label.name = item.label_name;
        label.archived = false;
        label.direction = models::FinancialLabelDirection::Both;
        const auto value = PrepareServiceCharge(charge, label,
            {.CounterpartyName = item.counterparty_name, .ContractNumber = std::nullopt},
            {.Millionths = 1000000, .Date = charge.service_date, .Source = "identity"});
        const auto rows =
            transaction.Execute(sql::kCreateServiceCharge, command.organization_id, value);
        if (rows.IsEmpty())
            throw ApiError<409>{"charge_conflict", "Услуга уже существует вне переноса"};
        const auto result = Convert(rows.AsSingleRow<pgtypes::ServiceChargeV1>());
        transaction.Execute(sql::kWriteServiceChargeHistory, command.organization_id, result.id,
            result.revision, actor.ActorId(), HistorySnapshot(result, actor));
    }
    transaction.Commit();
    return true;
}

ServiceChargesComponent::RegistryResult ServiceChargesComponent::Registry(
    const models::ServiceChargeRegistryRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    using namespace ::finance::helpers::service_charge_registry;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::FINANCE_READ_SERVICE_CHARGES);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto& body = request;
        const auto scope = platform::registry::CursorScope(body, organization);
        const auto order = platform::registry::RegistryOrderBy(fields::kFields, body.sort);
        const bool date_sort = order.CursorType == platform::registry::CursorKeyType::Date;
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
        std::optional<models::ServiceChargeRegistryCursor> cursor;
        if (more) {
            const auto& last = rows.back();
            cursor = models::ServiceChargeRegistryCursor{
                .date_value = date_sort ? last.DateKey : std::nullopt,
                .id = boost::uuids::to_string(last.Charge.Id),
                .scope = scope,
                .text_value = date_sort ? std::nullopt : std::optional{last.TextKey}};
        }
        RegistryResult response;
        for (const auto& row : rows)
            response.items.push_back(Convert(row.Charge));
        response.invoices = LoadServiceChargeInvoices(transaction, organization, response.items);
        response.next_cursor = cursor;
        transaction.Commit();
        return response;
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

ServiceChargesComponent::FilterValuesResult ServiceChargesComponent::FilterValues(
    models::ServiceChargeFilterValuesRequest request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    using namespace ::finance::helpers::service_charge_registry;
    try {
        const auto organization =
            platform::auth::GetOrganization(platform::auth::GetIdentity(context));
        FilterValuesResult result;
        result.fields = registry::DescribeFields<models::RegistryField>(fields::kFields);
        if (!request.field.has_value())
            return result;
        if (request.query.has_value())
            request.query = userver::utils::text::ToLower(request.query.value());
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

void ServiceChargesComponent::ValidateServiceChargeScope(
    const userver::server::request::RequestContext& context,
    const models::ServiceChargeScope& scope) const {
    const auto& deps = deps_;
    if (scope.transportation_id.has_value() && !scope.shipment_id.has_value())
        throw ApiError<400>{"shipment_required", "Для услуги перевозки нужна поставка"};
    const auto token = platform::auth::GetAccessToken(context);
    try {
        if (scope.transportation_id.has_value()) {
            const auto transportation =
                deps.transportations
                    .ApiV1TransportationsGet(
                        {.body = {.transportation_id = scope.transportation_id.value()}}, token)
                    .transportation;
            if (!SameId(transportation.shipment_id, scope.shipment_id.value()))
                throw ApiError<400>{"wrong_shipment", "Перевозка относится к другой поставке"};
        } else if (scope.shipment_id.has_value()) {
            deps.shipments.ApiV1ShipmentsGet(
                {.body = {.shipment_id = scope.shipment_id.value()}}, token);
        }
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"forbidden_reference", "Нет доступа к связанным данным услуги"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
    } catch (const platform::http::ApiError&) {
        throw ApiError<502>{
            "reference_unavailable", "Не удалось проверить связанные данные услуги"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::TransportError&) {
        throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
    }
}

ServiceChargesComponent::ServiceChargeParties ServiceChargesComponent::ValidateServiceChargeParties(
    const userver::server::request::RequestContext& context,
    const models::SaveServiceChargeRequest& value) const {
    const auto& deps = deps_;
    const auto token = platform::auth::GetAccessToken(context);
    try {
        const auto party =
            deps.counterparties
                .ApiV1CounterpartiesGet({.body = {.id = value.counterparty_id}}, token)
                .counterparty;
        if (party.archived && !value.archived)
            throw ApiError<409>{"archived_party", "Выберите действующего контрагента"};
        ServiceChargeParties result{.CounterpartyName = party.name, .ContractNumber = std::nullopt};
        if (value.contract_id.has_value()) {
            const auto contract =
                deps.contracts.ApiV1ContractsGet({.body = {.id = value.contract_id.value()}}, token)
                    .contract;
            if (!SameId(contract.counterparty_id, value.counterparty_id))
                throw ApiError<400>{"wrong_contract", "Договор относится к другому контрагенту"};
            if (contract.currency != value.invoice_currency)
                throw ApiError<400>{
                    "contract_currency", "Валюта счёта должна совпадать с валютой договора"};
            if (contract.archived && !value.archived)
                throw ApiError<409>{"archived_contract", "Выберите действующий договор"};
            result.ContractNumber = contract.number;
        }
        return result;
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"forbidden_reference", "Нет доступа к связанным данным услуги"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
    } catch (const platform::http::ApiError&) {
        throw ApiError<502>{
            "reference_unavailable", "Не удалось проверить связанные данные услуги"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::TransportError&) {
        throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
    }
}

pgtypes::ServiceChargeV1 ServiceChargesComponent::PrepareServiceCharge(
    const models::SaveServiceChargeRequest& value, const models::FinancialLabel& label,
    const ServiceChargeParties& parties, const Rate& exchange_rate) const {
    if (value.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция услуги"};
    if (label.archived && !value.archived)
        throw ApiError<409>{"archived_label", "Выберите действующую финансовую метку"};
    if (label.direction != models::FinancialLabelDirection::Both &&
        models::EnumValue(label.direction) != models::EnumValue(value.direction))
        throw ApiError<400>{"wrong_direction", "Метка не подходит для выбранного направления"};
    if (!platform::IsCalendarDateInRange(
            value.service_date, std::chrono::year{1900} / 1 / 1, std::chrono::year{9999} / 12 / 31))
        throw ApiError<400>{"invalid_service_date", "Укажите дату услуги не раньше 01.01.1900"};
    ValidateCurrency(value.currency);
    ValidateCurrency(value.invoice_currency);
    const auto amounts = platform::CalculateChargeAmounts(value.quantity_milli, value.price_minor,
        value.price_mode == models::PriceMode::Gross, value.vat_mode == models::VatMode::Exempt,
        value.vat_rate_basis_points);
    const auto invoice_net = ConvertAmount(amounts.Net, exchange_rate.Millionths);
    const auto invoice_total = ConvertAmount(amounts.Total, exchange_rate.Millionths);
    auto note = value.note;
    if (note.has_value()) {
        note = userver::utils::text::Trim(note.value());
        if (note.value().empty())
            note.reset();
    }
    return {
        .Id = boost::uuids::string_generator{}(value.id),
        .ShipmentId = value.shipment_id.has_value()
            ? std::optional{boost::uuids::string_generator{}(value.shipment_id.value())}
            : std::nullopt,
        .TransportationId = value.transportation_id.has_value()
            ? std::optional{boost::uuids::string_generator{}(value.transportation_id.value())}
            : std::nullopt,
        .LabelId = boost::uuids::string_generator{}(value.label_id),
        .LabelName = label.name,
        .Direction = models::EnumValue(value.direction),
        .Unit = models::EnumValue(value.unit),
        .CounterpartyId = boost::uuids::string_generator{}(value.counterparty_id),
        .CounterpartyName = parties.CounterpartyName,
        .ContractId = value.contract_id.has_value()
            ? std::optional{boost::uuids::string_generator{}(value.contract_id.value())}
            : std::nullopt,
        .ContractNumber = parties.ContractNumber,
        .QuantityMilli = value.quantity_milli,
        .PriceMinor = value.price_minor,
        .Currency = value.currency,
        .VatMode = models::EnumValue(value.vat_mode),
        .VatRateBasisPoints = value.vat_rate_basis_points.has_value()
            ? std::optional{static_cast<std::int32_t>(value.vat_rate_basis_points.value())}
            : std::nullopt,
        .NetMinor = amounts.Net,
        .VatMinor = amounts.Vat,
        .TotalMinor = amounts.Total,
        .Note = note,
        .Archived = value.archived,
        .Revision = value.expected_revision + 1,
        .PriceMode = models::EnumValue(value.price_mode),
        .ServiceDate = value.service_date,
        .InvoiceCurrency = value.invoice_currency,
        .InvoiceRateMillionths = exchange_rate.Millionths,
        .InvoiceRateDate = exchange_rate.Date,
        .InvoiceRateSource = exchange_rate.Source,
        .InvoiceNetMinor = invoice_net,
        .InvoiceVatMinor = invoice_total - invoice_net,
        .InvoiceTotalMinor = invoice_total,
    };
}

models::ReportingSummary ServiceChargesComponent::BuildReporting(
    const std::string& currency, const pg::ResultSet& rows, bool transportation_scope) const {
    const auto& deps = deps_;

    struct Totals {
        WideAmount Income = 0;
        WideAmount Expense = 0;
        bool Present = false;
    } direct, transportations, combined;

    models::ReportingSummary result{.currency = currency, .missing_rates = {}, .summary = {}};
    using Date = userver::utils::datetime::Date;
    std::map<std::pair<Date, std::string>, std::optional<Rate>> rates;
    std::set<std::pair<Date, std::string>> missing;
    for (const auto& row : rows) {
        const auto [date, from, invoice_currency, total, invoice_total, direction, transportation] =
            row.As<Date, std::string, std::string, std::int64_t, std::int64_t, std::string,
                std::optional<boost::uuids::uuid>>();
        WideAmount amount;
        if (currency == invoice_currency)
            amount = invoice_total;
        else {
            const auto key = std::pair{date, from};
            if (!rates.contains(key)) {
                try {
                    rates.emplace(key, deps.exchange_rates.Get(from, currency, date));
                } catch (const ApiError<503>&) {
                    rates.emplace(key, std::nullopt);
                }
            }
            if (!rates.at(key).has_value()) {
                missing.insert(key);
                continue;
            }
            amount = ConvertAmount(WideAmount{total}, rates.at(key).value().Millionths);
        }
        auto& group =
            transportation.has_value() && !transportation_scope ? transportations : direct;
        group.Present = combined.Present = true;
        if (models::ParseEnumScalar(direction, models::ServiceChargeDirection{}) ==
            models::ServiceChargeDirection::Income) {
            group.Income += amount;
            combined.Income += amount;
        } else {
            group.Expense += amount;
            combined.Expense += amount;
        }
    }
    for (const auto& [date, from] : missing)
        result.missing_rates.push_back({.currency = from, .date = date});
    const auto append = [&](auto& target, const Totals& totals) {
        if (totals.Present)
            target.push_back({.currency = currency,
                .expense_minor = totals.Expense.str(),
                .income_minor = totals.Income.str()});
    };
    append(result.summary.own, direct);
    append(result.summary.transportations, transportations);
    append(result.summary.combined, combined);
    return result;
}
}  // namespace finance
