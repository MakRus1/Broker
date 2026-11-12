#pragma once
#include <userver/formats/json/value.hpp>

namespace sales::models {
struct StoredContact {
    userver::formats::json::Value Contact;
    userver::formats::json::Value Request;
};
}  // namespace sales::models
