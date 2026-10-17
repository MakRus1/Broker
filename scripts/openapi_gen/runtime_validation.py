"""Общие C++ проверки строк, форматов, массивов и числовых ограничений."""
CODE = r'''
namespace broker_codegen {
inline std::size_t Utf8Length(const std::string& text) {
    std::size_t length = 0;
    for (std::size_t i = 0; i < text.size(); ++length) {
        const auto first = static_cast<unsigned char>(text[i]);
        const std::size_t count = first < 0x80 ? 1 : first >= 0xc2 && first <= 0xdf ? 2 :
            first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf4 ? 4 : 0;
        if (!count || i + count > text.size()) throw std::invalid_argument("UTF-8");
        for (std::size_t n = 1; n < count; ++n) {
            const auto byte = static_cast<unsigned char>(text[i+n]);
            if (byte < 0x80 || byte > 0xbf) throw std::invalid_argument("UTF-8");
        }
        if (count > 1) {
            const auto second = static_cast<unsigned char>(text[i+1]);
            if ((first == 0xe0 && second < 0xa0) || (first == 0xed && second >= 0xa0) ||
                (first == 0xf0 && second < 0x90) || (first == 0xf4 && second >= 0x90))
                throw std::invalid_argument("UTF-8");
        }
        i += count;
    }
    return length;
}
inline bool ValidDate(const std::string& value) {
    if (!userver::utils::regex_match(value, userver::utils::regex(R"([0-9]{4}-[0-9]{2}-[0-9]{2})"))) return false;
    const int year = std::stoi(value.substr(0, 4));
    return year > 0 && std::chrono::year_month_day{std::chrono::year{year},
        std::chrono::month{static_cast<unsigned>(std::stoi(value.substr(5, 2)))},
        std::chrono::day{static_cast<unsigned>(std::stoi(value.substr(8, 2)))}}.ok();
}
inline void CheckFormat(const std::string& value, const std::string& format) {
    bool valid = true;
    if (format == "uuid") valid = userver::utils::regex_match(value,
        userver::utils::regex(R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})"));
    else if (format == "date") valid = ValidDate(value);
    else if (format == "date-time") {
        valid = userver::utils::regex_match(value, userver::utils::regex(
            R"([0-9]{4}-[0-9]{2}-[0-9]{2}[Tt][0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]+)?([Zz]|[+-][0-9]{2}:[0-9]{2}))"));
        if (valid) {
            valid = ValidDate(value.substr(0, 10)) && std::stoi(value.substr(11, 2)) < 24 &&
                std::stoi(value.substr(14, 2)) < 60 && std::stoi(value.substr(17, 2)) < 60;
            if (value.back() != 'Z' && value.back() != 'z') {
                valid = valid && std::stoi(value.substr(value.size()-5, 2)) < 24 &&
                    std::stoi(value.substr(value.size()-2, 2)) < 60;
            }
        }
    } else if (format == "byte") valid = userver::utils::regex_match(value,
        userver::utils::regex(R"(([A-Za-z0-9+/]{4})*([A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?)"));
    if (!valid) throw std::invalid_argument("format");
}
inline void CheckMultiple(long double value, long double step, long double tolerance) {
    const auto remainder = std::abs(std::fmod(value, step));
    if (remainder > step * tolerance && std::abs(remainder-step) > step * tolerance)
        throw std::invalid_argument("multipleOf");
}
template <typename T> void RequireUnique(const std::vector<T>& values) {
    std::set<T> unique(values.begin(), values.end());
    if (unique.size() != values.size()) throw std::invalid_argument("uniqueItems");
}
}  // namespace broker_codegen
'''
