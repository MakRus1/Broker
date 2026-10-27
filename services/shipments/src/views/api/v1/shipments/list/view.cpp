#include <helpers/database.hpp>
#include <helpers/registry.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/shipments/list/view.hpp>

namespace shipments::views::api::v1::shipments::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::SHIPMENTS_READ);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto& body = request.body;
        const auto scope = platform::registry::CursorScope(body, organization);
        const bool date_sort = body.sort == models::ListShipmentsRequest_sort::ShipmentDate;
        if (body.date_from.has_value() && body.date_to.has_value() &&
            body.date_from.value() > body.date_to.value())
            throw ApiError{400, "invalid_date_range", "Начало периода позже окончания"};
        if (body.after.has_value() &&
            (body.cursor.has_value() || body.sort != models::ListShipmentsRequest_sort::Id ||
                body.descending))
            throw ApiError{400, "invalid_cursor", "Для выбранной сортировки используйте cursor"};
        if (body.cursor.has_value()) {
            const auto& cursor = body.cursor.value();
            if (cursor.scope != scope || date_sort != cursor.date_value.has_value() ||
                date_sort == cursor.text_value.has_value())
                throw ApiError{
                    400, "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        }
        const auto after =
            body.cursor.has_value() ? std::optional{body.cursor.value().id} : body.after;
        const auto text =
            body.cursor.has_value() ? body.cursor.value().text_value.value_or("") : "";
        const auto date = body.cursor.has_value() ? body.cursor.value().date_value : std::nullopt;
        // Транзакция ограничивает SET LOCAL; HTTP-вызовов и записи внутри нет.
        auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
        transaction.Execute(sql::kRegistryPlan);
        auto rows =
            transaction
                .Execute(sql::kListShipments, organization,
                    platform::registry::SearchPattern(body.number_query),
                    platform::registry::SearchPattern(body.cargo_query), body.customer_id,
                    body.statuses, body.date_from, body.date_to, models::EnumValue(body.sort),
                    body.descending, text, date, after, body.limit + 1)
                .AsContainer<std::vector<RegistryRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(body.limit);
        if (more)
            rows.pop_back();
        std::optional<models::ShipmentRegistryCursor> cursor;
        if (more) {
            const auto& last = rows.back();
            cursor = models::ShipmentRegistryCursor{
                .date_value = date_sort ? last.DateKey : std::nullopt,
                .id = boost::uuids::to_string(last.Shipment.Id),
                .scope = scope,
                .text_value = date_sort ? std::nullopt : std::optional{last.TextKey}};
        }
        transaction.Commit();
        return Response200{.items = Convert(rows), .next_cursor = cursor};
    } catch (const ::shipments::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        default:
            return Response409{.error = error.Code, .message = error.what()};
        }
    }
}
}  // namespace shipments::views::api::v1::shipments::list
