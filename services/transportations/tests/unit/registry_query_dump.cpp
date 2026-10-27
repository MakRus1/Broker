// Вспомогательная утилита для EXPLAIN: печатает SQL, построенный тем же кодом, что и API.
#include <helpers/registry_query.hpp>
#include <iostream>
#include <iterator>
#include <userver/formats/json.hpp>

int main() {
    const std::string input{std::istreambuf_iterator<char>{std::cin}, {}};
    const auto request =
        transportations::models::ParseModelJson(userver::formats::json::FromString(input),
            std::type_identity<transportations::models::ListTransportationsRequest>{});
    const auto query = transportations::helpers::BuildRegistryQuery(
        request, "00000000-0000-0000-0000-000000000001");
    std::cout << query.Statement.GetStatementView();
}
