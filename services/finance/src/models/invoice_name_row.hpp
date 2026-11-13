#pragma once
#include <boost/uuid/uuid.hpp>
#include <string>

namespace finance::models {
struct InvoiceNameRow {
    boost::uuids::uuid Id;
    std::string Number;
};
}  // namespace finance::models
