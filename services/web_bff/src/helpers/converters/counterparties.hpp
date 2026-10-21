#pragma once
#include <clients/counterparties/client.hpp>
#include <openapi/models.hpp>
#include <stdexcept>

namespace web_bff {
inline models::Counterparty Convert(const clients::counterparties::models::Counterparty& value) {
    models::Counterparty result;
    result.id = value.id;
    result.name = value.name;
    return result;
}

inline clients::counterparties::ApiV1CounterpartiesCreate::RequestBody Convert(
    const models::CreateCounterpartyRequest& value) {
    return {.id = value.id, .name = value.name};
}

inline clients::counterparties::ApiV1CounterpartiesList::RequestBody Convert(
    const models::ListCounterpartiesRequest& value) {
    return {.after = value.after, .limit = value.limit};
}
}  // namespace web_bff
