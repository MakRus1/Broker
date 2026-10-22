#pragma once
#include <helpers/access.hpp>

namespace organizations::helpers {
inline models::PlatformAccess Convert(const pgtypes::PlatformAccessV1& row) {
    return {.permissions = row.Permissions, .revision = row.Revision};
}

inline models::PlatformAccess ReadPlatformAccess(
    pg::Transaction& transaction, const std::string& issuer, const std::string& subject) {
    const auto rows = transaction.Execute(sql::kGetPlatformAccess, issuer, subject);
    return rows.IsEmpty() ? models::PlatformAccess{.permissions = {}, .revision = 0}
                          : Convert(rows.AsSingleRow<pgtypes::PlatformAccessV1>());
}
}  // namespace organizations::helpers
