#pragma once
#include <platform/auth/identity.hpp>
#include <userver/formats/json/value_builder.hpp>

namespace finance::helpers {
inline userver::formats::json::Value HistorySnapshot(
    const auto& value, const platform::auth::Identity& identity) {
    userver::formats::json::ValueBuilder snapshot(value);
    snapshot["actor_name"] = identity.DisplayName;
    return snapshot.ExtractValue();
}
}  // namespace finance::helpers
