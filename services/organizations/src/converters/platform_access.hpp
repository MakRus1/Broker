#pragma once
#include <openapi/models.hpp>
#include <postgresql/types.hpp>

namespace organizations::converters {
inline models::PlatformAccess Convert(const pgtypes::PlatformAccessV1& row) {
    return {.permissions = row.Permissions, .revision = row.Revision};
}

}  // namespace organizations::converters
