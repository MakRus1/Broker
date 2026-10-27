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
    EXPECT_NE(sql.find("AND organization_id = $1::UUID"), std::string::npos);
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
    EXPECT_NE(sql.find("container_type_code = ANY($3::TEXT[])"), std::string::npos);
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
}  // namespace
}  // namespace transportations::helpers
