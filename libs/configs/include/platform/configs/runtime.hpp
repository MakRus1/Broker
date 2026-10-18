#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <userver/dynamic_config/snapshot.hpp>
#include <userver/dynamic_config/source.hpp>
#include <userver/formats/json.hpp>

namespace platform::configs {

using Json = userver::formats::json::Value;

inline void ObjectKeys(const Json& value, const std::set<std::string>& allowed) {
    if (!value.IsObject())
        throw std::invalid_argument("Expected config object");
    for (auto it = value.begin(); it != value.end(); ++it) {
        if (!allowed.count(it.GetName()))
            throw std::invalid_argument("Unknown config field");
    }
}

struct Predicate {
    std::string Field;
    std::string Type;
    Json Value;
};

inline long double Number(const Json& value) {
    if (value.IsInt64())
        return value.As<std::int64_t>();
    return value.As<double>();
}

inline bool Match(const Predicate& predicate, const Json& context) {
    const bool present = context.HasMember(predicate.Field);
    if (predicate.Type == "exists")
        return present == predicate.Value.As<bool>();
    if (!present)
        return false;
    const auto actual = context[predicate.Field];
    if (predicate.Type == "eq")
        return actual == predicate.Value;
    if (predicate.Type == "in") {
        return std::any_of(predicate.Value.begin(), predicate.Value.end(),
                           [&](const auto& value) { return value == actual; });
    }
    if (actual.IsInt64() && predicate.Value.IsInt64()) {
        const auto left = actual.As<std::int64_t>(), right = predicate.Value.As<std::int64_t>();
        if (predicate.Type == "lt")
            return left < right;
        if (predicate.Type == "lte")
            return left <= right;
        if (predicate.Type == "gt")
            return left > right;
        if (predicate.Type == "gte")
            return left >= right;
    }
    const auto left = Number(actual), right = Number(predicate.Value);
    if (predicate.Type == "lt")
        return left < right;
    if (predicate.Type == "lte")
        return left <= right;
    if (predicate.Type == "gt")
        return left > right;
    if (predicate.Type == "gte")
        return left >= right;
    throw std::invalid_argument("Unknown config predicate");
}

template <typename Value, typename Context> struct Conditional {
    struct Clause {
        std::string Id;
        std::string Title;
        Value Result;
        std::vector<Predicate> Predicates;
    };
    Value Default;
    std::vector<Clause> Clauses;

    Value Resolve(const Context& context) const {
        const auto json = ToJson(context);
        for (const auto& clause : Clauses) {
            if (std::all_of(clause.Predicates.begin(), clause.Predicates.end(),
                            [&](const auto& predicate) { return Match(predicate, json); })) {
                return clause.Result;
            }
        }
        return Default;
    }
};

template <typename Value, typename Context, typename Parser, typename Validator>
Conditional<Value, Context> ParseConditional(const Json& json, Parser parser, Validator validator) {
    ObjectKeys(json, {"clauses", "default"});
    Conditional<Value, Context> result{parser(json["default"]), {}};
    const auto clauses = json["clauses"];
    if (!clauses.IsArray() || clauses.GetSize() > 1000)
        throw std::invalid_argument("Invalid config clauses");
    std::set<std::string> ids;
    for (const auto& clause : clauses) {
        ObjectKeys(clause, {"id", "title", "value", "predicates"});
        typename Conditional<Value, Context>::Clause item{
            clause.HasMember("id") ? clause["id"].As<std::string>() : std::string{},
            clause["title"].As<std::string>(),
            parser(clause["value"]),
            {}};
        if (clause.HasMember("id") && (item.Id.empty() || !ids.insert(item.Id).second))
            throw std::invalid_argument("Duplicate config clause id");
        const auto predicates = clause["predicates"];
        if (!predicates.IsArray() || predicates.GetSize() > 100)
            throw std::invalid_argument("Invalid config predicates");
        for (const auto& predicate : predicates) {
            ObjectKeys(predicate, {"field", "type", "value"});
            Predicate condition{predicate["field"].As<std::string>(),
                                predicate["type"].As<std::string>(), predicate["value"]};
            validator(condition);
            item.Predicates.push_back(std::move(condition));
        }
        result.Clauses.push_back(std::move(item));
    }
    return result;
}

template <typename Value> struct PlainKey {
    userver::dynamic_config::Key<Value> Native;
};
template <typename Value, typename Context> struct ConditionalKey {
    userver::dynamic_config::Key<Conditional<Value, Context>> Native;
};

class Snapshot {
  public:
    explicit Snapshot(userver::dynamic_config::Snapshot snapshot)
        : snapshot_(std::move(snapshot)) {}
    template <typename Value> Value Get(const PlainKey<Value>& key) const {
        return snapshot_[key.Native];
    }
    template <typename Value, typename Context>
    Value Get(const ConditionalKey<Value, Context>& key, const Context& context) const {
        return snapshot_[key.Native].Resolve(context);
    }

  private:
    userver::dynamic_config::Snapshot snapshot_;
};

class Source {
  public:
    explicit Source(userver::dynamic_config::Source source) : source_(std::move(source)) {}
    Snapshot GetSnapshot() const {
        return Snapshot{source_.GetSnapshot()};
    }

  private:
    userver::dynamic_config::Source source_;
};

struct HttpSettings {
    int Timeout;
    int MaxRetries;
};
struct HttpClientConfig {
    HttpSettings Default;
    std::map<std::string, HttpSettings> Operations;
    HttpSettings Get(const std::string& path) const {
        const auto it = Operations.find(path);
        return it == Operations.end() ? Default : it->second;
    }
};

inline HttpSettings ParseHttpSettings(const Json& value,
                                      std::optional<HttpSettings> fallback = {}) {
    ObjectKeys(value, {"timeout", "max_retries"});
    const auto timeout =
        value.HasMember("timeout")
            ? value["timeout"].As<int>()
            : (fallback ? fallback->Timeout : throw std::invalid_argument("Missing timeout"));
    const auto retries = value.HasMember("max_retries")
                             ? value["max_retries"].As<int>()
                             : (fallback ? fallback->MaxRetries
                                         : throw std::invalid_argument("Missing max_retries"));
    if (timeout < 1 || timeout > 3600000 || retries < 0 || retries > 10)
        throw std::invalid_argument("Invalid HTTP client settings");
    return {timeout, retries};
}
inline HttpClientConfig ParseHttpClient(const Json& value, const std::set<std::string>& paths) {
    if (!value.IsObject())
        throw std::invalid_argument("Expected HTTP client config object");
    HttpClientConfig result{ParseHttpSettings(value["**default**"]), {}};
    for (auto it = value.begin(); it != value.end(); ++it) {
        const auto name = it.GetName();
        if (name == "**default**")
            continue;
        if (!paths.count(name))
            throw std::invalid_argument("Unknown HTTP client path");
        result.Operations.emplace(name, ParseHttpSettings(*it, result.Default));
    }
    return result;
}

}  // namespace platform::configs
