#pragma once
#include <memory>
#include <platform/auth/identity.hpp>
#include <stdexcept>

namespace platform::auth {
class UnknownKey final : public std::runtime_error {
  public:
    UnknownKey() : std::runtime_error("Unknown signing key") {}
};

struct Settings {
    std::string Issuer;
    std::string Audience;
    std::string RolesClient;
};

class Verifier final {
  public:
    Verifier(Settings settings, const std::string& jwks);
    ~Verifier();
    Identity Verify(const std::string& token) const;
    Identity VerifyLogin(const std::string& access, const std::string& id_token,
        const std::string& client, const std::string& nonce) const;

  private:
    Identity VerifyKind(const std::string& token, std::string_view kind) const;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace platform::auth
