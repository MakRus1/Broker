#include <helpers/financial_labels.hpp>
#include <helpers/registry_query.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/financial_labels/list/view.hpp>

namespace finance::views::api::v1::financial_labels::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::FINANCE_READ_FINANCIAL_LABELS);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto& body = request.body;
        const auto scope = platform::registry::CursorScope(body, organization);
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
        std::optional<models::FinancialLabelRegistryCursor> cursor;
        if (more)
            cursor = models::FinancialLabelRegistryCursor{
                .id = boost::uuids::to_string(rows.back().Label.Id),
                .scope = scope,
                .value = rows.back().SortKey};
        transaction.Commit();
        return Response200{.items = Convert(rows), .next_cursor = cursor};
    } catch (const platform::registry::FilterError& error) {
        return Response400{.error = "invalid_filter", .message = error.what()};
    } catch (const ::finance::ApiError& error) {
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
}  // namespace finance::views::api::v1::financial_labels::list
