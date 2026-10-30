#include <helpers/financial_entries.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/financial_entries/list/view.hpp>

namespace finance::views::api::v1::financial_entries::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::FINANCE_READ_ENTRIES);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto& scope = request.body.scope;
        ValidateEntryScope(deps, context, scope);
        auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster,
            pg::TransactionOptions{
                pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
        const auto rows = transaction.Execute(sql::kListFinancialEntries, organization,
            scope.shipment_id, scope.transportation_id, request.body.after,
            request.body.include_archived, request.body.limit + 1);
        Response200 response;
        response.items = ConvertEntries(rows);
        if (response.items.size() > static_cast<std::size_t>(request.body.limit)) {
            response.items.pop_back();
            response.next_after = response.items.back().id;
        }
        const auto totals = transaction.Execute(
            sql::kFinancialEntryTotals, organization, scope.shipment_id, scope.transportation_id);
        for (const auto& row : totals) {
            const auto [group, currency, income, expense] =
                row.As<std::int32_t, std::string, std::string, std::string>();
            models::CurrencyTotal total{
                .currency = currency, .expense_minor = expense, .income_minor = income};
            if (group == 0)
                response.summary.own.push_back(total);
            else if (group == 1)
                response.summary.transportations.push_back(total);
            else
                response.summary.combined.push_back(total);
        }
        transaction.Commit();
        return response;
    } catch (const ::finance::ApiError<400>& error) {
        return Response400{.error = error.Code, .message = error.what()};
    } catch (const ::finance::ApiError<403>& error) {
        return Response403{.error = error.Code, .message = error.what()};
    } catch (const ::finance::ApiError<404>& error) {
        return Response404{.error = error.Code, .message = error.what()};
    } catch (const ::finance::ApiError<502>& error) {
        return Response502{.error = error.Code, .message = error.what()};
    }
}
}  // namespace finance::views::api::v1::financial_entries::list
