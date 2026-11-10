#pragma once
#include <components/directory_sync.hpp>

namespace transport_locations::converters {
template <typename Settings> SyncPolicy MakeSyncPolicy(const Settings& value) {
    return {value.Enabled, static_cast<int>(value.PeriodSeconds),
        std::chrono::milliseconds{value.HttpTimeoutMs}, static_cast<int>(value.HttpRetries),
        static_cast<int>(value.RetryInitialSeconds), static_cast<int>(value.RetryMaxSeconds)};
}

}  // namespace transport_locations::converters
