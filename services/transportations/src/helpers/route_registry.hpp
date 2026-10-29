#pragma once
#include <platform/registry_filters.hpp>
#include <transportations/sql_queries.hpp>

namespace transportations::helpers::route_registry {
namespace registry = platform::registry;

// Фильтры участка используют те же операторы, что поля основного реестра.
inline constexpr std::array<registry::RegistryField, 7> kFields{{
    {.Field = registry::Field<std::string>{"mode", "shared.mode COLLATE \"C\"", "TEXT"},
        .Label = "Тип участка",
        .ValueSql = "shared.mode",
        .Sortable = false,
        .ValuesFrom = "shipment_route_segments AS shared"},
    {.Field = registry::Field<std::string>{"origin_station",
         "(local.document #>> '{rail,origin_station,id}')::UUID", "UUID"},
        .Label = "Станция отправления",
        .ValueSql = "concat_ws(' · ', local.document #>> '{rail,origin_station,name}', "
                    "local.document #>> '{rail,origin_station,esr_code}')",
        .Sortable = false,
        .Lookup = "route_locations",
        .ValuesFrom = "route_segments AS local"},
    {.Field = registry::Field<std::string>{"destination_station",
         "(local.document #>> '{rail,destination_station,id}')::UUID", "UUID"},
        .Label = "Станция назначения",
        .ValueSql = "concat_ws(' · ', local.document #>> '{rail,destination_station,name}', "
                    "local.document #>> '{rail,destination_station,esr_code}')",
        .Sortable = false,
        .Lookup = "route_locations",
        .ValuesFrom = "route_segments AS local"},
    {.Field = registry::Field<std::string>{"pol", "(shared.document #>> '{sea,pol,id}')::UUID",
         "UUID"},
        .Label = "Порт отправления",
        .ValueSql = "concat_ws(' · ', shared.document #>> '{sea,pol,name}', shared.document #>> "
                    "'{sea,pol,code}')",
        .Sortable = false,
        .Lookup = "route_locations",
        .ValuesFrom = "shipment_route_segments AS shared"},
    {.Field = registry::Field<std::string>{"pod", "(shared.document #>> '{sea,pod,id}')::UUID",
         "UUID"},
        .Label = "Порт назначения",
        .ValueSql = "concat_ws(' · ', shared.document #>> '{sea,pod,name}', shared.document #>> "
                    "'{sea,pod,code}')",
        .Sortable = false,
        .Lookup = "route_locations",
        .ValuesFrom = "shipment_route_segments AS shared"},
    {.Field = registry::Field<std::string>{"origin_airport",
         "(shared.document #>> '{air,origin_airport,id}')::UUID", "UUID"},
        .Label = "Аэропорт отправления",
        .ValueSql = "concat_ws(' · ', shared.document #>> '{air,origin_airport,name}', "
                    "shared.document #>> '{air,origin_airport,code}')",
        .Sortable = false,
        .Lookup = "route_locations",
        .ValuesFrom = "shipment_route_segments AS shared"},
    {.Field = registry::Field<std::string>{"destination_airport",
         "(shared.document #>> '{air,destination_airport,id}')::UUID", "UUID"},
        .Label = "Аэропорт назначения",
        .ValueSql = "concat_ws(' · ', shared.document #>> '{air,destination_airport,name}', "
                    "shared.document #>> '{air,destination_airport,code}')",
        .Sortable = false,
        .Lookup = "route_locations",
        .ValuesFrom = "shipment_route_segments AS shared"},
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
    }
}

template <typename ApplyMain>
inline registry::BuiltQuery BuildValues(const models::ListTransportationsRequest& request,
    const std::string& organization, const std::string& field, int group,
    const std::optional<std::string>& search, const std::optional<std::string>& after, int limit,
    ApplyMain apply_main) {
    const auto& definition = registry::FindField(kFields, field);
    registry::QueryBuilder query{sql::kListTransportations,
        registry::Order{.Expression = "id", .ById = true, .PgType = "UUID"}, false};
    query.WhereEqual(
        registry::Field<std::string>{"organization_id", "candidate.organization_id", "UUID"},
        organization);
    const auto join = fmt::format(fmt::runtime(sql::kRegistryRouteJoin.GetStatementView()),
        fmt::arg("owner", "transportations"));
    const pg::Query match{
        fmt::format(fmt::runtime(sql::kRegistryRouteCandidateMatch.GetStatementView()),
            fmt::arg("route_join", join), fmt::arg("key", definition.Field.Expression),
            fmt::arg("where", "{where}"))};
    query.WhereExists(match, [&](auto& nested) {
        apply_main(nested);
        ApplyGroups(nested, request.route_filters, group);
        if (request.route_filters.has_value() &&
            group < static_cast<int>(request.route_filters.value().size()))
            registry::ApplyFilters(nested, kFields,
                std::optional{request.route_filters.value()[group].filters}, field);
    });
    auto candidate = definition;
    candidate.Field.Expression = "candidate.value";
    candidate.Field.SortExpression = {};
    candidate.ValueSql = "candidate.label";
    if (candidate.Field.PgType == "UUID")
        query.WhereEmpty(candidate.Field, false);
    const auto source = fmt::format(fmt::runtime(sql::kRegistryRouteCandidates.GetStatementView()),
        fmt::arg("key", definition.Field.Expression), fmt::arg("label", definition.ValueSql),
        fmt::arg("value_source", definition.ValuesFrom));
    return registry::BuildValues(std::move(query), sql::kRegistryRouteFilterValues, candidate,
        search, after, limit + 1, source);
}
}  // namespace transportations::helpers::route_registry
