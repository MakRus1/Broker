#pragma once
#include <algorithm>
#include <boost/uuid/name_generator_sha1.hpp>
#include <boost/uuid/string_generator.hpp>
#include <charconv>
#include <map>
#include <postgresql/types.hpp>
#include <set>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace transport_locations::helpers {
// CSV допускает запятые, переносы строк и удвоенные кавычки внутри поля.
inline std::vector<std::string> ReadAirportCsvRow(std::string_view csv, std::size_t& offset) {
    std::vector<std::string> fields;
    do {
        std::string field;
        if (offset < csv.size() && csv[offset] == '"') {
            ++offset;
            bool closed = false;
            while (offset < csv.size()) {
                const auto ch = csv[offset++];
                if (ch == '"') {
                    if (offset < csv.size() && csv[offset] == '"')
                        ++offset;
                    else {
                        closed = true;
                        break;
                    }
                }
                field += ch;
                if (field.size() > 16384)
                    throw std::runtime_error("Слишком длинное поле OurAirports");
            }
            if (!closed)
                throw std::runtime_error("Незакрытая кавычка CSV OurAirports");
        } else {
            while (offset < csv.size() && csv[offset] != ',' && csv[offset] != '\r' &&
                csv[offset] != '\n') {
                if (csv[offset] == '"' || field.size() >= 16384)
                    throw std::runtime_error("Некорректное поле CSV OurAirports");
                field += csv[offset++];
            }
        }
        fields.push_back(std::move(field));
        if (fields.size() > 100)
            throw std::runtime_error("Слишком много колонок CSV OurAirports");
        if (offset == csv.size())
            break;
        const auto delimiter = csv[offset++];
        if (delimiter == '\n')
            break;
        if (delimiter == '\r') {
            if (offset < csv.size() && csv[offset] == '\n')
                ++offset;
            break;
        }
        if (delimiter != ',')
            throw std::runtime_error("Некорректный разделитель CSV OurAirports");
    } while (offset <= csv.size());
    return fields;
}

inline bool AirportCode(std::string_view value, std::size_t length) {
    return value.size() == length &&
        std::all_of(value.begin(), value.end(), [](char ch) { return ch >= 'A' && ch <= 'Z'; });
}

inline std::vector<pgtypes::AirportSourceV1> ParseAirports(
    std::string_view csv, std::size_t minimum) {
    if (csv.size() > 32 * 1024 * 1024)
        throw std::runtime_error("Выгрузка OurAirports превышает 32 МиБ");
    if (csv.starts_with("\xEF\xBB\xBF"))
        csv.remove_prefix(3);
    std::size_t offset = 0;
    const auto header = ReadAirportCsvRow(csv, offset);
    std::map<std::string, std::size_t> columns;
    for (std::size_t index = 0; index < header.size(); ++index) {
        if (!columns.emplace(header[index], index).second)
            throw std::runtime_error("Повтор колонки OurAirports");
    }
    for (const auto* key : {"id", "type", "name", "iso_country", "iata_code", "icao_code",
             "municipality", "keywords"}) {
        if (!columns.contains(key))
            throw std::runtime_error("Не хватает обязательных колонок OurAirports");
    }
    const boost::uuids::name_generator_sha1 uuid{
        boost::uuids::string_generator{}("bc10495e-74fb-4c93-926c-bab338185341")};
    std::set<std::int64_t> ids;
    std::set<std::pair<std::string, std::string>> codes;
    std::vector<pgtypes::AirportSourceV1> result;
    std::size_t count = 0;
    while (offset < csv.size()) {
        const auto row = ReadAirportCsvRow(csv, offset);
        if (++count > 200000 || row.size() != header.size())
            throw std::runtime_error("Некорректное количество строк или полей OurAirports");
        const auto get = [&](const char* key) -> const std::string& {
            return row[columns.at(key)];
        };
        const auto& type = get("type");
        if (type != "large_airport" && type != "medium_airport" && type != "small_airport")
            continue;
        const auto& iata = get("iata_code");
        const auto& icao = get("icao_code");
        if (iata.empty() && icao.empty())
            continue;
        const auto& name = get("name");
        const auto& country = get("iso_country");
        const auto& source = get("id");
        std::int64_t id = 0;
        const auto parsed = std::from_chars(source.data(), source.data() + source.size(), id);
        if (parsed.ec != std::errc{} || parsed.ptr != source.data() + source.size() || id <= 0 ||
            name.empty() || name.size() > 1024 || !AirportCode(country, 2) ||
            (!iata.empty() && !AirportCode(iata, 3)) || (!icao.empty() && !AirportCode(icao, 4)) ||
            !ids.insert(id).second || !codes.emplace(country, iata.empty() ? icao : iata).second)
            throw std::runtime_error("Некорректный ID, код или дубль OurAirports");
        result.push_back({.SourceId = id,
            .LocationId = uuid(source),
            .Name = name,
            .CountryCode = country,
            .IataCode = iata.empty() ? std::nullopt : std::optional{iata},
            .IcaoCode = icao.empty() ? std::nullopt : std::optional{icao},
            .SearchTerms = iata + " " + icao + " " + get("municipality") + " " + get("keywords")});
    }
    if (result.size() < minimum)
        throw std::runtime_error("Выгрузка OurAirports подозрительно мала");
    return result;
}
}  // namespace transport_locations::helpers
