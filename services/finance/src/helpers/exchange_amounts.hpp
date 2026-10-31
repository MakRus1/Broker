#pragma once
#include <boost/multiprecision/cpp_int.hpp>
#include <limits>
#include <models/errors.hpp>

namespace finance::helpers {
using WideAmount = boost::multiprecision::cpp_int;

inline WideAmount ConvertAmount(const WideAmount& amount, std::int64_t millionths) {
    if (amount < 0 || millionths <= 0 || millionths > 1000000000000LL)
        throw ApiError<400>{
            "invalid_exchange_rate", "Укажите положительный курс не больше 1 000 000"};
    return (amount * millionths + 500000) / 1000000;
}

inline std::int64_t ConvertAmount(std::int64_t amount, std::int64_t millionths) {
    const auto result = ConvertAmount(WideAmount{amount}, millionths);
    if (result > 9000000000000000LL)
        throw ApiError<400>{"amount_overflow", "Сумма после пересчёта слишком велика"};
    return result.convert_to<std::int64_t>();
}

inline void ValidateCurrency(const std::string& value) {
    if (value.size() != 3 ||
        value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") != std::string::npos)
        throw ApiError<400>{"invalid_currency", "Выберите трёхбуквенный код валюты"};
}
}  // namespace finance::helpers
