#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <charconv>
#include <components/exchange_rates.hpp>
#include <finance/sql_queries.hpp>
#include <fmt/chrono.h>
#include <helpers/exchange_amounts.hpp>
#include <map>
#include <platform/date_validation.hpp>
#include <platform/http_client.hpp>
#include <sstream>
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

namespace {
std::int64_t PositiveInteger(const std::string& text) {
    std::int64_t result{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
    if (error != std::errc{} || end != text.data() + text.size() || result <= 0 ||
        result > 1000000000000LL)
        throw std::runtime_error("Некорректное число в курсе ЦБ");
    return result;
}

std::int64_t Millionths(std::string text) {
    const auto separator = text.find_first_of(",.");
    if (separator == std::string::npos)
        return PositiveInteger(text) * 1000000;
    auto fraction = text.substr(separator + 1);
    if (fraction.empty() || fraction.size() > 6)
        throw std::runtime_error("Некорректная точность курса ЦБ");
    fraction.append(6 - fraction.size(), '0');
    const auto whole = text.substr(0, separator);
    if (whole != "0")
        PositiveInteger(whole);
    return (whole == "0" ? 0 : PositiveInteger(whole)) * 1000000 +
        (fraction == "000000" ? 0 : PositiveInteger(fraction));
}
}  // namespace

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

void ExchangeRates::EnsureDay(userver::utils::datetime::Date date) const {
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
        if (response->status_code() != 200 || response->body().size() > 1024 * 1024 ||
            response->body().find("<!DOCTYPE") != std::string::npos)
            throw std::runtime_error("Недоступен ответ ЦБ");
        boost::property_tree::ptree xml;
        std::istringstream stream{response->body()};
        boost::property_tree::read_xml(stream, xml);
        const auto& root = xml.get_child("ValCurs");
        const auto text = root.get<std::string>("<xmlattr>.Date");
        if (text.size() != 10 || text[2] != '.' || text[5] != '.')
            throw std::runtime_error("Некорректная дата ЦБ");
        // Только внешний формат ЦБ преобразуется в календарную дату здесь.
        const auto effective = userver::utils::datetime::DateFromRFC3339String(
            text.substr(6, 4) + "-" + text.substr(3, 2) + "-" + text.substr(0, 2));
        if (effective > date)
            throw std::runtime_error("Курс ЦБ относится к будущей дате");
        std::vector<std::string> codes{"RUB"};
        std::vector<std::int64_t> values{1000000}, nominals{1};
        for (const auto& [name, value] : root) {
            if (name != "Valute")
                continue;
            const auto code = value.get<std::string>("CharCode");
            helpers::ValidateCurrency(code);
            if (std::find(codes.begin(), codes.end(), code) != codes.end())
                throw std::runtime_error("Повтор валюты ЦБ");
            codes.push_back(code);
            values.push_back(Millionths(value.get<std::string>("Value")));
            if (values.back() <= 0)
                throw std::runtime_error("Нулевой курс ЦБ");
            nominals.push_back(PositiveInteger(value.get<std::string>("Nominal")));
        }
        if (codes.size() < 2)
            throw std::runtime_error("Пустой список курсов ЦБ");
        auto transaction = pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        transaction.Execute(sql::kCreateExchangeDay, date, effective);
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

Rate ExchangeRates::Get(
    const std::string& from, const std::string& to, userver::utils::datetime::Date date) const {
    if (!platform::IsCalendarDateInRange(
            date, std::chrono::year{1900} / 1 / 1, std::chrono::year{9999} / 12 / 31))
        throw ApiError<400>{"invalid_rate_date", "Дата курса должна быть не раньше 01.01.1900"};
    helpers::ValidateCurrency(from);
    helpers::ValidateCurrency(to);
    if (from == to)
        return {.Millionths = 1000000, .Date = date, .Source = "identity"};
    EnsureDay(date);
    const auto rows = pg_->Execute(
        pg::ClusterHostType::kMaster, sql::kGetExchangeRates, date, std::vector{from, to});
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> quotes;
    for (const auto& row : rows) {
        const auto [code, value, nominal] = row.As<std::string, std::int64_t, std::int64_t>();
        quotes.emplace(code, std::pair{value, nominal});
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
