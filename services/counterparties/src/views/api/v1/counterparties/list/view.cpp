#include <helpers/details.hpp>
#include <helpers/registry_query.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/counterparties/list/view.hpp>

namespace counterparties::views::api::v1::counterparties::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_READ);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto& body = request.body;
        const auto scope = platform::registry::CursorScope(body, organization);
        if (body.after.has_value() &&
            (body.cursor.has_value() || body.sort != models::ListCounterpartiesRequest_sort::Id ||
                body.descending))
            throw ApiError{400, "invalid_cursor", "Для выбранной сортировки используйте cursor"};
        if (body.cursor.has_value() && body.cursor.value().scope != scope)
            throw ApiError{
                400, "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        const auto query = BuildRegistryQuery(body, organization);
        // Транзакция ограничивает SET LOCAL; HTTP-вызовов и записи внутри нет.
        auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster,
            pg::TransactionOptions{
                pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
        transaction.Execute(sql::kRegistryPlan);
        auto rows = transaction.Execute(query.Statement, query.Parameters)
                        .AsContainer<std::vector<RegistryRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(body.limit);
        if (more)
            rows.pop_back();
        std::optional<models::CounterpartyRegistryCursor> cursor;
        if (more)
            cursor = models::CounterpartyRegistryCursor{
                .id = boost::uuids::to_string(rows.back().Counterparty.Id),
                .scope = scope,
                .value = rows.back().SortKey};
        transaction.Commit();
        return Response200{.items = Convert(rows), .next_cursor = cursor};
    } catch (const ::counterparties::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        case 409:
            return Response409{.error = error.Code, .message = error.what()};
        default:
            throw;
        }
    }
}
}  // namespace counterparties::views::api::v1::counterparties::list
