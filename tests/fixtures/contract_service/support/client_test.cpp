#include <client_contract.hpp>
#include <userver/utest/utest.hpp>
#include <views/models/echo/responses.hpp>

namespace api = client_test::clients::contract;

namespace {
class MockTransport final : public platform::http::Transport {
  public:
    platform::http::Request last;
    platform::http::Response reply{204, "", ""};

    platform::http::Response Perform(const platform::http::Request& request) override {
        last = request;
        return reply;
    }
};
}  // namespace

UTEST(ClientContract, PathQueryAndEmptyResponse) {
    MockTransport transport;
    api::Client client(transport);
    static_assert(std::is_void_v<api::ItemsById::Response>);
    client.ItemsById({.id = 42, .active = false});
    EXPECT_EQ(transport.last.path, "/items/42");
    EXPECT_EQ(transport.last.path_template, "/items/{id}");
    ASSERT_EQ(transport.last.query.size(), 1);
    EXPECT_EQ(transport.last.query[0].second, "false");
    transport.reply.body = "unexpected";
    EXPECT_THROW(client.ItemsById({.id = 1}), platform::http::ContractError);
}

UTEST(ClientContract, CreatedConflictAndBody) {
    MockTransport transport;
    api::Client client(transport);
    api::ItemsByIdCreate::Request request;
    request.id = 7;
    request.body.name = "Груз";
    request.body.tags = std::vector<std::string>{"море", "авто"};
    transport.reply = {201, "application/json; charset=utf-8", R"({"id":7})"};
    EXPECT_EQ(client.ItemsByIdCreate(request).id, 7);
    EXPECT_TRUE(transport.last.body.has_value());
    transport.reply = {409, "application/json", R"({"id":7})"};
    try {
        client.ItemsByIdCreate(request);
        FAIL() << "Ожидалась типизированная ошибка API";
    } catch (const api::ItemsByIdCreate::Error409& error) {
        EXPECT_EQ(error.GetStatus(), 409);
        EXPECT_EQ(error.response.id, 7);
        EXPECT_STREQ(error.what(), "HTTP API error 409");
    }
    transport.reply.body = R"({"id":"bad"})";
    EXPECT_THROW(client.ItemsByIdCreate(request), platform::http::ContractError);
    transport.reply.body = R"({"id":7})";
    EXPECT_THROW(client.ItemsByIdCreate(request), platform::http::ApiError);
    transport.reply = {201, "text/plain", R"({"id":7})"};
    EXPECT_THROW(client.ItemsByIdCreate(request), platform::http::ContractError);
    transport.reply = {201, "application/json", R"({"id":"bad"})"};
    EXPECT_THROW(client.ItemsByIdCreate(request), platform::http::ContractError);
}

UTEST(ClientContract, SharedStatusErrorsKeepOperationTypes) {
    static_assert(
        std::is_base_of_v<platform::http::ErrorByStatus<409>, api::ItemsByIdCreate::Error409>);
    static_assert(
        std::is_base_of_v<platform::http::ErrorByStatus<409>, api::MultipleSuccesses::Error409>);
    static_assert(std::is_base_of_v<platform::http::ApiError, platform::http::ErrorByStatus<409>>);
    static_assert(
        !std::is_base_of_v<platform::http::ErrorByStatus<404>, api::ItemsByIdCreate::Error409>);
    const platform::http::Response response{409, "application/json", R"({"id":42})"};
    try {
        api::ItemsByIdCreate::Decode(response);
        FAIL() << "Ожидалась общая ошибка статуса";
    } catch (const platform::http::ErrorByStatus<409>& error) {
        EXPECT_EQ(error.GetStatus(), 409);
        EXPECT_EQ(error.GetBody(), R"({"id":42})");
        const auto* operation = dynamic_cast<const api::ItemsByIdCreate::Error409*>(&error);
        ASSERT_NE(operation, nullptr);
        EXPECT_EQ(operation->response.id, 42);
    }
    EXPECT_THROW(api::MultipleSuccesses::Decode(response), platform::http::ErrorByStatus<409>);
    EXPECT_THROW(api::EmptyError::Decode({404, "", ""}), platform::http::ErrorByStatus<404>);
    EXPECT_THROW(api::ItemsByIdCreate::Decode({409, "application/json", R"({"id":"bad"})"}),
        platform::http::ContractError);
    // Необъявленный статус остаётся нарушением контракта, а не ошибкой API.
    EXPECT_THROW(api::ItemsByIdCreate::Decode({403, "application/json", "{}"}),
        platform::http::ContractError);
}

