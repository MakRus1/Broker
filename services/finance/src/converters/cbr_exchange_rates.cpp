#include <algorithm>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <charconv>
#include <converters/cbr_exchange_rates.hpp>
#include <helpers/exchange_amounts.hpp>
#include <sstream>
#include <stdexcept>
#include <userver/utils/datetime/date.hpp>

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

namespace finance::converters {
models::CbrExchangeRates ParseCbrExchangeRates(
    const std::string& body, userver::utils::datetime::Date requested_date) {
    if (body.size() > 1024 * 1024 || body.find("<!DOCTYPE") != std::string::npos)
        throw std::runtime_error("Недопустимый ответ ЦБ");
    boost::property_tree::ptree xml;
    std::istringstream stream{body};
    boost::property_tree::read_xml(stream, xml);
    const auto& root = xml.get_child("ValCurs");
    const auto text = root.get<std::string>("<xmlattr>.Date");
    if (text.size() != 10 || text[2] != '.' || text[5] != '.')
        throw std::runtime_error("Некорректная дата ЦБ");
    // Только внешний формат ЦБ преобразуется в календарную дату здесь.
    const auto effective = userver::utils::datetime::DateFromRFC3339String(
        text.substr(6, 4) + "-" + text.substr(3, 2) + "-" + text.substr(0, 2));
    if (effective > requested_date)
        throw std::runtime_error("Курс ЦБ относится к будущей дате");
    models::CbrExchangeRates result{.EffectiveDate = effective,
        .Quotes = {{.Currency = "RUB", .Value = 1000000, .Nominal = 1}}};
    for (const auto& [name, value] : root) {
        if (name != "Valute")
            continue;
        const auto code = value.get<std::string>("CharCode");
        helpers::ValidateCurrency(code);
        if (std::any_of(result.Quotes.begin(), result.Quotes.end(),
                [&](const auto& quote) { return quote.Currency == code; }))
            throw std::runtime_error("Повтор валюты ЦБ");
        const auto amount = Millionths(value.get<std::string>("Value"));
        if (amount <= 0)
            throw std::runtime_error("Нулевой курс ЦБ");
        result.Quotes.push_back({.Currency = code,
            .Value = amount,
            .Nominal = PositiveInteger(value.get<std::string>("Nominal"))});
    }
    if (result.Quotes.size() < 2)
        throw std::runtime_error("Пустой список курсов ЦБ");
    return result;
}
}  // namespace finance::converters
