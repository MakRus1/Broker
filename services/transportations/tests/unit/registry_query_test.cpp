#include <helpers/registry_query.hpp>
#include <platform/calendar_date.hpp>
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
    const std::array<registry::FieldDefinition, 1> fields{ownership};
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

UTEST(RegistryQuery, DateRangeAndCursorRemainNativeDate) {
    using Date = userver::utils::datetime::Date;
    constexpr registry::Field<Date> date{"shipment_date", "shipment_date", "DATE"};
    const std::array<registry::FieldDefinition, 1> fields{date};
    const pg::Query selection{"SELECT {sort_key}, {date_key} FROM shipments WHERE {where} ORDER BY "
                              "{order_by} LIMIT {limit}"};
    registry::QueryBuilder builder{selection, registry::OrderBy(fields, "shipment_date"), true};
    builder.WhereEqual(registry_fields::kOrganization, kOrganization);
    builder.WhereAtLeast(date, std::optional{Date{2026, 1, 1}});
    builder.WhereAtMost(date, std::optional{Date{2026, 12, 31}});
    builder.After(std::optional{kOrganization}, Date{2026, 10, 27});
    const auto query = std::move(builder).Build(51);
    const auto sql = Statement(query);
    EXPECT_NE(sql.find("shipment_date >= $2::DATE"), std::string::npos);
    EXPECT_NE(sql.find("shipment_date <= $3::DATE"), std::string::npos);
    EXPECT_NE(sql.find("(shipment_date, id) < ($4::DATE, $5::UUID)"), std::string::npos);
    EXPECT_NE(sql.find("ORDER BY shipment_date DESC, id DESC"), std::string::npos);
    EXPECT_NE(sql.find("SELECT '' COLLATE \"C\", shipment_date"), std::string::npos);
}

UTEST(RegistryQuery, FilterAndOrderCanUseDifferentRepresentationsOfSameField) {
    constexpr auto archived = registry::Field<bool>{"archived", "archived", "BOOLEAN"}.SortedAs(
        "CASE WHEN archived THEN '1' ELSE '0' END");
    const std::array<registry::FieldDefinition, 1> fields{archived};
    registry::QueryBuilder builder{
        sql::kListTransportations, registry::OrderBy(fields, "archived"), false};
    builder.WhereEqual(archived, false);
    const auto sql = Statement(std::move(builder).Build(51));
    EXPECT_NE(sql.find("WHERE archived = $1::BOOLEAN"), std::string::npos);
    EXPECT_NE(sql.find("ORDER BY CASE WHEN archived THEN '1' ELSE '0' END ASC, id ASC"),
        std::string::npos);
}

UTEST(RegistryQuery, TextIdentityIsUsedForCursorAndTieBreaker) {
    constexpr registry::Field<std::string> subject{"subject", "subject", "TEXT", true};
    constexpr registry::Field<std::string> name{"name", "lower(name) COLLATE \"C\""};
    const std::array<registry::FieldDefinition, 2> fields{subject, name};
    registry::QueryBuilder builder{
        sql::kListTransportations, registry::OrderBy(fields, "name"), false, subject};
    builder.After(std::optional<std::string>{"external-subject"}, std::string{"alpha"});
    const auto sql = Statement(std::move(builder).Build(51));
    EXPECT_NE(sql.find("(lower(name) COLLATE \"C\", subject) > ($1::TEXT COLLATE \"C\", $2::TEXT)"),
        std::string::npos);
    EXPECT_NE(sql.find("ORDER BY lower(name) COLLATE \"C\" ASC, subject ASC"), std::string::npos);
    EXPECT_EQ(sql.find("::UUID"), std::string::npos);
}
}  // namespace
}  // namespace transportations::helpers
