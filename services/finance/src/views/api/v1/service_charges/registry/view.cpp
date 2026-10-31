#include <charconv>
#include <helpers/service_charge_registry.hpp>
#include <limits>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/service_charges/registry/view.hpp>

namespace finance::views::api::v1::service_charges::registry {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    using namespace ::finance::helpers::service_charge_registry;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::FINANCE_READ_SERVICE_CHARGES);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto& body = request.body;
        const auto scope = platform::registry::CursorScope(body, organization);
        const auto order = platform::registry::RegistryOrderBy(fields::kFields, body.sort);
        const bool date_sort = order.CursorType == platform::registry::CursorKeyType::Date;
        if (body.cursor.has_value()) {
            const auto& cursor = body.cursor.value();
            if (cursor.scope != scope || date_sort != cursor.date_value.has_value() ||
                date_sort == cursor.text_value.has_value())
                throw ApiError<400>{
                    "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        }
        if (body.cursor.has_value() && (order.PgType == "BIGINT" || order.PgType == "INTEGER")) {
            const auto& text = body.cursor.value().text_value.value();
            std::int64_t number{};
            const auto [end, error] =
                std::from_chars(text.data(), text.data() + text.size(), number);
            if (error != std::errc{} || end != text.data() + text.size() || number < 0 ||
                (order.PgType == "INTEGER" && number > std::numeric_limits<int>::max()))
                throw ApiError<400>{"invalid_cursor", "Некорректное числовое значение курсора"};
        }
        const auto query = platform::registry::BuildPage(kRegistry, body, organization);
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
        std::optional<models::ServiceChargeRegistryCursor> cursor;
        if (more) {
            const auto& last = rows.back();
            cursor = models::ServiceChargeRegistryCursor{
                .date_value = date_sort ? last.DateKey : std::nullopt,
                .id = boost::uuids::to_string(last.Charge.Id),
                .scope = scope,
                .text_value = date_sort ? std::nullopt : std::optional{last.TextKey}};
        }
        Response200 response;
        for (const auto& row : rows)
            response.items.push_back(Convert(row.Charge));
        response.invoices = LoadServiceChargeInvoices(transaction, organization, response.items);
        response.next_cursor = cursor;
        transaction.Commit();
        return response;
    } catch (const platform::registry::FilterError& error) {
        return Response400{.error = "invalid_filter", .message = error.what()};
    }
}
}  // namespace finance::views::api::v1::service_charges::registry