UTEST(ClientContract, DefaultAndEnum) {
    api::Validated::Request request;
    EXPECT_EQ(request.limit, 10);
    EXPECT_EQ(request.status, api::Validated::status::New);
    request.limit = 20;
    const auto encoded = api::Validated::Encode(request);
    EXPECT_EQ(encoded.query.size(), 2);
    request.limit = 0;
    EXPECT_THROW(api::Validated::Encode(request), std::invalid_argument);
}

UTEST(ClientContract, PathEscaping) {
    std::string path = "/items/{id}";
    platform::http::ReplacePath(path, "{id}", "A /?+Б");
    EXPECT_EQ(path, "/items/A%20%2F%3F%2B%D0%91");
    path = "/items/{id}";
    platform::http::ReplacePath(path, "{id}", "..");
    EXPECT_EQ(path, "/items/%2E%2E");
}

UTEST(ClientContract, BodyConstraintsAndDefaults) {
    api::ValidatedCreate::Request request;
    request.body.status = api::ValidatedCreate::status::New;
    EXPECT_EQ(request.body.count, 3);
    EXPECT_EQ(request.body.labels, (std::vector<std::string>{"a"}));
    auto encoded = api::ValidatedCreate::Encode(request);
    const auto json = userver::formats::json::FromString(*encoded.body);
    EXPECT_EQ(json["status"].As<std::string>(), "new");
    EXPECT_EQ(json["count"].As<int>(), 3);
    EXPECT_THROW(api::ValidatedCreate::ParseBody(userver::formats::json::FromString(
                     R"({"status":"new","created":"2025-02-29T12:00:00Z"})")),
        std::exception);
    request.body.labels = {"same", "same"};
    EXPECT_THROW(api::ValidatedCreate::Encode(request), std::invalid_argument);
}

UTEST(ClientContract, MultipleSuccessfulResponses) {
    using Api = api::MultipleSuccesses::Response;
    static_assert(std::variant_size_v<Api> == 2);
    const auto response = api::MultipleSuccesses::Decode({200, "application/json", R"({"id":42})"});
    EXPECT_EQ(std::get<api::MultipleSuccesses::Response200>(response).id, 42);
    EXPECT_TRUE(std::holds_alternative<api::MultipleSuccesses::Response204>(
        api::MultipleSuccesses::Decode({204, "", ""})));
    EXPECT_THROW(api::MultipleSuccesses::Decode({409, "application/json", R"({"id":42})"}),
        api::MultipleSuccesses::Error409);
}

UTEST(ClientContract, OnlyErrorsAndEmptyError) {
    static_assert(std::is_void_v<api::ErrorsOnly::Response>);
    EXPECT_THROW(api::ErrorsOnly::Decode({409, "application/json", R"({"id":42})"}),
        api::ErrorsOnly::Error409);
    EXPECT_THROW(api::ErrorsOnly::Decode({200, "application/json", R"({"id":42})"}),
        platform::http::ContractError);
    EXPECT_THROW(api::EmptyError::Decode({404, "", ""}), api::EmptyError::Error404);
    EXPECT_THROW(api::EmptyError::Decode({404, "", "unexpected"}), platform::http::ContractError);
}

