#pragma once
#include <openapi/models.hpp>

namespace sales::models {
struct ContactAddition {
    models::SalesCard card;
    models::SalesContact item;
};

struct ContactsPage {
    std::vector<models::SalesContact> items;
    std::optional<std::string> next_before;
};

}  // namespace sales::models
