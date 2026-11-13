#pragma once
#include <boost/uuid/string_generator.hpp>
#include <string>

namespace finance::helpers {

inline bool SameId(const std::string& left, const std::string& right) {
    return boost::uuids::string_generator{}(left) == boost::uuids::string_generator{}(right);
}

}  // namespace finance::helpers
