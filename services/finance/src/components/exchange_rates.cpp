#include <components/exchange_rates.hpp>
#include <converters/cbr_exchange_rates.hpp>
#include <finance/sql_queries.hpp>
#include <fmt/chrono.h>
#include <helpers/exchange_amounts.hpp>
#include <map>
#include <models/errors.hpp>
#include <models/exchange_quote.hpp>
#include <platform/date_validation.hpp>
#include <platform/http_client.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/clients/http/request.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/date.hpp>
#include <userver/storages/postgres/transaction.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace finance {
namespace pg = userver::storages::postgres;

ExchangeRates::ExchangeRates(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      http_(context.FindComponent<userver::components::HttpClient>("cbr-http-client")
                .GetHttpClient()),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-finance").GetCluster()),
      endpoint_(config["endpoint"].As<std::string>()), timeout_(config["timeout-ms"].As<int>()) {
    if (timeout_.count() <= 0 ||
        !(endpoint_.starts_with("https://") ||
            (config["allow-http"].As<bool>() && endpoint_.starts_with("http://"))))
        throw std::invalid_argument("Некорректные настройки клиента ЦБ");
}

void ExchangeRates::LoadRatesForDate(userver::utils::datetime::Date date) const {
    if (!pg_->Execute(pg::ClusterHostType::kMaster, sql::kGetExchangeDay, date).IsEmpty())
        return;
    try {
        const auto response =
            http_.CreateRequest()
                .get(endpoint_ + "?date_req=" + fmt::format("{:%d/%m/%Y}", date.GetSysDays()))
                .timeout(timeout_)
                .retry(0)
                .follow_redirects(false)
                .perform();
        if (response->status_code() != 200)
            throw std::runtime_error("Недоступен ответ ЦБ");
        const auto rates = converters::ParseCbrExchangeRates(response->body(), date);
        std::vector<std::string> codes;
        std::vector<std::int64_t> values, nominals;
        for (const auto& quote : rates.Quotes) {
            codes.push_back(quote.Currency);
            values.push_back(quote.Value);
            nominals.push_back(quote.Nominal);
        }

        auto transaction = pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        transaction.Execute(sql::kCreateExchangeDay, date, rates.EffectiveDate);
        transaction.Execute(sql::kCreateExchangeRates, date, codes, values, nominals);
        transaction.Commit();
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception&) {
        throw ApiError<503>{"exchange_rate_unavailable",
            "Не удалось получить курс ЦБ. Повторите позже или укажите курс вручную"};
    }
}

models::ConversionRate ExchangeRates::GetConversionRate(
    const std::string& from, const std::string& to, userver::utils::datetime::Date date) const {
    if (!platform::IsCalendarDateInRange(
            date, std::chrono::year{1900} / 1 / 1, std::chrono::year{9999} / 12 / 31))
        throw ApiError<400>{"invalid_rate_date", "Дата курса должна быть не раньше 01.01.1900"};
    helpers::ValidateCurrency(from);
    helpers::ValidateCurrency(to);
    if (from == to)
        return {.Millionths = 1000000, .Date = date, .Source = "identity"};
    LoadRatesForDate(date);
    const auto rows = pg_->Execute(
        pg::ClusterHostType::kMaster, sql::kGetExchangeRates, date, std::vector{from, to});
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> quotes;
    for (const auto& row : rows) {
        const auto quote = row.As<models::ExchangeQuote>(pg::kRowTag);
        quotes.emplace(quote.Currency, std::pair{quote.Value, quote.Nominal});
    }
    if (!quotes.contains(from) || !quotes.contains(to))
        throw ApiError<503>{"exchange_rate_missing",
            "ЦБ не публикует курс выбранной валюты на эту дату. Укажите курс вручную"};
    const auto [from_value, from_nominal] = quotes.at(from);
    const auto [to_value, to_nominal] = quotes.at(to);
    const helpers::WideAmount denominator = helpers::WideAmount{from_nominal} * to_value;
    const helpers::WideAmount rate =
        (helpers::WideAmount{from_value} * to_nominal * 1000000 + denominator / 2) / denominator;
    if (rate <= 0 || rate > 1000000000000LL)
        throw ApiError<503>{
            "exchange_rate_range", "Курс выходит за допустимый диапазон. Укажите курс вручную"};
    return {.Millionths = rate.convert_to<std::int64_t>(),
        .Date = pg_->Execute(pg::ClusterHostType::kMaster, sql::kGetExchangeDay, date)
                    .AsSingleRow<userver::utils::datetime::Date>(),
        .Source = "cbr"};
}

userver::yaml_config::Schema ExchangeRates::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Исторические курсы ЦБ с постоянным кешем по дате запроса
additionalProperties: false
properties:
    endpoint: {type: string, description: XML API Банка России}
    timeout-ms: {type: integer, minimum: 1, description: Таймаут внешнего запроса}
    allow-http: {type: boolean, description: HTTP для тестового mockserver}
)");
}
}  // namespace finance
