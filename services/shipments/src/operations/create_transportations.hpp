#pragma once
#include <openapi/models.hpp>
#include <platform/auth/principal.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace shipments::operations {
models::CreationOperation CreateTransportations(const userver::storages::postgres::ClusterPtr&,
    const platform::auth::Principal&, const models::CreateOperationRequest&);
}  // namespace shipments::operations
