#include <jwt-cpp/jwt.h>
#include <map>
#include <openssl/pem.h>
#include <platform/auth/verifier.hpp>
#include <userver/crypto/base64.hpp>
#include <userver/crypto/public_key.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/parse/common_containers.hpp>

namespace {
std::string PublicPem(const userver::formats::json::Value& key) {
    using Key = userver::crypto::PublicKey;
    const auto n = userver::crypto::base64::Base64UrlDecode(key["n"].As<std::string>());
    const auto e = userver::crypto::base64::Base64UrlDecode(key["e"].As<std::string>());
    const auto public_key = Key::LoadRSAFromComponents(Key::ModulusView{n}, Key::ExponentView{e});
    if (EVP_PKEY_bits(public_key.GetNative()) < 2048)
        throw std::invalid_argument("Weak RSA key");
    std::unique_ptr<BIO, decltype(&BIO_free)> bio{BIO_new(BIO_s_mem()), BIO_free};
    if (!bio || PEM_write_bio_PUBKEY(bio.get(), public_key.GetNative()) != 1)
        throw std::runtime_error("Cannot serialize public key");
    char* data = nullptr;
    const auto size = BIO_get_mem_data(bio.get(), &data);
    return std::string{data, static_cast<std::size_t>(size)};
}
}  // namespace

namespace platform::auth {
struct Verifier::Impl {
    Settings Config;
    using Check = decltype(jwt::verify());
    std::map<std::string, Check> Keys;
};
Verifier::Verifier(Settings settings, const std::string& jwks) : impl_(std::make_unique<Impl>()) {
    if (settings.Issuer.empty() || settings.Audience.empty() || settings.RolesClient.empty())
        throw std::invalid_argument("Issuer, audience and roles client are required");
    impl_->Config = std::move(settings);
    if (jwks.size() > 131072)
        throw std::invalid_argument("JWKS is too large");
    const auto keys = userver::formats::json::FromString(jwks)["keys"];
    if (!keys.IsArray() || keys.GetSize() > 32)
        throw std::invalid_argument("Invalid JWKS");
    for (const auto& key : keys) {
        if (key["kty"].As<std::string>() != "RSA" || key["use"].As<std::string>("sig") != "sig" ||
            key["alg"].As<std::string>("RS256") != "RS256")
            continue;
        if (key.HasMember("key_ops")) {
            const auto operations = key["key_ops"].As<std::set<std::string>>();
            if (!operations.contains("verify"))
                continue;
        }
        const auto id = key["kid"].As<std::string>();
        if (id.empty() || impl_->Keys.contains(id))
            throw std::invalid_argument("Invalid key id");
        auto check = jwt::verify()
                         .allow_algorithm(jwt::algorithm::rs256{PublicPem(key)})
                         .with_issuer(impl_->Config.Issuer)
                         .with_audience(impl_->Config.Audience)
                         .leeway(10);
        impl_->Keys.emplace(id, std::move(check));
    }
    if (impl_->Keys.empty())
        throw std::invalid_argument("No supported signing keys");
}
Verifier::~Verifier() = default;
Principal Verifier::Verify(const std::string& token) const {
    if (token.empty() || token.size() > 16384)
        throw std::invalid_argument("Invalid token size");
    const auto decoded = jwt::decode(token);
    if (decoded.get_algorithm() != "RS256" || decoded.has_header_claim("crit") ||
        decoded.has_header_claim("jku") || decoded.has_header_claim("jwk") ||
        decoded.has_header_claim("x5u"))
        throw std::invalid_argument("Unsupported JWT header");
    const auto key = impl_->Keys.find(decoded.get_key_id());
    if (key == impl_->Keys.end())
        throw UnknownKey{};
    if (!decoded.has_expires_at() || !decoded.has_issued_at() || !decoded.has_subject())
        throw std::invalid_argument("Missing required claims");
    key->second.verify(decoded);
    // Keycloak отличает access token от ID/refresh token полем typ в payload.
    if (!decoded.has_payload_claim("typ") ||
        decoded.get_payload_claim("typ").as_string() != "Bearer")
        throw std::invalid_argument("Expected access token");
    const auto subject = decoded.get_subject();
    if (subject.empty() || subject.size() > 128 ||
        subject.find_first_of("\r\n\t") != std::string::npos)
        throw std::invalid_argument("Invalid subject");
    if (decoded.get_expires_at() <= decoded.get_issued_at())
        throw std::invalid_argument("Invalid lifetime");
    Principal principal{impl_->Config.Issuer, subject, {}};
    const auto payload = userver::formats::json::FromString(decoded.get_payload());
    const auto roles = payload["resource_access"][impl_->Config.RolesClient]["roles"];
    if (!roles.IsMissing())
        principal.Permissions = roles.As<std::set<std::string>>();
    return principal;
}
}  // namespace platform::auth
