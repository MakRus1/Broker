#pragma once
#include <clients/organizations/client.hpp>
#include <openapi/models.hpp>

namespace admin_bff {
inline models::Organization Convert(const clients::organizations::models::Organization& value) {
    return {
        .enabled = value.enabled, .id = value.id, .name = value.name, .revision = value.revision};
}

inline models::Member Convert(const clients::organizations::models::Member& value) {
    return {.display_name = value.display_name,
        .email = value.email,
        .enabled = value.enabled,
        .revision = value.revision,
        .role = models::ParseEnumScalar(
            clients::organizations::models::EnumValue(value.role), models::Member_role{}),
        .subject = value.subject,
        .username = value.username};
}

template <typename Value> auto ConvertOrganizations(const std::vector<Value>& values) {
    std::vector<decltype(Convert(std::declval<Value>()))> result;
    result.reserve(values.size());
    for (const auto& value : values)
        result.push_back(Convert(value));
    return result;
}

inline clients::organizations::ApiAdminOrganizationsCreate::RequestBody Convert(
    const models::CreateOrganizationRequest& value) {
    return {.id = value.id, .name = value.name};
}

inline clients::organizations::ApiAdminOrganizationsUpdate::RequestBody Convert(
    const models::UpdateOrganizationRequest& value) {
    return {.enabled = value.enabled,
        .expected_revision = value.expected_revision,
        .id = value.id,
        .name = value.name};
}

inline clients::organizations::ApiAdminOrganizationsMembersCreate::RequestBody Convert(
    const models::CreateMemberRequest& value) {
    return {.display_name = value.display_name,
        .email = value.email,
        .organization_id = value.organization_id,
        .request_id = value.request_id,
        .role = clients::organizations::models::ParseEnumScalar(models::EnumValue(value.role),
            clients::organizations::models::CreateMemberRequest_role{}),
        .temporary_password = value.temporary_password,
        .username = value.username};
}

inline clients::organizations::ApiAdminOrganizationsMembersAdd::RequestBody Convert(
    const models::AddMemberRequest& value) {
    return {.organization_id = value.organization_id,
        .role = clients::organizations::models::ParseEnumScalar(
            models::EnumValue(value.role), clients::organizations::models::AddMemberRequest_role{}),
        .username = value.username};
}

inline clients::organizations::ApiAdminOrganizationsMembersUpdate::RequestBody Convert(
    const models::UpdateMemberRequest& value) {
    return {.enabled = value.enabled,
        .expected_revision = value.expected_revision,
        .organization_id = value.organization_id,
        .role = clients::organizations::models::ParseEnumScalar(models::EnumValue(value.role),
            clients::organizations::models::UpdateMemberRequest_role{}),
        .subject = value.subject};
}
}  // namespace admin_bff