UTEST(ClientContract, ConfigsAdminJsonObjects) {
    namespace config_api = client_test::clients::configs_api;
    MockTransport transport;
    config_api::Client client(transport);
    config_api::AdminConfigsCurrent::Request request;
    request.body.config_name = "SHIPMENT_TERMS_CONFIG";
    transport.reply = {200, "application/json",
        R"({"config_name":"SHIPMENT_TERMS_CONFIG","version":5,"value":{"nested":[{"enabled":true}]}})"};
    const auto response = client.AdminConfigsCurrent(request, "test-access-token");
    EXPECT_EQ(transport.last.path, "/admin/configs/current");
    ASSERT_TRUE(response.value.has_value());
    EXPECT_TRUE(response.value.value()["nested"][0]["enabled"].As<bool>());
    transport.reply = {
        400, "application/json", R"({"error":"invalid_request","message":"Bad config"})"};
    EXPECT_THROW(client.AdminConfigsCurrent(request, "test-access-token"),
        config_api::AdminConfigsCurrent::Error400);
    transport.reply = {200, "application/json",
        R"({"items":[{"active_draft_id":null,"config_name":"example","version":1}]})"};
    const auto catalog = client.AdminConfigsList({}, "test-access-token");
    ASSERT_EQ(catalog.items.size(), 1);
    EXPECT_EQ(catalog.items.front().config_name, "example");
    transport.reply = {200, "application/json", R"({"items":[42]})"};
    EXPECT_THROW(client.AdminConfigsList({}, "test-access-token"), platform::http::ContractError);
}

UTEST(ClientContract, SharedNestedModelsAndTypedErrors) {
    namespace api = client_test::clients::contract;
    MockTransport transport;
    api::Client client(transport);
    api::ModelsEcho::Request request;
    request.body.profile.id = 7;
    request.body.profile.address.city = "Москва";
    request.body.profiles.push_back(request.body.profile);
    request.body.note.emplace(std::nullopt);
    transport.reply = {200, "application/json",
        R"({"profile":{"id":7,"address":{"city":"Москва"}},"profiles":[{"id":7,"address":{"city":"Москва"}}],"note":null})"};
    const auto response = client.ModelsEcho(request);
    EXPECT_EQ(response.profile.address.city, "Москва");
    EXPECT_EQ(response.profile.state, api::models::State::Active);
    EXPECT_EQ(response.profile.address.kind, api::models::Address_kind::City);
    EXPECT_EQ(response.limit, 5);
    ASSERT_TRUE(response.note.has_value());
    EXPECT_FALSE(response.note->has_value());
    const auto encoded = userver::formats::json::FromString(*transport.last.body);
    EXPECT_TRUE(encoded["note"].IsNull());
    EXPECT_EQ(encoded["profiles"][0]["state"].As<std::string>(), "active");

    for (const auto& invalid :
        {R"({"profile":{"id":0,"address":{"city":"Москва"}},"profiles":[{"id":1,"address":{"city":"Москва"}}]})",
            R"({"profile":{"id":1,"address":{"city":""}},"profiles":[{"id":1,"address":{"city":"Москва"}}]})",
            R"({"profile":{"id":1,"address":{"city":"Москва"}},"profiles":[{}]})"}) {
        transport.reply = {200, "application/json", invalid};
        EXPECT_THROW(client.ModelsEcho(request), platform::http::ContractError);
    }
    transport.reply = {400, "application/json", R"({"error":"invalid_request","message":"Bad"})"};
    EXPECT_THROW(client.ModelsEcho(request), api::ModelsEcho::Error400);
    transport.reply.status = 409;
    EXPECT_THROW(client.ModelsEcho(request), api::ModelsEcho::Error409);
    request.body.profiles.front().address.city = "";
    EXPECT_THROW(api::ModelsEcho::Encode(request), std::invalid_argument);
}

UTEST(ClientContract, SharedModelPresenceAndNull) {
    namespace api = client_test::clients::contract;
    const auto absent = broker_codegen::ParseJson<
        api::models::Payload>(userver::formats::json::FromString(
        R"({"profile":{"id":1,"address":{"city":"Москва"}},"profiles":[{"id":1,"address":{"city":"Москва"}}]})"));
    EXPECT_EQ(absent.limit, 5);
    EXPECT_FALSE(absent.WasProvided("limit"));
    EXPECT_FALSE(absent.note.has_value());
    auto json = userver::formats::json::ValueBuilder{
        userver::formats::json::ValueBuilder(absent).ExtractValue()};
    json["note"] = nullptr;
    const auto explicit_null = broker_codegen::ParseJson<api::models::Payload>(json.ExtractValue());
    EXPECT_TRUE(explicit_null.WasProvided("note"));
    ASSERT_TRUE(explicit_null.note.has_value());
    EXPECT_FALSE(explicit_null.note->has_value());
}

