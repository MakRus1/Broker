#include <configs/keys.hpp>
#include <userver/dynamic_config/storage_mock.hpp>
#include <userver/utest/utest.hpp>

namespace {
using Json = platform::configs::Json;
Json JsonValue(const std::string& text) {
    return userver::formats::json::FromString(text);
}
const auto kTerms =
    R"({"clauses":[{"id":"sea","title":"Море","value":{"is_enabled":true,"max_days":60},"predicates":[{"field":"transport_mode","type":"eq","value":"sea"}]}],"default":{"is_enabled":false,"max_days":30}})";
}  // namespace

UTEST(DynamicConfigs, SnapshotAndFirstMatchingClause) {
    const auto value = configs::detail_SHIPMENT_TERMS_CONFIG::ParseConfig(JsonValue(kTerms));
    userver::dynamic_config::StorageMock storage{{configs::SHIPMENT_TERMS_CONFIG.Native, value}};
    platform::configs::Source source{storage.GetSource()};
    const auto old = source.GetSnapshot();
    config_contexts::ShipmentContext context;
    EXPECT_FALSE(old.Get(configs::SHIPMENT_TERMS_CONFIG, context).IsEnabled);
    context.TransportMode = config_contexts::detail_SHIPMENT_CONTEXT::transport_mode::Sea;
    EXPECT_EQ(old.Get(configs::SHIPMENT_TERMS_CONFIG, context).MaxDays, 60);
    auto updated = userver::formats::json::ValueBuilder{JsonValue(kTerms)};
    updated["clauses"][0]["value"]["max_days"] = 90;
    storage.Extend({{configs::SHIPMENT_TERMS_CONFIG.Native,
                     configs::detail_SHIPMENT_TERMS_CONFIG::ParseConfig(updated.ExtractValue())}});
    EXPECT_EQ(source.GetSnapshot().Get(configs::SHIPMENT_TERMS_CONFIG, context).MaxDays, 90);
    EXPECT_EQ(old.Get(configs::SHIPMENT_TERMS_CONFIG, context).MaxDays, 60);
}

UTEST(DynamicConfigs, RejectInvalidValuesAndPredicates) {
    const std::vector<std::string> values{
        R"({"clauses":[],"default":{"max_days":30}})",
        R"({"clauses":[],"default":{"is_enabled":true,"max_days":0}})",
        R"({"clauses":[{"id":"x","title":"x","value":{"is_enabled":true,"max_days":1},"predicates":[{"field":"unknown","type":"eq","value":"a"}]}],"default":{"is_enabled":false,"max_days":30}})",
        R"({"clauses":[{"id":"x","title":"x","value":{"is_enabled":true,"max_days":1},"predicates":[{"field":"transport_mode","type":"lt","value":"sea"}]}],"default":{"is_enabled":false,"max_days":30}})"};
    for (const auto& value : values)
        EXPECT_ANY_THROW(configs::detail_SHIPMENT_TERMS_CONFIG::ParseConfig(JsonValue(value)));
}

UTEST(DynamicConfigs, HttpSettingsInheritance) {
    const auto value = configs::detail_BROKER_HTTP_CLIENT_CONFIG::ParseValue(
        JsonValue(R"({"**default**":{"timeout":100,"max_retries":1},"/hello":{"timeout":500}})"));
    EXPECT_EQ(value.Get("/hello").Timeout, 500);
    EXPECT_EQ(value.Get("/hello").MaxRetries, 1);
    EXPECT_EQ(value.Get("/foo/bar").Timeout, 100);
    EXPECT_ANY_THROW(configs::detail_BROKER_HTTP_CLIENT_CONFIG::ParseValue(
        JsonValue(R"({"**default**":{"timeout":100},"/unknown":{"timeout":100}})")));
}

UTEST(DynamicConfigs, FirstClauseAndMissingContext) {
    auto json = userver::formats::json::ValueBuilder{JsonValue(kTerms)};
    json["clauses"][0].Remove("id");
    auto second = userver::formats::json::ValueBuilder{JsonValue(kTerms)["clauses"][0]};
    second.Remove("id");
    second["value"]["max_days"] = 90;
    json["clauses"].PushBack(second.ExtractValue());
    const auto parsed = configs::detail_SHIPMENT_TERMS_CONFIG::ParseConfig(json.ExtractValue());
    config_contexts::ShipmentContext context;
    EXPECT_EQ(parsed.Resolve(context).MaxDays, 30);
    context.TransportMode = config_contexts::detail_SHIPMENT_CONTEXT::transport_mode::Sea;
    EXPECT_EQ(parsed.Resolve(context).MaxDays, 60);
    EXPECT_EQ(parsed.Resolve(context).Description, "");
}

UTEST(DynamicConfigs, IntegerComparisonKeepsPrecision) {
    const auto context = JsonValue(R"({"counter":9007199254740992})");
    const platform::configs::Predicate predicate{"counter", "lt", JsonValue("9007199254740993")};
    EXPECT_TRUE(platform::configs::Match(predicate, context));
    EXPECT_TRUE(platform::configs::Match({"absent", "exists", JsonValue("false")}, context));
    EXPECT_FALSE(platform::configs::Match({"absent", "eq", JsonValue("false")}, context));
}
