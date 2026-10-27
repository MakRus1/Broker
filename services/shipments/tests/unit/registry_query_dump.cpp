// Печать реального SQL для изолированного EXPLAIN, не часть обычной сборки.
#include <helpers/registry_query.hpp>
#include <iostream>
#include <iterator>
#include <userver/formats/json.hpp>

int main() {
    const std::string input{std::istreambuf_iterator<char>{std::cin}, {}};
    const auto request =
        shipments::models::ParseModelJson(userver::formats::json::FromString(input),
            std::type_identity<shipments::models::ListShipmentsRequest>{});
    const auto query =
        shipments::helpers::BuildRegistryQuery(request, "00000000-0000-0000-0000-000000000001");
    std::cout << query.Statement.GetStatementView();
}
