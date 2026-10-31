#pragma once
#include <helpers/contracts.hpp>
#include <platform/auth/organization_admin.hpp>
#include <platform/document_number.hpp>
#include <tuple>

namespace contracts::helpers {
inline models::ContractNumbering ConvertNumbering(const pg::ResultSet& rows) {
    if (rows.IsEmpty())
        return {
            .number_template = "Д-{sequence}/{yyyy}", .reset_number_yearly = false, .revision = 0};
    const auto [mask, yearly, revision] =
        rows.AsSingleRow<std::tuple<std::string, bool, std::int64_t>>(pg::kRowTag);
    return {.number_template = mask, .reset_number_yearly = yearly, .revision = revision};
}

inline void RequireNumberingAdministrator(const Deps& deps,
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

inline std::string GenerateContractNumber(pg::Transaction& transaction,
    const std::string& organization, userver::utils::datetime::Date date) {
    const auto settings = ConvertNumbering(transaction.Execute(sql::kGetNumbering, organization));
    const auto sequence =
        transaction
            .Execute(sql::kNextDocumentNumber, organization, std::string{"contract"},
                platform::DocumentNumberPeriod(date, settings.reset_number_yearly))
            .AsSingleRow<std::int64_t>();
    const auto number = platform::FormatDocumentNumber(settings.number_template, date, sequence);
    if (number.size() > 128)
        throw ApiError<400>{"number_too_long", "Номер договора длиннее 128 символов"};
    return number;
}
}  // namespace contracts::helpers
