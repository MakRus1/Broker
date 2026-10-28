#pragma once
#include <algorithm>
#include <boost/uuid/name_generator_sha1.hpp>
#include <boost/uuid/string_generator.hpp>
#include <map>
#include <optional>
#include <postgresql/types.hpp>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <userver/formats/json/value.hpp>
#include <vector>

namespace transport_locations::helpers {
using RailwaySource = pgtypes::RailwayStationSourceV1;

inline std::optional<std::string> RailwayCode(std::string value, std::size_t length) {
    if (value.size() != length ||
        !std::all_of(value.begin(), value.end(), [](char ch) { return ch >= '0' && ch <= '9'; }) ||
        value.find_first_not_of('0') == std::string::npos)
        return std::nullopt;
    return value;
}

// OSM-объект служит ключом источника. Ни одинаковое название, ни код-подсказка
// не дают достаточного основания объединять разные объекты.
inline std::vector<RailwaySource> ParseRailwaySource(
    const userver::formats::json::Value& document, std::size_t minimum) {
    const auto rows = document["results"]["bindings"];
    if (!rows.IsArray() || rows.GetSize() >= 200000 ||
        (!document["meta"]["result-size-total"].IsMissing() &&
            document["meta"]["result-size-total"].As<std::size_t>() != rows.GetSize()))
        throw std::runtime_error("Неполная выгрузка железнодорожных станций OSM");
    const boost::uuids::name_generator_sha1 uuid{
        boost::uuids::string_generator{}("08df9f49-9f2e-4627-9e63-1cc3221194ca")};
    std::map<std::string, RailwaySource> stations;
    std::set<std::string> ambiguous;
    for (const auto& row : rows) {
        const auto get = [&](const char* key) {
            return row[key].IsMissing() ? std::string{} : row[key]["value"].As<std::string>();
        };
        const auto source = get("station");
        bool valid_source = false;
        for (const auto prefix : {"https://www.openstreetmap.org/node/",
                 "https://www.openstreetmap.org/way/", "https://www.openstreetmap.org/relation/"}) {
            if (source.starts_with(prefix)) {
                const auto id = std::string_view{source}.substr(std::string_view{prefix}.size());
                valid_source = !id.empty() && std::all_of(id.begin(), id.end(), [](char ch) {
                    return ch >= '0' && ch <= '9';
                });
            }
        }
        const auto country = get("country_code");
        const auto name = get("name");
        const auto kind = get("kind");
        if (!valid_source || name.empty() || name.size() > 1024 || country.size() != 2 ||
            !std::all_of(
                country.begin(), country.end(), [](char ch) { return ch >= 'A' && ch <= 'Z'; }) ||
            (kind != "station" && kind != "yard"))
            throw std::runtime_error("Некорректная запись железнодорожной станции OSM");
        RailwaySource station{.SourceId = source,
            .StationId = uuid(source),
            .Name = name,
            .CountryCode = country,
            .EsrCode = RailwayCode(get("esr_hint"), 6),
            .UicCode = RailwayCode(get("uic"), 7),
            .SearchTerms = get("name_ru") + " " + get("name_en")};
        const auto [existing, inserted] = stations.emplace(source, station);
        if (!inserted &&
            (existing->second.CountryCode != country || existing->second.Name != name ||
                existing->second.EsrCode != station.EsrCode ||
                existing->second.UicCode != station.UicCode))
            ambiguous.insert(source);
    }
    std::vector<RailwaySource> result;
    result.reserve(stations.size());
    for (auto& [source, station] : stations) {
        if (!ambiguous.contains(source))
            result.push_back(std::move(station));
    }
    if (result.size() < minimum)
        throw std::runtime_error("Выгрузка железнодорожных станций OSM подозрительно мала");
    return result;
}
}  // namespace transport_locations::helpers
