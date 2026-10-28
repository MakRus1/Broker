#include <helpers/inherited_route.hpp>
#include <helpers/registry_query.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <set>
#include <views/api/v1/transportations/list/view.hpp>

namespace transportations::views::api::v1::transportations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_READ);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto scope = CursorScope(request.body, organization);
        const auto order = registry::RegistryOrderBy(registry_fields::kFields, request.body.sort);
        if (request.body.after.has_value() &&
            (request.body.cursor.has_value() || !order.ById || request.body.descending))
            throw ApiError{400, "invalid_cursor", "Для выбранной сортировки используйте cursor"};
        if (request.body.cursor.has_value() && request.body.cursor.value().scope != scope)
            throw ApiError{400, "invalid_cursor",
                "Фильтры или сортировка изменились. Начните с первой страницы"};
        const auto query = BuildRegistryQuery(request.body, organization);
        // Перевозки и плечи страницы должны относиться к одному снимку.
        auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
        transaction.Execute(sql::kRegistryPlan);
        auto page = transaction.Execute(query.Statement, query.Parameters)
                        .AsContainer<std::vector<RegistryRow>>(pg::kRowTag);
        const bool has_more = page.size() > static_cast<std::size_t>(request.body.limit);
        if (has_more)
            page.pop_back();
        std::optional<models::RegistryCursor> next_cursor;
        if (has_more)
            next_cursor =
                models::RegistryCursor{.id = boost::uuids::to_string(page.back().Transportation.Id),
                    .scope = scope,
                    .value = page.back().SortKey};
        const auto rows = Convert(page);
        std::vector<std::string> ids;
        ids.reserve(rows.size());
        for (const auto& row : rows)
            ids.push_back(boost::uuids::to_string(row.Id));
        // Все плечи страницы читаются одним запросом, без N+1.
        const auto route_segments = ids.empty()
            ? std::vector<pgtypes::RouteSegmentV1>{}
            : transaction.Execute(sql::kListRouteSegments, organization, ids)
                  .AsContainer<std::vector<pgtypes::RouteSegmentV1>>();
        auto result = Convert(rows, route_segments);
        transaction.Commit();
        std::set<std::string> unique_shipments;
        for (const auto& item : result)
            unique_shipments.insert(item.shipment_id);
        const auto sources =
            LoadShipmentContexts(deps, context, {unique_shipments.begin(), unique_shipments.end()});
        for (auto& item : result)
            InheritRoute(item, FindShipmentContext(sources, item.shipment_id));
        return Response200{.items = result, .next_cursor = next_cursor};
    } catch (const platform::registry::FilterError& error) {
        return Response400{.error = "invalid_filter", .message = error.what()};
    } catch (const ::transportations::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        case 503:
            return Response503{.error = error.Code, .message = error.what()};
        default:
            return Response409{.error = error.Code, .message = error.what()};
        }
    }
}
}  // namespace transportations::views::api::v1::transportations::list
