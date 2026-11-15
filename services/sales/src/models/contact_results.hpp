#pragma once
#include <boost/uuid/uuid.hpp>
#include <openapi/models.hpp>

namespace sales::models {
struct ContactAddition {
    models::SalesCard card;
    models::SalesContact item;
};

struct ContactsPage {
    std::vector<models::SalesContact> items;
    std::optional<boost::uuids::uuid> next_before;
};

}  // namespace sales::models