UTEST(ServerContract, NestedResponseValidation) {
    namespace api = contract_service::views::models::echo;
    api::Response200 response;
    response.profile.id = 1;
    response.profile.address.city = "Москва";
    response.profiles.push_back(response.profile);
    EXPECT_NO_THROW(api::ToJson(response));
    response.profiles.front().address.city = "";
    EXPECT_THROW(api::ToJson(response), std::invalid_argument);
}

UTEST(ClientContract, BearerTokenIsExplicit) {
    namespace api = client_test::clients::configs_api;
    api::AdminConfigsCurrent::Request request;
    request.body.config_name = "SHIPMENT_TERMS_CONFIG";
    const auto encoded = api::AdminConfigsCurrent::Encode(request, "test-access-token");
    ASSERT_TRUE(encoded.bearer_token);
    EXPECT_EQ(*encoded.bearer_token, "test-access-token");
    ASSERT_TRUE(encoded.body);
    EXPECT_THROW(api::AdminConfigsCurrent::Encode(request, ""), std::invalid_argument);
    EXPECT_THROW(api::AdminConfigsCurrent::Encode(request, "token\r\nInjected: value"),
        std::invalid_argument);
}

UTEST(ClientContract, JsonBoundaryForBff) {
    namespace api = client_test::clients::configs_api::AdminConfigsCurrent;
    const auto body = api::ParseBody(
        userver::formats::json::FromString(R"({"config_name":"SHIPMENT_TERMS_CONFIG"})"));
    EXPECT_EQ(body.config_name, "SHIPMENT_TERMS_CONFIG");
    EXPECT_THROW(
        api::ParseBody(userver::formats::json::FromString(R"({"config_name":5})")), std::exception);
    EXPECT_THROW(
        api::ParseBody(userver::formats::json::FromString(R"({"config_name":"A","actor":"fake"})")),
        std::exception);

    const platform::http::Response response{
        409, "application/json", R"({"error":"conflict","message":"Version changed"})"};
    try {
        (void)api::Decode(response);
        FAIL() << "Expected typed conflict";
    } catch (const api::Error409& error) {
        EXPECT_EQ(error.GetStatus(), 409);
        EXPECT_EQ(error.response.message, "Version changed");
        const auto json = userver::formats::json::FromString(error.GetBody());
        EXPECT_EQ(json["error"].As<std::string>(), "conflict");
        EXPECT_EQ(json["message"].As<std::string>(), error.response.message);
    }
}

UTEST(CalendarDateClient, QueryAndJsonUseIsoWireRepresentation) {
    const auto day = userver::utils::datetime::DateFromRFC3339String("2000-02-29");
    const auto request = api::CalendarCheck::Encode(api::CalendarCheck::Request{.day = day});
    ASSERT_EQ(request.query.size(), 1);
    EXPECT_EQ(request.query.front().first, "day");
    EXPECT_EQ(request.query.front().second, "2000-02-29");
    const auto body =
        api::CalendarEcho::ToJson(api::CalendarEcho::RequestBody{.day = day, .days = {day}});
    const auto value = userver::formats::json::FromString(body);
    EXPECT_EQ(value["day"].As<std::string>(), "2000-02-29");
    EXPECT_EQ(value["days"][0].As<std::string>(), "2000-02-29");
}

UTEST(CalendarDateClient, InstantPreservesMomentAndPrecision) {
    const auto instant =
        userver::utils::datetime::FromRfc3339StringSaturating<std::chrono::system_clock::duration>(
            "2026-10-25T12:34:56.123456+03:00");
    const auto body = api::CalendarEcho::ToJson(api::CalendarEcho::RequestBody{.instant = instant});
    const auto json = userver::formats::json::FromString(body);
    EXPECT_EQ(json["instant"].As<std::chrono::system_clock::time_point>(), instant);
    EXPECT_EQ(platform::http::Scalar(instant).substr(0, 19), "2026-10-25T09:34:56");
}
