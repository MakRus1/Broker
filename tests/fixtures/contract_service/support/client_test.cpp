#include <client_contract.hpp>
#include <userver/utest/utest.hpp>

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
    static_assert(std::is_void_v<api::GetItemsId::Response>);
    client.GetItemsId({.id = 42, .active = false});
    EXPECT_EQ(transport.last.path, "/items/42");
    ASSERT_EQ(transport.last.query.size(), 1);
    EXPECT_EQ(transport.last.query[0].second, "false");
    transport.reply.body = "unexpected";
    EXPECT_THROW(client.GetItemsId({.id = 1}), platform::http::ContractError);
}

UTEST(ClientContract, CreatedConflictAndBody) {
    MockTransport transport;
    api::Client client(transport);
    api::PostItemsId::Request request;
    request.id = 7;
    request.body.name = "Груз";
    request.body.tags = std::vector<std::string>{"море", "авто"};
    transport.reply = {201, "application/json; charset=utf-8", R"({"id":7})"};
    EXPECT_EQ(client.PostItemsId(request).id, 7);
    EXPECT_TRUE(transport.last.body.has_value());
    transport.reply = {409, "application/json", R"({"id":7})"};
    try {
        client.PostItemsId(request);
        FAIL() << "Ожидалась типизированная ошибка API";
    } catch (const api::PostItemsId::Error409& error) {
        EXPECT_EQ(error.GetStatus(), 409);
        EXPECT_EQ(error.response.id, 7);
        EXPECT_STREQ(error.what(), "HTTP API error 409");
    }
    transport.reply.body = R"({"id":"bad"})";
    EXPECT_THROW(client.PostItemsId(request), platform::http::ContractError);
    transport.reply.body = R"({"id":7})";
    EXPECT_THROW(client.PostItemsId(request), platform::http::ApiError);
    transport.reply = {201, "text/plain", R"({"id":7})"};
    EXPECT_THROW(client.PostItemsId(request), platform::http::ContractError);
    transport.reply = {201, "application/json", R"({"id":"bad"})"};
    EXPECT_THROW(client.PostItemsId(request), platform::http::ContractError);
}

UTEST(ClientContract, DefaultAndEnum) {
    api::GetValidated::Request request;
    EXPECT_EQ(request.limit, 10);
    EXPECT_EQ(request.status, api::GetValidated::status::New);
    request.limit = 20;
    const auto encoded = api::GetValidated::Encode(request);
    EXPECT_EQ(encoded.query.size(), 2);
    request.limit = 0;
    EXPECT_THROW(api::GetValidated::Encode(request), std::invalid_argument);
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
    api::PostValidated::Request request;
    request.body.status = api::PostValidated::status::New;
    EXPECT_EQ(request.body.count, 3);
    EXPECT_EQ(request.body.labels, (std::vector<std::string>{"a"}));
    auto encoded = api::PostValidated::Encode(request);
    const auto json = userver::formats::json::FromString(*encoded.body);
    EXPECT_EQ(json["status"].As<std::string>(), "new");
    EXPECT_EQ(json["count"].As<int>(), 3);
    request.body.created = "2025-02-29T12:00:00Z";
    EXPECT_THROW(api::PostValidated::Encode(request), std::invalid_argument);
    request.body.created.reset();
    request.body.labels = {"same", "same"};
    EXPECT_THROW(api::PostValidated::Encode(request), std::invalid_argument);
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
