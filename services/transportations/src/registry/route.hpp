#pragma once
#include <algorithm>
#include <openapi/models.hpp>
#include <platform/registry_filters.hpp>
#include <transportations/sql_queries.hpp>

namespace transportations::registry_query::route_registry {
namespace registry = platform::registry;
namespace pg = userver::storages::postgres;

// Фильтры участка используют те же операторы, что поля основного реестра.
inline constexpr std::array<registry::RegistryField, 7> kFields{{
    {.Field = registry::Field<boost::uuids::uuid>{"mode", "shared.mode COLLATE \"C\"", "TEXT"},
        .ValueSql = "shared.mode",
        .Sortable = false},
    {.Field = registry::Field<boost::uuids::uuid>{"origin_station",
         "(local.document #>> '{rail,origin_station,id}')::UUID", "UUID"},
        .ValueSql = "concat_ws(' · ', local.document #>> '{rail,origin_station,name}', "
                    "local.document #>> '{rail,origin_station,esr_code}')",
        .Sortable = false,
        .Lookup = "route_locations"},
    {.Field = registry::Field<boost::uuids::uuid>{"destination_station",
         "(local.document #>> '{rail,destination_station,id}')::UUID", "UUID"},
        .ValueSql = "concat_ws(' · ', local.document #>> '{rail,destination_station,name}', "
                    "local.document #>> '{rail,destination_station,esr_code}')",
        .Sortable = false,
        .Lookup = "route_locations"},
    {.Field = registry::Field<boost::uuids::uuid>{"pol",
         "(shared.document #>> '{sea,pol,id}')::UUID", "UUID"},
        .ValueSql = "concat_ws(' · ', shared.document #>> '{sea,pol,name}', shared.document #>> "
                    "'{sea,pol,code}')",
        .Sortable = false,
        .Lookup = "route_locations"},
    {.Field = registry::Field<boost::uuids::uuid>{"pod",
         "(shared.document #>> '{sea,pod,id}')::UUID", "UUID"},
        .ValueSql = "concat_ws(' · ', shared.document #>> '{sea,pod,name}', shared.document #>> "
                    "'{sea,pod,code}')",
        .Sortable = false,
        .Lookup = "route_locations"},
    {.Field = registry::Field<boost::uuids::uuid>{"origin_airport",
         "(shared.document #>> '{air,origin_airport,id}')::UUID", "UUID"},
        .ValueSql = "concat_ws(' · ', shared.document #>> '{air,origin_airport,name}', "
                    "shared.document #>> '{air,origin_airport,code}')",
        .Sortable = false,
        .Lookup = "route_locations"},
    {.Field = registry::Field<boost::uuids::uuid>{"destination_airport",
         "(shared.document #>> '{air,destination_airport,id}')::UUID", "UUID"},
        .ValueSql = "concat_ws(' · ', shared.document #>> '{air,destination_airport,name}', "
                    "shared.document #>> '{air,destination_airport,code}')",
        .Sortable = false,
        .Lookup = "route_locations"},
}};

inline const pg::Query& Relation() {
    static const pg::Query relation{
        fmt::format(fmt::runtime(sql::kRegistryRouteRows.GetStatementView()),
            fmt::arg("route_join",
                fmt::format(fmt::runtime(sql::kRegistryRouteJoin.GetStatementView()),
                    fmt::arg("owner", "route_owner"))),
            fmt::arg("where", "{where}"))};
    return relation;
}

// Индекс отбирает кандидатов; EXISTS ниже сохраняет условия на один и тот же участок.
template <typename Filters>
void ApplyStationCandidates(registry::QueryBuilder& query, const Filters& filters,
    const std::optional<std::string>& exclude = std::nullopt) {
    for (const auto& filter : filters) {
        if ((filter.field != "origin_station" && filter.field != "destination_station") ||
            (exclude && filter.field == *exclude) || !filter.values || filter.values->empty() ||
            std::any_of(filter.values->begin(), filter.values->end(),
                [](const auto& value) { return value.empty(); }))
            continue;
        const auto expression =
            "public.transportation_route_station_ids(transportations.route_segments, '" +
            filter.field + "')";
        query.WhereValues(registry::Field<boost::uuids::uuid>{filter.field, expression, "UUID"},
            *filter.values, false, true);
    }
}

template <typename Groups>
void ApplyGroups(registry::QueryBuilder& query, const Groups& groups,
    const std::optional<int>& skip_group = std::nullopt) {
    if (!groups.has_value())
        return;
    for (std::size_t index = 0; index < groups.value().size(); ++index) {
        if (skip_group.has_value() && static_cast<int>(index) == skip_group.value())
            continue;
        query.WhereExists(Relation(), [&](auto& nested) {
            registry::ApplyFilters(nested, kFields, std::optional{groups.value()[index].filters});
        });
        ApplyStationCandidates(query, groups.value()[index].filters);
    }
}

template <typename ApplyMain>
inline registry::BuiltQuery BuildValues(const models::ListTransportationsRequest& request,
    const boost::uuids::uuid& organization, const std::string& field, int group,
    const std::optional<std::string>& search, const std::optional<std::string>& after, int limit,
    ApplyMain apply_main) {
    const auto& definition = registry::FindField(kFields, field);
    registry::QueryBuilder query{sql::kListTransportations,
        registry::Order{.Expression = "id", .ById = true, .PgType = "UUID"}, false};
    query.WhereEqual(registry::Field<boost::uuids::uuid>{"organization_id",
                         "transportations.organization_id", "UUID"},
        organization);
    apply_main(query);
    ApplyGroups(query, request.route_filters, group);
    if (request.route_filters.has_value() &&
        group < static_cast<int>(request.route_filters->size()))
        registry::ApplyFilters(
            query, kFields, std::optional{request.route_filters.value()[group].filters}, field);
    if (definition.Field.PgType == "UUID")
        query.WhereEmpty(definition.Field, false);
    const auto join = fmt::format(fmt::runtime(sql::kRegistryRouteJoin.GetStatementView()),
        fmt::arg("owner", "transportations"));
    const pg::Query selection{
        fmt::format(fmt::runtime(sql::kRegistryFilterValues.GetStatementView()),
            fmt::arg("key", "{key}"), fmt::arg("label", "{label}"), fmt::arg("facet_join", join),
            fmt::arg("where", "{where}"), fmt::arg("limit", "{limit}"))};
    return registry::BuildValues(std::move(query), selection, definition, search, after, limit + 1);
}
}  // namespace transportations::registry_query::route_registry
