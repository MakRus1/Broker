#pragma once
#include <clients/counterparties/client.hpp>
#include <openapi/models.hpp>
#include <stdexcept>

namespace web_bff {
inline models::counterparties::Counterparty Convert(
    const clients::counterparties::models::Counterparty& value) {
    models::counterparties::Counterparty result;
    result.id = value.id;
    result.name = value.name;
    return result;
}

inline clients::counterparties::ApiV1CounterpartiesCreate::RequestBody Convert(
    const models::counterparties::CreateCounterpartyRequest& value) {
    return {.id = value.id, .name = value.name};
}

inline clients::counterparties::ApiV1CounterpartiesList::RequestBody Convert(
    const models::counterparties::ListCounterpartiesRequest& value) {
    return {.after = value.after, .limit = value.limit};
}
}  // namespace web_bff
