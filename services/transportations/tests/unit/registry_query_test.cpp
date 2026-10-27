#include <helpers/registry_query.hpp>
#include <userver/utest/utest.hpp>

namespace transportations::helpers {
namespace {
const std::string kOrganization = "00000000-0000-0000-0000-000000000001";

std::string Statement(const registry::BuiltQuery& query) {
    return std::string{query.Statement.GetStatementView()};
}

UTEST(RegistryQuery, MissingFiltersProduceOnlyOrganizationAndLimit) {
    const auto query = BuildRegistryQuery({}, kOrganization);
    const auto sql = Statement(query);
    EXPECT_EQ(query.Parameters.Size(), 2);
    EXPECT_NE(sql.find("WHERE organization_id = $1::UUID"), std::string::npos);
    EXPECT_NE(sql.find("ORDER BY id ASC\nLIMIT $2::INTEGER"), std::string::npos);
    EXPECT_EQ(sql.find("CASE"), std::string::npos);
    EXPECT_EQ(sql.find(" IS NULL"), std::string::npos);
}

UTEST(RegistryQuery, ValuesNeverBecomeSqlText) {
    models::ListTransportationsRequest request;
    request.query = "' OR 1=1 --";
    request.hazardous = false;
    request.container_types = std::vector<std::string>{};
    const auto query = BuildRegistryQuery(request, kOrganization);
    const auto sql = Statement(query);
    EXPECT_EQ(query.Parameters.Size(), 5);
    EXPECT_EQ(sql.find(request.query.value()), std::string::npos);
    EXPECT_EQ(sql.find(kOrganization), std::string::npos);
    EXPECT_NE(sql.find("LIKE lower($2::TEXT)"), std::string::npos);
    EXPECT_NE(sql.find("COALESCE(container_type_code, '') COLLATE \"C\" = ANY($3::TEXT[])"),
        std::string::npos);
    EXPECT_NE(sql.find("hazardous = $4::BOOLEAN"), std::string::npos);
}

UTEST(RegistryQuery, DescendingCursorUsesSameExpressionAsOrdering) {
    models::ListTransportationsRequest request;
    request.sort = models::ListTransportationsRequest_sort::ContainerNumber;
    request.descending = true;
    request.cursor = models::RegistryCursor{.id = kOrganization, .scope = "scope", .value = "mscu"};
    const auto query = BuildRegistryQuery(request, kOrganization);
    const auto sql = Statement(query);
    EXPECT_EQ(query.Parameters.Size(), 4);
    EXPECT_NE(sql.find("(lower(COALESCE(container_number, '')) COLLATE \"C\", id) < "
                       "($2::TEXT COLLATE \"C\", $3::UUID)"),
        std::string::npos);
    EXPECT_NE(
        sql.find("ORDER BY lower(COALESCE(container_number, '')) COLLATE \"C\" DESC, id DESC"),
        std::string::npos);
}

UTEST(RegistryQuery, RegisteringFieldEnablesOrderingWithoutSwitch) {
    constexpr registry::Field<std::string> ownership{
        "container_ownership", "COALESCE(container_ownership, '') COLLATE \"C\""};
    const std::array fields{ownership};
    registry::QueryBuilder builder{
        sql::kListTransportations, registry::OrderBy(fields, "container_ownership"), false};
    builder.WhereEqual(registry_fields::kOrganization, kOrganization);
    builder.WhereEqual(ownership, std::optional<std::string>{"SOC"});
    const auto query = std::move(builder).Build(51);
    const auto sql = Statement(query);
    EXPECT_NE(
        sql.find("COALESCE(container_ownership, '') COLLATE \"C\" = $2::TEXT"), std::string::npos);
    EXPECT_NE(sql.find("ORDER BY COALESCE(container_ownership, '') COLLATE \"C\" ASC, id ASC"),
        std::string::npos);
    EXPECT_EQ(sql.find("WITH registry"), std::string::npos);
    EXPECT_EQ(sql.find("{where}"), std::string::npos);
}

UTEST(RegistryQuery, RejectsUnregisteredSort) {
    EXPECT_THROW(
        registry::OrderBy(registry_fields::kSortableFields, "id; DROP TABLE transportations"),
        std::logic_error);
}

UTEST(RegistryQuery, RejectsMissingOrRepeatedTemplateSlots) {
    const auto order = registry::OrderBy(registry_fields::kSortableFields, "id");
    EXPECT_THROW(
        (registry::QueryBuilder{pg::Query{"SELECT {sort_key} LIMIT {limit}"}, order, false}),
        std::logic_error);
    EXPECT_THROW(
        (registry::QueryBuilder{
            pg::Query{
                "SELECT {sort_key} WHERE {where} OR {where} ORDER BY {order_by} LIMIT {limit}"},
            order, false}),
        std::logic_error);
}
}  // namespace
}  // namespace transportations::helpers
