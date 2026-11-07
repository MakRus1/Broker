#include <helpers/registry.hpp>
#include <views/api/v1/sales/inquiries/list/view.hpp>

namespace sales::views::api::v1::sales::inquiries::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    try {
        const auto& body = request.body;
        const auto organization =
            platform::auth::GetOrganization(platform::auth::GetIdentity(context));
        const auto scope = registry::CursorScope(body, organization);
        const bool date_sort =
            registry::RegistryOrderBy(kInquiriesRegistry.Fields, body.sort).CursorType ==
            registry::CursorKeyType::Date;
        if (body.cursor.has_value()) {
            const auto& cursor = body.cursor.value();
            if (cursor.scope != scope || date_sort != cursor.date_value.has_value() ||
                date_sort == cursor.text_value.has_value())
                throw ApiError<400>{
                    "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        }
        const auto query = registry::BuildPage(kInquiriesRegistry, body, organization);
        auto rows = deps.pg_cluster
                        ->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                        .AsContainer<std::vector<RegistryRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(body.limit);
        if (more)
            rows.pop_back();
        Response200 response;
        response.items.reserve(rows.size());
        for (const auto& row : rows)
            response.items.push_back(
                models::ParseModelJson(row.Document, std::type_identity<models::Inquiry>{}));
        if (more) {
            const auto& last = rows.back();
            response.next_cursor =
                models::SalesRegistryCursor{.date_value = date_sort ? last.DateKey : std::nullopt,
                    .id = response.items.back().id,
                    .scope = scope,
                    .text_value = date_sort ? std::nullopt : std::optional{last.TextKey}};
        }
        return response;
    } catch (const registry::FilterError& error) {
        return Response400{.error = "invalid_filter", .message = error.what()};
    }
}
}  // namespace sales::views::api::v1::sales::inquiries::list
