#pragma once
#include <algorithm>
#include <array>
#include <boost/uuid/name_generator_sha1.hpp>
#include <boost/uuid/string_generator.hpp>
#include <helpers/csv.hpp>
#include <map>
#include <postgresql/types.hpp>
#include <set>
#include <userver/formats/json.hpp>

namespace transport_locations::helpers {
inline bool OfficialLocodeRelease(std::string_view value) {
    return value.size() == 6 && value.starts_with("20") && value[2] >= '0' && value[2] <= '9' &&
        value[3] >= '0' && value[3] <= '9' && value[4] == '-' &&
        (value[5] == '1' || value[5] == '2');
}

inline std::string LatestLocodeRelease(std::string_view body) {
    if (body.size() > 2 * 1024 * 1024)
        throw std::runtime_error("UN/LOCODE: слишком большой список выпусков");
    const auto tags = userver::formats::json::FromString(body);
    if (!tags.IsArray())
        throw std::runtime_error("UN/LOCODE: ожидался список выпусков");
    std::string latest;
    for (const auto& tag : tags) {
        const auto name = tag["name"].As<std::string>();
        if (OfficialLocodeRelease(name) && name > latest)
            latest = name;
    }
    if (latest.empty())
        throw std::runtime_error("UN/LOCODE: не найден официальный выпуск");
    return latest;
}

inline bool ConfirmedPortStatus(std::string_view status) {
    constexpr std::array statuses{"AM", "RL", "AA", "AC", "AF", "AI", "AS", "AQ", "RN"};
    return std::find(statuses.begin(), statuses.end(), status) != statuses.end();
}

inline int LocodeDate(std::string_view value, int release_year) {
    if (value.empty())
        return 0;
    if (value.size() != 4 ||
        !std::all_of(value.begin(), value.end(), [](char ch) { return ch >= '0' && ch <= '9'; }))
        throw std::runtime_error("UN/LOCODE: некорректная дата записи");
    int year = release_year / 100 * 100 + (value[0] - '0') * 10 + value[1] - '0';
    if (year > release_year)
        year -= 100;
    const auto month = (value[2] - '0') * 10 + value[3] - '0';
    if (month < 1 || month > 12)
        throw std::runtime_error("UN/LOCODE: некорректный месяц записи");
    return year * 100 + month;
}

inline std::vector<pgtypes::SeaPortSourceV1> ParseSeaPorts(
    const std::array<std::string, 3>& parts, std::size_t minimum, int release_year) {
    struct Port {
        std::string name;
        std::set<std::string> aliases;
        int date = 0;
        bool renamed = false;
    };

    std::map<std::string, Port> ports;
    std::size_t total = 0;
    std::size_t rows = 0;
    for (const auto& part : parts) {
        total += part.size();
        if (part.empty() || total > 32 * 1024 * 1024)
            throw std::runtime_error("UN/LOCODE: отсутствует часть или превышен размер выгрузки");
        std::string_view csv = part;
        if (csv.starts_with("\xEF\xBB\xBF"))
            csv.remove_prefix(3);
        std::size_t offset = 0;
        while (offset < csv.size()) {
            const auto row = ReadCsvRow(csv, offset);
            if (++rows > 200000 || row.size() != 12)
                throw std::runtime_error("UN/LOCODE: некорректная структура CSV");
            // Формат официального выпуска: Change, Country, Location, Name,
            // NameWoDiacritics, Subdivision, Function, Status, Date, IATA, Coordinates, Remarks.
            if (row[2].empty() || row[0] == "=" || row[0] == "X" || row[6].size() != 8 ||
                row[6][0] != '1' || !ConfirmedPortStatus(row[7]))
                continue;
            const auto code = row[1] + row[2];
            if (row[1].size() != 2 || row[2].size() != 3 ||
                !std::all_of(
                    row[1].begin(), row[1].end(), [](char ch) { return ch >= 'A' && ch <= 'Z'; }) ||
                !std::all_of(row[2].begin(), row[2].end(),
                    [](char ch) { return (ch >= 'A' && ch <= 'Z') || (ch >= '2' && ch <= '9'); }) ||
                row[3].empty() || row[3].size() > 1024)
                throw std::runtime_error("UN/LOCODE: некорректный код или название порта");
            const auto date = LocodeDate(row[8], release_year);
            const bool renamed = row[0] == "#";
            auto& port = ports[code];
            // Повторы LOCODE допустимы: новое название выбираем по дате/пометке,
            // остальные названия оставляем для поиска. Порядок CSV не влияет на результат.
            if (port.name.empty() ||
                std::tuple{date, renamed, row[3]} >
                    std::tuple{port.date, port.renamed, port.name}) {
                port.name = row[3];
                port.date = date;
                port.renamed = renamed;
            }
            port.aliases.insert(row[3]);
            if (!row[4].empty())
                port.aliases.insert(row[4]);
        }
    }
    if (ports.size() < minimum)
        throw std::runtime_error("UN/LOCODE: выгрузка подозрительно мала");
    const boost::uuids::name_generator_sha1 uuid{
        boost::uuids::string_generator{}("c4e81618-176c-4096-8b4b-c128b00cb913")};
    std::vector<pgtypes::SeaPortSourceV1> result;
    result.reserve(ports.size());
    for (const auto& [code, port] : ports) {
        std::string search = code + " ";
        for (const auto& alias : port.aliases)
            search += alias + " ";
        result.push_back({.Locode = code,
            .LocationId = uuid(code),
            .Name = port.name,
            .CountryCode = code.substr(0, 2),
            .SearchTerms = std::move(search)});
    }
    return result;
}
}  // namespace transport_locations::helpers
