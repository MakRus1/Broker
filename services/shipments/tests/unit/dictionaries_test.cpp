#include <helpers/dictionaries.hpp>
#include <userver/utest/utest.hpp>

UTEST(ShipmentDictionary, KnownCodesAndRoundTrip) {
    using namespace shipments::helpers;
    for (const auto code : {"incoterms", "carriage_terms", "loading", "route_type"})
        EXPECT_EQ(DictionaryCode(ParseDictionary(code)), code);
}

UTEST(ShipmentDictionary, UnknownCodeIsNotSilentlyIgnored) {
    EXPECT_THROW(shipments::helpers::ParseDictionary("unknown_dictionary"), std::runtime_error);
}
