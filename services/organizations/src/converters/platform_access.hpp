#pragma once
#include <models/pg/platform_access_raw.hpp>
#include <openapi/models.hpp>

namespace organizations::converters {
inline models::PlatformAccess Convert(const models::pg::PlatformAccessRaw& row) {
    return {.permissions = row.Permissions, .revision = row.Revision};
}

}  // namespace organizations::converters
