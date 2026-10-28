#pragma once
#include <string_view>

namespace transport_locations::helpers {
inline constexpr std::string_view kRailwayQuery = R"sparql(
PREFIX osmkey: <https://www.openstreetmap.org/wiki/Key:>
PREFIX ogc: <http://www.opengis.net/rdf#>
PREFIX xsd: <http://www.w3.org/2001/XMLSchema#>
SELECT DISTINCT ?station ?name ?country_code ?kind ?uic ?esr_hint ?name_ru ?name_en
WHERE {
    VALUES ?kind { "station" "yard" }
    ?station osmkey:railway ?kind ; osmkey:name ?name .
    FILTER NOT EXISTS {
        ?station osmkey:station ?excluded .
        VALUES ?excluded { "subway" "light_rail" "monorail" "funicular" }
    }
    FILTER NOT EXISTS { ?station osmkey:subway "yes" }
    ?country osmkey:ISO3166-1 ?country_code ;
        osmkey:boundary "administrative" ;
        osmkey:admin_level "2"^^xsd:int ;
        ogc:sfContains ?station .
    FILTER(REGEX(?country_code, "^[A-Z]{2}$"))
    OPTIONAL { ?station osmkey:uic_ref ?uic }
    OPTIONAL { ?station <https://www.openstreetmap.org/wiki/Key:esr:user> ?esr_hint }
    OPTIONAL { ?station <https://www.openstreetmap.org/wiki/Key:name:ru> ?name_ru }
    OPTIONAL { ?station <https://www.openstreetmap.org/wiki/Key:name:en> ?name_en }
}
LIMIT 200000
)sparql";
}  // namespace transport_locations::helpers
