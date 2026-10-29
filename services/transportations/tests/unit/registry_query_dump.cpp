// Печать реального SQL API для изолированных бенчмарков; не часть обычной сборки.
#include <helpers/registry_query.hpp>
#include <iostream>
#include <iterator>
#include <userver/formats/json.hpp>

int main() {
    const std::string input{std::istreambuf_iterator<char>{std::cin}, {}};
    const auto document = userver::formats::json::FromString(input);
    const auto& body = document.HasMember("request") ? document["request"] : document;
    const auto request = transportations::models::ParseModelJson(
        body, std::type_identity<transportations::models::ListTransportationsRequest>{});
    const std::string organization = "00000000-0000-0000-0000-000000000001";
    if (document.HasMember("route_group")) {
        const auto query = transportations::helpers::route_registry::BuildValues(request,
            organization, document["facet"].As<std::string>(), document["route_group"].As<int>(),
            document["query"].As<std::optional<std::string>>(),
            document["after"].As<std::optional<std::string>>(), 50, [&](auto& builder) {
                platform::registry::ApplyFilters(
                    builder, transportations::helpers::registry_fields::kFields, request.filters);
            });
        std::cout << query.Statement.GetStatementView();
        return 0;
    }
    const auto query = document.HasMember("facet")
        ? transportations::helpers::BuildRegistryValues(request, organization,
              document["facet"].As<std::string>(),
              document["query"].As<std::optional<std::string>>(),
              document["after"].As<std::optional<std::string>>(), 50)
        : transportations::helpers::BuildRegistryQuery(request, organization);
    std::cout << query.Statement.GetStatementView();
}
