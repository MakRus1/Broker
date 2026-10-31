#pragma once
#include <deps.hpp>
#include <finance/sql_queries.hpp>
#include <helpers/exchange_amounts.hpp>
#include <openapi/models.hpp>
#include <platform/auth/organization_admin.hpp>
#include <platform/document_number.hpp>
#include <postgresql/types.hpp>
#include <tuple>
#include <userver/storages/postgres/transaction.hpp>

namespace finance::helpers {
namespace pg = userver::storages::postgres;

inline models::FinanceSettings ConvertFinanceSettings(const pg::ResultSet& rows) {
    if (rows.IsEmpty())
        return {.incoming_number_template = "ВС-{sequence}/{yyyy}",
            .outgoing_number_template = "ИС-{sequence}/{yyyy}",
            .reporting_currency = "RUB",
            .reset_numbers_yearly = false,
            .revision = 0};
    const auto [currency, revision, outgoing, incoming, yearly] =
        rows.AsSingleRow<std::tuple<std::string, std::int64_t, std::string, std::string, bool>>(
            pg::kRowTag);
    return {.incoming_number_template = incoming,
        .outgoing_number_template = outgoing,
        .reporting_currency = currency,
        .reset_numbers_yearly = yearly,
        .revision = revision};
}

inline models::FinanceSettings GetFinanceSettings(
    const Deps& deps, const std::string& organization) {
    return ConvertFinanceSettings(deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetFinanceSettings, organization));
}

inline std::string GenerateInvoiceNumber(pg::Transaction& transaction,
    const std::string& organization, const pgtypes::InvoiceV1& invoice) {
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

inline void RequireFinanceAdministrator(const Deps& deps,
    const userver::server::request::RequestContext& context, const std::string& organization) {
    try {
        platform::auth::RequireOrganizationAdministrator(deps.organizations, context, organization);
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"forbidden", "Нет права управления организацией"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<404>{"organization_not_found", "Организация не найдена"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::ApiError&) {
        throw ApiError<503>{
            "organizations_unavailable", "Не удалось проверить доступ к организации"};
    } catch (const platform::http::TransportError&) {
        throw ApiError<503>{"organizations_unavailable", "Сервис организаций недоступен"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<503>{"organizations_unavailable", "Некорректный ответ сервиса организаций"};
    }
}
}  // namespace finance::helpers
