// Печать реального SQL API для изолированных бенчмарков; не часть обычной сборки.
#include <helpers/registry_query.hpp>
#include <iostream>
#include <iterator>
#include <userver/formats/json.hpp>

int main() {
    const std::string input{std::istreambuf_iterator<char>{std::cin}, {}};
    const auto document = userver::formats::json::FromString(input);
    const auto& body = document.HasMember("request") ? document["request"] : document;
    const auto request = counterparties::models::ParseModelJson(
        body, std::type_identity<counterparties::models::ListCounterpartiesRequest>{});
    const std::string organization = "00000000-0000-0000-0000-000000000001";
    const auto query = document.HasMember("facet")
        ? counterparties::helpers::BuildRegistryValues(request, organization,
              document["facet"].As<std::string>(),
              document["query"].As<std::optional<std::string>>(),
              document["after"].As<std::optional<std::string>>(), 50)
        : counterparties::helpers::BuildRegistryQuery(request, organization);
    std::cout << query.Statement.GetStatementView();
}
